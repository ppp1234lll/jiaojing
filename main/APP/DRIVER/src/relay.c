#include "relay.h"
#include "led.h"
#include "delay.h"

/*
	3、继电器

		继电器 1           RELAY 1     : PB14
		继电器 2           RELAY 2     : PB15
		继电器 3           RELAY 3     : PD11
		继电器 4           RELAY 4     : PD12
		继电器 5           RELAY 5     : PD13
		继电器 6           RELAY 6     : PD14
		继电器 7           RELAY 7     : PA11
		继电器 8           RELAY 8     : PA12
*/

//#define RELAY1_CTRL   PBout(14)
//#define RELAY2_CTRL   PBout(15)
//#define RELAY3_CTRL   PDout(11)
//#define RELAY4_CTRL   PDout(12)
//#define RELAY5_CTRL   PDout(13)
//#define RELAY6_CTRL   PDout(14)
//#define RELAY7_CTRL   PAout(11)
//#define RELAY8_CTRL   PAout(12)
//#define RELAY8_CTRL   PBout(14)
//#define RELAY7_CTRL   PBout(15)
//#define RELAY6_CTRL   PDout(11)
//#define RELAY5_CTRL   PDout(12)
//#define RELAY4_CTRL   PDout(13)
//#define RELAY3_CTRL   PDout(14)
//#define RELAY2_CTRL   PAout(11)
//#define RELAY1_CTRL   PAout(12)

#define RELAY8_CTRL   PEout(15)
#define RELAY7_CTRL   PEout(14)
#define RELAY6_CTRL   PEout(13)
#define RELAY5_CTRL   PEout(12)
#define RELAY4_CTRL   PEout(11)
#define RELAY3_CTRL   PEout(10)
#define RELAY2_CTRL   PEout(9)
#define RELAY1_CTRL   PEout(8)


typedef struct
{
	uint8_t relay_1; 
	uint8_t relay_2; 
	uint8_t relay_3; 
	uint8_t relay_4; 
	uint8_t relay_5; 
	uint8_t relay_6; 
	uint8_t relay_7; 
	uint8_t relay_8; 
} relay_t;

relay_t sg_relay_states_t;
/************************************************************
*
* Function name	: relay_gpio_init_function
* Description	: 继电器初始化
* Parameter		: 
* Return		: 
*	
************************************************************/
void relay_gpio_init_function(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE,ENABLE);

	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8|GPIO_Pin_9|GPIO_Pin_10|GPIO_Pin_11|GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;  		// 输出
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;  		// 上拉
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_High_Speed;
	GPIO_Init(GPIOE,&GPIO_InitStructure);

	
	relay_control(RELAY_1,RELAY_ON);
	relay_control(RELAY_2,RELAY_ON);
	relay_control(RELAY_3,RELAY_ON);
	relay_control(RELAY_4,RELAY_ON);
	relay_control(RELAY_5,RELAY_ON);
	relay_control(RELAY_6,RELAY_ON);
	relay_control(RELAY_7,RELAY_ON);
	relay_control(RELAY_8,RELAY_ON); 
	

}


/************************************************************
*
* Function name	: relay_control
* Description	: 继电器控制
* Parameter		: 
* Return		: 
*	
************************************************************/
void relay_control(RELAY_DEV dev, RELAY_STATUS state)
{
	switch(dev)
	{
		case RELAY_1:
			sg_relay_states_t.relay_1 = state;
			RELAY1_CTRL = (state?0:1);
			if(state == RELAY_ON)
				led_control_function(LD_OUT_1,LD_ON);
			else
				led_control_function(LD_OUT_1,LD_OFF);
			break;
			
		case RELAY_2:
			sg_relay_states_t.relay_2 = state;
			RELAY2_CTRL = (state?0:1);
			if(state == RELAY_ON)
				led_control_function(LD_OUT_2,LD_ON);
			else
				led_control_function(LD_OUT_2,LD_OFF);
			break;

		case RELAY_3:
			sg_relay_states_t.relay_3 = state;
			RELAY3_CTRL = (state?0:1);
			if(state == RELAY_ON)
				led_control_function(LD_OUT_3,LD_ON);
			else
				led_control_function(LD_OUT_3,LD_OFF);
			break;

		case RELAY_4:
			sg_relay_states_t.relay_4 = state;
			RELAY4_CTRL = (state?0:1);
			if(state == RELAY_ON)
				led_control_function(LD_OUT_4,LD_ON);
			else
				led_control_function(LD_OUT_4,LD_OFF);
			break;
			
		case RELAY_5:
			sg_relay_states_t.relay_5 = state;
			RELAY5_CTRL = (state?0:1);
			if(state == RELAY_ON)
				led_control_function(LD_OUT_5,LD_ON);
			else
				led_control_function(LD_OUT_5,LD_OFF);
			break;
			
		case RELAY_6:
			sg_relay_states_t.relay_6 = state;
			RELAY6_CTRL = (state?0:1);
			if(state == RELAY_ON)
				led_control_function(LD_OUT_6,LD_ON);
			else
				led_control_function(LD_OUT_6,LD_OFF);
			break;
			
		case RELAY_7:
			sg_relay_states_t.relay_7 = state;
			RELAY7_CTRL = (state?0:1);
			if(state == RELAY_ON)
				led_control_function(LD_OUT_7,LD_ON);
			else
				led_control_function(LD_OUT_7,LD_OFF);
			break;
			
		case RELAY_8:
			sg_relay_states_t.relay_8 = state;
			RELAY8_CTRL = (state?0:1);
			if(state == RELAY_ON)
				led_control_function(LD_OUT_8,LD_ON);
			else
				led_control_function(LD_OUT_8,LD_OFF);
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
int8_t relay_get_status_function(RELAY_DEV dev)
{
	switch(dev)
	{
		case RELAY_1:
			return  (sg_relay_states_t.relay_1?1:0);
		case RELAY_2:
			return  (sg_relay_states_t.relay_2?1:0);
		case RELAY_3:
			return  (sg_relay_states_t.relay_3?1:0);
		case RELAY_4:
			return  (sg_relay_states_t.relay_4?1:0);		
		case RELAY_5:
			return  (sg_relay_states_t.relay_5?1:0);
		case RELAY_6:
			return  (sg_relay_states_t.relay_6?1:0);
		case RELAY_7:
			return  (sg_relay_states_t.relay_7?1:0);
		case RELAY_8:
			return  (sg_relay_states_t.relay_8?1:0);		
		
		default:
			break;
	}
	return 0;
}


/************************************************************
*
* Function name	: relay_get_status
* Description	:  获取继电器状态
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t relay_get_status(uint8_t dev)
{
	switch(dev)
	{
		case 0:
			if(sg_relay_states_t.relay_1)	return 1;
			else	return 2;
		case 1:
			if(sg_relay_states_t.relay_2)	return 1;
			else	return 2;
		case 2:
			if(sg_relay_states_t.relay_3)	return 1;
			else	return 2;
		case 3:
			if(sg_relay_states_t.relay_4)	return 1;
			else	return 2;		
		case 4:
			if(sg_relay_states_t.relay_5)	return 1;
			else	return 2;
		case 5:
			if(sg_relay_states_t.relay_6)	return 1;
			else	return 2;
		case 6:
			if(sg_relay_states_t.relay_7)	return 1;
			else	return 2;
		case 7:
			if(sg_relay_states_t.relay_8)	return 1;
			else	return 2;	
		
		default:
			break;
	}
	return 0;
}

/************************************************************
*
* Function name	: relay_test
* Description	: 继电器测试
* Parameter		:
* Return		:
*
************************************************************/
void relay_test(void)
{
	while(1)
	{
		relay_control(RELAY_1,RELAY_ON); // 开继电器1
		relay_control(RELAY_2,RELAY_ON); // 开继电器2
		relay_control(RELAY_3,RELAY_ON); // 开继电器1
		relay_control(RELAY_4,RELAY_ON); // 开继电器2
		relay_control(RELAY_5,RELAY_ON); // 开继电器1
		relay_control(RELAY_6,RELAY_ON); // 开继电器2
		relay_control(RELAY_7,RELAY_ON); // 开继电器1
		relay_control(RELAY_8,RELAY_ON); // 开继电器2
		delay_ms(2000);
		relay_control(RELAY_1,RELAY_OFF); // 关继电器1
		relay_control(RELAY_2,RELAY_OFF); // 关继电器2
		relay_control(RELAY_3,RELAY_OFF); // 关继电器1
		relay_control(RELAY_4,RELAY_OFF); // 关继电器2
		relay_control(RELAY_5,RELAY_OFF); // 关继电器1
		relay_control(RELAY_6,RELAY_OFF); // 关继电器2
		relay_control(RELAY_7,RELAY_OFF); // 关继电器1
		relay_control(RELAY_8,RELAY_OFF); // 关继电器2
		delay_ms(2000);	
	}
}


