/********************************************************************************
* @File name  : 电能计量通信驱动
* @Description: 串口4
* @Author     : ZHLE
*  Version Date        Modification Description
	7、单相计量芯片: 串口1，波特率4800，
	   引脚分配为：  USART1_TX： PA9
									 USART1_RX： PA10
********************************************************************************/

#include "elec_uart.h"
#include "BL0910.h"
#include "dma.h"

/*
	10、BL0910电能计量芯片：(硬件SPI方式)，引脚分配为：
		MOSI:   PC12
		MISO:   PC11
		CLK:    PC10
*/

//#define SPI_BL0910_SCK		PBout(3)
//#define SPI_BL0910_MOSI	  PBout(5)
//#define SPI_BL0910_MISO	  PBin(4)

///************************************************************
//*
//* Function name	: SPI_INIT
//* Description	: spi初始化函数
//* Parameter		: 
//* Return		: 
//*	
//************************************************************/
//void BL0910_SPI_INIT(void)
//{
//	GPIO_InitTypeDef GPIO_InitStructure;
//	SPI_InitTypeDef  SPI_InitStructure;
//	
//	RCC_AHB1PeriphClockCmd( RCC_AHB1Periph_GPIOC,ENABLE);
//	RCC_APB1PeriphClockCmd(	RCC_APB1Periph_SPI3,  ENABLE );//SPI2时钟使能

//  //GPIOFB3,4,5初始化设置
//  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10|GPIO_Pin_11|GPIO_Pin_12;//PB3~5复用功能输出	
//  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用功能
//  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
//  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
//  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;//上拉
//  GPIO_Init(GPIOC, &GPIO_InitStructure);//初始化

//	GPIO_PinAFConfig(GPIOC,GPIO_PinSource10,GPIO_AF_SPI3); //PB3复用为 SPI1
//	GPIO_PinAFConfig(GPIOC,GPIO_PinSource11,GPIO_AF_SPI3); //PB4复用为 SPI1
//	GPIO_PinAFConfig(GPIOC,GPIO_PinSource12,GPIO_AF_SPI3); //PB5复用为 SPI1
// 
//	//这里只针对SPI口初始化
//	RCC_APB1PeriphResetCmd(RCC_APB1Periph_SPI3,ENABLE);//复位SPI1
//	RCC_APB1PeriphResetCmd(RCC_APB1Periph_SPI3,DISABLE);//停止复位SPI1
//	
//	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;  //设置SPI单向或者双向的数据模式:SPI设置为双线双向全双工
//	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;		//设置SPI工作模式:设置为主SPI
//	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;		//设置SPI的数据大小:SPI发送接收8位帧结构
//	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;		//串行同步时钟的空闲状态为高电平
//	SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;	//串行同步时钟的第二个跳变沿（上升或下降）数据被采样
//	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;		//NSS信号由硬件（NSS管脚）还是软件（使用SSI位）管理:内部NSS信号有SSI位控制
//	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_64;		//定义波特率预分频的值:波特率预分频值为256
//	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;	//指定数据传输从MSB位还是LSB位开始:数据传输从MSB位开始
//	SPI_InitStructure.SPI_CRCPolynomial = 7;	//CRC值计算的多项式
//	SPI_Init(SPI3, &SPI_InitStructure);  //根据SPI_InitStruct中指定的参数初始化外设SPIx寄存器
// 
//	SPI_Cmd(SPI3, ENABLE); //使能SPI外设
//	
////	SPI_ReadWriteByte(0xff);//启动传输		 
//}

///************************************************************
//*
//* Function name	: SPI_ReadWriteByte
//* Description	: 读写字节函数
//* Parameter		: 
//*	@TxData		: 写入字节
//* Return		: 读取到的字节
//*	
//************************************************************/
//uint8_t BL0910_SPI_ReadWriteByte(uint8_t TxData)
//{
//	u8 retry=0;				 	
//	while((SPI3->SR&SPI_I2S_FLAG_TXE)==RESET) //检查指定的SPI标志位设置与否:发送缓存空标志位
//	{
//		retry++;
//		if(retry>200)return 0;
//	}			  
//	SPI3->DR=TxData;	 	//发送一个byte   //通过外设SPIx发送一个数据
//	retry=0;

//	while((SPI3->SR&SPI_I2S_FLAG_RXNE)==RESET) //检查指定的SPI标志位设置与否:接受缓存非空标志位
//	{
//		retry++;
//		if(retry>200)return 0;
//	}	  						    
//	return SPI3->DR; //返回通过SPIx最近接收的数据
//}


#define HARD_SPI_SCLK_GPIO_CLK		RCC_AHB1Periph_GPIOC
#define HARD_SPI_SCLK_GPIO 				GPIOC
#define HARD_SPI_SCLK_PIN  				GPIO_Pin_10

#define HARD_SPI_MOSI_GPIO_CLK		RCC_AHB1Periph_GPIOC
#define HARD_SPI_MOSI_GPIO 				GPIOC
#define HARD_SPI_MOSI_PIN 				GPIO_Pin_12

#define HARD_SPI_MISO_GPIO_CLK		RCC_AHB1Periph_GPIOC
#define HARD_SPI_MISO_GPIO 				GPIOC
#define HARD_SPI_MISO_PIN 				GPIO_Pin_11

#define HARD_SPI_SCLK		PCout(10)
#define HARD_SPI_MOSI		PCout(12)
#define HARD_SPI_MISO		PCin(11)

 	    										
/************************************************************
*
* Function name	: SPI_INIT
* Description	: spi初始化函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void elec_hard_spi_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	RCC_AHB1PeriphClockCmd( HARD_SPI_SCLK_GPIO_CLK|HARD_SPI_MOSI_GPIO_CLK|    
													HARD_SPI_MISO_GPIO_CLK,ENABLE);
	
	GPIO_InitStructure.GPIO_Pin   = HARD_SPI_SCLK_PIN;
	//GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_OUT;			// 输出
	GPIO_InitStructure.GPIO_OType=GPIO_OType_PP; 
	
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(HARD_SPI_SCLK_GPIO,&GPIO_InitStructure); 	

	GPIO_InitStructure.GPIO_Pin   = HARD_SPI_MOSI_PIN;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_OUT;			// 输出
	GPIO_InitStructure.GPIO_OType=GPIO_OType_PP; 
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(HARD_SPI_MOSI_GPIO,&GPIO_InitStructure); 
	
	GPIO_InitStructure.GPIO_Pin   = HARD_SPI_MISO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(HARD_SPI_MISO_GPIO,&GPIO_InitStructure);
	
	
//	GPIO_InitTypeDef GPIO_InitStructure;
//	SPI_InitTypeDef  SPI_InitStructure;
//	
//	RCC_APB1PeriphClockCmd( RCC_AHB1Periph_GPIOC,ENABLE);
//	RCC_APB1PeriphClockCmd(	RCC_APB1Periph_SPI3, ENABLE );//SPI2时钟使能
////	GPIO_PinRemapConfig(GPIO_Remap_SPI3,ENABLE);	GPIO_PinAFConfig(GPIOC, uint16_t GPIO_PinSource, uint8_t GPIO_AF)
//	
//	GPIO_InitStructure.GPIO_Pin = HARD_SPI_SCLK_PIN | HARD_SPI_MOSI_PIN;
////	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  //PB13/14/15复用推挽输出 
//	
//	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用功能
//  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
//	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
//	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;//上拉
//	GPIO_Init(GPIOC, &GPIO_InitStructure);//初始化GPIOB

//	
//	GPIO_InitStructure.GPIO_Pin   = HARD_SPI_MISO_PIN;
////	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
//	
//	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用功能
//  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
//	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
//	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;//上拉
//	GPIO_Init(GPIOC,&GPIO_InitStructure);
	
	
//	GPIO_PinAFConfig(GPIOC,GPIO_PinSource10,GPIO_AF_SPI3); //PC10复用为 SPI3
//	GPIO_PinAFConfig(GPIOC,GPIO_PinSource11,GPIO_AF_SPI3); //PC11复用为 SPI3
//	GPIO_PinAFConfig(GPIOC,GPIO_PinSource12,GPIO_AF_SPI3); //PC12复用为 SPI3

//	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;  //设置SPI单向或者双向的数据模式:SPI设置为双线双向全双工
//	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;		//设置SPI工作模式:设置为主SPI
//	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;		//设置SPI的数据大小:SPI发送接收8位帧结构
//	SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;		//串行同步时钟的空闲状态为高电平
//	SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;	//串行同步时钟的第二个跳变沿（上升或下降）数据被采样
//	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;		//NSS信号由硬件（NSS管脚）还是软件（使用SSI位）管理:内部NSS信号有SSI位控制
//	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_64;		//定义波特率预分频的值:波特率预分频值为256
//	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;	//指定数据传输从MSB位还是LSB位开始:数据传输从MSB位开始
//	SPI_InitStructure.SPI_CRCPolynomial = 7;	//CRC值计算的多项式
//	SPI_Init(SPI3, &SPI_InitStructure);  //根据SPI_InitStruct中指定的参数初始化外设SPIx寄存器
// 
//	SPI_Cmd(SPI3, ENABLE); //使能SPI外设

//	RCC_APB2PeriphClockCmd( HARD_SPI_SCLK_GPIO_CLK|HARD_SPI_MOSI_GPIO_CLK|
//													HARD_SPI_MISO_GPIO_CLK,ENABLE);
//	
//	GPIO_InitStructure.GPIO_Pin   = HARD_SPI_SCLK_PIN;
//	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
//	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
//	GPIO_Init(HARD_SPI_SCLK_GPIO,&GPIO_InitStructure); 	

//	GPIO_InitStructure.GPIO_Pin   = HARD_SPI_MOSI_PIN;
//	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
//	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
//	GPIO_Init(HARD_SPI_MOSI_GPIO,&GPIO_InitStructure); 
//	
//	GPIO_InitStructure.GPIO_Pin   = HARD_SPI_MISO_PIN;
//	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
//	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
//	GPIO_Init(HARD_SPI_MISO_GPIO,&GPIO_InitStructure);


}



void hard_spi_delay(uint16_t time)	
{
	do
	{
	} while (--time);

}

/************************************************************
*
* Function name	: SPI_ReadWriteByte
* Description	: 读写字节函数
* Parameter		: 
*	@TxData		: 写入字节
* Return		: 读取到的字节
*	
************************************************************/
uint8_t hardSPI_ReadWriteByte(uint8_t TxData)
{
	uint8_t RecevieData=0;
	uint8_t i = 0;

	for(i=0; i<8; i++)
	{
		HARD_SPI_SCLK=0;
		hard_spi_delay(6);
		if(TxData&0x80) HARD_SPI_MOSI=1;
		else HARD_SPI_MOSI=0;
		TxData<<=1;
		hard_spi_delay(6);
		HARD_SPI_SCLK=1;  // 上升沿采样
		hard_spi_delay(6);
		RecevieData<<=1;
		if(HARD_SPI_MISO) RecevieData |= 0x01;
		else RecevieData &= ~0x01;   // 下降沿接收数据
		hard_spi_delay(6);
	}
	HARD_SPI_SCLK=0;  // idle情况下SCK为电平
	hard_spi_delay(6);
	return RecevieData;
}

 /*
* 函数名： void SPI_WriteByte(uint8_t data)
* 输入参数： data -> 要写的数据
* 输出参数：无  
* 返回值：无
* 函数作用：模拟 SPI 写一个字节
*/ // SPI写1 Byte，循环8次，每次发送1 Bit；
void hardSPI_WriteByte(uint8_t TxData)  
{
	uint8_t i = 0;  
	for(i=0; i<8; i++) 
	{
		HARD_SPI_SCLK = 0; //CPOL=0              //拉低时钟，即空闲时钟为低电平， CPOL=0；
		if(TxData&0x80) HARD_SPI_MOSI=1;
		else HARD_SPI_MOSI=0;
		TxData<<=1;
		hard_spi_delay(15); 
		HARD_SPI_SCLK=1;  // 上升沿采样 //CPHA=0  //拉高时钟， W25Q64只支持SPI模式0或1，即会在时钟上升沿采样MOSI数据；
		hard_spi_delay(15); 
	}
	HARD_SPI_SCLK = 0;                          //最后SPI发送完后，拉低时钟，进入空闲状态；
}
/************************************************************
*
* Function name	: softSPI_Write_Multi_Byte
* Description	: 写字节函数
* Parameter		: 
*	@TxData		: 写入字节
*	
************************************************************/
void hardSPI_Write_Multi_Byte(uint8_t *buff, uint16_t len)
{
	while(len--) {
		hardSPI_WriteByte(buff[0]);
		buff++;
	}
}

/*
* 函数名： uint8_t SPI_ReadByte(void)
* 输入参数：
* 输出参数：无
* 返回值：读到的数据
* 函数作用：模拟 SPI 读一个字节
*/  // SPI读1 Byte，循环8次，每次接收1 Bit；  
uint8_t hardSPI_ReadByte(void)
{
	uint8_t i = 0;
	uint8_t RecevieData=0;
	for(i=0; i<8; i++) 
	{
		HARD_SPI_SCLK = 1;                  //拉低时钟，即空闲时钟为低电平；  
		hard_spi_delay(15); 
		HARD_SPI_SCLK = 0;   
		RecevieData<<=1;
		if(HARD_SPI_MISO) RecevieData |= 0x01;
		else RecevieData &= ~0x01;   // 下降沿接收数据
		hard_spi_delay(15); 
	}
	return RecevieData;
}



