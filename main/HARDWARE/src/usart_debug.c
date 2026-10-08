#include "usart_debug.h"
#include "stdio.h"
#include "delay.h"

#define USART_DEBUG_TX_GPIO_CLK				       	RCC_AHB1Periph_GPIOA
#define USART_DEBUG_TX_GPIO_PORT              GPIOA
#define USART_DEBUG_TX_PIN                    GPIO_Pin_9

#define USART_DEBUG_RX_GPIO_CLK				       	RCC_AHB1Periph_GPIOA
#define USART_DEBUG_RX_GPIO_PORT              GPIOA
#define USART_DEBUG_RX_PIN                    GPIO_Pin_10

#define USART_DEBUG_CLK                 			RCC_APB2Periph_USART1
#define USART_DEBUG                    				USART1
#define USART_DEBUG_IRQn               				USART1_IRQn
#define USART_DEBUG_IRQHandler				 				USART1_IRQHandler

uint8_t usart_debug_rx_buff[USART_DEBUG_RX_MAX] = {0};

////////////////////////////////////////////////////////////////
//加入以下代码,支持printf函数,而不需要选择use MicroLIB	  
#if 1
#pragma import(__use_no_semihosting)             
//标准库需要的支持函数                 
struct __FILE 
{ 
	int handle; 

}; 

FILE __stdout;   
//__use_no_semihosting was requested, but _ttywrch was
void _ttywrch(int ch)
{
ch = ch;
}
//定义_sys_exit()以避免使用半主机模式    
void _sys_exit(int x) 
{ 
	x = x; 
} 
//重定义fputc函数 
int fputc(int ch, FILE *f)
{  
	USART_DEBUG->DR = (int8_t) ch;
	while((USART_DEBUG->SR&0X40)==0);
	return ch;
}
#endif 


/************************************************************
*
* Function name	: usart2_init_function
* Description	: 串口2初始化函数
* Parameter		: 
*	@bau
* Return		: 
*	
************************************************************/
void usart_debug_init_function(uint32_t baudrate)
{	
	GPIO_InitTypeDef 	GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef 	NVIC_InitStructure;
	
	/* 时钟初始化 */
	RCC_AHB1PeriphClockCmd(USART_DEBUG_TX_GPIO_CLK|USART_DEBUG_RX_GPIO_CLK, ENABLE); 
	RCC_APB2PeriphClockCmd(USART_DEBUG_CLK , ENABLE);

	//串口1对应引脚复用映射
	GPIO_PinAFConfig(USART_DEBUG_TX_GPIO_PORT,GPIO_PinSource9,GPIO_AF_USART1); //GPIOA9复用为USART1
	GPIO_PinAFConfig(USART_DEBUG_RX_GPIO_PORT,GPIO_PinSource10,GPIO_AF_USART1); //GPIOA10复用为USART1
	
	GPIO_InitStructure.GPIO_Pin 	= USART_DEBUG_TX_PIN; 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用功能
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	//速度50MHz
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; //推挽复用输出
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; //上拉
	GPIO_Init(USART_DEBUG_TX_GPIO_PORT, &GPIO_InitStructure);	

	GPIO_InitStructure.GPIO_Pin 	= USART_DEBUG_RX_PIN;		 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用功能
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	//速度50MHz
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; //推挽复用输出
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; //上拉
	GPIO_Init(USART_DEBUG_RX_GPIO_PORT, &GPIO_InitStructure);				 
	
	NVIC_InitStructure.NVIC_IRQChannel = USART_DEBUG_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3; 
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;		  
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			  
	NVIC_Init(&NVIC_InitStructure);						
	
	USART_DeInit(USART_DEBUG);
	
	USART_InitStructure.USART_BaudRate = baudrate; // 波特率设置
	USART_InitStructure.USART_WordLength = USART_WordLength_8b; // 字长为8位数据格式
	USART_InitStructure.USART_StopBits = USART_StopBits_1; // 一个停止位
	USART_InitStructure.USART_Parity = USART_Parity_No; // 无奇偶校验位
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 无硬件数据流控制
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	// 收发模式
	USART_Init(USART_DEBUG, &USART_InitStructure);	   									
	
	USART_ITConfig(USART_DEBUG, USART_IT_RXNE, ENABLE); 		// 开启串口接受中断

	USART_ClearITPendingBit(USART_DEBUG, USART_IT_TC);
	USART_Cmd(USART_DEBUG, ENABLE);					   							// 使能串口1
	
}

/************************************************************
*
* Function name	: usart2_send_char
* Description	: 发送一个字节
* Parameter		: 
*	@ch			: 待发送的字节数据
* Return		: 
*	
************************************************************/
void usart_debug_send_char(uint8_t ch)
{
	USART_DEBUG->DR = (uint8_t)ch;
	while ((USART_DEBUG->SR & 0X40) == 0);
}

/************************************************************
*
* Function name	: usart2_send_str
* Description	: 发送字符串
* Parameter		: 
*	@buff		: 字符串指针
*	@len		: 发送数据长度
* Return		: 
*	
************************************************************/
void usart_debug_send_str(uint8_t *buff, uint16_t len)
{
	while(len--) {
		usart_debug_send_char(buff[0]);
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
void USART_DEBUG_IRQHandler(void)
{
	static uint8_t test = 0;
	uint8_t res = 0;
	
	if (USART_GetITStatus(USART_DEBUG, USART_IT_RXNE) != RESET) {
		USART_ClearITPendingBit(USART_DEBUG, USART_IT_RXNE);
		res = USART_ReceiveData(USART_DEBUG);
		usart_debug_rx_buff[test++] = res;
	}
}



/************************************************************
*
* Function name	: usart_debug_test
* Description	: 串口测试
* Parameter		:
* Return		:
*
************************************************************/
void usart_debug_test(void)
{
	while(1)
	{
		printf("串口测试\n");
		delay_ms(1000);		
	}
}





