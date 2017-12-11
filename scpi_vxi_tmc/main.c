


#include "scpi_msg_adpt.h"


int scpi_test_thread(void *arg)
{
	//scpi_msg_t scpi_msg;
	int retValue=0;
	int msg_buf[100];
	
 	while(1)
	{	
		vxi_debug("~"); 
		memset(msg_buf,0,sizeof(msg_buf));
		//sem_wait(&scpi_sem);
		//retValue = msgrcv(scpi_msgid,&scpi_msg,sizeof(scpi_msg_t),0,0);
		retValue = scpi_msg_cmd_read(msg_buf,100);

		vxi_debug("~msg_cmd_read[%d]:%s",retValue,msg_buf); 
	}   
}

	/*
		pshared:number of process
		value:max number of sem that shared
		sem:this sem
		int sem_init(sem_t *sem,int pshared,unsigned int value);
	*/

    
int main (int argc, char **argv)
{
 	int fd = -1,retValue=0;
	pthread_t id;


   vxi_debug("~");  
   scpi_adpt_init();  
   vxi_debug("~"); 
   
	retValue = pthread_create(&id, NULL, (void *) scpi_test_thread, NULL);
	if (retValue != 0 )
	{
		vxi_debug_err("creat ep0_thread error,err=%d",retValue);
		return -1;
	}

   
 scpi_adpt_vxi11_srv_run();
	exit(1);
   
}

