#ifndef _START_H_
#define _START_H_

#include "sys.h"

#define STM2F1_UUID_ADDR  352525   // 任意的一个数
#define STM2F3_UUID_ADDR  352525   // 任意的一个数
#define STM2F4_UUID_ADDR  352525   // 任意的一个数
#define STM2F7_UUID_ADDR  352525   // 任意的一个数

typedef struct
{
	uint8_t id_flag;				// 更新标志位
	uint32_t crc_dat;       // 校验数据				
}code_id_t;

/* 参数 */
typedef struct
{
  uint32_t id[3];
} ChipID_t;

/* 函数声明 */
void PVD_Init(void);  // 初始化PVD 
void start_system_init_function(void);  // 系统初始化函数
void start_get_device_id_function(void);  // 获取本机ID
void start_get_device_id_str(uint8_t *str); // 获取本机ID
void start_get_device_id(uint32_t *id);
void start_creat_task_function(void);   // 创建任务
void task_suspend_function(void);
void start_pack_device_uuid_str(char *data);
#endif
