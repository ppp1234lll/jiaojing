#include "fan.h"
#include "led.h"
#include "delay.h"

/*
	14、风扇控制
	    风扇1 ：PC6
			风扇2 ：PC7
*/

#define FAN1_CTRL   PCout(9)
#define FAN2_CTRL   PCout(8)


typedef struct
{
	uint8_t fan_1; 
	uint8_t fan_2;  
} fan_t;

fan_t sg_fan_states_t;
/************************************************************
*
* Function name	: fan_gpio_init_function
* Description	: 风扇初始化
* Parameter		: 
* Return		: 
*	
************************************************************/
void fan_gpio_init_function(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,ENABLE);

	GPIO_InitStructure.GPIO_Pin    = GPIO_Pin_8|GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode   = GPIO_Mode_OUT;			// 输出
	GPIO_InitStructure.GPIO_OType  = GPIO_OType_PP;  		// 推挽输出
	GPIO_InitStructure.GPIO_PuPd   = GPIO_PuPd_NOPULL;   	// 上拉
	GPIO_InitStructure.GPIO_Speed  = GPIO_Speed_100MHz; 	// 高速GPIO
	GPIO_Init(GPIOC,&GPIO_InitStructure);
	
	fan_control(FAN_1,FAN_OFF);
	fan_control(FAN_2,FAN_OFF);


}


/************************************************************
*
* Function name	: fan_control
* Description	: 继电器控制
* Parameter		: 
* Return		: 
*	
************************************************************/
void fan_control(FAN_DEV dev, FAN_STATUS state)
{
	switch(dev)
	{
		case FAN_1:
			sg_fan_states_t.fan_1 = state;
			FAN1_CTRL = (state?0:1);
			break;
		case FAN_2:
			sg_fan_states_t.fan_2 = state;
			FAN2_CTRL = (state?0:1);
			break;
		default:
			break;
	}
}

/************************************************************
*
* Function name	: relay_get_status_function
* Description	:  获取继电器状态
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t fan_get_status_function(FAN_DEV dev)
{
	switch(dev)
	{
		case FAN_1:
			return  (sg_fan_states_t.fan_1?1:0);
		case FAN_2:
			return  (sg_fan_states_t.fan_2?1:0);	
		
		default:
			break;
	}
	return 0;
}

/************************************************************
*
* Function name	: fan_test
* Description	: 继电器测试
* Parameter		:
* Return		:
*
************************************************************/
void fan_test(void)
{
	while(1)
	{
		fan_control(FAN_1,FAN_ON); // 开继电器1
		fan_control(FAN_2,FAN_ON); // 开继电器2
		delay_ms(5000);
		fan_control(FAN_1,FAN_OFF); // 关继电器1
		fan_control(FAN_2,FAN_OFF); // 关继电器2
		delay_ms(5000);	
	}
}


