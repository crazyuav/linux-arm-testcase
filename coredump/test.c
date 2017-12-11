

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/ioctl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/select.h>
#include<sys/time.h>
#include<fcntl.h>
#include<errno.h>
#include<linux/input.h>
#include <stdarg.h>
#include <string.h>

int g_buf[10]={0x55};

int print_buf(void)
{
    int loop=0;
	printf("\r\n");
    for (loop=0;loop<(sizeof(g_buf)/4);loop++)
    {
        printf("g_buf[%d]=0x%d\r\n",loop,g_buf[loop]);
    }
	printf("\r\n");
    return 0;
}
int test_err(void)
{
//定义一个字符指针变量a，指向地址1，这个地址肯定不是自己可以访问的，但是这行不会产生段错误    
char* p = 1; 
*p = 'a'; //真正产生段错误的在这里，试图更改地址1的值，此时内核会终止该进程，并且把core文件dump出来
    return 0;
}
int main(int argc, char *argv[])
{
    
	printf("\r\n*************test for core dump*************\r\n");
    print_buf();
    
    test_err();
    while(1)
    {
        sleep(1);
printf("*************\r\n");        
    }
	printf("\r\n");
	return 0;
}
