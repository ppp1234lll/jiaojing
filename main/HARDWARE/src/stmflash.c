#include "stmflash.h"
#include "delay.h"
#include "IWDG.h" 
#include "w25qxx.h"
 #include "includes.h"
 
//读取指定地址的半字(16位数据) 
//faddr:读地址 
//返回值:对应数据.
u32 STMFLASH_ReadWord(u32 faddr)
{
	return *(vu32*)faddr; 
}  
//获取某个地址所在的flash扇区
//addr:flash地址
//返回值:0~11,即addr所在的扇区
uint16_t STMFLASH_GetFlashSector(u32 addr)
{
	if(addr<ADDR_FLASH_SECTOR_1)return FLASH_Sector_0;
	else if(addr<ADDR_FLASH_SECTOR_2)return FLASH_Sector_1;
	else if(addr<ADDR_FLASH_SECTOR_3)return FLASH_Sector_2;
	else if(addr<ADDR_FLASH_SECTOR_4)return FLASH_Sector_3;
	else if(addr<ADDR_FLASH_SECTOR_5)return FLASH_Sector_4;
	else if(addr<ADDR_FLASH_SECTOR_6)return FLASH_Sector_5;
	else if(addr<ADDR_FLASH_SECTOR_7)return FLASH_Sector_6;
	else if(addr<ADDR_FLASH_SECTOR_8)return FLASH_Sector_7;
	else if(addr<ADDR_FLASH_SECTOR_9)return FLASH_Sector_8;
	else if(addr<ADDR_FLASH_SECTOR_10)return FLASH_Sector_9;
	else if(addr<ADDR_FLASH_SECTOR_11)return FLASH_Sector_10; 
	return FLASH_Sector_11;	
}
//从指定地址开始写入指定长度的数据
//特别注意:因为STM32F4的扇区实在太大,没办法本地保存扇区数据,所以本函数
//         写地址如果非0XFF,那么会先擦除整个扇区且不保存扇区数据.所以
//         写非0XFF的地址,将导致整个扇区数据丢失.建议写之前确保扇区里
//         没有重要数据,最好是整个扇区先擦除了,然后慢慢往后写. 
//该函数对OTP区域也有效!可以用来写OTP区!
//OTP区域地址范围:0X1FFF7800~0X1FFF7A0F
//WriteAddr:起始地址(此地址必须为4的倍数!!)
//pBuffer:数据指针
//NumToWrite:字(32位)数(就是要写入的32位数据的个数.) 
void STMFLASH_Write(u32 WriteAddr,u32 *pBuffer,u32 NumToWrite)	
{ 
	FLASH_Status status = FLASH_COMPLETE;
	u32 addrx=0;
	u32 endaddr=0;	
	if(WriteAddr<STM32_FLASH_BASE||WriteAddr%4) {
		return;	//非法地址
	}
	FLASH_Unlock();										//解锁 
	FLASH_DataCacheCmd(DISABLE);						//FLASH擦除期间,必须禁止数据缓存
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGAERR|FLASH_FLAG_PGPERR | FLASH_FLAG_WRPERR|FLASH_FLAG_PGSERR);
	
	addrx=WriteAddr;				//写入的起始地址
	endaddr=WriteAddr+NumToWrite*4;	//写入的结束地址
	if(addrx<0X1FFF0000)			//只有主存储区,才需要执行擦除操作!!
	{
		while(addrx<endaddr)		//扫清一切障碍.(对非FFFFFFFF的地方,先擦除)
		{
			IWDG_Feed();
			if(STMFLASH_ReadWord(addrx)!=0XFFFFFFFF)//有非0XFFFFFFFF的地方,要擦除这个扇区
			{   
//				FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGAERR|FLASH_FLAG_PGPERR | FLASH_FLAG_WRPERR);
				status=FLASH_EraseSector(STMFLASH_GetFlashSector(addrx),VoltageRange_3);//VCC=2.7~3.6V之间!!
				if(status!=FLASH_COMPLETE){
					break;	//发生错误了
				}
			}else addrx+=4;
		} 
	}
	if(status==FLASH_COMPLETE)
	{
		while(WriteAddr<endaddr)//写数据
		{
			IWDG_Feed();
			if(FLASH_ProgramWord(WriteAddr,*pBuffer)!=FLASH_COMPLETE)//写入数据
			{ 
				break;	//写入异常
			}
			WriteAddr+=4;
			pBuffer++;
		} 
	}
	FLASH_DataCacheCmd(ENABLE);	//FLASH擦除结束,开启数据缓存
	FLASH_Lock();//上锁
} 

#define STM_SECTOR_SIZE	2048
u32 STMFLASH_BUF[STM_SECTOR_SIZE/4];//最多是2K字节
//ReadAddr : 保存区所在扇区的基地址
//WriteAddr: 目标数据的绝对地址(必须与读取时STMFLASH_Read使用的地址一致)
//pBuffer  : 数据指针
//NumToWrite: 字(32位)数
void STMFLASH_Write_SAVE(u32 ReadAddr,u32 WriteAddr,u32 *pBuffer,u32 NumToWrite)	
{
	u32 secoff;	   //扇区内偏移地址(32位字计算)
 	u16 i; 
	secoff = (WriteAddr - ReadAddr) / 4;		//扇区内偏移地址(以32位字为单位,须除以4,否则写入地址与读取地址不一致)
	IWDG_Feed();
	STMFLASH_Read(ReadAddr,(uint32_t*)STMFLASH_BUF,STM_SECTOR_SIZE/4); //读出2K扇区的内容
	for(i=0;i<NumToWrite;i++)//复制
	{
		STMFLASH_BUF[i+secoff]=pBuffer[i];	  
	}
	STMFLASH_Write(ReadAddr, STMFLASH_BUF,STM_SECTOR_SIZE/4);
}



//从指定地址开始读出指定长度的数据
//ReadAddr:起始地址
//pBuffer:数据指针
//NumToRead:字(4位)数
void STMFLASH_Read(u32 ReadAddr,u32 *pBuffer,u32 NumToRead)   	
{
	u32 i;
	for(i=0;i<NumToRead;i++)
	{
		pBuffer[i]=STMFLASH_ReadWord(ReadAddr);//读取4个字节.
		ReadAddr+=4;//偏移4个字节.	
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//WriteAddr:起始地址
//WriteData:要写入的数据
void Test_Write(u32 WriteAddr,u32 WriteData)   	
{
	STMFLASH_Write(WriteAddr,&WriteData,1);//写入一个字 
}

/************************************************************
*
* Function name	: save_stroage_update_file_infor_function
* Description	: 存储更新信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void save_stroage_update_file_infor_function(save_param_t param)
{
	OS_CPU_SR 	cpu_sr;
	OS_ENTER_CRITICAL();// 关中断
  #if (UPDATE_INFOR_SAVEMODE == 0)
  	STMFLASH_Write(UPDATA_PARAM_ADDR, (u32 *)&param, sizeof(save_param_t) / 4);
	#else
		W25QXX_Write((uint8_t *)&param,UPDATA_PARAM_ADDR,sizeof(save_param_t));
//	  W25QXX_Write_Update_Param((uint8_t *)&param,UPDATA_PARAM_ADDR,sizeof(save_param_t));
	#endif
	OS_EXIT_CRITICAL();// 开中断
}

/************************************************************
*
* Function name	: save_read_update_file_infor_function
* Description	: 读取更新信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void save_read_update_file_infor_function(save_param_t *param)
{
  #if (UPDATE_INFOR_SAVEMODE == 0)
  	STMFLASH_Read(UPDATA_PARAM_ADDR, (u32 *)param, sizeof(save_param_t) / 4);
	#else
		W25QXX_Read((uint8_t*)param,UPDATA_PARAM_ADDR,sizeof(save_param_t));
	#endif
}



