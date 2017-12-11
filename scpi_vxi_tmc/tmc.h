/* 
 * File:   tmc.h
 * Author: karlp
 *
 * USB TMC definitions
 */

#ifndef TMC_H
#define	TMC_H

#ifdef	__cplusplus
extern "C" {
#endif

	/* Definitions of Test & Measurement Class from
	 * "Universal Serial Bus Test and Measurement Class
	 * Revision 1.0"
	 */

	/* Table 43: TMC Class Code */
	/*
	 * Application-Class” class code, assigned by USB-IF. The Host must 
	 * not load a USBTMC driver based on just the bInterfaceClass field.
	 */
#define USB_CLASS_APPLICATION		0xfe
#define USB_APPLICATION_SUBCLASS_TMC	0x03

	/* Table 44 */
#define USB_TMC_PROTOCOL_NONE		0
#define USB_TMC_PROTOCOL_USB488		1

	/* USB TMC Class Specific Requests, Table 15 sec 4.2.1 */
	/* These are all required */
#define USB_TMC_REQ_INITIATE_ABORT_BULK_OUT		1
#define USB_TMC_REQ_CHECK_ABORT_BULK_OUT_STATUS		2
#define USB_TMC_REQ_INITIATE_ABORT_BULK_IN		3
#define USB_TMC_REQ_CHECK_ABORT_BULK_IN_STATUS		4
#define USB_TMC_REQ_INITIATE_CLEAR			5
#define USB_TMC_REQ_CHECK_CLEAR_STATUS			6
#define USB_TMC_REQ_GET_CAPABILITIES			7
#define USB_TMC_REQ_INDICATOR_PULSE			64 /* optional */

//bRequest values (Table 15)
#define INITIATE_ABORT_BULK_OUT      1		//Aborts a Bulk-OUT transfer.
#define CHECK_ABORT_BULK_OUT_STATUS  2		//Returns the status of the previously sent
											//INITIATE_ABORT_BULK_OUT request.
#define INITIATE_ABORT_BULK_IN       3		//Aborts a Bulk-IN transfer.
#define CHECK_ABORT_BULK_IN_STATUS   4		//Returns the status of the previously sent
											//INITIATE_ABORT_BULK_IN request
#define INITIATE_CLEAR               5		//Clears all previously sent pending and
											//unprocessed Bulk-OUT USBTMC message
											//content and clears all pending Bulk-IN transfers
											//from the USBTMC interface
#define CHECK_CLEAR_STATUS           6		//Returns the status of the previously sent
											//INITIATE_CLEAR request
#define GET_CAPABILITIES             7		//Returns attributes and capabilities of the
											//USBTMC interface
//Table 15 -- USBTMC bRequest values--USBTMC_1_00.PDF
//INDICATOR_PULSE 64
//NI 使用的INDICATOR_PULSE是0XA0
#define INDICATOR_PULSE              0XA0

	/* USB TMC status values Table 16 */
#define USB_TMC_STATUS_SUCCESS				1
#define USB_TMC_STATUS_PENDING				2
#define USB_TMC_STATUS_FAILED				0x80
#define USB_TMC_STATUS_TRANSFER_NOT_IN_PROGRESS		0x81
#define USB_TMC_STATUS_SPLIT_NOT_IN_PROGRESS		0x82
#define USB_TMC_STATUS_SPLIT_IN_PROGRESS		0x83

#define USB_TMC_INTERFACE_CAPABILITY_INDICATOR_PULSE	(1<<2)
#define USB_TMC_INTERFACE_CAPABILITY_TALK_ONLY		(1<<1)
#define USB_TMC_INTERFACE_CAPABILITY_LISTEN_ONLY	(1<<0)
#define USB_TMC_DEVICE_CAPABILITY_TERMCHAR		(1<<0)

#define USB_TMC_HEADER_LEN 12
/*usb_tmc_bulk_header.MsgID*/
#define DEV_DEP_MSG_OUT			1
#define REQUEST_DEV_DEP_MSG_IN			2
#define VENDOR_SPECIFIC_OUT 		126
#define REQUEST_VENDOR_SPECIFIC_IN	127

struct usb_tmc_bulk_header {
	unsigned char MsgID;
	unsigned char bTag;
	unsigned char bTagInverse;
	unsigned char reserved;

	union {
		struct _dev_dep_msg_out {
			unsigned int transferSize;
			unsigned char bmTransferAttributes;
			unsigned char reserved[3];
		} dev_dep_msg_out;

		struct _req_dev_dep_msg_in {
			unsigned int transferSize;
			unsigned char bmTransferAttributes;
			unsigned char TermChar;
			unsigned char reserved[2];
		} req_dev_dep_msg_in;

		struct _dev_dep_msg_in {
			unsigned int transferSize;
			unsigned char bmTransferAttributes;
			unsigned char reserved[3];
		} dev_dep_msg_in;

		struct _vendor_specific_out {
			unsigned int transferSize;
			unsigned char reserved[4];
		} vendor_specific_out;

		struct _req_vendor_specific_in {
			unsigned int transferSize;
			unsigned char reserved[4];
		} req_vendor_specific_in;

		struct _vendor_specific_in {
			unsigned int transferSize;
			unsigned char reserved[4];
		} vendor_specific_in;
		unsigned char raw[8];
	} command_specific;

} __attribute__((packed));

	/* Table 2, MsgId values */
#define USB_TMC_MSGID_OUT_DEV_DEP_MSG_OUT		1
#define USB_TMC_MSGID_OUT_REQUEST_DEV_DEP_MSG_IN	2
#define USB_TMC_MSGID_IN_DEV_DEP_MSG_IN			2
	/* 3-125 Reserved for USBTMC */
#define USB_TMC_MSGID_OUT_VENDOR_SPECIFIC_OUT		126
#define USB_TMC_MSGID_OUT_REQUEST_VENDOR_SPECIFIC_IN	127
#define USB_TMC_MSGID_IN_VENDOR_SPECIFIC_IN		127
	/* 128-255 Reserved for USBTMC subclass and VISA */

	/* long lines are gross, but consistent naming wins? */
#define USB_TMC_BULK_HEADER_BMTRANSFER_ATTRIB_EOM	    (1<<0)
#define USB_TMC_BULK_HEADER_BMTRANSFER_ATTRIB_TERMCHAR	(1<<1)

#define	USB_BUFSIZE	 512//(7 * 1024)
//#define USB_TMC_BUF_MAX 1024
#define USB_TMC_DATA_MAX (USB_BUFSIZE-USB_TMC_HEADER_LEN)

#define SCPI_BUF_MAX 1024
#define SCPI_DATA_MAX (SCPI_BUF_MAX-USB_TMC_HEADER_LEN)
#if 0
#define SCPI_CMD_RX_AVAIL() \
	(g_scpi_msg.rxTail >= g_scpi_msg.rxHead) ? \
	(g_scpi_msg.rxTail - g_scpi_msg.rxHead) : \
	(SCPI_BUF_MAX - g_scpi_msg.rxHead + g_scpi_msg.rxTail)

#define SCPI_RESPONSE_TX_AVAIL() \
	(g_scpi_msg.txHead > g_scpi_msg.txTail) ? \
	(g_scpi_msg.txHead - g_scpi_msg.txTail - 1) : \
	(SCPI_BUF_MAX - g_scpi_msg.txTail + g_scpi_msg.txHead - 1)
#define SCPI_RESPONSE_STREAM_AVAIL()(g_scpi_msg.txmsg_stream_size - g_scpi_msg.txmsg_stream_offset)	

#endif


#ifdef	__cplusplus
}
#endif

#endif	/* TMC_H */