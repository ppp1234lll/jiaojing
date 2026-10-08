#include "rtc.h"
#include "delay.h"
#include "time.h"
#include "usart_debug.h"

static uint8_t get_week_form_time(uint16_t year,uint8_t month,uint8_t day);

//RTC时间设置
//hour,min,sec:小时,分钟,秒钟
//ampm:@RTC_AM_PM_Definitions  :RTC_H12_AM/RTC_H12_PM
//返回值:SUCEE(1),成功
//       ERROR(0),进入初始化模式失败
ErrorStatus RTC_Set_Time(u8 hour,u8 min,u8 sec,u8 ampm)
{
	RTC_TimeTypeDef RTC_TimeTypeInitStructure;

	RTC_TimeTypeInitStructure.RTC_Hours=hour;
	RTC_TimeTypeInitStructure.RTC_Minutes=min;
	RTC_TimeTypeInitStructure.RTC_Seconds=sec;
	RTC_TimeTypeInitStructure.RTC_H12=ampm;

	return RTC_SetTime(RTC_Format_BIN,&RTC_TimeTypeInitStructure);
}

/************************************************************
*
* Function name	: TimeBySecond
* Description	: 根据获取到的时间戳设置时间
* Parameter		: 
* Return		: 
*	
************************************************************/
void TimeBySecond(u32 second)
{
	struct tm *pt,t;
	rtc_time_t time_t;
	second += 8*60*60;
	pt = localtime(&second);
	
	if(pt == NULL)
		return;
	t=*pt;
	t.tm_year+=1900;
	t.tm_mon++;
	time_t.year  = t.tm_year;
	time_t.month = t.tm_mon;
	time_t.data  = t.tm_mday;
	time_t.hour  = t.tm_hour;
	time_t.min   = t.tm_min;
	time_t.sec   = t.tm_sec;
	/* 设置时间 */
	RTC_set_Time(time_t);
}

//RTC日期设置
//year,month,date:年(0~99),月(1~12),日(0~31)
//week:星期(1~7,0,非法!)
//返回值:SUCEE(1),成功
//       ERROR(0),进入初始化模式失败
ErrorStatus RTC_Set_Date(u8 year,u8 month,u8 date,u8 week)
{
	RTC_DateTypeDef RTC_DateTypeInitStructure;
	RTC_DateTypeInitStructure.RTC_Date=date;
	RTC_DateTypeInitStructure.RTC_Month=month;
	RTC_DateTypeInitStructure.RTC_WeekDay=week;
	RTC_DateTypeInitStructure.RTC_Year=year;
	return RTC_SetDate(RTC_Format_BIN,&RTC_DateTypeInitStructure);
}


/* 月份数据表 */										 
const uint8_t cg_table_week[12]={0,3,3,6,1,4,6,2,5,0,3,5};
/************************************************************
*
* Function name	: get_week_form_time
* Description	: 通过年月日 获取星期
* Parameter		: 
* Return		: 
*	
************************************************************/
static uint8_t get_week_form_time(uint16_t year,uint8_t month,uint8_t day)
{	
	uint16_t temp2;
	uint8_t yearH,yearL;
	
	yearH=year/100;	yearL=year%100; 
	// 如果为21世纪,年份数加100  
	if (yearH>19)yearL+=100;
	// 所过闰年数只算1900年之后的  
	temp2=yearL+yearL/4;
	temp2=temp2%7; 
	temp2=temp2+day+cg_table_week[month-1];
	if (yearL%4==0&&month<3)temp2--;
	
	return(temp2%7);
}	

/************************************************************
*
* Function name	: RTC_set_Time
* Description	: 设置时间
* Parameter		:
* Return		:
*
************************************************************/
void RTC_set_Time(rtc_time_t rtc)
{
	rtc.week = get_week_form_time(rtc.year,rtc.month,rtc.data);
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, DISABLE); // 使能PWR时钟
	delay_ms(10);
	
	if(rtc.hour > 12)
	{
//		rtc.hour -= 12;
		RTC_Set_Time(rtc.hour,rtc.min,rtc.sec,RTC_H12_PM);
	}
	else
	{
		RTC_Set_Time(rtc.hour,rtc.min,rtc.sec,RTC_H12_AM);
	}
	RTC_Set_Date(rtc.year-2000,rtc.month,rtc.data,rtc.week);
	RTC_Set_Date(rtc.year-2000,rtc.month,rtc.data,rtc.week);
	
	delay_ms(10);
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);  // 使能PWR时钟	
}

/************************************************************
*
* Function name	: RTC_Get_Time
* Description	: 获取当前时间
* Parameter		:
* Return		:
*
************************************************************/
void RTC_Get_Time(rtc_time_t *rtc)
{

	RTC_DateTypeDef RTC_DateTypeInitStructure;

	RTC_TimeTypeDef RTC_TimeTypeInitStructure;
	RTC_GetTime(RTC_Format_BIN,&RTC_TimeTypeInitStructure);
	rtc->hour = RTC_TimeTypeInitStructure.RTC_Hours;
	rtc->min  = RTC_TimeTypeInitStructure.RTC_Minutes;
	rtc->sec  = RTC_TimeTypeInitStructure.RTC_Seconds;

	RTC_GetDate(RTC_Format_BIN,&RTC_DateTypeInitStructure);
	rtc->year  = RTC_DateTypeInitStructure.RTC_Year+2000;
	rtc->month = RTC_DateTypeInitStructure.RTC_Month;
	rtc->data  = RTC_DateTypeInitStructure.RTC_Date;
	rtc->week  = RTC_DateTypeInitStructure.RTC_WeekDay;

}

//RTC初始化
//返回值:0,初始化成功;
//       1,LSE开启失败;
//       2,进入初始化模式失败;
#define LSE_STARTUP_TIMEOUT     ((uint16_t)0x05000)
u8 My_RTC_Init(void)
{
	RTC_InitTypeDef RTC_InitStructure;
  __IO uint16_t StartUpCounter = 0;
	FlagStatus LSEStatus = RESET;	
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);//使能PWR时钟
	PWR_BackupAccessCmd(ENABLE);	//使能后备寄存器访问

/*=========================选择RTC时钟源==============================*/
///* 默认使用LSE，如果LSE出故障则使用LSI */
//  /* 使能LSE */
//  RCC_LSEConfig(RCC_LSE_ON);	
//	
//	/* 等待LSE启动稳定，如果超时则退出 */
//  do
//  {
//    LSEStatus = RCC_GetFlagStatus(RCC_FLAG_LSERDY);
//    StartUpCounter++;
//  }while((LSEStatus == RESET) && (StartUpCounter != LSE_STARTUP_TIMEOUT));
	if(LSEStatus == SET )
  {
		printf("\n\r LSE 启动成功 \r\n");
		/* 选择LSE作为RTC的时钟源 */
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
  }
	else
	{
		printf("\n\r LSE 故障，转为使用LSI \r\n");
		/* 使能LSI */	
		RCC_LSICmd(ENABLE);
		/* 等待LSI稳定 */ 
		while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
		{			
		}
		
		printf("\n\r LSI 启动成功 \r\n");
		/* 选择LSI作为RTC的时钟源 */
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
	}	
	
	RCC_RTCCLKCmd(ENABLE);	//使能RTC时钟
	RTC_WaitForSynchro();   /* 等待 RTC APB 寄存器同步 */

	/*=====================初始化同步/异步预分频器的值======================*/
	/* 驱动日历的时钟ck_spare = LSE/[(255+1)*(127+1)] = 1HZ */
	RTC_InitStructure.RTC_AsynchPrediv = 0x7f;//RTC异步分频系数(1~0X7F)
	RTC_InitStructure.RTC_SynchPrediv  = 0xFF;//RTC同步分频系数(0~7FFF)
	RTC_InitStructure.RTC_HourFormat   = RTC_HourFormat_24;//RTC设置为,24小时格式
	RTC_Init(&RTC_InitStructure);

	RTC_Set_Time(11,48,0,RTC_H12_AM);	//设置时间
	RTC_Set_Date(24,5,16,4);		//设置日期

	return 0;
}

//设置闹钟时间(按星期闹铃,24小时制)
//week:星期几(1~7) @ref  RTC_Alarm_Definitions
//hour,min,sec:小时,分钟,秒钟
void RTC_Set_AlarmA(uint8_t week,uint8_t hour,uint8_t min,uint8_t sec)
{
	EXTI_InitTypeDef   EXTI_InitStructure;
	RTC_AlarmTypeDef   RTC_AlarmTypeInitStructure;
	RTC_TimeTypeDef    RTC_TimeTypeInitStructure;
	NVIC_InitTypeDef   NVIC_InitStructure;

	RTC_AlarmCmd(RTC_Alarm_A,DISABLE);//关闭闹钟A

	RTC_TimeTypeInitStructure.RTC_Hours   = hour;//小时
	RTC_TimeTypeInitStructure.RTC_Minutes = min;//分钟
	RTC_TimeTypeInitStructure.RTC_Seconds = sec;//秒
	RTC_TimeTypeInitStructure.RTC_H12     = RTC_H12_AM;

	RTC_AlarmTypeInitStructure.RTC_AlarmDateWeekDay = week;//星期
	RTC_AlarmTypeInitStructure.RTC_AlarmDateWeekDaySel=RTC_AlarmDateWeekDaySel_WeekDay;//按星期闹
	RTC_AlarmTypeInitStructure.RTC_AlarmMask=RTC_AlarmMask_None;//精确匹配星期，时分秒
	RTC_AlarmTypeInitStructure.RTC_AlarmTime=RTC_TimeTypeInitStructure;
	RTC_SetAlarm(RTC_Format_BIN,RTC_Alarm_A,&RTC_AlarmTypeInitStructure);


	RTC_ClearITPendingBit(RTC_IT_ALRA);//清除RTC闹钟A的标志
	EXTI_ClearITPendingBit(EXTI_Line17);//清除LINE17上的中断标志位

	RTC_ITConfig(RTC_IT_ALRA,ENABLE);//开启闹钟A中断
	RTC_AlarmCmd(RTC_Alarm_A,ENABLE);//开启闹钟A

	EXTI_InitStructure.EXTI_Line = EXTI_Line17;//LINE17
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;//中断事件
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising; //上升沿触发
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;//使能LINE17
	EXTI_Init(&EXTI_InitStructure);//配置

	NVIC_InitStructure.NVIC_IRQChannel = RTC_Alarm_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;//抢占优先级1
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;//子优先级2
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;//使能外部中断通道
	NVIC_Init(&NVIC_InitStructure);//配置
}

//周期性唤醒定时器设置
/*wksel:  @ref RTC_Wakeup_Timer_Definitions
#define RTC_WakeUpClock_RTCCLK_Div16        ((uint32_t)0x00000000)
#define RTC_WakeUpClock_RTCCLK_Div8         ((uint32_t)0x00000001)
#define RTC_WakeUpClock_RTCCLK_Div4         ((uint32_t)0x00000002)
#define RTC_WakeUpClock_RTCCLK_Div2         ((uint32_t)0x00000003)
#define RTC_WakeUpClock_CK_SPRE_16bits      ((uint32_t)0x00000004)
#define RTC_WakeUpClock_CK_SPRE_17bits      ((uint32_t)0x00000006)
*/
//cnt:自动重装载值.减到0,产生中断.
void RTC_Set_WakeUp(u32 wksel,u16 cnt)
{
	EXTI_InitTypeDef   EXTI_InitStructure;
	NVIC_InitTypeDef   NVIC_InitStructure;

	RTC_WakeUpCmd(DISABLE);//关闭WAKE UP

	RTC_WakeUpClockConfig(wksel);//唤醒时钟选择

	RTC_SetWakeUpCounter(cnt);//设置WAKE UP自动重装载寄存器


	RTC_ClearITPendingBit(RTC_IT_WUT); //清除RTC WAKE UP的标志
	EXTI_ClearITPendingBit(EXTI_Line22);//清除LINE22上的中断标志位

	RTC_ITConfig(RTC_IT_WUT,ENABLE);//开启WAKE UP 定时器中断
	RTC_WakeUpCmd( ENABLE);//开启WAKE UP 定时器　

	EXTI_InitStructure.EXTI_Line = EXTI_Line22;//LINE22
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;//中断事件
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising; //上升沿触发
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;//使能LINE22
	EXTI_Init(&EXTI_InitStructure);//配置


	NVIC_InitStructure.NVIC_IRQChannel = RTC_WKUP_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x02;//抢占优先级1
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x02;//子优先级2
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;//使能外部中断通道
	NVIC_Init(&NVIC_InitStructure);//配置
}

//RTC闹钟中断服务函数
void RTC_Alarm_IRQHandler(void)
{
	if(RTC_GetFlagStatus(RTC_FLAG_ALRAF)==SET)//ALARM A中断?
	{
		printf("time alarm\n");
		RTC_ClearFlag(RTC_FLAG_ALRAF);//清除中断标志
		NVIC_SystemReset(); //复位
	}
	EXTI_ClearITPendingBit(EXTI_Line17);	//清除中断线17的中断标志
}

//RTC WAKE UP中断服务函数
void RTC_WKUP_IRQHandler(void)
{
    if(RTC_GetFlagStatus(RTC_FLAG_WUTF)==SET)//WK_UP中断?
    {
        RTC_ClearFlag(RTC_FLAG_WUTF);	//清除中断标志
    }
    EXTI_ClearITPendingBit(EXTI_Line22);//清除中断线22的中断标志
}


/************************************************************
*
* Function name	: time_to_second_function
* Description	: 时间转时间戳 - NB
* Parameter		: 
* Return		: 
*	
************************************************************/
void time_to_second_function(uint32_t *time, uint32_t *second)
{
	struct tm pt;
	
	pt.tm_year = time[0]+100;
	pt.tm_mon  = time[1] - 1;
	pt.tm_mday = time[2];
	pt.tm_hour = time[3];
	pt.tm_min  = time[4];
	pt.tm_sec  = time[5];
	
	*second = mktime(&pt);
}

/************************************************************
*
* Function name	: RTC_Get_Time
* Description	: 获取当前时间
* Parameter		:
* Return		:
*
************************************************************/
void RTC_Get_Time_Test(void)
{
	static rtc_time_t rtc_test;
	while(1)
	{
		RTC_Get_Time(&rtc_test);
		printf("data:%d-%d-%d,week:%d,time:%d:%d:%d",rtc_test.year,rtc_test.month,rtc_test.data,
																		rtc_test.week,rtc_test.hour,rtc_test.min,rtc_test.sec);
		delay_ms(1000);
	}
}





