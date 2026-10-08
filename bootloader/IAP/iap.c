#include "main.h"

iapfun jump2app;
u32 iapbuf[128];
//appxaddr:应用程序的起始地址
//appbuf:应用程序CODE.
//appsize:应用程序大小(字节).
void iap_write_appbin(u32 appxaddr,u8 *appbuf,u32 appsize)
{
	u16 t;
	u16 i=0;
	u32 temp;
	u32 buff[4];
	u32 fwaddr=appxaddr;//当前写入的地址
	u8 *dfu=appbuf;
	for(t=0;t<appsize;t+=4)
	{		
		buff[0] = dfu[0];
		buff[1] = dfu[1];
		buff[2] = dfu[2];
		buff[3] = dfu[3];
		
		temp = buff[0]|(buff[1]<<8)|(buff[2]<<16)|(buff[3]<<24);
		
		dfu+=4;//偏移2个字节
		iapbuf[i++]=temp;	    
		if(i==128)
		{
			i=0;
			STMFLASH_Write(fwaddr,iapbuf,128);	
			fwaddr+=512;//偏移2048  16=2*8.所以要乘以2.
		}
	}
	if(i)STMFLASH_Write(fwaddr,iapbuf,i);//将最后的一些内容字节写进去.  
}

/**************************************************************************************************
* 名    称：  void Iap_Load_App(u32 appxAddr)
* 说    明：  跳转执行APP程序
* 入口参数：
*				 @param1  appxAddr:     用户代码起始地址.
*				     @arg  0x0801C000： 从Flash地址0x08010000开始执行APP
* 说    明： 
* 调用方法： Iap_Load_App(FLASH_APP1_ADDR) ;//执行FLASH APP代码
*            Iap_Load_App(0X20001000) ;//SRAM地址
*************************************************************************************************/
void iap_load_app(u32 appxaddr)
{
	if(((*(vu32*)appxaddr)&0x2FFE0000)==0x20000000)	//检查栈顶地址是否合法.
	{ 
		jump2app=(iapfun)*(vu32*)(appxaddr+4);		//用户代码区第二个字为程序开始地址(复位地址)		
		MSR_MSP(*(vu32*)appxaddr);					//初始化APP堆栈指针(用户代码区的第一个字用于存放栈顶地址)
		jump2app();									//跳转到APP.
	}
}		 
