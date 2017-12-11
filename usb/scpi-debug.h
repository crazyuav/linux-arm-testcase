#ifndef __SCPI_DEBUG_H_
#define __SCPI_DEBUG_H_

#include <syslog.h>

#ifdef	__cplusplus
extern "C" {
#endif
/*SCPI debug 0:disable   1:enable*/
#define SCPI_DEBUG_ENABLE 1

#if (SCPI_DEBUG_ENABLE)
	
#define scpi_debug(_fmt_, ...)        printf("[scpi] f:%s,l:%d,"_fmt_"\r\n",__FUNCTION__,__LINE__,##__VA_ARGS__)	
#define scpi_debug_warn(_fmt_, ...)   printf("[scpi-WARN] f%s,l:%d,"_fmt_"\r\n",__FUNCTION__,__LINE__,##__VA_ARGS__)	
#define scpi_debug_err(_fmt_, ...)    printf("[scpi-ERRO] f:%s,l:%d,"_fmt_"\r\n",__FUNCTION__,__LINE__,##__VA_ARGS__)	
#define scpi_debug_info(_fmt_, ...)   printf("[scpi-INFO] f:%s,l:%d,"_fmt_"\r\n",__FUNCTION__,__LINE__,##__VA_ARGS__)	


#else

#define scpi_debug(_fmt_, ...)       //printf("[scpi]:"_fmt_, ##__VA_ARGS__)	
#define UI_debug_warn(_fmt_, ...)    do{\
                                        printf("[scpi-WARN]:"_fmt_,##__VA_ARGS__);\
 									    syslog(LOG_WARNING, _fmt_",f:%s,l:%d",##__VAARGS__,__FILE__,__LINE__);\
 									    }while(0)
#define scpi_debug_err(_fmt_, ...)   do{\
                                        printf("[scpi-ERRO]:"_fmt_, ##__VA_ARGS__);\
										syslog(LOG_WARNING, _fmt_",f:%s,l:%d",##__VA_ARGS__,__FILE__,__LINE__);\
										}while(0)
#define scpi_debug_info(_fmt_, ...)   //printf("[scpi-INFO]:"_fmt_, ##__VA_ARGS__)	
#endif /*SCPI_DEBUG_ENABLE*/



/*SCPI debug 0:disable   1:enable*/
#define USB_DEBUG_ENABLE 1

#if (USB_DEBUG_ENABLE)
	
#define usb_debug(_fmt_, ...)        printf("[usb] f:%s,l:%d,"_fmt_"\r\n",__FUNCTION__,__LINE__,##__VA_ARGS__)	
#define usb_debug_warn(_fmt_, ...)   printf("[usb-WARN] f%s,l:%d,"_fmt_"\r\n",__FUNCTION__,__LINE__,##__VA_ARGS__)	
#define usb_debug_err(_fmt_, ...)    printf("[usb-ERRO] f:%s,l:%d,"_fmt_"\r\n",__FUNCTION__,__LINE__,##__VA_ARGS__)	
#define usb_debug_info(_fmt_, ...)   printf("[usb-INFO] f:%s,l:%d,"_fmt_"\r\n",__FUNCTION__,__LINE__,##__VA_ARGS__)	


#else

#define usb_debug(_fmt_, ...)       //printf("[scpi]:"_fmt_, ##__VA_ARGS__)	
#define usb_debug_warn(_fmt_, ...)    do{\
                                        printf("[usb-WARN]:"_fmt_, ##__VA_ARGS__);\
 									    syslog(LOG_WARNING, _fmt_",f:%s,l:%d",##__VA_ARGS__,__FILE__,__LINE__);\
 									    }while(0)
#define usb_debug_err(_fmt_, ...)   do{\
                                        printf("[usb-ERRO]:"_fmt_, ##__VA_ARGS__);\
										syslog(LOG_WARNING, _fmt_",f:%s,l:%d",##__VA_ARGS__,__FILE__,__LINE__);\
										}while(0)
#define usb_debug_info(_fmt_, ...)   //printf("[scpi-INFO]:"_fmt_, ##__VA_ARGS__)	
#endif /*USB_DEBUG_ENABLE*/

#ifdef  __cplusplus
}
#endif

#endif // __SCPI_DEBUG_H_

