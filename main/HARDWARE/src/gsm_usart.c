/********************************************************************************
* @File name  : 4G模块
* @Description: 串口1-对应4G
* @Author     : ZHLE
*  Version Date        Modification Description
	12、ML307（4G模块）：串口4，波特率115200，引脚分配为	
			USART4_TX： PC10
			USART4_RX： PC11
			4G_PWRK: PD0
			4G_NRST: PC12
			SIM_DET: PD1
********************************************************************************/

#include "gsm_usart.h"
#include "dma.h"
#include "GPRS.h"

#define GSM_USART_TX_GPIO_CLK               RCC_AHB1Periph_GPIOD
#define GSM_USART_TX_GPIO_PORT              GPIOD
#define GSM_USART_TX_PIN                    GPIO_Pin_5

#define GSM_USART_RX_GPIO_CLK               RCC_AHB1Periph_GPIOD
#define GSM_USART_RX_GPIO_PORT              GPIOD
#define GSM_USART_RX_PIN                    GPIO_Pin_6

#define GSM_USART_CLK                       RCC_APB1Periph_USART2
#define GSM_USART                           USART2
#define GSM_USART_IRQn                      USART2_IRQn
#define GSM_USART_IRQHandler                USART2_IRQHandler

#define GSM_USART_RX_DMA 

#define GSM_USART_TX_DMA_STREAM             DMA1_Stream6
#define GSM_USART_RX_DMA_STREAM             DMA1_Stream5

#define GSM_USART_TX_DMA_CH               	DMA_Channel_4
#define GSM_USART_RX_DMA_CH                 DMA_Channel_4

uint8_t gsm_usart_rx_buff[GSM_USART_RX_MAX] = {0};

/************************************************************
*
* Function name	: gsm_usart_init
* Description	: 串口1初始化函数
* Parameter		: 
*	@bound		: 波特率
* Return		: 
*	
************************************************************/
void gsm_usart_init(uint32_t bound)
{
	/* GPIO端口设置 */
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	/* 时钟初始化 */
	RCC_AHB1PeriphClockCmd(GSM_USART_TX_GPIO_CLK|GSM_USART_RX_GPIO_CLK, ENABLE); 
	RCC_APB1PeriphClockCmd(GSM_USART_CLK , ENABLE);

	/* 串口对应引脚复用映射 */
	GPIO_PinAFConfig(GSM_USART_TX_GPIO_PORT,GPIO_PinSource5, GPIO_AF_USART2); 
	GPIO_PinAFConfig(GSM_USART_RX_GPIO_PORT,GPIO_PinSource6,GPIO_AF_USART2);
	
	/* USART端口配置 */
	GPIO_InitStructure.GPIO_Pin = GSM_USART_TX_PIN; 
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AF;			// 输出
	GPIO_InitStructure.GPIO_OType=GPIO_OType_PP;  		// 推挽输出
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_UP;   	// 上拉
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_100MHz; 	// 高速GPIO
	GPIO_Init(GSM_USART_TX_GPIO_PORT, &GPIO_InitStructure);	

	GPIO_InitStructure.GPIO_Pin = GSM_USART_RX_PIN;		 
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_AF;			// 输出
	GPIO_InitStructure.GPIO_OType=GPIO_OType_PP;  		// 推挽输出
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_UP;   	// 上拉
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_100MHz; 	// 高速GPIO
	GPIO_Init(GSM_USART_RX_GPIO_PORT, &GPIO_InitStructure);			

	NVIC_InitStructure.NVIC_IRQChannel = GSM_USART_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2; 
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;		  
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			  
	NVIC_Init(&NVIC_InitStructure);			

	/* USART 初始化设置 */
	USART_DeInit(GSM_USART);
	USART_InitStructure.USART_BaudRate = bound;									// 串口波特率
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;						// 字长为8位数据格式
	USART_InitStructure.USART_StopBits = USART_StopBits_1;							// 一个停止位
	USART_InitStructure.USART_Parity = USART_Parity_No;								// 无奇偶校验位
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 无硬件数据流控制
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;					// 收发模式
	USART_Init(GSM_USART, &USART_InitStructure);	 

	#ifdef GSM_USART_RX_DMA
	DMA_Config( GSM_USART_RX_DMA_STREAM,GSM_USART_RX_DMA_CH,(uint32_t)&(GSM_USART->DR),(uint32_t)gsm_usart_rx_buff,
							DMA_DIR_PeripheralToMemory,GSM_USART_RX_MAX);
	DMA_Enable(GSM_USART_RX_DMA_STREAM,GSM_USART_RX_MAX);
	USART_ITConfig(GSM_USART, USART_IT_IDLE, ENABLE);
	USART_DMACmd(GSM_USART,USART_DMAReq_Rx,ENABLE);
	#else
	USART_ITConfig(GSM_USART, USART_IT_RXNE, ENABLE); 									// 开启串口接受中断
	#endif
	
	USART_ClearITPendingBit(GSM_USART, USART_IT_TC);
	USART_Cmd(GSM_USART, ENABLE);					   									// 使能串口3
}

/************************************************************
*
* Function name	: gsm_usart_send_char
* Description	: 发送一个字节
* Parameter		: 
*	@ch			: 待发送的字节数据
* Return		: 
*	
************************************************************/
void gsm_usart_send_char(uint8_t ch)
{
	GSM_USART->DR = (uint8_t)ch;
	while ((GSM_USART->SR & 0X40) == 0);
}

/************************************************************
*
* Function name	: gsm_usart_send_str
* Description	: 发送字符串
* Parameter		: 
*	@buff		: 字符串指针
*	@len		: 发送数据长度
* Return		: 
*	
************************************************************/
void gsm_usart_send_str(uint8_t *buff, uint16_t len)
{
	while(len--) {
		gsm_usart_send_char(buff[0]);
		buff++;
	}
}
/************************************************************
*
* Function name	: USART2_IRQHandler
* Description	: 串口2中断函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void GSM_USART_IRQHandler(void)
{
#ifdef GSM_USART_RX_DMA
	uint16_t size = 0;
	if (USART_GetITStatus(GSM_USART, USART_IT_IDLE) != RESET) 
	{
		USART_ClearITPendingBit(GSM_USART, USART_IT_IDLE);
		USART_ReceiveData(GSM_USART);
		
		DMA_Cmd(GSM_USART_RX_DMA_STREAM, DISABLE);/* 停止DMA */
		size = GSM_USART_RX_MAX - DMA_GetCurrDataCounter(GSM_USART_RX_DMA_STREAM);
		gprs_get_receive_data_function(gsm_usart_rx_buff,size);

		DMA_Enable(GSM_USART_RX_DMA_STREAM,GSM_USART_RX_MAX);/* 设置传输模式 */
	}
#else
	static uint8_t test = 0;
	uint8_t res = 0;
	
	if (USART_GetITStatus(GSM_USART, USART_IT_RXNE) != RESET) {
		USART_ClearITPendingBit(GSM_USART, USART_IT_RXNE);
		res = USART_ReceiveData(GSM_USART);
		gsm_usart_rx_buff[test++] = res;
	}
	#endif
}



