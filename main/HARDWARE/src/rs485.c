/********************************************************************************
* @File name  : 
* @Description: 
* @Author     : ZHLE
*  Version Date        Modification Description
	6、485通信：串口3 ,波特率 115200
			485_TX : PD8
			485_RX : PD9
			485_RE : PD10 
********************************************************************************/
#include "rs485.h"
#include "dma.h"

#define RS485_TX_GPIO_CLK				       	RCC_AHB1Periph_GPIOD
#define RS485_TX_GPIO_PORT              GPIOD
#define RS485_TX_PIN                    GPIO_Pin_8

#define RS485_RX_GPIO_CLK				       	RCC_AHB1Periph_GPIOD
#define RS485_RX_GPIO_PORT              GPIOD
#define RS485_RX_PIN                    GPIO_Pin_9

#define RS485_RE_GPIO_CLK				       	RCC_AHB1Periph_GPIOD
#define RS485_RE_GPIO_PORT              GPIOD
#define RS485_RE_PIN                    GPIO_Pin_10

#define RS485_USART_CLK                 RCC_APB1Periph_USART3
#define RS485_USART                    	USART3
#define RS485_IRQn               				USART3_IRQn
#define RS485_IRQHandler				 				USART3_IRQHandler

//#define RS485_RX_DMA  
#define RS485_TX_DMA_CH               	DMA1_Channel2
#define RS485_RX_DMA_CH                 DMA1_Channel3

#define RS485_RX_MAX  20
uint8_t rs485_recv_buff[RS485_RX_MAX] = {0};
uint8_t rs485_recv_len = 0;
#define RS485_RE    PDout(10)

/************************************************************
*
* Function name	: rs485_init
* Description	: 485初始化函数
* Parameter		:
* Return		:
*
************************************************************/
void rs485_init(uint32_t bound)
{
	//GPIO端口设置
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_AHB1PeriphClockCmd(RS485_TX_GPIO_CLK|RS485_RX_GPIO_CLK|RS485_RE_GPIO_CLK,ENABLE);  // 使能GPIOB/USART1时钟
	RCC_APB1PeriphClockCmd(RS485_USART_CLK , ENABLE);

	/* 串口对应引脚复用映射 */
	GPIO_PinAFConfig(RS485_TX_GPIO_PORT,GPIO_PinSource8,GPIO_AF_USART3); 						// GPIOC10复用为UART4
	GPIO_PinAFConfig(RS485_RX_GPIO_PORT,GPIO_PinSource9,GPIO_AF_USART3); 						// GPIOC11复用为UART4
	
	//USART1端口配置
	GPIO_InitStructure.GPIO_Pin = RS485_TX_PIN ;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;		    						// 复用功能
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	    						// 速度50MHz
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; 		    						// 推挽复用输出
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; 		    						// 上拉
	GPIO_Init(RS485_TX_GPIO_PORT,&GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = RS485_RX_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;		    						// 复用功能
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	    						// 速度50MHz
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; 		    						// 推挽复用输出
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; 		    						// 上拉
	GPIO_Init(RS485_RX_GPIO_PORT,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = RS485_RE_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;		    						// 复用功能
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	    						// 速度50MHz
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; 		    						// 推挽复用输出
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; 		    						// 上拉
	GPIO_Init(RS485_RE_GPIO_PORT,&GPIO_InitStructure);
	RS485_RE = 0;
	
	//USART1 初始化设置
	USART_InitStructure.USART_BaudRate = bound; // 波特率设置
	USART_InitStructure.USART_WordLength = USART_WordLength_8b; // 字长为8位数据格式
	USART_InitStructure.USART_StopBits = USART_StopBits_1; // 一个停止位
	USART_InitStructure.USART_Parity = USART_Parity_No; // 无奇偶校验位
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 无硬件数据流控制
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	// 收发模式
	USART_Init(RS485_USART, &USART_InitStructure); // 初始化串口3

	//Usart1 NVIC 配置
	NVIC_InitStructure.NVIC_IRQChannel = RS485_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=3; // 抢占优先级3
	NVIC_InitStructure.NVIC_IRQChannelSubPriority =1;		// 子优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			// IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);	                        // 根据指定的参数初始化VIC寄存器、

#ifdef RS485_RX_DMA
	DMA_Config( RS485_RX_DMA_CH,(uint32_t)&(RS485_USART->DR),(uint32_t)rs485_recv_buff, // 自定义的接收数据buf
						  DMA_DIR_PeripheralSRC,RS485_RX_MAX);	
	USART_ITConfig(RS485_USART, USART_IT_IDLE, ENABLE); //开启相关中断
	USART_DMACmd(RS485_USART, USART_DMAReq_Rx, ENABLE);
#else 
	USART_ITConfig(RS485_USART, USART_IT_RXNE, ENABLE);//开启相关中断
#endif
	
	USART_ClearFlag(RS485_USART, USART_FLAG_TC);
	USART_Cmd(RS485_USART, ENABLE);  // 使能串口1
}

/************************************************************
*
* Function name	: rs485_send_char
* Description	: 串口1-字节发送函数
* Parameter		: 
*   @ch         : 需要发送的字节
* Return		: 
*	
************************************************************/
void rs485_send_char(uint8_t ch)
{
	RS485_USART->DR = (int8_t) ch;
	while((RS485_USART->SR&0X40)==0);
}

/************************************************************
*
* Function name	: usart1_send_str_function
* Description	: 串口1-字符串发送函数
* Parameter		: 
*   @data       : 数据
*   @len        : 长度
* Return		: 
*	
************************************************************/
void rs485_send_str(uint8_t *data, uint16_t len)
{
	RS485_RE = 1;
    
	while(len--) {
		rs485_send_char(data[0]);
		data++;
	}
	while ((RS485_USART->SR & 0X40) == 0);
	RS485_RE = 0;
}

/************************************************************
*
* Function name	: USART1_IRQHandler
* Description	: 串口1中断函数
* Parameter		:
* Return		:
*
************************************************************/
void RS485_IRQHandler(void)
{
	#ifdef RS485_RX_DMA
	uint16_t size = 0;

	if( USART_GetITStatus(RS485_USART,USART_IT_IDLE) != RESET ) 
	{
		USART_ClearITPendingBit(RS485_USART,USART_IT_IDLE);
		USART_ReceiveData(RS485_USART);
		
		DMA_Cmd(RS485_RX_DMA_CH, DISABLE);                              // 关闭DMA，准备重新配置

		size = RS485_RX_MAX - DMA_GetCurrDataCounter(RS485_RX_DMA_CH); // 计算接收数据长度					
		rs485_send_str(rs485_recv_buff,size);
//		sensor_recv_data_function(rs485_recv_buff,size);
		DMA_Enable(RS485_RX_DMA_CH,RS485_RX_MAX);
	}
	#else
	static uint8_t rev_count = 0;
	uint8_t res = 0;
	
	if(USART_GetITStatus(RS485_USART, USART_IT_RXNE) != RESET)
	{
		USART_ClearITPendingBit(RS485_USART,USART_IT_RXNE);
		res = USART_ReceiveData(RS485_USART);
		rs485_recv_buff[rev_count++] = res;
		
		if(rev_count == 1)
		{
			if((rs485_recv_buff[rev_count-1] == 0x01) || (rs485_recv_buff[rev_count-1] == 0x02))
			{
				rs485_recv_len = 9;
			}
			else if(rs485_recv_buff[rev_count-1] == 0x03)
			{
				rs485_recv_len = 11;
			}
			else
			{
				rev_count =0;
				memset(rs485_recv_buff,0,RS485_RX_MAX);
			}
		}
				
		if(rev_count >= rs485_recv_len)
		{
			rev_count =0;
//			sensor_recv_data_function(rs485_recv_buff,rs485_recv_len);
		}
	}
	#endif
}

