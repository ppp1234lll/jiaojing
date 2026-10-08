#include "timer.h"
#include "led.h"
#include "lwip_comm.h"
#include "lwip_ping.h"
#include "app.h"
#include "adc.h"
#include "eth.h"
#include "key.h"
#include "bootload.h"
#include "BL0910.h"
#include "gsm.h"
#include "lfs_port.h"
#include "com.h"
#include "usart_debug.h"

#define RELOAD_TIME  172800 // 设备定时重启时间 每周  2*24*60*60
uint32_t reload_time_cnt = 0;  // 定时重启
uint32_t sync_time_cnt = 0;     // 定时同步时间
//通用定时器3中断初始化
//arr：自动重装值。
//psc：时钟预分频数
//定时器溢出时间计算方法:Tout=((arr+1)*(psc+1))/Ft us.
//Ft=定时器工作频率,单位:Mhz
//这里使用的是定时器3!
void TIM3_Int_Init(u16 arr,u16 psc)
{

	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3,ENABLE);  			// 使能TIM3时钟
	
	TIM_TimeBaseInitStructure.TIM_Prescaler=psc;  					// 定时器分频
	TIM_TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up; 	// 向上计数模式
	TIM_TimeBaseInitStructure.TIM_Period=arr;   					// 自动重装载值
	TIM_TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1; 
	TIM_TimeBaseInit(TIM3,&TIM_TimeBaseInitStructure);
		
	NVIC_InitStructure.NVIC_IRQChannel=TIM3_IRQn; 
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority=0x03; 
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	
	TIM_ClearITPendingBit(TIM3, TIM_IT_Update);//清除更新中断请求位
	TIM_ITConfig(TIM3,TIM_IT_Update,ENABLE); 						// 允许定时器3更新中断
	TIM_Cmd(TIM3,ENABLE); 	
}

void TIM2_Int_Init(u16 arr,u16 psc)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);  ///使能TIM3时钟
	
	TIM_TimeBaseInitStructure.TIM_Prescaler=psc;  //定时器分频
	TIM_TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up; //向上计数模式
	TIM_TimeBaseInitStructure.TIM_Period=arr;   //自动重装载值
	TIM_TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1; 
	TIM_TimeBaseInit(TIM2,&TIM_TimeBaseInitStructure);

	NVIC_InitStructure.NVIC_IRQChannel=TIM2_IRQn; //定时器3中断
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority= 1; //抢占优先级1
	NVIC_InitStructure.NVIC_IRQChannelSubPriority= 0; //子优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	
	TIM_ClearITPendingBit(TIM2, TIM_IT_Update);//清除更新中断请求位
	TIM_ITConfig(TIM2,TIM_IT_Update,ENABLE); //允许定时器3更新中断
	TIM_Cmd(TIM2,DISABLE); //使能定时器3
	
}
void TIM6_Int_Init(u16 arr,u16 psc)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6,ENABLE);  ///使能TIM3时钟
	
	TIM_TimeBaseInitStructure.TIM_Prescaler=psc;  //定时器分频
	TIM_TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up; //向上计数模式
	TIM_TimeBaseInitStructure.TIM_Period=arr;   //自动重装载值
	TIM_TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1; 
	TIM_TimeBaseInit(TIM6,&TIM_TimeBaseInitStructure);
	
	TIM_ClearITPendingBit(TIM6,TIM_IT_Update);//清空中断状态
	TIM_ITConfig(TIM6,TIM_IT_Update,ENABLE); //允许定时器3更新中断
	TIM_Cmd(TIM6,ENABLE); //使能定时器3
	
	NVIC_InitStructure.NVIC_IRQChannel	=	TIM6_DAC_IRQn; //定时器3中断
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority= 1; //抢占优先级1
	NVIC_InitStructure.NVIC_IRQChannelSubPriority= 2; //子优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	
}

void TIM6_DAC_IRQHandler(void)
{	
//	static uint16_t time6_count =0;
	if(TIM_GetITStatus(TIM6,TIM_IT_Update) != RESET)
	{
//		time6_count++;
//		if(time6_count >= 3)
//		{
//			time6_count =0;
//			printf("time6 test\n");
//		}	
		reload_time_cnt++;
		if(reload_time_cnt >= app_get_device_reload_time())
		{
			reload_time_cnt = 0;
			System_SoftReset();
		}	
		app_fan_timer_function();	
		TIM_ClearITPendingBit(TIM6,TIM_IT_Update);//清空中断状态
	}
} 
void TIM2_IRQHandler(void)
{ 	
//	static uint16_t time2_count =0;
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)//是更新中断
	{	 
//		time2_count++;
//		if(time2_count >= 100)
//		{
//			time2_count =0;
//			printf("time2 test\n");
//		}			
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update  );  //清除TIM5更新中断标志    
		my_modem_timer_task();
	}	   

}

static uint32_t sg_reboot_time = 0;

/************************************************************
*
* Function name	: 
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
void set_reboot_time_function(uint32_t time)
{
	lfs_unmount(&g_lfs_t);
	sg_reboot_time = time;
}

/************************************************************
*
* Function name	: 
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
static void device_reboot_timer_function(void)
{
	if(sg_reboot_time != 0) {
		sg_reboot_time--;
		if(sg_reboot_time==0) {
			System_SoftReset();
		}
	}
}

//定时器3中断服务函数
void TIM3_IRQHandler(void)
{
//	static uint16_t time3_count =0;
	if(TIM_GetITStatus(TIM3,TIM_IT_Update)==SET) //溢出中断
	{
//		time3_count++;
//		if(time3_count >= 1000)
//		{
//			time3_count =0;
//			printf("time3 test\n");
//		}
		lwip_ping_timer_function();
		app_com_time_function();
		app_sys_operate_timer_function();
		eth_ping_timer_function();
		led_flicker_control_timer_function();
		device_reboot_timer_function();
		bl0910_run_timer_function();
		com_queue_time_function();
	}
	TIM_ClearITPendingBit(TIM3,TIM_IT_Update);  //清除中断标志位

}
