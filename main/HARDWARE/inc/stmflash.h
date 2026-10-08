#ifndef __STMFLASH_H
#define __STMFLASH_H

#include "sys.h"  
#include "iap.h"
#include "appconfig.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
//用户根据自己的需要设置
#define STM32_FLASH_SIZE 512 	 		//所选STM32的FLASH容量大小(单位为K)
#define STM32_FLASH_WREN 1        //使能FLASH写入(0，不是能;1，使能)
//////////////////////////////////////////////////////////////////////////////////////////////////////

//FLASH起始地址
#define STM32_FLASH_BASE 0x08000000 	//STM32 FLASH的起始地址
#define UPDATE_INFOR_SAVEMODE   1	 	// 0-存储在内部flash 1-存储在外部flash(升级参数改存W25Q128,避免占用/擦除内部Flash扇区)

#if (UPDATE_INFOR_SAVEMODE == 0)
	#define UPDATA_SPIFLASH_ADDR    3*1024*1024	        // 升级BIN文件
	#define UPDATA_PARAM_ADDR       0x08008000          // 升级参数
	#define APP_RUN_PARAM_ADDR      3*1024*1024-4*1024	// 程序运行参数
#else
	#define UPDATA_SPIFLASH_ADDR    3*1024*1024	        // 升级BIN文件
	#define UPDATA_PARAM_ADDR       3*1024*1024-16*1024 // 升级参数(外部W25Q128, 0x2FC000)
	#define APP_RUN_PARAM_ADDR      3*1024*1024-4*1024	// 程序运行参数
#endif


#define DEVICE_FLASH_STORE	0x08004000   
#define APP1_FLASH_STORE	  0x0800C000 
#define APP2_FLASH_STORE	  0x08040000 

#define DEVICE_ID_ADDR      DEVICE_FLASH_STORE
#define DEVICE_MAC_ADDR     DEVICE_FLASH_STORE + 64

//FLASH 扇区的起始地址
#define ADDR_FLASH_SECTOR_0     ((u32)0x08000000) 	//扇区0起始地址, 16 Kbytes  
#define ADDR_FLASH_SECTOR_1     ((u32)0x08004000) 	//扇区1起始地址, 16 Kbytes  
#define ADDR_FLASH_SECTOR_2     ((u32)0x08008000) 	//扇区2起始地址, 16 Kbytes  
#define ADDR_FLASH_SECTOR_3     ((u32)0x0800C000) 	//扇区3起始地址, 16 Kbytes  
#define ADDR_FLASH_SECTOR_4     ((u32)0x08010000) 	//扇区4起始地址, 64 Kbytes  
#define ADDR_FLASH_SECTOR_5     ((u32)0x08020000) 	//扇区5起始地址, 128 Kbytes  
#define ADDR_FLASH_SECTOR_6     ((u32)0x08040000) 	//扇区6起始地址, 128 Kbytes  
#define ADDR_FLASH_SECTOR_7     ((u32)0x08060000) 	//扇区7起始地址, 128 Kbytes  
#define ADDR_FLASH_SECTOR_8     ((u32)0x08080000) 	//扇区8起始地址, 128 Kbytes  
#define ADDR_FLASH_SECTOR_9     ((u32)0x080A0000) 	//扇区9起始地址, 128 Kbytes  
#define ADDR_FLASH_SECTOR_10    ((u32)0x080C0000) 	//扇区10起始地址,128 Kbytes  
#define ADDR_FLASH_SECTOR_11    ((u32)0x080E0000) 	//扇区11起始地址,128 Kbytes  

u32 STMFLASH_ReadWord(u32 faddr);		  	//读出字  
void STMFLASH_Write(u32 WriteAddr,u32 *pBuffer,u32 NumToWrite);		//从指定地址开始写入指定长度的数据
void STMFLASH_Read(u32 ReadAddr,u32 *pBuffer,u32 NumToRead);   		//从指定地址开始读出指定长度的数据
	void STMFLASH_Write_SAVE(u32 ReadAddr,u32 WriteAddr,u32 *pBuffer,u32 NumToWrite);	
//测试写入
void Test_Write(u32 WriteAddr,u32 WriteData);			

void save_read_update_file_infor_function(save_param_t *param);
void save_stroage_update_file_infor_function(save_param_t param);


#endif


