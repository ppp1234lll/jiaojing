#ifndef __IAP_H__
#define __IAP_H__
#include "sys.h"  

typedef  void (*iapfun)(void);				//定义一个函数类型的参数.

#define FSCTORY_APP_ADDR		APP2_FLASH_STORE  	// 出厂程序起始地址，永不改变
#define RUN_APP_ADDR		    APP2_FLASH_STORE  	// 运行程序起始地址(存放在FLASH)

void iap_load_app(uint32_t appxaddr);			// 执行flash里面的app程序
void iap_load_appsram(uint32_t appxaddr);	// 执行sram里面的app程序
void iap_write_appbin(uint32_t appxaddr,uint8_t *appbuf,uint32_t applen);	//在指定地址开始,写入bin
#endif
