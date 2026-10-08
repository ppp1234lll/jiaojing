#include "lis3dh_iic.h"
#include "delay.h"

/*
	8、3轴加速度计LIS3DH: (模拟IIC方式)，引脚分配为：
		SCL:   PE7
		SDA:   PB1
*/

#define IIC_SCL_GPIO_CLK				    RCC_AHB1Periph_GPIOE
#define IIC_SCL_GPIO_PORT           GPIOE
#define IIC_SCL_PIN                 GPIO_Pin_7

#define IIC_SDA_GPIO_CLK				    RCC_AHB1Periph_GPIOB
#define IIC_SDA_GPIO_PORT           GPIOB
#define IIC_SDA_PIN                 GPIO_Pin_1

/* IO操作 */
#define IIC_SCL   PEout(7) // SCL
#define IIC_SDA   PBout(1) // SDA
#define READ_SDA  PBin(1)  // 输入SDA

/************************************************************
*
* Function name	: iic_sda_in_function
* Description	: SDA配置为输入模式
* Parameter		: 
* Return		: 
*	
************************************************************/
void iic_sda_in_function(void)
{
	GPIOB->MODER&=~(3<<(1*2));
	GPIOB->MODER|= 0<<1*2;	//PE5输入模式
}

/************************************************************
*
* Function name	: iic_sda_out_function
* Description	: SDA配置为输出模式
* Parameter		: 
* Return		: 
*	
************************************************************/
void iic_sda_out_function(void)
{
	GPIOB->MODER&=~(3<<(1*2));
	GPIOB->MODER|= 1<<1*2;  //PE5输出模式
}

/* IO方向设置 */
#define SDA_IN()  iic_sda_in_function()
#define SDA_OUT() iic_sda_out_function()


/************************************************************
*
* Function name	: HAL_IIC_Init
* Description	: 初始化函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void LIS3DH_IIC_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(IIC_SCL_GPIO_CLK|IIC_SDA_GPIO_CLK,ENABLE);

	GPIO_InitStructure.GPIO_Pin   = IIC_SCL_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;  		// 输出
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;  		// 上拉
	GPIO_InitStructure.GPIO_Speed = GPIO_High_Speed;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(IIC_SCL_GPIO_PORT,&GPIO_InitStructure); 

	GPIO_InitStructure.GPIO_Pin   = IIC_SDA_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;  		// 输出
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;  		// 上拉
	GPIO_InitStructure.GPIO_Speed = GPIO_High_Speed;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(IIC_SDA_GPIO_PORT,&GPIO_InitStructure);

	IIC_SDA=1;
	IIC_SCL=1;  
}

/************************************************************
*
* Function name	: IIC_Start
* Description	: 起始信号
* Parameter		: 
* Return		: 
*	
************************************************************/
static void IIC_Start(void)
{
	SDA_OUT(); // sda线输出
	IIC_SDA=1;
	delay_us(2);
	IIC_SCL=1;
	delay_us(4);
 	IIC_SDA=0; // START:when CLK is high,DATA change form high to low 
	delay_us(4);
	IIC_SCL=0; // 钳住I2C总线，准备发送或接收数据 
}

/************************************************************
*
* Function name	: IIC_Stop
* Description	: 停止信号
* Parameter		: 
* Return		: 
*	
************************************************************/
static void IIC_Stop(void)
{
	SDA_OUT(); // sda线输出
	IIC_SCL=0;
	delay_us(2);
	IIC_SDA=0; // STOP:when CLK is high DATA change form low to high
 	delay_us(4);
	IIC_SCL=1; 
	IIC_SDA=1; // 发送I2C总线结束信号
	delay_us(4);							   	
}
      
/************************************************************
*
* Function name	: IIC_Wait_Ack
* Description	: 应答信号
* Parameter		: 
* Return		: 1，接收应答失败 0，接收应答成功
*	
************************************************************/
static uint8_t IIC_Wait_Ack(void)
{
	uint8_t ucErrTime=0;
	SDA_IN();      // SDA设置为输入  
	IIC_SDA=1;delay_us(1);	   
	IIC_SCL=1;delay_us(1);	 
	while(READ_SDA)
	{
		ucErrTime++;
		if(ucErrTime>250)
		{
			IIC_Stop();
			return 1;
		}
	}
	IIC_SCL=0; // 时钟输出0 	   
	return 0;  
} 

/************************************************************
*
* Function name	: IIC_Ack
* Description	: 产生应答信号
* Parameter		: 
* Return		: 
*	
************************************************************/
static void IIC_Ack(void)
{
	IIC_SCL=0;
	SDA_OUT();
	IIC_SDA=0;
	delay_us(2);
	IIC_SCL=1;
	delay_us(2);
	IIC_SCL=0;
}

/************************************************************
*
* Function name	: IIC_NAck
* Description	: 不参数应答信号
* Parameter		: 
* Return		: 
*	
************************************************************/
static void IIC_NAck(void)
{
	IIC_SCL=0;
	SDA_OUT();
	IIC_SDA=1;
	delay_us(2);
	IIC_SCL=1;
	delay_us(2);
	IIC_SCL=0;
}					 				     

/************************************************************
*
* Function name	: IIC_Send_Byte
* Description	: 发送一个字节
* Parameter		: 
*	@txd		: 字节
* Return		: 1，有应答 0，无应答	
*	
************************************************************/
static void IIC_Send_Byte(uint8_t txd)
{                        
	uint8_t t;  

	SDA_OUT(); 	    
	IIC_SCL=0;	// 拉低时钟开始数据传输
	for(t=0;t<8;t++)
	{              
		IIC_SDA=(txd&0x80)>>7;
		txd<<=1; 	  
		delay_us(2);   // 对TEA5767这三个延时都是必须的
		IIC_SCL=1;
		delay_us(2); 
		IIC_SCL=0;	
		delay_us(2);
	}	 
} 	    

/************************************************************
*
* Function name	: IIC_Read_Byte
* Description	: 读取一个字节
* Parameter		: 
*	@ack		: 1:发送ACK 0:发送nACK
* Return		: 字节
*	
************************************************************/
static uint8_t IIC_Read_Byte(unsigned char ack)
{
	unsigned char i,receive=0;
	
	SDA_IN();//SDA设置为输入
	for(i=0;i<8;i++ )
	{
		IIC_SCL=0; 
			delay_us(1);
		receive<<=1;
			if(READ_SDA)receive++;   
		delay_us(1);
		IIC_SCL=1;
		delay_us(1);
		delay_us(1);
	}					 
	if (!ack)
		IIC_NAck();//发送nACK
	else
		IIC_Ack(); //发送ACK   

	return receive;
}

static uint8_t m_DeviceIDs[1] = {0x30}; //默认地址
/************************************************************
*
* Function name	: HAL_IIC_EMU_Read
* Description	: 读取数据
* Parameter		: 
*	@Addr		: 地址
*	@pBuf		: 数据指针
*	@Len		: 数据长度
* Return		: 
*	
************************************************************/
bool HAL_IIC_EMU_Read(uint8_t Addr,uint8_t *pBuf,uint32_t Len)
{
	bool 	 Vale = true;
	uint32_t i;

	IIC_Start();
	IIC_Send_Byte(m_DeviceIDs[0]);
	if(IIC_Wait_Ack())
	{
		Vale = false;
		goto Exit;
	}
	IIC_Send_Byte(Addr);
	if(IIC_Wait_Ack())
	{
		Vale = false;
		goto Exit;
	}

//	delay_ms(2);

	IIC_Start();
	IIC_Send_Byte((1+ m_DeviceIDs[0]));
	if(IIC_Wait_Ack())
	{
		Vale = false;
		goto Exit;
	}
	for( i = 0;i< Len;i ++ )
	{
		if( i == (Len-1) )
		{
			pBuf[i]= IIC_Read_Byte(false);
		}
		else
		{
			pBuf[i] = IIC_Read_Byte(true);
		}
	}
	Exit:
	IIC_Stop();
	return Vale;
}

/************************************************************
*
* Function name	: HAL_IIC_EMU_Write
* Description	: 写数据
* Parameter		: 
*	@Addr		: 地址
*	@pBuff		: 数据指针
*	@Len		: 数据长度
* Return		: 
*	
************************************************************/
bool HAL_IIC_EMU_Write(uint8_t Addr,uint8_t *pBuf,uint32_t Len)
{
	uint8_t Vale = true;
	uint32_t i = 0;

	IIC_Start();
	IIC_Send_Byte(m_DeviceIDs[0]);
	if(IIC_Wait_Ack())
	{
		Vale = false;
		goto Exit;
	}
	IIC_Send_Byte(Addr);
	if(IIC_Wait_Ack())
	{
		Vale = false;
		goto Exit;
	}
	for( i = 0;i < Len;i ++ )
	{
		IIC_Send_Byte(pBuf[i]);
		if(IIC_Wait_Ack())
		{
			Vale = false;
			goto Exit;
		}
	}
	Exit:
	IIC_Stop();
	return Vale;
}
