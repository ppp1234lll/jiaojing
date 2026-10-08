#include "key.h"
#include "det.h"
#include "delay.h"
#include "includes.h"
/*
	4、输入检测
		按键(恢复出厂设置):      PD4
		箱门检测:                PE14
		12V电源输入监测:         PD10
		浸水检测:								 PE8
		
		输入 1:                 PE11
		输入 2:                 PE12
		输入 3:                 PE13
		
		SIM_DET: 								PD1
*/

//#define RESET_KEY_READ 			GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_4)   	// 复位检测
//#define OPEN_DOOR_READ  		GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_14)    // 箱门检测
//#define PWR_TST_READ   	 		GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_10)    // 12V检测
//#define WATER_READ   	 			GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_8)   	// 浸水检测
//#define INPUT1_READ   	 		GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_11)   	// 输入1
//#define INPUT2_READ   	 		GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_12)   	// 输入2
//#define INPUT3_READ   	 		GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_13)   	// 输入3
//#define SIM_DET_READ   	 		GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_1)   	// SIM卡检测

#define RESET_KEY_READ 			GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_2)   	// 复位检测
#define OPEN_DOOR_READ  		GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_7)    // 箱门检测
#define PWR_TST_READ   	 		GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_0)    // 12V检测
#define WATER_READ   	 			GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_13)   	// 浸水检测
#define INPUT1_READ   	 		GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_14)   	// 输入1
#define INPUT2_READ   	 		GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_15)   	// 输入2
#define INPUT3_READ   	 		GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_6)   	// 输入3
#define SIM_DET_READ   	 		GPIO_ReadInputDataBit(GPIOE,GPIO_Pin_1)   	// SIM卡检测

key_detect_t sg_keydetect_t[3] = {0};

/************************************************************
*
* Function name	: key_init_function
* Description	: 按键初始化函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void key_init_function(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD|RCC_AHB1Periph_GPIOE|RCC_AHB1Periph_GPIOC,ENABLE);

	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_0|GPIO_Pin_2|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;  // 输入
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;  // 上拉输入
	GPIO_Init(GPIOD,&GPIO_InitStructure); 	// 初始化GPIOD

	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;  // 输入
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;  // 上拉输入
	GPIO_Init(GPIOE,&GPIO_InitStructure); 	// 初始化GPIOE
	
	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_6|GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;  // 输入
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;  // 上拉输入
	GPIO_Init(GPIOC,&GPIO_InitStructure); 	// 初始化GPIOC

	/* 参数初始化 */
	memset(sg_keydetect_t,0,sizeof(sg_keydetect_t)*3);
}

/************************************************************
*
* Function name	: key_detection_function
* Description	: 按键检测
* Parameter		: 
* Return		: 
*	
************************************************************/
void key_detection_function(void)
{
	if(RESET_KEY_READ ==0 )   //key1读取KEY按键，表示key1按下
	{                   
		switch(sg_keydetect_t[RESET_KEY_NUM].state)
		{
			case KEY_NONE:            
			 sg_keydetect_t[RESET_KEY_NUM].state	=	KEY_1_DOWN; //按键状态切换为单次按下
			 break;
			
			case KEY_1_DOWN:  //这里为按键长按检测   
				sg_keydetect_t[RESET_KEY_NUM].long_count++;   
				if(sg_keydetect_t[RESET_KEY_NUM].long_count	>=	KEY_LONG_TIME)
				{
					sg_keydetect_t[RESET_KEY_NUM].key_resault	=	KEY_1_LONG;
					sg_keydetect_t[RESET_KEY_NUM].state	=	KEY_1_LONG;
				}
			 break;
				
			case KEY_1_UP: //如果本次按键按下距上次按下时间小于双击的最小时间间隔，则会执行以下程序             
				sg_keydetect_t[RESET_KEY_NUM].state	=	KEY_1_DUBDOW; //按键状态切换为双击
				sg_keydetect_t[RESET_KEY_NUM].count=0;
			 break;
			
			case KEY_1_LONG: // 按键长按状态，不进行状态切换         
			 break;
			
			case KEY_1_DUBDOW:          
			 break;
			
			case KEY_1_TIEBLE:          
			 break;
			
			case KEY_1_DOUBLE: //双击按下后再次按下按键，为三击操作
				sg_keydetect_t[RESET_KEY_NUM].state	=	KEY_1_TIEBLE;
			 break;
			
			default:
			 break;
		}
	}
	else
	{
		switch(sg_keydetect_t[RESET_KEY_NUM].state) //按键松开
		{
			 case KEY_NONE:            
				sg_keydetect_t[RESET_KEY_NUM].count	=	0;
				sg_keydetect_t[RESET_KEY_NUM].double_count=0;
				sg_keydetect_t[RESET_KEY_NUM].long_count=0;
				sg_keydetect_t[RESET_KEY_NUM].trible_count=0;     
				break;
		 
			 case KEY_1_DOWN: //单击按键松开后，进行双击时间检测，为了和长按区分，变换按键状态
				sg_keydetect_t[RESET_KEY_NUM].state	=	KEY_1_UP;
			 break;
			 
			 case KEY_1_UP://双击时间间隔检测
				 sg_keydetect_t[RESET_KEY_NUM].count++;         
				 sg_keydetect_t[RESET_KEY_NUM].double_count++;
				 if(sg_keydetect_t[RESET_KEY_NUM].count>=KEY_DOUBLE_MAX) //表示超过双击最小时间间隔
				 {
						sg_keydetect_t[RESET_KEY_NUM].key_resault=KEY_1_DOWN; //按键检测最终结果为单次按下
						sg_keydetect_t[RESET_KEY_NUM].state=KEY_NONE;//清除按键状态
				 }
				break;
				 
				case KEY_1_LONG:  //长按按键后松开，需要对按键结果和状态即使清除
//					 sg_keydetect_t[RESET_KEY_NUM].state=KEY_NONE;
//					 sg_keydetect_t[RESET_KEY_NUM].key_resault=KEY_NONE;
				 break;
				
				case KEY_1_DUBDOW: //双击按键松手后，进行双击状态切换
					 sg_keydetect_t[RESET_KEY_NUM].state=KEY_1_DOUBLE;
					 break;
				
				case KEY_1_DOUBLE:
					sg_keydetect_t[RESET_KEY_NUM].count++;        
					sg_keydetect_t[RESET_KEY_NUM].trible_count++;
					if(sg_keydetect_t[RESET_KEY_NUM].count>=KEY_DOUBLE_MAX)
					{
						if((sg_keydetect_t[RESET_KEY_NUM].double_count>=KEY_DOUBLE_MIN)&&(sg_keydetect_t[RESET_KEY_NUM].double_count<=KEY_DOUBLE_MAX)) //双击按键间隔最小限制可以消抖
						{
							sg_keydetect_t[RESET_KEY_NUM].key_resault=KEY_1_DOUBLE;
						}
						sg_keydetect_t[RESET_KEY_NUM].state=KEY_NONE;
					}      
					break;
					
				 case KEY_1_TIEBLE:
						if((sg_keydetect_t[RESET_KEY_NUM].trible_count>=KEY_DOUBLE_MIN)&&(sg_keydetect_t[RESET_KEY_NUM].trible_count<=KEY_DOUBLE_MAX))
						{	
							sg_keydetect_t[RESET_KEY_NUM].key_resault=KEY_1_TIEBLE; //三击同双击
						}
						sg_keydetect_t[RESET_KEY_NUM].state=KEY_NONE;  
					break;
						
			 default:
				break;
		}
	}
}

/************************************************************
*
* Function name	: key_get_state_function
* Description	: 获取按键事件信息
* Parameter		: 
*	@key_num	：按键号
* Return		: 
*	
************************************************************/
uint8_t key_get_state_function(uint8_t key_num)
{
	uint8_t resault = 0;
	if(sg_keydetect_t[key_num].key_resault != 0)
	{
		resault = sg_keydetect_t[key_num].key_resault;
		sg_keydetect_t[key_num].key_resault = KEY_NONE;
		sg_keydetect_t[key_num].state = KEY_NONE;
		return resault;
	}
	return 0;
}


/************************************************************
*
* Function name	: open_door_detection
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
void open_door_detection(void)
{
	if(OPEN_DOOR_READ == 1)	/* 监测到箱门被开启 */
	{
		det_set_open_door(1);
	}
	else
	{
		det_set_open_door(0);
	}
}

/************************************************************
*
* Function name	: pwr_tst_detection
* Description	: 市电输入检测
* Parameter		: 
* Return		: 
*	
************************************************************/
void pwr_tst_detection(void)
{
	if(PWR_TST_READ != 0) 
	{
		det_set_220v_in_function(0);  // 12V断电
	} 
	else 
	{
		det_set_220v_in_function(1);  // 12V正常
	}
}

/************************************************************
*
* Function name	: spd_status_detection
* Description	: 防雷检测
* Parameter		: 0=无，1=正常，2=失效
* Return		:  
*	   20230721
************************************************************/
void spd_status_detection(void)
{
#ifdef SPD_ENABLE
	if(SPD_ON_READ == 0 || SPD_OFF_READ == 1) 
	{
		det_set_spd_status(1);  // 1：正常
	}
	else
	{
		det_set_spd_status(2);
	}
#else
	det_set_spd_status(0); // 无
#endif
}

/************************************************************
*
* Function name	: sim_status_detection
* Description	: SIM检测(插入为低电平)
* Parameter		: 0=无，1=插入，2=拔出
* Return		:  
*	   20230721
************************************************************/
void sim_status_detection(void)
{
#ifdef SIM_ENABLE
	if(SIM_DET_READ == 0) 
	{
		det_set_sim_status(1);  // 1：正常
	}
	else
	{
		det_set_sim_status(2);
	}
#else
	det_set_sim_status(0); // 无
#endif
}

/************************************************************
*
* Function name	: water_status_detection
* Description	:浸水检测(正常为高电平)
* Parameter		: 0=无，1=正常，2=浸水
* Return		:  
*	   202301201
************************************************************/
void water_status_detection(void)
{
#ifdef WATER_ENABLE
	static uint8_t water_count = 0;
	static uint8_t water_count2 = 0;
	if(WATER_READ == 1) 
	{
		water_count++;
		if(water_count > 10)  // 10*20 = 200
		{
			water_count = 0;
			if(WATER_READ == 1) 
				det_set_water_status(2);  // 1：正常
		}
	}
	else
	{
		water_count2++;
		if(water_count2 > 10)  // 10*20 = 200
		{
			water_count2 = 0;
			if(WATER_READ == 0) 	
				det_set_water_status(1);
		}
	}
#else
	det_set_water_status(0); // 无
#endif
}

/************************************************************
*
* Function name	: key_test
* Description	: 按键测试
* Parameter		:
* Return		:
*
************************************************************/
void key_test(void)
{
	while(1)
	{
//		if(RESET_KEY_READ == 0)
//			printf("复位按键按下\n");
//		else
//			printf("复位按键弹起\n");

//		if(OPEN_DOOR_READ == 0)
//			printf("门开关按下\n");
//		else
//			printf("门开关弹起\n");

//		if(PWR_TST_READ == 0)
//			printf("适配器按下\n");
//		else
//			printf("适配器弹起\n");

//		if(INPUT1_READ == 0)
//			printf("输入1正常\n");
//		else
//			printf("输入1关\n");	

//		if(INPUT2_READ == 0)
//			printf("输入2正常\n");
//		else
//			printf("输入2关\n");	

//		if(INPUT3_READ == 0)
//			printf("输入3正常\n");
//		else
//			printf("输入3关\n");

//		if(WATER_READ == 0)
//			printf("浸水正常\n");
//		else
//			printf("浸水关\n");
//		
//		if(SIM_DET_READ == 0)
//			printf("SIM正常\n");
//		else
//			printf("SIM弹出\n");	

		delay_ms(1000);		
	}
}

