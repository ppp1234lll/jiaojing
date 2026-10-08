#include "led.h"
#include "delay.h"
#include "includes.h"
#include "iwdg.h"
/*
		系统状态指示灯     STATE      : PE0
		网口指示灯         LAN        : PB9
		4G指示灯          GPRS        : PB8
		输出 1            Camera1     : PB7
		输出 2            Camera2     : PB6
		输出 3            Camera3     : PE1
		输出 4            Camera4     : PE2
		输出 5            Camera5     : PE3
		输出 6            Camera6     : PE4
		输出 7            Camera7     : PE5
		输出 8            Camera8     : PE6
*/

/*
实际指示：
		系统状态指示灯     STATE      : PA12
		网口指示灯         LAN        : PA11
		4G指示灯           GPRS       : PC8
		摄像头1            Camera1    : PC9
		摄像头2            Camera2    : PC7
*/


#define LED_STATE 		PBout(8)
#define LED_LAN   		PBout(9)
#define LED_GPRS	  	PEout(0)
//#define LED_RELAY_1 	PBout(7)
//#define LED_RELAY_2   PBout(6)
//#define LED_RELAY_3 	PEout(1)
//#define LED_RELAY_4   PEout(2)
//#define LED_RELAY_5 	PEout(3)
//#define LED_RELAY_6   PEout(4)
//#define LED_RELAY_7 	PEout(5)
//#define LED_RELAY_8   PEout(6)

//#define LED_OUT_LAN     PCout(8)
//#define LED_OUT_POWER   PCout(9)
//#define LED_OUT_LAN_TOG GPIO_ReadOutputDataBit(GPIOC,GPIO_Pin_8) 

#define LED_STATE_TOG   GPIO_ReadOutputDataBit(GPIOB,GPIO_Pin_8) 
#define LED_LAN_STA     GPIO_ReadOutputDataBit(GPIOB,GPIO_Pin_9) 
#define LED_GPRS_TOG    GPIO_ReadOutputDataBit(GPIOE,GPIO_Pin_0) 

led_flicker_t sg_ledflicker_t = {0};

/************************************************************
*
* Function name	: led_gpio_init_function
* Description	: 初始化指示灯控制io:默认不开启
* Parameter		: 
* Return		: 
*	
************************************************************/
void led_gpio_init_function(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB|RCC_AHB1Periph_GPIOE,ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,ENABLE);

	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8|GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;			// 输出
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;  		// 推挽输出
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_DOWN;   	// 上拉
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz; 	// 高速GPIO
	GPIO_Init(GPIOB,&GPIO_InitStructure);
	GPIO_SetBits(GPIOB,GPIO_Pin_8|GPIO_Pin_9);

	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;			// 输出
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;  		// 推挽输出
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_DOWN;   	// 上拉
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz; 	// 高速GPIO
	GPIO_Init(GPIOE,&GPIO_InitStructure);
	GPIO_SetBits(GPIOE,GPIO_Pin_0);

}

/************************************************************
*
* Function name	: led_control_function
* Description	: led灯输出控制函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void led_control_function(LED_DEV dev, LED_STATUS state)
{
	switch(dev)
	{
		case LD_STATE:  // 系统运行指示灯
			sg_ledflicker_t.state = state;
			switch(state) 
			{
        case LD_ON:	LED_STATE = 0;break;
        case LD_OFF:LED_STATE = 1;break;
        default:		break;
      }
      break;
		case LD_GPRS:  // 4G指示灯
			sg_ledflicker_t.gprs = state;
			switch(state) 
			{
        case LD_ON:			LED_GPRS = 0;break;
        case LD_OFF:		LED_GPRS = 1;break;
        default:break;
			}
			break;
		case LD_LAN:   // 有线指示灯
			sg_ledflicker_t.lan = state;
			switch(state) 
			{
        case LD_ON:			LED_LAN = 0;break;
        case LD_OFF:		LED_LAN = 1;break;
        default:break;
			}
			break;

		default:		break;
	}
}

/************************************************************
*
* Function name	: led_outside_control_function
* Description	: 外部led控制
* Parameter		: 
* Return		: 
*	
************************************************************/
void led_out_control_function(LED_DEV dev, LED_STATUS state)
{
//	switch(dev)
//	{
//		case LD_POWER:	
//			LED_OUT_POWER = !state;
//		break;
//		case LD_LAN_LED:
//			sg_ledflicker_t.lan_out = state;
//			switch(state) 
//			{
//				case LD_ON:		LED_OUT_LAN = 1;break;
//				case LD_OFF:	LED_OUT_LAN = 0;break;
//				default:break;
//			}
//			break;
//		default:			break;
//	}
}

//#define FLICKER_TIME 		(500)
//#define FLICKER_TIME_1S (1000)
//#define FLICKER_TIME_2S (2000)

#define FLICKER_TIME_Q	(250)
#define FLICKER_TIME 		(500)
#define FLICKER_TIME_1S (1000)

/************************************************************
*
* Function name	: led_flicker_control_timer_function
* Description	: led闪动
* Parameter		: 
* Return		: 
*	
************************************************************/
void led_flicker_control_timer_function(void)
{
	static uint16_t count   = 0;
//	static uint16_t count2	= 0;
//	static uint8_t  flag[4] = {0};
	static uint16_t count3	= 0;
	
	count++;
	if(count > FLICKER_TIME)
	{
		count = 0;
		if(sg_ledflicker_t.gprs == LD_FLICKER)		/* 显示无线网络状态 */
		{
			LED_GPRS = !LED_GPRS_TOG;
		}

		if(sg_ledflicker_t.lan == LD_FLICKER)		/* 显示有线网络状态 */
		{
			LED_LAN = !LED_LAN_STA;
		} 
	
		if(sg_ledflicker_t.state == LD_FLICKER) 	/* 系统状态灯 */
		{
			LED_STATE = !LED_STATE_TOG;
		}
	}
	
	//	count2++;
	//	if(count2>FLICKER_TIME_1S) 
	//	{
	//		count2 = 0;
	//		if(sg_ledflicker_t.lan_out == LD_FLICKER) 	/* 现在主网络状态 */
	//		{
	//			LED_OUT_LAN = !LED_OUT_LAN_TOG;
	//		}
	//	}

	count3++;
	if(count3 > FLICKER_TIME_Q)
	{
		count3 = 0;
		if(sg_ledflicker_t.gprs == LD_FLIC_Q)		/* 显示无线网络状态 */
			LED_GPRS = !LED_GPRS_TOG;

		if(sg_ledflicker_t.lan == LD_FLIC_Q)		/* 显示有线网络状态 */
			LED_LAN = !LED_LAN_STA;
	}
}


/************************************************************
*
* Function name	: led_get_status_function
* Description	: 获取LED状态
* Parameter		: 
* Return		: 
*	
************************************************************/
void *led_get_status_function(void)
{
	return (&sg_ledflicker_t);
}

/************************************************************
*
* Function name	: led_light_all
* Description	: led灯全亮
* Parameter		:
* Return		:
*
************************************************************/
void led_light_all(void)
{
	GPIO_ResetBits(GPIOB,GPIO_Pin_8|GPIO_Pin_9);
	GPIO_ResetBits(GPIOE,GPIO_Pin_0);

}

/************************************************************
*
* Function name	: led_light_off
* Description	: led灯全灭
* Parameter		:
* Return		:
*
************************************************************/
void led_light_off(void)
{
	GPIO_SetBits(GPIOB,GPIO_Pin_8|GPIO_Pin_9);
	GPIO_SetBits(GPIOE,GPIO_Pin_0);
}


/************************************************************
*
* Function name	: led_relay_flicker_all
* Description	: 继电器闪烁
* Parameter		:
* Return		:
*
************************************************************/
void led_flicker_all(uint8_t num)
{
	while(num--)
	{
		led_light_all();
		IWDG_Feed();					// 喂狗
		OSTimeDlyHMSM(0,0,1,0); 
		led_light_off();
		IWDG_Feed();					// 喂狗
		OSTimeDlyHMSM(0,0,1,0); 
	}
}

/************************************************************
*
* Function name	: led_test
* Description	: led测试
* Parameter		:
* Return		:
*
************************************************************/
void led_test(void)
{
	while(1)
	{
		led_light_all();
//		LED_OUT_LAN = 1;
//		LED_OUT_POWER = 1;
		delay_ms(1000);
		led_light_off();
//		LED_OUT_LAN = 0;
//		LED_OUT_POWER = 0;
		delay_ms(1000);	
	}
}


