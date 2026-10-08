#ifndef __STMFLASH_H__
#define __STMFLASH_H__
#include "sys.h"   

/* FLASH地址划分
| 扇区    |  起始地址  |  |  最终地址  |   大小  | 名称         | 成分              |
| :---:   | :--------: | | :--------: |------:  | :---------   | :---------------  |
|   0     | 0x08000000 | | 0x08003FFF |  16K    | bootloader   | 升级、引导APP      |
|   1     | 0x08004000 | | 0x08007FFF |  16K    | 永久保存数据 | 设备ID、MAC地址    |
| 2,3,4,5 | 0x08008000 | | 0x0803F000 |  208K   | param_bak    | 出厂程序，永不变更 |
|   6,7   | 0x08040000 | | 0x0807F000 |  256K   | -            | APP2,可升级        |
*/
//////////////////////////////////////////////////////////////////////////////////////////////////////
//用户根据自己的需要设置
#define STM32_FLASH_SIZE 512 	 		      // 所选STM32的FLASH容量大小(单位为K)
#define STM32_FLASH_WREN 1              // 使能FLASH写入(0，不是能;1，使能)
//////////////////////////////////////////////////////////////////////////////////////////////////////

//FLASH起始地址
#define STM32_FLASH_BASE	  0x08000000 	//STM32 FLASH的起始地址

#define DEVICE_FLASH_STORE	0x08004000   
#define APP1_FLASH_STORE	  0x0800C000 
#define APP2_FLASH_STORE	  0x08040000 
 

//FLASH 扇区的起始地址
#define ADDR_FLASH_SECTOR_0     ((uint32_t)0x08000000) 	//扇区0起始地址, 16 Kbytes  
#define ADDR_FLASH_SECTOR_1     ((uint32_t)0x08004000) 	//扇区1起始地址, 16 Kbytes  
#define ADDR_FLASH_SECTOR_2     ((uint32_t)0x08008000) 	//扇区2起始地址, 16 Kbytes  
#define ADDR_FLASH_SECTOR_3     ((uint32_t)0x0800C000) 	//扇区3起始地址, 16 Kbytes  
#define ADDR_FLASH_SECTOR_4     ((uint32_t)0x08010000) 	//扇区4起始地址, 64 Kbytes  
#define ADDR_FLASH_SECTOR_5     ((uint32_t)0x08020000) 	//扇区5起始地址, 128 Kbytes  
#define ADDR_FLASH_SECTOR_6     ((uint32_t)0x08040000) 	//扇区6起始地址, 128 Kbytes  
#define ADDR_FLASH_SECTOR_7     ((uint32_t)0x08060000) 	//扇区7起始地址, 128 Kbytes  
#define ADDR_FLASH_SECTOR_8     ((uint32_t)0x08080000) 	//扇区8起始地址, 128 Kbytes  
#define ADDR_FLASH_SECTOR_9     ((uint32_t)0x080A0000) 	//扇区9起始地址, 128 Kbytes  
#define ADDR_FLASH_SECTOR_10    ((uint32_t)0x080C0000) 	//扇区10起始地址,128 Kbytes  
#define ADDR_FLASH_SECTOR_11    ((uint32_t)0x080E0000) 	//扇区11起始地址,128 Kbytes  

uint32_t STMFLASH_ReadWord(uint32_t faddr);		  	//读出字  
void STMFLASH_Write(uint32_t WriteAddr,uint32_t *pBuffer,uint32_t NumToWrite);		//从指定地址开始写入指定长度的数据
void STMFLASH_Read(uint32_t ReadAddr,uint32_t *pBuffer,uint32_t NumToRead);   		//从指定地址开始读出指定长度的数据
						   
#endif

















