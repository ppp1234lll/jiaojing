/********************************************************************************
* @File name  : ADC采集
* @Description: 
* @Author     : ZHLE
*  Version Date        Modification Description

	13、锂电池ADC采集：PC0(ADC10_IN10)
		  电池充电控制： PD3
********************************************************************************/
 
#include "adc.h"
#include "dma.h"
#include "delay.h"
#include "bsp.h"

#define BAT_ADC_CLK				    RCC_APB2Periph_ADC1
#define BAT_ADC_GPIO_CLK			RCC_AHB1Periph_GPIOC
#define BAT_ADC_GPIO_PORT     GPIOC
#define BAT_ADC_PIN           GPIO_Pin_0

#define BAT_ADC   						ADC1
#define BAT_ADC_CH  					ADC_Channel_10

#define BAT_CTRL_READ   	 	GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_3)   // 电池检测
#define BAT_CTRL			   	 	PDout(3)   // 电池检测


#ifdef  ADC_DMA_ENABLE

#define ADC_BUFF_LEN  32

#define ADC1_DMA_CLK  RCC_AHBPeriph_DMA1
#define ADC1_DMA_CH   DMA1_Channel1

uint16_t adc_buff[ADC_BUFF_LEN]  = {0};

#endif

/************************************************************
*
* Function name	: Adc_Init
* Description	: 初始化ADC
* Parameter		: 
* Return		: 
*	
************************************************************/															   
void  bat_gpio_Init(void)
{ 	
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD,ENABLE);
	
	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;  		// 输出
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;  		// 上拉
	GPIO_InitStructure.GPIO_Speed = GPIO_High_Speed;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_Init(GPIOD,&GPIO_InitStructure); 	// 初始化GPIOC	
	
	BAT_CTRL = 0;  // 关闭充电
}
/************************************************************
*
* Function name	: Adc_Init
* Description	: 初始化ADC
* Parameter		: 
* Return		: 
*	
************************************************************/															   
void  Adc_Init(void)
{ 	
	ADC_InitTypeDef ADC_InitStructure; 
	GPIO_InitTypeDef GPIO_InitStructure;
	ADC_CommonInitTypeDef ADC_CommonInitStructure;
	
	bat_gpio_Init();
	
	RCC_AHB1PeriphClockCmd(BAT_ADC_GPIO_CLK	, ENABLE );	 //使能GPIOA时钟
	RCC_APB2PeriphClockCmd(BAT_ADC_CLK	, ENABLE );	  //使能ADC1通道时钟
	
	//PA3 作为模拟通道输入引脚                         
	GPIO_InitStructure.GPIO_Pin = BAT_ADC_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;//模拟输入
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;//不带上下拉
	GPIO_Init(BAT_ADC_GPIO_PORT, &GPIO_InitStructure);	
	
	RCC_APB2PeriphResetCmd(BAT_ADC_CLK,ENABLE);	  //ADC1复位
	RCC_APB2PeriphResetCmd(BAT_ADC_CLK,DISABLE);	//复位结束	 

  ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;//独立模式
  ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;//两个采样阶段之间的延迟5个时钟
  ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled; //DMA失能
  ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div4;//预分频4分频。ADCCLK=PCLK2/4=84/4=21Mhz,ADC时钟最好不要超过36Mhz 
  ADC_CommonInit(&ADC_CommonInitStructure);//初始化
	
  ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;//12位模式
  ADC_InitStructure.ADC_ScanConvMode = DISABLE;//非扫描模式	
  ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;//关闭连续转换
  ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;//禁止触发检测，使用软件触发
  ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;//右对齐	
  ADC_InitStructure.ADC_NbrOfConversion = 1;//1个转换在规则序列中 也就是只转换规则序列1 
  ADC_Init(BAT_ADC, &ADC_InitStructure);//ADC初始化  

	ADC_Cmd(BAT_ADC, ENABLE);	//使能指定的ADC1
	
	//设置指定ADC的规则组通道，一个序列，采样时间
	ADC_RegularChannelConfig(BAT_ADC, BAT_ADC_CH, 1, ADC_SampleTime_480Cycles );	//ADC1,ADC通道,480个周期,提高采样时间可以提高精确度			    
	ADC_SoftwareStartConv(BAT_ADC);		//使能指定的ADC1的软件转换启动功能	

#ifdef  ADC_DMA_ENABLE	
	ADC_DMA_Config();
#endif	

}			

#ifdef  ADC_DMA_ENABLE	
//初始化ADC_DMA	
void ADC_DMA_Config(void)
{ 
	DMA_InitTypeDef  DMA_InitStructure;
		
// 打开DMA时钟
  RCC_AHBPeriphClockCmd(ADC1_DMA_CLK, ENABLE);
	// 复位DMA控制器
	DMA_DeInit(ADC1_DMA_CH);	

	// 配置 DMA 初始化结构体
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)( &(ADC1->DR));	// 外设基址为：ADC 数据寄存器地址
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)adc_buff;	// 存储器地址，实际上就是一个内部SRAM的变量
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;	// 数据源来自外设
	DMA_InitStructure.DMA_BufferSize = ADC_BUFF_LEN;	// 缓冲区大小为1，缓冲区的大小应该等于存储器的大小
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;	// 外设寄存器只有一个，地址不用递增
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;  		// 内存地址寄存器递增
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;	// 外设数据大小为半字，即两个字节
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;	// 存储器数据大小也为半字，跟外设数据大小相同
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;	// 循环传输模式
	DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;	// DMA 传输通道优先级为高，当使用一个DMA通道时，优先级设置不影响
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;	// 禁止存储器到存储器模式，因为是从外设到存储器
	DMA_Init(ADC1_DMA_CH, &DMA_InitStructure);	// 初始化DMA

	// 使能 DMA 通道
	DMA_Cmd(ADC1_DMA_CH , ENABLE);
	ADC_DMACmd(ADC1, ENABLE);
} 
#endif	

/************************************************************
*
* Function name	: Get_Key_Adc
* Description	: 获得ADC值
* Parameter		: 
* Return		: 
*	
************************************************************/				
uint16_t Get_Key_Adc(void)   
{
//	//设置指定ADC的规则组通道，一个序列，采样时间
//	ADC_RegularChannelConfig(KEY_ADC, KEY_ADC_CH, 1, ADC_SampleTime_28Cycles5 );	//ADC1,ADC通道,采样时间为239.5周期	  			    
//	ADC_SoftwareStartConvCmd(KEY_ADC, ENABLE);		//使能指定的ADC1的软件转换启动功能	
//	while(!ADC_GetFlagStatus(KEY_ADC, ADC_FLAG_EOC ));//等待转换结束

	return ADC_GetConversionValue(BAT_ADC);	//返回最近一次ADC1规则组的转换结果
}

/************************************************************
*
* Function name	: Get_Adc_Average
* Description	: 平均值
* Parameter		: 
* Return		: 
*	
************************************************************/
u16 Get_Adc_Average(u8 times)
{
	u32 temp_val=0;
	u8 t;
	for(t=0;t<times;t++)
	{
		temp_val+=Get_Key_Adc();
		delay_ms(5);
	}
	return temp_val/times;
} 	 
/************************************************************
*
* Function name	: Get_Adc_Values
* Description	: 获得电压值
* Parameter		: 
* Return		: 
*	
************************************************************/
uint16_t Get_Adc_Values(void)
{
	return Get_Key_Adc()*3300/4096;
} 	

/************************************************************
*
* Function name	: bat_control_status_detection
* Description	: 电池充电控制
* Parameter		:
* Return		:  
*	   20230808
************************************************************/
void bat_set_control_status_detection(uint8_t status)
{
#ifdef BAT_ENABLE
	BAT_CTRL = status;
#endif
}

/************************************************************
*
* Function name	: bat_control_status_detection
* Description	: 电池充电控制
* Parameter		:
* Return		:  
*	   20230808
************************************************************/
uint8_t bat_get_control_status_detection(void)
{
#ifdef BAT_ENABLE
	return BAT_CTRL_READ;
#endif
	return 0;
}


/************************************************************
*
* Function name	: Adc_Test
* Description	: Adc测试
* Parameter		: 
* Return		: 
*	
************************************************************/
void Adc_Test(void)
{
//	uint16_t adcx;
	
//	adcx=Get_Adc_Average(10)*3300/4096;
//	printf("adcx:%d V\n",adcx);
	
	while(1)
	{
		printf("adcx:%d V\n",Get_Adc_Values());
		delay_ms(1000);	
	}
} 	


