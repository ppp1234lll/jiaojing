#ifndef _RELAY_H_
#define _RELAY_H_

#include "sys.h"

typedef enum
{
	RELAY_1 = 0, // 继电器 1
	RELAY_2 = 1, // 继电器 2
	RELAY_3 = 2, // 继电器 3
	RELAY_4 = 3, // 继电器 4
	RELAY_5 = 4, // 继电器 5
	RELAY_6 = 5, // 继电器 6
	RELAY_7 = 6, // 继电器 7
	RELAY_8 = 7, // 继电器 8
} RELAY_DEV;

typedef enum
{
	RELAY_OFF = 0, // 关闭
	RELAY_ON  = 1, // 打开
} RELAY_STATUS;

/* 函数声明 */

void relay_gpio_init_function(void); // 初始化函数
void relay_control(RELAY_DEV dev, RELAY_STATUS state);
int8_t relay_get_status_function(RELAY_DEV dev);
int8_t relay_get_status(uint8_t dev);
void relay_test(void);
	
#endif
