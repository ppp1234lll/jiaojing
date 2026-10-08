#ifndef _FAN_H_
#define _FAN_H_

#include "sys.h"

typedef enum
{
	FAN_1 = 0, // 风扇 1
	FAN_2 = 1 // 风扇 2
} FAN_DEV;

typedef enum
{
	FAN_OFF = 0, // 关闭
	FAN_ON  = 1 // 打开
} FAN_STATUS;

/* 函数声明 */

void fan_gpio_init_function(void); // 初始化函数
void fan_control(FAN_DEV dev, FAN_STATUS state);
uint8_t fan_get_status_function(FAN_DEV dev);
void fan_test(void);
	
#endif
