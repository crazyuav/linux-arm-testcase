
#ifndef __SCPI_MSG_ADPT_H_
#define __SCPI_MSG_ADPT_H_
#include "scpi-debug.h"


int scpi_msg_cmd_write(char *buf, int len);
int scpi_msg_cmd_read(char *buf, int len);

int scpi_msg_reponse_avail_len(void);
//send to host
int scpi_msg_reponse_write(char *buf,int len,int msgsrc);
int scpi_msg_reponse_read(char *dst,int len);

#endif
