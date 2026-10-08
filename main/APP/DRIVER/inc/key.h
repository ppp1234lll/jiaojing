#ifndef _KEY_H_
#define _KEY_H_

#include "sys.h"

/* 按键号 */
#define RESET_KEY_NUM   (0) // 系统复位按键

typedef struct
{
	uint8_t get_key_num; 
	uint8_t state;
	uint8_t long_count; 
	uint8_t key_resault;
	uint8_t count;    
	uint8_t double_count;
	uint8_t trible_count;
} key_detect_t;

#define KEY_NONE 			0 
#define KEY_1_DOWN 		1
#define	KEY_1_UP 			2
#define	KEY_1_LONG 		3
#define	KEY_1_DUBDOW 	4
#define	KEY_1_TIEBLE 	5
#define	KEY_1_DOUBLE 	6
#define	KEY_1_FOUR 	  7

#define KEY_LONG_TIME 		100  	//三击按键间隔最小时间 100 * 20 = 2000 ms
#define KEY_DOUBLE_MAX 		10 		//双击按键间隔最小时间 20 * 20  = 400 ms
#define KEY_DOUBLE_MIN 		1  		//双击按键消抖时间 1*20=20 ms


/* 函数声明 */
void key_init_function(void);
void key_detection_function(void);

void open_door_detection(void); // 箱门检测
void pwr_tst_detection(void);		// 市电检测
void spd_status_detection(void);  // 20230721
void sim_status_detection(void);  // 20230721
void water_status_detection(void);

uint8_t key_get_state_function(uint8_t key_num);
void key_test(void);

#endif
