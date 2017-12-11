/* $(CROSS_COMPILE)cc -Wall       -g -o usb usb.c usbstring.c -lpthread
 *	-OR-
 * $(CROSS_COMPILE)cc -Wall -DAIO -g -o usb usb.c usbstring.c -lpthread -laio
 */

/*
 * this is an example pthreaded USER MODE driver implementing a
 * USB Gadget/Device with simple bulk source/sink functionality.
 * you could implement pda sync software this way, or some usb class
 * protocols (printers, test-and-measurement equipment, and so on).
 *
 * with hardware that also supports isochronous data transfers, this
 * can stream data using multi-buffering and AIO.  that's the way to
 * handle audio or video data, where on-time delivery is essential.
 *
 * needs "gadgetfs" and a supported USB device controller driver
 * in the kernel; this autoconfigures, based on the driver it finds.
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <memory.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>

#include <asm/byteorder.h>

#include <linux/types.h>
#include <linux/usb/gadgetfs.h>
#include <linux/usb/ch9.h>


#include "usbstring.h"
#include "tmc.h"
#include "scpi_msg_adpt.h"


static int verbose;
static int pattern;


/* Thanks to NetChip Technologies for donating this product ID.
 *
 * DO NOT REUSE THESE IDs with any protocol-incompatible driver!!  Ever!!
 * Instead:  allocate your own, using normal USB-IF procedures.
 */
#define DRIVER_VENDOR_NUM	0XC03E//0x0525		/* NetChip */
#define DRIVER_ISO_PRODUCT_NUM	0XB007//0xa4a3		/* user mode iso sink/src */
#define DRIVER_PRODUCT_NUM	0xa4a4		/* user mode sink/src */

#define source_open(name) \
	ep_config(name,__FUNCTION__, &fs_source_desc, &hs_source_desc)
#define sink_open(name) \
	ep_config(name,__FUNCTION__, &fs_sink_desc, &hs_sink_desc)

/* full duplex data, with at least three threads: ep0, sink, and source */

static pthread_t	ep0;

static pthread_t	source;
static int		source_fd = -1;

static pthread_t	sink;
static int		sink_fd = -1;
/* these descriptors are modified based on what controller we find */

#define	STRINGID_MFGR		1
#define	STRINGID_PRODUCT	2
#define	STRINGID_SERIAL		3
#define	STRINGID_CONFIG		4
#define	STRINGID_INTERFACE	5


/* kernel drivers could autoconfigure like this too ... if
 * they were willing to waste the relevant code/data space.
 */

static int	HIGHSPEED;
static char	*DEVNAME;
static char	*EP_IN_NAME, *EP_OUT_NAME, *EP_STATUS_NAME;

struct  usb_tmc_get_capabilities_response {
	unsigned char USBTMC_status;
	unsigned char reserved0;
	unsigned short bcdUSBTMC;
	unsigned char device_capabilities;  /* bitmap! */
	unsigned char interface_capabilities;  /* bitmap! */
	unsigned char reserved_subclass[12];
		unsigned char reserved1[6];
}__attribute__ ((packed));

static const struct usb_tmc_get_capabilities_response capabilities = {
	.USBTMC_status = 0x01,
	.reserved0 = 0,
	.bcdUSBTMC = 0x0100,
	.device_capabilities = 0,
	.interface_capabilities = 0x01,
	.reserved_subclass = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x07,0x0f,0x00,0x00},
	.reserved1 = {0},
};
	
static struct usb_device_descriptor
device_desc = {
	.bLength =		sizeof device_desc,
	.bDescriptorType =	USB_DT_DEVICE,

	.bcdUSB =		__constant_cpu_to_le16 (0x0200),
	.bDeviceClass =		0,//USB_CLASS_VENDOR_SPEC,
	.bDeviceSubClass =	0,
	.bDeviceProtocol =	0,
	// .bMaxPacketSize0 ... set by gadgetfs
	.idVendor =		__constant_cpu_to_le16 (DRIVER_VENDOR_NUM),
	.idProduct =		__constant_cpu_to_le16 (DRIVER_PRODUCT_NUM),
	.bcdDevice = 0x0001,
	.iManufacturer =	STRINGID_MFGR,
	.iProduct =		STRINGID_PRODUCT,
	.iSerialNumber =	STRINGID_SERIAL,
	.bNumConfigurations =	1,
};

static const struct usb_interface_descriptor 
	tmc_interface =
{
	.bLength = sizeof tmc_interface,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_APP_SPEC,
	.bInterfaceSubClass = 0x03,
	.bInterfaceProtocol = 0x01,
	.iInterface = 5,
};
	
#define	MAX_USB_POWER		1
#define	CONFIG_VALUE		1

static const struct usb_config_descriptor
config = {
	.bLength =		USB_DT_CONFIG_SIZE,//sizeof config,
	.bDescriptorType =	USB_DT_CONFIG,
	.wTotalLength = 0x20,//...
	/* must compute wTotalLength ... */
	.bNumInterfaces =	1,
	.bConfigurationValue =	CONFIG_VALUE,
	.iConfiguration =	STRINGID_CONFIG,
	.bmAttributes =		USB_CONFIG_ATT_ONE,//| USB_CONFIG_ATT_SELFPOWER,
	.bMaxPower =		1,//(MAX_USB_POWER + 1) / 2,
};

static struct usb_interface_descriptor
source_sink_intf = {
#if 1
	.bLength =		sizeof source_sink_intf,
	.bDescriptorType =	USB_DT_INTERFACE,

	.bInterfaceClass =	USB_CLASS_VENDOR_SPEC,
	.iInterface =		STRINGID_INTERFACE,
#else
	.bLength = sizeof source_sink_intf,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_APP_SPEC,
	.bInterfaceSubClass = 0x03,
	.bInterfaceProtocol = 0x01,
	.iInterface = 5,
#endif

};

/* Full speed configurations are used for full-speed only devices as
 * well as dual-speed ones (the only kind with high speed support).
 */

static struct usb_endpoint_descriptor
fs_source_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	/* NOTE some controllers may need FS bulk max packet size
	 * to be smaller.  it would be a chip-specific option.
	 */
	.wMaxPacketSize =	__constant_cpu_to_le16 (USB_BUFSIZE),
	.bInterval = 0,
};

static struct usb_endpoint_descriptor
fs_sink_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16 (USB_BUFSIZE),
};

/* some devices can handle other status packet sizes */
//#define STATUS_MAXPACKET	8
#define	LOG2_STATUS_POLL_MSEC	3

static struct usb_endpoint_descriptor
fs_status_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	__constant_cpu_to_le16 (USB_BUFSIZE),
	.bInterval =	0,//(1 << LOG2_STATUS_POLL_MSEC),
};

static const struct usb_endpoint_descriptor *fs_eps [3] = {
	&fs_source_desc,
	&fs_sink_desc,
	&fs_status_desc,
};


/* High speed configurations are used only in addition to a full-speed
 * ones ... since all high speed devices support full speed configs.
 * Of course, not all hardware supports high speed configurations.
 */

static struct usb_endpoint_descriptor
hs_source_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16 (USB_BUFSIZE),
};

static struct usb_endpoint_descriptor
hs_sink_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16 (USB_BUFSIZE),
	.bInterval =		0,//1,
};

static struct usb_endpoint_descriptor
hs_status_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	__constant_cpu_to_le16 (USB_BUFSIZE),
	.bInterval =		0,//LOG2_STATUS_POLL_MSEC + 3,
};

static const struct usb_endpoint_descriptor *hs_eps [] = {
	&hs_source_desc,
	&hs_sink_desc,
	&hs_status_desc,
};


/*-------------------------------------------------------------------------*/

//static char serial [64];

static struct usb_string stringtab [] = {
	{ STRINGID_MFGR,	"UNI-T", },
	{ STRINGID_PRODUCT,	"OSCILLOSCOPE", },
	{ STRINGID_SERIAL,	"UPO3000CS", },//serial
	{ STRINGID_CONFIG,	"DEMO", },
	{ STRINGID_INTERFACE,	"Source/Sink", },
};
static struct usb_gadget_strings strings = {
	.language =	0x0409,		/* "en-us" */
	.strings =	stringtab,
};



/* gadgetfs currently has no chunking (or O_DIRECT/zerocopy) support
 * to turn big requests into lots of smaller ones; so this is "small".
 */


static enum usb_device_speed	current_speed;

static inline int min(unsigned a, unsigned b)
{
	return (a < b) ? a : b;
}

static int autoconfig ()
{
	struct stat	statb;
	if(stat(DEVNAME = "/dev/gadget/musb-hdrc", &statb) == 0) 
	{
        HIGHSPEED = 1;
        device_desc.bcdDevice = __constant_cpu_to_le16(0x0100),
        fs_source_desc.bEndpointAddress = hs_source_desc.bEndpointAddress
                            			= USB_DIR_IN | 0x01;
        EP_IN_NAME = "/dev/gadget/ep1in";
        fs_sink_desc.bEndpointAddress = hs_sink_desc.bEndpointAddress
                                      = USB_DIR_OUT | 0x03;
        EP_OUT_NAME = "/dev/gadget/ep3out";
        source_sink_intf.bNumEndpoints = 2;
		usb_debug("~");
    }
	else
	{
		DEVNAME = 0;
		return -ENODEV;
	}
	return 0;
}


// FIXME no status i/o yet

static void close_fd (void *fd_ptr)
{
	int	status, fd;

	fd = *(int *)fd_ptr;
	*(int *)fd_ptr = -1;

	/* test the FIFO ioctls (non-ep0 code paths) */
	if (pthread_self () != ep0) 
	{
		status = ioctl (fd, GADGETFS_FIFO_STATUS);
		if (status < 0) {
			/* ENODEV reported after disconnect */
			if (errno != ENODEV && errno != -EOPNOTSUPP)
				perror ("get fifo status");
		}
		else 
		{
			fprintf (stderr, "fd %d, unclaimed = %d\n",fd, status);
			if (status) 
			{
				status = ioctl (fd, GADGETFS_FIFO_FLUSH);
				if (status < 0)
					perror ("fifo flush");
			}
		}
	}

	if (close (fd) < 0)
		perror ("close");
}


/* you should be able to open and configure endpoints
 * whether or not the host is connected
 */
static int
ep_config (char *name, const char *label,
	struct usb_endpoint_descriptor *fs,
	struct usb_endpoint_descriptor *hs
)
{
	int		fd, status;
	char		buf [USB_BUFSIZE];

	/* open and initialize with endpoint descriptor(s) */
	fd = open (name, O_RDWR);
	if (fd < 0) {
		status = -errno;
		fprintf (stderr, "%s open %s error %d (%s)\n",
			label, name, errno, strerror (errno));
		return status;
	}

	/* one (fs or ls) or two (fs + hs) sets of config descriptors */
	*(__u32 *)buf = 1;	/* tag for this format */
	memcpy (buf + 4, fs, USB_DT_ENDPOINT_SIZE);
	if (HIGHSPEED)
		memcpy (buf + 4 + USB_DT_ENDPOINT_SIZE,hs, USB_DT_ENDPOINT_SIZE);
	status = write (fd, buf, 4 + USB_DT_ENDPOINT_SIZE
			+ (HIGHSPEED ? USB_DT_ENDPOINT_SIZE : 0));
	if (status < 0) {
		status = -errno;
		fprintf (stderr, "%s config %s error %d (%s)\n",
			label, name, errno, strerror (errno));
		close (fd);
		return status;
	} else if (verbose) {
		unsigned long	id;

		id = pthread_self ();
		fprintf (stderr, "%s start %ld fd %d\n", label, id, fd);
	}
	
	return fd;
}




static unsigned long fill_in_buf(void *buf, unsigned long nbytes)
{
#ifdef	DO_PIPE
	/* pipe stdin to host */
	nbytes = fread (buf, 1, nbytes, stdin);
	if (nbytes == 0) {
		if (ferror (stdin))
			perror ("read stdin");
		if (feof (stdin))
			errno = ENODEV;
	}
#else
	switch (pattern) {
	unsigned	i;

	default:
		// FALLTHROUGH
	case 0:		/* endless streams of zeros */
		memset (buf, 0, nbytes);
		break;
	case 1:		/* mod63 repeating pattern */
		for (i = 0; i < nbytes; i++)
			((__u8 *)buf)[i] = (__u8) (i % 63);
		break;
	}
#endif
	return nbytes;
}

static int empty_out_buf(void *buf, unsigned long nbytes)
{
	unsigned	i;
	unsigned char *data=buf;

	for (i = 0; i < nbytes; i++) 
	{
		fprintf (stderr, " %02x", *(data+i));
	}
	fprintf (stderr, "\n");
	
	return i;
}

static void *simple_source_thread (void *param)
{

   return ;//???
	char		*name = (char *) param;
	int		status;
	char		buf [USB_BUFSIZE];

	status = source_open (name);
	if (status < 0)
		return 0;
	source_fd = status;
usb_debug("~source_fd=%d",source_fd);
	pthread_cleanup_push (close_fd, &source_fd);
	do {
		unsigned long	len;

		/* original LinuxThreads cancelation didn't work right
		 * so test for it explicitly.
		 */usb_debug("~");
		pthread_testcancel ();
		usb_debug("~");
		len = fill_in_buf (buf, sizeof buf);
		if (len > 0)
		{	
			usb_debug("~.....");
			usb_debug("~.....len=%d",len);
			status = write (source_fd, buf, len);
			usb_debug("~status=%d",status);
		}
		else
			status = 0;

	} while (status > 0);
	if (status == 0) 
	{
		if (verbose) fprintf (stderr, "done %s\n", __FUNCTION__);
	} 
	else if (verbose > 2 || errno != ESHUTDOWN) /* normal disconnect */
	{
		perror ("write");
	}
	fflush (stdout);
	fflush (stderr);
	pthread_cleanup_pop (1);
usb_debug("~");
	return 0;
}
int usbtmc_msg_cmd_read(struct usb_tmc_bulk_header  *pt_usb_tmc_packet,char *rev_buf,int rcv_len,char *cmd_buf,int *cmd_len)
{
#define REPEAT_READ_CNT 10

	int transfersize=0;
	int retValue    =0;
	int rcved_len   = rcv_len;
	int nend_len    =0;
	int loop =0;
	//transfersize :4的倍数
	transfersize = pt_usb_tmc_packet->command_specific.dev_dep_msg_out.transferSize + USB_TMC_HEADER_LEN;
	transfersize += 0x03;
	transfersize &= (~0x03);
	
	if(transfersize == rcved_len)
	{
		goto  rcv_finish;
	}
	else if(transfersize < rcved_len)
	{
		return -1;
	}
	else
	{
		usb_debug("~continue read tmc msg data");
	}

rcv_continue:
	loop++;
	if (loop > REPEAT_READ_CNT)
	{ 
		return -2; 
	}
	nend_len = transfersize - rcved_len;	
	retValue = read(sink_fd, (rev_buf), nend_len);
	if(retValue > 0)
	{
		rcved_len += retValue;
		if(transfersize > rcved_len) goto rcv_continue;
		else goto rcv_finish;
	}
	else
	{
		usb_debug_err("usb read error, errcode=%d!",retValue);
	}

rcv_finish:
	*cmd_len = strlen(rev_buf+USB_TMC_HEADER_LEN);
	memcpy(cmd_buf,(rev_buf+USB_TMC_HEADER_LEN),(*cmd_len));
	//add ‘\0’
	//*(cmd_buf + (*cmd_len)) = '\0';
	//(*cmd_len)+=1;
	
	return 0;
}
char test_buf[]={0x02,0x08,0xf7,0x00,0x06,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x2a,0x49,
0x44,0x4e,0x3f,0x0a,0x00,0x00};
int usbtmc_msg_cmd_response(struct usb_tmc_bulk_header  *pt_usb_tmc_packet,char *rev_buf,int rcv_len)
{
	int retValue    =0;
	int loop        = 0;
	int host_rcv_cap = 0;/*接收容量*/
	int response_total_len = scpi_msg_reponse_avail_len();/**/
	int response_sended =0; 
	int response_len = 0;
	char tmp_buf[USB_BUFSIZE];
	
	if (pt_usb_tmc_packet->command_specific.req_dev_dep_msg_in.bmTransferAttributes & USB_TMC_BULK_HEADER_BMTRANSFER_ATTRIB_TERMCHAR) 
	{
		//usb_debug_err("FAIL! requested term char!");
		return -1;	/* TODO reply error? */
	}
	if(response_total_len <= 0)
	{
		//usb_debug_err("requested no data!");
		return -2;	/* TODO reply error? */
	}

	host_rcv_cap = pt_usb_tmc_packet->command_specific.req_dev_dep_msg_in.transferSize;

	usb_debug("response_total_len[%d],host_rcv_cap[%d]",response_total_len,host_rcv_cap);
	pt_usb_tmc_packet->MsgID = REQUEST_DEV_DEP_MSG_IN;
	pt_usb_tmc_packet->command_specific.dev_dep_msg_in.transferSize = ((response_total_len > host_rcv_cap) ? host_rcv_cap: response_total_len);
	/* only support short stuff now! */
	pt_usb_tmc_packet->command_specific.dev_dep_msg_in.bmTransferAttributes = USB_TMC_BULK_HEADER_BMTRANSFER_ATTRIB_EOM;

	if(response_total_len < USB_TMC_DATA_MAX ) response_len = response_total_len;
	else response_len = USB_TMC_DATA_MAX;
	/*send :tmc header+ msg datas*/
	scpi_msg_reponse_read((rev_buf+USB_TMC_HEADER_LEN),response_len);
	write(source_fd, rev_buf, (response_len + USB_TMC_HEADER_LEN+0x03)&(~0x03)); 
	usb_debug("source_fd =%d,response[%d]:%s",source_fd,pt_usb_tmc_packet->command_specific.dev_dep_msg_in.transferSize,(rev_buf+USB_TMC_HEADER_LEN));

	empty_out_buf(rev_buf,(response_len + USB_TMC_HEADER_LEN+0x03)&(~0x03));
	response_sended += response_len;
	while(response_sended < response_total_len)
	{
		if((response_total_len - response_sended) < USB_BUFSIZE) response_len = (response_total_len - response_sended);
		else response_len = USB_BUFSIZE;

		memset(tmp_buf,0,USB_BUFSIZE);
		retValue = scpi_msg_reponse_read(tmp_buf,response_len);
		//if (retValue != response_len) maybe some error!
		write(source_fd, tmp_buf, (response_len+0x03)&(~0x03)); 
		response_sended += response_len;	
		usb_debug("response_sended[%d]:response_total_len[%d]",response_sended,response_total_len);
	}

	return 0;

}

static void *simple_sink_thread (void *param)
{
	char		*name = (char *) param;
	int		     status;
	char		 buf [USB_BUFSIZE];
	struct usb_tmc_bulk_header  *pt_usb_tmc_packet=NULL;
	char cmd_buf[100];
	int cmd_len = 0;
	int retValue =0;


	status = sink_open (name);
	if (status < 0) return 0;
	sink_fd = status;
	//??????
	status = source_open (EP_IN_NAME);
	if (status < 0) return 0;
	source_fd = status;
	usb_debug("~sink_fd=%d,source_fd=%d",sink_fd,source_fd);
	/* synchronous reads of endless streams of data */
	pthread_cleanup_push (close_fd, &sink_fd);
	do {
		/* original LinuxThreads cancelation didn't work right
		 * so test for it explicitly.
		 */
		usb_debug("~");
		pthread_testcancel ();
		usb_debug("~");
		errno = 0;
		status = read (sink_fd, buf,USB_BUFSIZE);

		if (status == 0) 
		{
			usb_debug_err("~device close connect,ret=%d:",status);
			break;
		}
		else if (status < 0) 
		{
			usb_debug_err("~usb error or slave disconnected,ret=%d",status);
			break;
		}
		else if(status < USB_TMC_HEADER_LEN)
		{
			usb_debug_err("~tmc rev packet error, ret=%d",status);
		}

		usb_debug("~rev len=%d:",status);
		status = empty_out_buf (buf, status);

		pt_usb_tmc_packet = (struct usb_tmc_bulk_header  *)buf;
		switch(pt_usb_tmc_packet->MsgID)
		{
			case DEV_DEP_MSG_OUT:
				usb_debug("dev dep out btag: %d", pt_usb_tmc_packet->bTag);
				retValue = usbtmc_msg_cmd_read(pt_usb_tmc_packet,buf,status,cmd_buf,&cmd_len);
				if (retValue == 0)
				{
					usb_debug("~rcv cmd[%d]:%s",cmd_len,cmd_buf);
					/*send msg to scpi thread*/
					scpi_msg_cmd_write(cmd_buf,cmd_len);
					/*just for test*/
					scpi_msg_reponse_write(cmd_buf,cmd_len,0);
				}
				else
				{
					usb_debug_err("~tmc read msg errcode=%d",retValue);
				}
			break;
			case REQUEST_DEV_DEP_MSG_IN:
				usb_debug("~response maxsize[%d],bTag=%d",pt_usb_tmc_packet->command_specific.req_dev_dep_msg_in.transferSize,pt_usb_tmc_packet->bTag);
				retValue = usbtmc_msg_cmd_response(pt_usb_tmc_packet,buf,status);
				if (retValue != 0)
				{
					usb_debug_err("~tmc response errcode=%d",retValue);
				}

			break;
			
		}
	} while (status > 0);
	if (status == 0) 
	{
		if (verbose)fprintf (stderr, "done %s\n", __FUNCTION__);
	} else if (verbose > 2 || errno != ESHUTDOWN) /* normal disconnect */
	{
		perror ("read");
	}
	fflush (stdout);
	fflush (stderr);
	pthread_cleanup_pop (1);
usb_debug("~");
	return 0;
}

static void *(*source_thread) (void *);
static void *(*sink_thread) (void *);


static void start_io ()
{
	sigset_t	allsig, oldsig;

	sigfillset (&allsig);
	errno = pthread_sigmask (SIG_SETMASK, &allsig, &oldsig);
	if (errno < 0) {
		perror ("set thread signal mask");
		return;
	}

	/* is it true that the LSB requires programs to disconnect
	 * from their controlling tty before pthread_create()?
	 * why?  this clearly doesn't ...
	 */

	if (pthread_create (&source, 0,source_thread, (void *) EP_IN_NAME) != 0) {
		perror ("can't create source thread");
		goto cleanup;
	}

	if (pthread_create (&sink, 0,sink_thread, (void *) EP_OUT_NAME) != 0) {
		perror ("can't create sink thread");
		pthread_cancel (source);
		source = ep0;
		goto cleanup;
	}

	/* give the other threads a chance to run before we report
	 * success to the host.
	 * FIXME better yet, use pthread_cond_timedwait() and
	 * synchronize on ep config success.
	 */
	 usb_debug("~thread id: source=%d,sink=%d,ep0=%d",source,sink,ep0);
	sched_yield ();

cleanup:
	errno = pthread_sigmask (SIG_SETMASK, &oldsig, 0);
	if (errno != 0) {
		perror ("restore sigmask");
		exit (-1);
	}
}

static void stop_io ()
{
	if (!pthread_equal (source, ep0)) 
	{
		pthread_cancel (source);
		if (pthread_join (source, 0) != 0)
			perror ("can't join source thread");
		source = ep0;
	}

	if (!pthread_equal (sink, ep0)) 
	{
		pthread_cancel (sink);
		if (pthread_join (sink, 0) != 0)
			perror ("can't join sink thread");
		sink = ep0;
	}
	usb_debug("~thread id: source=%d,sink=%d,ep0=%d",source,sink,ep0);

}

/*-------------------------------------------------------------------------*/

static char *
build_config(char *cp, const struct usb_endpoint_descriptor **ep) {
    struct usb_config_descriptor *c;
    int i;
    c = (struct usb_config_descriptor *) cp;
    memcpy(cp, &config, config.bLength);
	
    cp += config.bLength;
    memcpy(cp, &tmc_interface, tmc_interface.bLength);
    cp += tmc_interface.bLength;
	

    for(i = 0; i < tmc_interface.bNumEndpoints; i++) {		

        memcpy(cp, ep [i], USB_DT_ENDPOINT_SIZE);
        cp += USB_DT_ENDPOINT_SIZE;
    }
    c->wTotalLength = __cpu_to_le16(cp - (char *) c);
    return cp;
}

static int init_device (void)
{
	char		buf [4096], *cp = &buf [0];
	int		fd;
	int		status;
		status = autoconfig ();
	if (status < 0) {
		fprintf (stderr, "?? don't recognize /dev/gadget %s device\n", "bulk");
		return status;
	}

	fd = open (DEVNAME, O_RDWR);
	usb_debug("~fd=%d,DEVNAME=%s",fd,DEVNAME);
	if (fd < 0) {
		perror (DEVNAME);
		return -errno;
	}

	*(__u32 *)cp = 0;	/* tag for this format */
	cp += 4;

	/* write full then high speed configs */
	cp = build_config (cp, fs_eps);
	if (HIGHSPEED)
		cp = build_config (cp, hs_eps);

	/* and device descriptor at the end */
	memcpy (cp, &device_desc, sizeof device_desc);
	cp += sizeof device_desc;

	status = write (fd, &buf [0], cp - &buf [0]);
	if (status < 0) {
		perror ("write dev descriptors");
		close (fd);
		return status;
	} else if (status != (cp - buf)) {
		fprintf (stderr, "dev init, wrote %d expected %ld\n",
				status, cp - buf);
		close (fd);
		return -EIO;
	}
	return fd;
}

static void handle_control (int fd, struct usb_ctrlrequest *setup)
{
	int		status, tmp;
	__u8		buf [256];
	__u16		value, index, length;

	value = __le16_to_cpu(setup->wValue);
	index = __le16_to_cpu(setup->wIndex);
	length = __le16_to_cpu(setup->wLength);

	if (verbose)
		fprintf (stderr, "SETUP %02x.%02x ""v%04x i%04x %d\n",
		          setup->bRequestType, setup->bRequest,
			      value, index, length);

	/*
	if ((setup->bRequestType & USB_TYPE_MASK) != USB_TYPE_STANDARD)
		goto special;
	*/
	/*Table 14 --USBTMC class specific request format*/
	if(setup->bRequestType == 0XA1)
	{
		switch (setup->bRequest)
		{
/*			case INITIATE_ABORT_BULK_OUT:
			case CHECK_ABORT_BULK_OUT_STATUS:
			case INITIATE_ABORT_BULK_IN:
			case CHECK_ABORT_BULK_IN_STATUS:
				goto stall;*/

			case INITIATE_CLEAR:
				usb_debug("~INITIATE_CLEAR value = %d",(value) );
				buf[0] = USB_TMC_STATUS_SUCCESS;
				status = write(fd, buf, 1);	
				return;
			case CHECK_CLEAR_STATUS:
				usb_debug("~CHECK_CLEAR_STATUS value = %d",(value) );
				buf[0] = USB_TMC_STATUS_SUCCESS;
				status = write(fd, buf, 1);			
				return;
			case GET_CAPABILITIES:
				usb_debug("~GET_CAPABILITIES value = %d",(value) );
				if ((setup->bRequestType)&(USB_DIR_IN) != USB_DIR_IN) goto stall;
				
				length = sizeof(capabilities);
				memcpy(buf,&capabilities,length);
		        status = write(fd, buf, length);

		        if(status < 0)
				{
		           if(errno == EIDRM)usb_debug_err("GET_INTERFACE timeout");
		           else usb_debug("write GET_INTERFACE data");
			    } else if(status != length)
			    {
			        usb_debug_err("short USB_REQ_SET_DESCRIPTOR write, %d",status);
			    }		
				return;
			case INDICATOR_PULSE:
				usb_debug("~INDICATOR_PULSE value = %d",(value) );
				buf[0] = USB_TMC_STATUS_SUCCESS;
				status = write(fd, buf, 1);
				return;
			default:
				usb_debug_err("setup->bRequest=0x%x,unkown bRequest=0x%x",setup->bRequest,setup->bRequest);
				goto stall;
	
		}
	}
	else if(setup->bRequestType == 0XA2)
	{
		switch (setup->bRequest)
		{
			case INITIATE_ABORT_BULK_OUT:
				buf[0] = USB_TMC_STATUS_SUCCESS;
				buf[1] = 0x00;
				status = write(fd, buf, 2);
				return;
			case CHECK_ABORT_BULK_OUT_STATUS:
				buf[0] = USB_TMC_STATUS_SUCCESS;
				buf[1] = 0x00;
				status = write(fd, buf, 2);
				return;

			case INITIATE_ABORT_BULK_IN:
				buf[0] = USB_TMC_STATUS_SUCCESS;
				buf[1] = 0x00;
				status = write(fd, buf, 2);
				return;				
			case CHECK_ABORT_BULK_IN_STATUS:
				buf[0] = USB_TMC_STATUS_SUCCESS;
				buf[1] = 0x00;
				status = write(fd, buf, 2);
				return;
			default:
				usb_debug_err("setup->bRequest=0x%x,unkown bRequest=0x%x",setup->bRequest,setup->bRequest);
				goto stall;
	
		}
	}

	switch (setup->bRequest) 
	{	/* usb 2.0 spec ch9 requests */
	case USB_REQ_GET_DESCRIPTOR:
		if (setup->bRequestType != USB_DIR_IN)
			goto stall;
		switch (value >> 8) 
		{
		case USB_DT_STRING:
			tmp = value & 0x0ff;
			if (verbose > 1)
				fprintf (stderr,"... get string %d lang %04x\n",tmp, index);
			if (tmp != 0 && index != strings.language) goto stall;
			
			status = usb_gadget_get_string (&strings, tmp, buf);
			if (status < 0) goto stall;
			
			tmp = status;
			if (length < tmp)tmp = length;
			
			status = write (fd, buf, tmp);
			if (status < 0)
			{
				if (errno == EIDRM) fprintf (stderr, "string timeout\n");
				else perror ("write string data");
			} else if (status != tmp) 
			{
				fprintf (stderr, "short string write, %d\n",
					status);
			}
			
			break;
		default:
			goto stall;
		}
		return;
	case USB_REQ_SET_CONFIGURATION:
		if (setup->bRequestType != USB_DIR_OUT) goto stall;
		
		if (verbose) fprintf (stderr, "CONFIG #%d\n", value);

		/* Kernel is normally waiting for us to finish reconfiguring
		 * the device.
		 *
		 * Some hardware can't, notably older PXA2xx hardware.  (With
		 * racey and restrictive config change automagic.  PXA 255 is
		 * OK, most PXA 250s aren't.  If it has a UDC CFR register,
		 * it can handle deferred response for SET_CONFIG.)  To handle
		 * such hardware, don't write code this way ... instead, keep
		 * the endpoints always active and don't rely on seeing any
		 * config change events, either this or SET_INTERFACE.
		 */
		switch (value) {
		case CONFIG_VALUE:
			start_io ();
			break;
		case 0:
			stop_io ();
			usb_debug("~stop_io ----2222");
			break;
		default:
			/* kernel bug -- "can't happen" */
			fprintf (stderr, "? illegal config\n");
			goto stall;
		}

		/* ... ack (a write would stall) */
		status = read (fd, &status, 0);
		if (status)
			perror ("ack SET_CONFIGURATION");
		return;
	case USB_REQ_GET_INTERFACE:
		usb_debug("~USB_REQ_GET_INTERFACE value >> 8 = %d",(value >> 8) );
		if (setup->bRequestType != (USB_DIR_IN|USB_RECIP_INTERFACE)
				|| index != 0
				|| length > 1)
			goto stall;

		/* only one altsetting in this driver */
		buf [0] = 0;
		status = write (fd, buf, length);
		if (status < 0) {
			if (errno == EIDRM)
				fprintf (stderr, "GET_INTERFACE timeout\n");
			else
				perror ("write GET_INTERFACE data");
		} else if (status != length) {
			fprintf (stderr, "short GET_INTERFACE write, %d\n",
				status);
		}
		return;
	case USB_REQ_SET_INTERFACE:
		usb_debug("~USB_REQ_SET_INTERFACE value >> 8 = %d",(value >> 8) );
		if (setup->bRequestType != USB_RECIP_INTERFACE
				|| index != 0
				|| value != 0)
			goto stall;

		/* just reset toggle/halt for the interface's endpoints */
		status = 0;
		if (ioctl (source_fd, GADGETFS_CLEAR_HALT) < 0) {
			status = errno;
			perror ("reset source fd");
		}
		if (ioctl (sink_fd, GADGETFS_CLEAR_HALT) < 0) {
			status = errno;
			perror ("reset sink fd");
		}
		/* FIXME eventually reset the status endpoint too */
		if (status)
			goto stall;

		/* ... and ack (a write would stall) */
		status = read (fd, &status, 0);
		if (status)
			perror ("ack SET_INTERFACE");
		return;
	default:
		usb_debug_err("unkown bRequest=0x%x",setup->bRequest);
		goto stall;
	}

stall:
	if (verbose)
		fprintf (stderr, "... protocol stall %02x.%02x\n",
			setup->bRequestType, setup->bRequest);

	/* non-iso endpoints are stalled by issuing an i/o request
	 * in the "wrong" direction.  ep0 is special only because
	 * the direction isn't fixed.
	 */
	if (setup->bRequestType & USB_DIR_IN)
		status = read (fd, &status, 0);
	else
		status = write (fd, &status, 0);
	if (status != -1)
		fprintf (stderr, "can't stall ep0 for %02x.%02x\n",
			setup->bRequestType, setup->bRequest);
	else if (errno != EL2HLT)
		perror ("ep0 stall");
}

static void signothing (int sig, siginfo_t *info, void *ptr)
{
	/* NOP */
	if (verbose > 2)
		fprintf (stderr, "%s %d\n", __FUNCTION__, sig);
}

static const char *speed (enum usb_device_speed s)
{
	switch (s) {
	case USB_SPEED_LOW:	return "low speed";
	case USB_SPEED_FULL:	return "full speed";
	case USB_SPEED_HIGH:	return "high speed";
	default:		return "UNKNOWN speed";
	}
}

/*-------------------------------------------------------------------------*/

/* control thread, handles main event loop  */

#define	NEVENT		5
#define	LOGDELAY	(15 * 60)	/* seconds before stdout timestamp */

static void *ep0_thread (void *param)
{
	int			fd = *(int*) param;
	struct sigaction	action;
	time_t			now, last;
	struct pollfd		ep0_poll;

	source = sink = ep0 = pthread_self ();
	pthread_cleanup_push (close_fd, param);

	/* REVISIT signal handling ... normally one pthread should
	 * be doing sigwait() to handle all async signals.
	 */
	action.sa_sigaction = signothing;
	sigfillset (&action.sa_mask);
	action.sa_flags = SA_SIGINFO;
	if (sigaction (SIGINT, &action, NULL) < 0) {
		perror ("SIGINT");
		return 0;
	}
	if (sigaction (SIGQUIT, &action, NULL) < 0) {
		perror ("SIGQUIT");
		return 0;
	}

	ep0_poll.fd = fd;
	ep0_poll.events = POLLIN | POLLOUT | POLLHUP;
	usb_debug("~fd=%d ",fd);
	/* event loop */
	last = 0;
	for (;;) {
		int				tmp;
		struct usb_gadgetfs_event	event [NEVENT];
		int				connected = 0;
		int				i, nevent;

		/* Use poll() to test that mechanism, to generate
		 * activity timestamps, and to make it easier to
		 * tweak this code to work without pthreads.  When
		 * AIO is needed without pthreads, ep0 can be driven
		 * instead using SIGIO.
		 */
		tmp = poll(&ep0_poll, 1, -1);
		if (verbose) {
			time (&now);
			if ((now - last) > LOGDELAY) {
				char		timebuf[26];

				last = now;
				ctime_r (&now, timebuf);
				printf ("\n** %s", timebuf);
			}
		}
		if (tmp < 0) {
			/* exit path includes EINTR exits */
			perror("poll");
			break;
		}

		tmp = read (fd, &event, sizeof event);
		if (tmp < 0) {
			if (errno == EAGAIN) {
				sleep (1);
				continue;
			}
			perror ("ep0 read after poll");
			goto done;
		}
		nevent = tmp / sizeof event [0];
		if (nevent != 1 && verbose)
			fprintf (stderr, "read %d ep0 events\n",
				nevent);

		for (i = 0; i < nevent; i++) {
			switch (event [i].type) {
			case GADGETFS_NOP:
				if (verbose)
					fprintf (stderr, "NOP\n");
				usb_debug("~");
				break;
			case GADGETFS_CONNECT:
				connected = 1;
				current_speed = event [i].u.speed;
				if (verbose)
					fprintf (stderr,
						"CONNECT %s\n",
					    speed (event [i].u.speed));
				usb_debug("~ GADGETFS_CONNECT");
				break;
			case GADGETFS_SETUP:
				connected = 1;
				handle_control (fd, &event [i].u.setup);
				break;
			case GADGETFS_DISCONNECT:
				connected = 0;
				current_speed = USB_SPEED_UNKNOWN;
				if (verbose)
					fprintf(stderr, "DISCONNECT\n");
				stop_io ();
				usb_debug("~ GADGETFS_DISCONNECT");
				break;
			case GADGETFS_SUSPEND:
				// connected = 1;
				if (verbose)
					fprintf (stderr, "SUSPEND\n");
				break;
			default:
				fprintf (stderr,
					"* unhandled event %d\n",
					event [i].type);
			}
		}
		continue;
done:
		fflush (stdout);
		if (connected)
			stop_io ();
		break;
	}
	if (verbose)
		fprintf (stderr, "done\n");
	fflush (stdout);

	pthread_cleanup_pop (1);
	return 0;
}

/*-------------------------------------------------------------------------*/
int mountGadgetfs()
{
	struct stat statbuff;
	int ret;
     if(stat("/dev/gadget", &statbuff) < 0) 
	 {
	 	
    	ret = system("mkdir /dev/gadget");
		if(ret)
		{
			usb_debug("mkdir falase %d\r\n",ret);
			return -1;
		}
	}

    usleep(1000);
    ret = system("mount -t gadgetfs gadgetfs /dev/gadget");
	if(ret)
	{
		usb_debug("mount falase %d\r\n",ret);
		return -2;
	}
    usleep(1000);
	return 0;

}
#if 1
int main (int argc, char **argv)
{
	int		fd, c, i;

	/* random initial serial number 
	srand ((int) time (0));
	for (i = 0; i < sizeof serial - 1; ) 
	{
		c = rand () % 127;
		if ((('a' <= c && c <= 'z') || ('0' <= c && c <= '9')))
			serial [i++] = c;
	}
	*/

	source_thread = simple_source_thread;
	sink_thread = simple_sink_thread;

	while ((c = getopt (argc, argv, "I:a:i:o:p:r:s:v")) != EOF) {
		switch (c) {

		case 'p':		/* i/o pattern */
			pattern = atoi (optarg);
			continue;
		case 'v':		/* verbose */
			verbose++;
			continue;
		}
		fprintf (stderr, "usage:  %s "
				"[-p pattern] [-r serial] [-v]\n",
				argv [0]);
		return 1;
	}
	
	mountGadgetfs();
	if (chdir ("/dev/gadget") < 0) 
	{
		perror ("can't chdir /dev/gadget");
		return 1;
	}

	fd = init_device ();
	usb_debug("~DEVNAME=%s,EP_IN_NAME=%s,EP_OUT_NAME=%s",DEVNAME,EP_IN_NAME,EP_OUT_NAME);
	if (fd < 0)
		return 1;
	fprintf (stderr, "/dev/gadget/%s \n",DEVNAME);
	fflush (stderr);
	
	(void) ep0_thread (&fd);
	
	return 0;
}


#else

int usb_tmc_init()
{
	int fd = -1,retValue=0;
	pthread_t id;
	
	source_thread = simple_source_thread;
	sink_thread = simple_sink_thread;
	
	retValue = mountGadgetfs();
	if(retValue < 0)
	{
		usb_debug_err("mount usb error,err=%d",retValue);
		return -1;
	}
	
	fd = init_device ();
	if (fd < 0) 
	{
		usb_debug_err("init_device error,err=%d",retValue);	
		return -1;
	}
	
	retValue = pthread_create(&id, NULL, (void *) ep0_thread, NULL);
	if (retValue != 0 )
	{
		usb_debug_err("creat ep0_thread error,err=%d",retValue);
		return -1;
	}

	return 0;

}

#endif
