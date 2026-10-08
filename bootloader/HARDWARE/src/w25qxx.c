#include "w25qxx.h"
#include "spi.h"
#include "delay.h"
#include "malloc.h"

/*
	10、W25Q128存储芯片：(硬件SPI方式)，引脚分配为：
		MOSI:   PB5
		MISO:   PB4
		CLK:    PB3
		CS:     PD7
*/

uint16_t W25QXX_TYPE ;	// 默认是W25Q128

#define	W25QXX_CS 			PDout(7)  	// W25QXX的片选信号

/************************************************************
*
* Function name	: w25qxx_cs_init_function
* Description	: CS控制脚初始化函数
* Parameter		:
* Return		:
*
************************************************************/
void w25qxx_cs_init_function(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD,ENABLE);

	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_7;
 	GPIO_InitStructure.GPIO_Mode 	= GPIO_Mode_OUT;//输出
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
	GPIO_InitStructure.GPIO_PuPd 	= GPIO_PuPd_UP;//上拉
	GPIO_Init(GPIOD,&GPIO_InitStructure); 	
}

/************************************************************
*
* Function name	: W25QXX_Init
* Description	: 初始化函数-4Kbytes为一个Sector-16个扇区为1个Block
*				: -容量为16M字节,共有128个Block,4096个Sector
* Parameter		: 
* Return		: 
*	
************************************************************/
void W25QXX_Init(void)
{
	uint8_t flag = 0;

	w25qxx_cs_init_function();
	W25QXX_CS = 1;			                // SPI FLASH不选中
	w25qxx_SPI_INIT();						// 初始化SPI
	W25QXX_TYPE=W25QXX_ReadID();	        // 读取FLASH ID.
	W25QXX_CS = 1;							// SPI FLASH不选中

	if(flag == 1) {
			W25QXX_Erase_Chip();
	}
}

/************************************************************
*
* Function name	: W25QXX_ReadSR
* Description	: 读取SR寄存器状态
* Parameter		: 
* Return		: 状态
*	
************************************************************/
static uint8_t W25QXX_ReadSR(void)
{
	uint8_t byte=0;

	W25QXX_CS=0;                            // 使能器件
	SPI_ReadWriteByte(W25X_ReadStatusReg);	// 发送读取状态寄存器命令
	byte=SPI_ReadWriteByte(0Xff);        	// 读取一个字节
	W25QXX_CS=1;                            // 取消片选

	return byte;
}
#if 0
/************************************************************
*
* Function name	: W25QXX_Write_SR
* Description	: 写SR状态寄存器
* Parameter		: 
*	@sr			: 写入数据
* Return		: 
*	
************************************************************/
static void W25QXX_Write_SR(uint8_t sr)
{
    W25QXX_CS=0;                            // 使能器件
    SPI_ReadWriteByte(W25X_WriteStatusReg); // 发送写取状态寄存器命令
    SPI_ReadWriteByte(sr);              	// 写入一个字节
    W25QXX_CS=1;                            // 取消片选
}
#endif

/************************************************************
*
* Function name	: W25QXX_Write_Enable
* Description	: 写使能
* Parameter		: 
* Return		: 
*	
************************************************************/
static void W25QXX_Write_Enable(void)
{
    W25QXX_CS=0;                            // 使能器件
    SPI_ReadWriteByte(W25X_WriteEnable); 	// 发送写使能
    W25QXX_CS=1;                            // 取消片选
}
#if 0
/************************************************************
*
* Function name	: W25QXX_Write_Disable
* Description	: 写禁止
* Parameter		: 
* Return		: 
*	
************************************************************/
static void W25QXX_Write_Disable(void)
{
    W25QXX_CS=0;                            // 使能器件
    SPI_ReadWriteByte(W25X_WriteDisable); 	// 发送写禁止指令
    W25QXX_CS=1;                            // 取消片选
}
#endif
/************************************************************
*
* Function name	: W25QXX_ReadID
* Description	: 读取ID号
* Parameter		: 
* Return		: ID号
*	
************************************************************/
uint16_t W25QXX_ReadID(void)
{
	uint16_t Temp = 0;

	W25QXX_CS=0;
	SPI_ReadWriteByte(0x90); // 发送读取ID命令
	SPI_ReadWriteByte(0x00);
	SPI_ReadWriteByte(0x00);
	SPI_ReadWriteByte(0x00);
	Temp|=SPI_ReadWriteByte(0xFF)<<8;
	Temp|=SPI_ReadWriteByte(0xFF);
	W25QXX_CS=1;

	return Temp;
}

/************************************************************
*
* Function name	   : W25QXX_Read
* Description	   : 读取数据
* Parameter		   : 
*	@pBuffer	   : 数据存储区
*	@ReadAddr	   : 开始读取的地址(24bit)
*	@NumByteToRead : 要读取的字节数(最大65535)
* Return		   : 
*	
************************************************************/
void W25QXX_Read(uint8_t* pBuffer,uint32_t ReadAddr,uint16_t NumByteToRead)
{
	uint16_t i;
	W25QXX_CS=0;                            		// 使能器件
	SPI_ReadWriteByte(W25X_ReadData);   			// 发送读取命令
	SPI_ReadWriteByte((uint8_t)((ReadAddr)>>16));	// 发送24bit地址
	SPI_ReadWriteByte((uint8_t)((ReadAddr)>>8));
	SPI_ReadWriteByte((uint8_t)ReadAddr);
	for(i=0; i<NumByteToRead; i++)
	{
			pBuffer[i]=SPI_ReadWriteByte(0XFF);   		// 循环读数
	}
	W25QXX_CS=1;
}

/************************************************************
*
* Function name	: W25QXX_Wait_Busy
* Description	: 等待空闲
* Parameter		: 
* Return		: 
*	
************************************************************/
static void W25QXX_Wait_Busy(void)
{
    while((W25QXX_ReadSR()&0x01)==0x01);   // 等待BUSY位清空
}

/************************************************************
*
* Function name		: W25QXX_Write_Page
* Description		: 
* Parameter			: 
*	@pBuffer		: 数据存储区
*	@WriteAddr		: 开始写入的地址(24bit)
*	@NumByteToWrite : 要写入的字节数(最大256),该数不应该超过该页的剩余字节数!!!
* Return			: 
*	
************************************************************/
void W25QXX_Write_Page(uint8_t* pBuffer,uint32_t WriteAddr,uint16_t NumByteToWrite)
{
    uint16_t i;
    W25QXX_Write_Enable();                    // SET WEL
    W25QXX_CS=0;                              // 使能器件
    SPI_ReadWriteByte(W25X_PageProgram);      // 发送写页命令
    SPI_ReadWriteByte((uint8_t)((WriteAddr)>>16)); // 发送24bit地址
    SPI_ReadWriteByte((uint8_t)((WriteAddr)>>8));
    SPI_ReadWriteByte((uint8_t)WriteAddr);
    for(i=0; i<NumByteToWrite; i++)SPI_ReadWriteByte(pBuffer[i]); // 循环写数
    W25QXX_CS=1;                 			// 取消片选
    W25QXX_Wait_Busy();						// 等待写入结束
}

/************************************************************
*
* Function name		: W25QXX_Write_NoCheck
* Description		: 写数据-无需校验
* Parameter			: 
*	@pBuffer		: 数据存储区
*	@WriteAddr		: 开始写入的地址(24bit)
*	@NumByteToWrite : 要写入的字节数(最大65535)
* Return			: 
*	
************************************************************/
void W25QXX_Write_NoCheck(uint8_t* pBuffer,uint32_t WriteAddr,uint16_t NumByteToWrite)
{
    uint16_t pageremain;
    pageremain=256-WriteAddr%256; // 单页剩余的字节数
    if(NumByteToWrite<=pageremain)pageremain=NumByteToWrite; // 不大于256个字节
    while(1)
    {
        W25QXX_Write_Page(pBuffer,WriteAddr,pageremain);
        if(NumByteToWrite==pageremain) {
			break; 									// 写入结束了
        } else {									// NumByteToWrite>pageremain
            pBuffer+=pageremain;
            WriteAddr+=pageremain;

            NumByteToWrite-=pageremain;			  	// 减去已经写入了的字节数
            if(NumByteToWrite>256)pageremain=256; 	// 一次可以写入256个字节
            else pageremain=NumByteToWrite; 	  	// 不够256个字节了
        }
    };
}

#if W25QXX_USE_MALLOC==0
uint8_t W25QXX_BUFFER[4096] = {0};
#endif
/************************************************************
*
* Function name		: W25QXX_Write
* Description		: 写SPI FLASH
* Parameter			: 
*	@pBuffer		: 数据存储区
*	@WriteAddr		: 开始写入的地址(24bit)
*	@NumByteToWrite : 要写入的字节数(最大65535)
* Return			: 
*	
************************************************************/
void W25QXX_Write(uint8_t* pBuffer,uint32_t WriteAddr,uint16_t NumByteToWrite)
{
    uint32_t secpos;
    uint32_t secoff;
    uint32_t secremain;
    uint32_t i;
    uint8_t * W25QXX_BUF;
#if	W25QXX_USE_MALLOC==1	// 动态内存管理
    W25QXX_BUF=mymalloc(SRAMIN,4096);//申请内存
#else
    W25QXX_BUF=W25QXX_BUFFER;
#endif
    secpos=WriteAddr/4096; // 扇区地址
    secoff=WriteAddr%4096; // 在扇区内的偏移
    secremain=4096-secoff; // 扇区剩余空间大小

    if(NumByteToWrite<=secremain)secremain=NumByteToWrite; // 不大于4096个字节
    while(1)
    {
        W25QXX_Read(W25QXX_BUF,secpos*4096,4096); 	// 读出整个扇区的内容
        for(i=0; i<secremain; i++) 					// 校验数据
        {
            if(W25QXX_BUF[secoff+i]!=0XFF)break;	// 需要擦除
        }
        if(i<secremain)								// 需要擦除
        {
            W25QXX_Erase_Sector(secpos);			// 擦除这个扇区
            for(i=0; i<secremain; i++)	  			// 复制
            {
                W25QXX_BUF[i+secoff]=pBuffer[i];
            }
            W25QXX_Write_NoCheck(W25QXX_BUF,secpos*4096,4096);	// 写入整个扇区
        } else {
            W25QXX_Write_NoCheck(pBuffer,WriteAddr,secremain);  // 写已经擦除了的,直接写入扇区剩余区间.
        }
        if(NumByteToWrite==secremain) {
			break;												// 写入结束了
        } else {												// 写入未结束
            secpos++;											// 扇区地址增1
            secoff=0;											// 偏移位置为0

            pBuffer+=secremain;  								// 指针偏移
            WriteAddr+=secremain;								// 写地址偏移
            NumByteToWrite-=secremain;							// 字节数递减
            if(NumByteToWrite>4096) {
				secremain=4096;									// 下一个扇区还是写不完
            } else { 
				secremain = NumByteToWrite;						// 下一个扇区可以写完了
			}
        }
    };
#if	W25QXX_USE_MALLOC==1
    myfree(SRAMIN,W25QXX_BUF);	// 释放内存
#endif
}

/************************************************************
*
* Function name	: W25QXX_Erase_Chip
* Description	: 擦除片
* Parameter		: 
* Return		: 
*	
************************************************************/
void W25QXX_Erase_Chip(void)
{
    W25QXX_Write_Enable();                  // SET WEL
    W25QXX_Wait_Busy();
    W25QXX_CS=0;                            // 使能器件
    SPI_ReadWriteByte(W25X_ChipErase);		// 发送片擦除命令
    W25QXX_CS=1;                            // 取消片选
    W25QXX_Wait_Busy();   				   	// 等待芯片擦除结束
}

/************************************************************
*
* Function name	: W25QXX_Erase_Sector
* Description	: 擦除扇区
* Parameter		: 
*	@Dst_Addr	: 地址
* Return		: 
*	
************************************************************/
void W25QXX_Erase_Sector(uint32_t Dst_Addr)
{
    Dst_Addr*=4096;
    W25QXX_Write_Enable();                  // SET WEL
    W25QXX_Wait_Busy();
    W25QXX_CS=0;                            // 使能器件
    SPI_ReadWriteByte(W25X_SectorErase);  	// 发送扇区擦除指令
    SPI_ReadWriteByte((uint8_t)((Dst_Addr)>>16));  // 发送24bit地址
    SPI_ReadWriteByte((uint8_t)((Dst_Addr)>>8));
    SPI_ReadWriteByte((uint8_t)Dst_Addr);
    W25QXX_CS=1;                       		// 取消片选
    W25QXX_Wait_Busy();   				   	// 等待擦除完成
}



/************************************************************
*
* Function name	: W25QXX_PowerDown
* Description	: 进入掉电模式
* Parameter		: 
* Return		: 
*	
************************************************************/
void W25QXX_PowerDown(void)
{
    W25QXX_CS=0;                            // 使能器件
    SPI_ReadWriteByte(W25X_PowerDown);      // 发送掉电命令
    W25QXX_CS=1;                            // 取消片选
    delay_us(3);                            // 等待TPD
}

/************************************************************
*
* Function name	: W25QXX_WAKEUP
* Description	: 唤醒
* Parameter		: 
* Return		: 
*	
************************************************************/
void W25QXX_WAKEUP(void)
{
    W25QXX_CS=0;                            	// 使能器件
    SPI_ReadWriteByte(W25X_ReleasePowerDown);	// send W25X_PowerDown command 0xAB
    W25QXX_CS=1;                            	// 取消片选
    delay_us(3);                               	// 等待TRES1
}


