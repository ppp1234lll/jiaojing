#ifndef _UPDATE_H_
#define _UPDATE_H_
#include "sys.h"

#define UPDATE_MODE_NULL 0
#define UPDATE_MODE_LWIP 1
#define UPDATE_MODE_GPRS 2

#define UPDATE_NETWORK_CNT (6) // 网络连接次数

#define UPDATE_REC_BUFF_MAX (1280) 

uint8_t update_get_mode_function(void);
int8_t update_tcp_send_function(uint8_t *buff, uint16_t len) ;
void update_error_occurred_in_function(void);
uint8_t *update_addr_ip(void);
uint32_t update_addr_port(void);
void update_set_update_addr(uint8_t *ip,uint32_t port);

void update_read_addr(void);
void update_save_addr(void);
void update_set_update_mode(uint8_t mode);
uint8_t update_detection_status_function(void);
void update_end_function(void);

/* 20201103 */
typedef struct {
	uint8_t mode;			// 0-不更新 1-通过LWIP更新 2-通过GPRS更新
	uint8_t error;		// 更新错误标识
	uint8_t success;	// 更新成功
	uint8_t end;			// 结束
	
	uint8_t ip[4]; 			// 更新地址
	uint32_t port; 			// 更新端口
	struct {
		uint8_t state; 		// 状态
		uint8_t connect; 	// 连接
	} tcp_t;
	struct {
		uint8_t connect;
	} gprs_t;
} update_param_t;

void *update_get_infor_data_function(void);

/* 开机检测上次升级结果并上报平台 */
void update_status_detection(void);

/* 无线模块对应的更新函数 */
int8_t update_mobile_task_function(void);

/* 有线更新函数 */
int8_t update_lwip_task_function(void);

#endif
