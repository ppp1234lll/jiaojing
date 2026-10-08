#include "aht20_drv.h"
#include "delay.h"

/*
	9、SHTC3温湿度传感器：(模拟IIC方式)，引脚分配为：
		SCL:	  PE10
		SDA:    PE9

*/

#define AHT20_SCL_GPIO_CLK        RCC_AHB1Periph_GPIOD
#define AHT20_SCL_GPIO            GPIOD
#define AHT20_SCL_PIN             GPIO_Pin_12

#define AHT20_SDA_GPIO_CLK        RCC_AHB1Periph_GPIOD
#define AHT20_SDA_GPIO            GPIOD
#define AHT20_SDA_PIN             GPIO_Pin_11

#define AHT20_SCL_OUT 	PDout(12)
#define AHT20_SDA_IN  	PDin(11)
#define AHT20_SDA_OUT   PDout(11)


#define AHT20_SCL(x) (x ? GPIO_SetBits(AHT20_SCL_GPIO,AHT20_SCL_PIN) : GPIO_ResetBits(AHT20_SCL_GPIO,AHT20_SCL_PIN))

#define AHT20_SDA(x) (x ? GPIO_SetBits(AHT20_SDA_GPIO,AHT20_SDA_PIN) : GPIO_ResetBits(AHT20_SDA_GPIO,AHT20_SDA_PIN))
#define RD_AHT20_SDA  GPIO_ReadInputDataBit(AHT20_SDA_GPIO,AHT20_SDA_PIN)


static void aht20_drive_sda_in(void)
{
	GPIOD->MODER&=~(3<<(11*2));
	GPIOD->MODER|= 0<<11*2;	//PE5输入模式

}

static void aht20_drive_sda_out(void)
{
	GPIOD->MODER&=~(3<<(11*2));
	GPIOD->MODER|= 1<<11*2;  //PE5输出模式

}

#define HTU21D_SDA_OUT() aht20_drive_sda_out()
#define HTU21D_SDA_IN()  aht20_drive_sda_in()

void aht20_i2c_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(AHT20_SCL_GPIO_CLK|AHT20_SDA_GPIO_CLK, ENABLE);

	GPIO_InitStructure.GPIO_Pin   = AHT20_SCL_PIN;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_OUT;			// 输出
	GPIO_InitStructure.GPIO_OType=GPIO_OType_PP;  		// 推挽输出
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_NOPULL;   	// 上拉
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_100MHz; 	// 高速GPIO
	GPIO_Init(AHT20_SCL_GPIO,&GPIO_InitStructure); 

	GPIO_InitStructure.GPIO_Pin   = AHT20_SDA_PIN;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_OUT;			// 输出
	GPIO_InitStructure.GPIO_OType=GPIO_OType_PP;  		// 推挽输出
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_NOPULL;   	// 上拉
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_100MHz; 	// 高速GPIO
	GPIO_Init(AHT20_SDA_GPIO,&GPIO_InitStructure); 
	AHT20_SCL(1);
	AHT20_SDA(1);
}

void aht20_i2c_start(void)
{
	HTU21D_SDA_OUT();
	AHT20_SDA(1);
	AHT20_SCL(1);
	delay_us(4);
 	AHT20_SDA(0);
	delay_us(4);
	AHT20_SCL(0);
}

void aht20_i2c_stop(void)
{
	HTU21D_SDA_OUT();
	AHT20_SCL(0);
	AHT20_SDA(0);
 	delay_us(4);
	AHT20_SCL(1);
	delay_us(4);
	AHT20_SDA(1);
}

uint8_t aht20_i2c_wait_ack(void)
{
	uint8_t ucErrTime=0;
    
	HTU21D_SDA_IN();
	AHT20_SDA(1);
	delay_us(1);	   
	AHT20_SCL(1);
	delay_us(1);
	
	while(RD_AHT20_SDA)
	{
		ucErrTime++;
		if(ucErrTime > 250)
		{
			aht20_i2c_stop();
			return 1;
		}
	}
	AHT20_SCL(0);//时钟输出0
	return 0;
} 

void aht20_i2c_ack(void)
{
	AHT20_SCL(0);
	HTU21D_SDA_OUT();
	AHT20_SDA(0);
	delay_us(2);
	AHT20_SCL(1);
	delay_us(2);
	AHT20_SCL(0);
}

void aht20_i2c_nack(void)
{
	AHT20_SCL(0);
	HTU21D_SDA_OUT();
	AHT20_SDA(1);
	delay_us(2);
	AHT20_SCL(1);
	delay_us(2);
	AHT20_SCL(0);
}					 				     

void aht20_drive_write_byte(uint8_t dat)
{                        
	uint8_t t;   

	HTU21D_SDA_OUT();
	AHT20_SCL(0);//拉低时钟开始数据传输
	
	for(t=0;t<8;t++)
	{              
		if(dat&0x80)
		{
			AHT20_SDA(1);
		}
		else
		{
			AHT20_SDA(0);
		}
		dat<<=1; 	  
		delay_us(2); 
		AHT20_SCL(1);
		delay_us(2);
		AHT20_SCL(0);
		delay_us(2); 
	}
} 	    

uint8_t aht20_drive_read_byte(uint8_t ack)
{
	uint8_t i = 0;
	uint8_t receive = 0;
	
	HTU21D_SDA_IN();

	for(i=0;i<8;i++ )
	{
		AHT20_SCL(0);
		delay_us(2);
		AHT20_SCL(1);
		receive <<= 1;
		if(RD_AHT20_SDA)
		{
			receive++;
		}
		delay_us(1);
  }

	if(ack == 0)
		aht20_i2c_nack();//发送nACK
	else if(ack == 1)
		aht20_i2c_ack(); //发送ACK

	return receive;
}
