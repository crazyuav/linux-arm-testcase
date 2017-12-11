#include "scpi_msg_adpt.h"
#include "tmc.h"

#include <stdio.h>
#ifndef NULL
#define NULL (void *)0
#endif

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

SCPI_TX_CFG_t g_scpi_msg;


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
	scpi_msg_rx_read(buf,len);
}
int scpi_msg_cmd_read(char *buf, int len)
{
	scpi_msg_rx_read(buf,len);
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
          char tmp_buf[SCPI_BUF_MAX];
          scpi_msg_tx_read(tmp_buf,SCPI_BUF_MAX);  
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

//
//extern int usb_tmc_init();
//extern int vxi11__init();
int scpi_adpt_init(void)
{
	int retValue = 0;
	
	//retValue = usb_tmc_init();
	if(retValue < 0)
	{
		usb_debug_err("usb-tmc init error,retValue=%d",retValue);
	}


	return 0;
}