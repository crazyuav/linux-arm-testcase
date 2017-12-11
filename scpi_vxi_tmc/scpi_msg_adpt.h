
#ifndef __SCPI_MSG_ADPT_H_
#define __SCPI_MSG_ADPT_H_
#include "scpi-debug.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>

#include <pthread.h>
#include <semaphore.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>


typedef struct
{
//int msgSrc;
//int len;

long msgSrc;
//int len;
char *mtext;
}scpi_msg_t;



int scpi_msg_cmd_write(char *buf, int len);
int scpi_msg_cmd_read(char *buf, int len);

int scpi_msg_reponse_avail_len(void);
//send to host
int scpi_msg_reponse_write(char *buf,int len,int msgsrc);
int scpi_msg_reponse_read(char *dst,int len);



int scpi_adpt_init(void);
int scpi_adpt_vxi11_srv_run(void);

#endif
