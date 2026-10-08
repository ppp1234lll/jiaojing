#ifndef __RTC_H
#define __RTC_H
#include "sys.h"

// 时钟源宏定义
//#define RTC_CLOCK_SOURCE_LSE      
#define RTC_CLOCK_SOURCE_LSI  

typedef struct
{
	uint16_t year;
	uint8_t  month;
	uint8_t  data;
	uint8_t  week;
	uint8_t  hour;
	uint8_t  min;
	uint8_t  sec;
} rtc_time_t;

u8 My_RTC_Init(void);										// RTC初始化
ErrorStatus RTC_Set_Time(u8 hour,u8 min,u8 sec,u8 ampm);	// RTC时间设置
ErrorStatus RTC_Set_Date(u8 year,u8 month,u8 date,u8 week); // RTC日期设置
void RTC_Set_AlarmA(u8 week,u8 hour,u8 min,u8 sec);			// 设置闹钟时间(按星期闹铃,24小时制)
void RTC_Set_WakeUp(u32 wksel,u16 cnt);						// 周期性唤醒定时器设置
void RTC_Get_Time(rtc_time_t *rtc);
void RTC_set_Time(rtc_time_t rtc);
void TimeBySecond(u32 second);

void RTC_Get_Time_Test(void);

#endif

















