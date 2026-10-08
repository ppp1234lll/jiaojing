#ifndef _LED_H_
#define _LED_H_

#include "sys.h"

/* 参数 */
typedef enum
{
	LD_STATE   = 0, // 系统指示灯
	LD_GPRS    = 1, // 2G指示灯
	LD_LAN     = 2, // 网口
	LD_POWER   = 3, // 电源-外接
	LD_LAN_LED = 4, // 网口-外接
	LD_OUT_1   = 5, // 继电器 1
	LD_OUT_2   = 6, // 继电器 2
	LD_OUT_3   = 7, // 继电器 3
	LD_OUT_4   = 8, // 继电器 4
	LD_OUT_5   = 9, // 继电器 5
	LD_OUT_6   = 10, // 继电器 6
	LD_OUT_7   = 11, // 继电器 7
	LD_OUT_8   = 12, // 继电器 8
	LED_ALL
} LED_DEV;

typedef enum
{
	LD_OFF 	   = 0, // 熄灭
	LD_ON  	   = 1, // 常亮
	LD_FLICKER = 2, // 闪烁
	LD_FLIC_Q  = 3, // 快速闪烁
} LED_STATUS;

typedef struct
{
	uint8_t gprs;
	uint8_t lan;
	uint8_t lan_out;
	uint8_t state;
	uint8_t relay[8];
} led_flicker_t;


/* 函数声明 */
void led_gpio_init_function(void);	// 初始化函数
void led_flicker_control_timer_function(void);

void led_control_function(LED_DEV dev, LED_STATUS state);
void led_out_control_function(LED_DEV dev, LED_STATUS state);
void led_light_all(void);
void led_light_off(void);
void *led_get_status_function(void);
void led_flicker_all(uint8_t num);
void led_test(void);
#endif
