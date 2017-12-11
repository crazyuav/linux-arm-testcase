#include "scpi_msg_adpt.h"
#include "tmc.h"

#include <stdio.h>
#ifndef NULL
#define NULL (void *)0
#endif

#define MSGKEY 1025


#define SCPI_CMD_RX_AVAIL() \
	(g_scpi_msg.rxTail >= g_scpi_msg.rxHead) ? \
	(g_scpi_msg.rxTail - g_scpi_msg.rxHead) : \
	(SCPI_BUF_MAX - g_scpi_msg.rxHead + g_scpi_msg.rxTail)


#define SCPI_RESPONSE_TX_AVAIL() \
	(g_scpi_msg.txTail >= g_scpi_msg.txHead) ? \
	(g_scpi_msg.txTail - g_scpi_msg.txHead) : \
	(SCPI_BUF_MAX - g_scpi_msg.txHead + g_scpi_msg.txTail)
/*	
#define SCPI_RESPONSE_TX_AVAIL() \
	(g_scpi_msg.txHead > g_scpi_msg.txTail) ? \
	(g_scpi_msg.txHead - g_scpi_msg.txTail - 1) : \
	(SCPI_BUF_MAX - g_scpi_msg.txTail + g_scpi_msg.txHead - 1)
*/	
#define SCPI_RESPONSE_STREAM_TX_AVAIL()(g_scpi_msg.txmsg_stream_size - g_scpi_msg.txmsg_stream_offset)	

typedef struct
{
#if 1
/*rx */
  char rxBuf[SCPI_BUF_MAX];
  int rxHead;
  volatile int rxTail;
#endif
/*tx*/
  char txBuf[SCPI_BUF_MAX];
  volatile int txHead;
  int txTail;
/*msg stream : TX/RX length > 1024 bytes*/  
/*
note:1.考虑是否加锁
*/
  int txmsg_stream_size;
  int txmsg_stream_offset;
 /*空指针判断！！！*/
  char *txmsg_strem_buf;
  /*fifo/address pointer [0/1]*/
  int msgsrc;
  
} SCPI_TX_CFG_t;
/*msg buffer*/
SCPI_TX_CFG_t g_scpi_msg;

sem_t scpi_sem;
int scpi_msgid;

static int scpi_msg_tx_read(char *buf, int len)
{
  int cnt=0;
  while ((g_scpi_msg.txHead != g_scpi_msg.txTail) && (cnt < len))
  {
    *buf++ = g_scpi_msg.txBuf[g_scpi_msg.txHead++];
    if (g_scpi_msg.txHead >= SCPI_BUF_MAX)
    {
      g_scpi_msg.txHead = 0;
    }
    cnt++;
  }
  
  return cnt;
}

static int scpi_msg_tx_write(char *buf, int len)
{
  int cnt;

  for (cnt = 0; cnt < len; cnt++)
  {
    g_scpi_msg.txBuf[g_scpi_msg.txTail] = *buf++;
    if (g_scpi_msg.txTail >= SCPI_BUF_MAX-1)
    {
      g_scpi_msg.txTail = 0;
    }
    else
    {
      g_scpi_msg.txTail++;
    }
  }

  return cnt;
}

static int scpi_msg_rx_read(char *buf, int len)
{
  int cnt=0;
  while ((g_scpi_msg.rxHead != g_scpi_msg.rxTail) && (cnt < len))
  {
    *buf++ = g_scpi_msg.rxBuf[g_scpi_msg.rxHead++];
    if (g_scpi_msg.rxHead >= SCPI_BUF_MAX)
    {
      g_scpi_msg.rxHead = 0;
    }
    cnt++;
  }
  
  return cnt;
}
static int scpi_msg_rx_write(char *buf, int len)
{
  int cnt = 0;

  for (cnt = 0; cnt < len; cnt++)
  {
    g_scpi_msg.rxBuf[g_scpi_msg.rxTail] = *buf++;

    if (g_scpi_msg.rxTail >= SCPI_BUF_MAX-1)
    {
      g_scpi_msg.rxTail = 0;
    }
    else
    {
      g_scpi_msg.rxTail++;
    }
  }

  return cnt;
}

int scpi_msg_cmd_write(char *buf, int len)
{
	int retValue = 0;
	scpi_msg_rx_write(buf,len);
	scpi_postmsg(1,len);

	return retValue;
}
int scpi_msg_cmd_read(char *buf, int len)
{
	scpi_msg_t scpi_msg;
	int retValue = 0;
	
	sem_wait(&scpi_sem);
	retValue = msgrcv(scpi_msgid,&scpi_msg,sizeof(scpi_msg_t),0,0);	
	
	retValue = scpi_msg_rx_read(buf,len);
	return retValue;

}
int scpi_msg_reponse_avail_len(void)
{
	//printf("txTail=%d,txHead=%d\r\n",g_scpi_msg.txTail,g_scpi_msg.txHead);
    if(g_scpi_msg.msgsrc == 0) return SCPI_RESPONSE_TX_AVAIL();
    else return SCPI_RESPONSE_STREAM_TX_AVAIL();
   //return SCPI_RESPONSE_TX_AVAIL();
}
int scpi_msg_reponse_read(char *dst,int len)
{
    int rd_len=0;
    if(g_scpi_msg.msgsrc == 0)
    {
       rd_len = scpi_msg_tx_read(dst,len); 
    }
    else
    {
        //!!!!!!!!!!!!!!!!!!!!!!!!!!!!
       // g_scpi_msg.txmsg_strem_buf = 
    }
    
	return 0;
	
}
//send to host
int scpi_msg_reponse_write(char *buf,int len,int msgsrc)
{ 
    if (buf==NULL) return -1;
    
    g_scpi_msg.msgsrc = msgsrc;
    if(msgsrc == 0)
    {
        //clear txbuf
        if(SCPI_RESPONSE_TX_AVAIL()!= 0)
        {
          //char tmp_buf[SCPI_BUF_MAX];
          //scpi_msg_tx_read(tmp_buf,SCPI_BUF_MAX);  
        }       
        scpi_msg_tx_write(buf,len);     
    }
    else
    {
        g_scpi_msg.txmsg_strem_buf = buf;
        g_scpi_msg.txmsg_stream_size = len;
        g_scpi_msg.txmsg_stream_offset = 0;
    }
    
	return 0;
}
int scpi_postmsg(int msgSrc,int len)
{
	scpi_msg_t tempmsg;

	tempmsg.msgSrc = msgSrc;
	//tempmsg.mtext = len;

	usb_debug("msgSrc[%d],len[%d]\n",msgSrc,len);
	
	int retValue = msgsnd(scpi_msgid,&tempmsg,sizeof(scpi_msg_t),IPC_NOWAIT);
	if ( retValue < 0 ) {  
	 	usb_debug_err("SCPI msgsnd failed,retValue=%d",retValue);  
		return -1;
	}
	sem_post(&scpi_sem);
	
	return 0;
}
//
extern int usb_tmc_init(void);
extern int vxi11_init(void);
extern int vxi11_srv_run(void);


int scpi_adpt_init(void)
{
	int retValue = 0;

	sem_init(&scpi_sem,0,1);
	scpi_msgid=msgget(MSGKEY,IPC_EXCL);
	if(scpi_msgid < 0)
	{  
		scpi_msgid = msgget(MSGKEY,IPC_CREAT|0666);
		if(scpi_msgid <0)
		{  
			usb_debug_err("failed to create scpi sem!"); 
			return -1;
		}
	}
	/*1.RS232*/
	//REVERSED
	/*2.USB-TMC*/
	retValue = usb_tmc_init();
	if(retValue < 0)
	{
		usb_debug_err("usb-tmc init error,retValue=%d",retValue);
	}
	usb_debug("usb_tmc_init ok!"); 
	/*3.LXI*/
	retValue = vxi11_init();
	if(retValue < 0)
	{
		usb_debug_err("vxi11 server init error,retValue=%d",retValue);
	}
	usb_debug("vxi11_init ok!"); 
	/*4.GPIB*/
	//REVERSED




	return 0;
}

int scpi_adpt_vxi11_srv_run(void)
{
	return vxi11_srv_run();
}

	

