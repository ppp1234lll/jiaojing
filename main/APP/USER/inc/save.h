#ifndef _SAVE_H_
#define _SAVE_H_

#include "bsp.h"
#include "app.h"
#include "bootload.h"

#define LITTLFS_MODE (0)

#define SAVE_FLIE_MODE LITTLFS_MODE

/* 参数 */
/* 默认参数 */
#define DEFALUT_LOCAL_IP0 (192)
#define DEFALUT_LOCAL_IP1 (168)
#define DEFALUT_LOCAL_IP2 (1)
#define DEFALUT_LOCAL_IP3 (30)

#define DEFALUT_NETMASK0 (255)
#define DEFALUT_NETMASK1 (255)
#define DEFALUT_NETMASK2 (255)
#define DEFALUT_NETMASK3 (0)

#define DEFALUT_GATEWAY0 (192)
#define DEFALUT_GATEWAY1 (168)
#define DEFALUT_GATEWAY2 (1)
#define DEFALUT_GATEWAY3 (1)
#define DEFALUT_MULTICAST_IP0 (239)
#define DEFALUT_MULTICAST_IP1 (255)
#define DEFALUT_MULTICAST_IP2 (255)
#define DEFALUT_MULTICAST_IP3 (249)

#define DEFALUT_MULTICAST_PORT (65000)

#define DEFALUT_DNS0	 (114)
#define DEFALUT_DNS1	 (114)
#define DEFALUT_DNS2	 (114)
#define DEFALUT_DNS3	 (114)

#define DEFALUT_PORT	 (8088)
#define DEFALUT_SERVERMODE	 (3)

#define DEFALUT_VOLT_MAX   		(300)
#define DEFALUT_VOLT_MIN 			(80)
#define DEFALUT_CURRENT_MAX	  (5000)
#define DEFAULT_ANGLE		 			(40)

#define DEFALUT_TEMP_HIGH  	(60)
#define DEFALUT_TEMP_LOW 		(1)
#define DEFALUT_HUMI_HIGH		(80)
#define DEFAULT_HUMI_LOW 		(10)

#define DEFALUT_HEAT_UP	  (-20)	
#define DEFALUT_HEAT_DOWN (-5)

#define DEFALUT_TIME_START0 (12) 
#define DEFALUT_TIME_START1 (00)

#define DEFALUT_TIME_DOWN0 (12) 
#define DEFALUT_TIME_DOWN1 (00)

#define DEFALUT_PASSWORD   ("88888888")

#define DEFALUT_HEART			(90*1000)
#define DEFALUT_REPORT		(180*1000)
#define DEFALUT_PING			(20*1000)   // 每轮ping的间隔时间
#define DEFALUT_DEV_PING	(10*1000) // 下一次ping的时间
#define DEFALUT_NETWORK_DELAY	(200) // 网络延时时间  20220308
#define DEFALUT_ONVIF_TIME  	(120) // ONVIF搜索时间  20230811
#define DEFALUT_RELOAD_TIME  	(48*60*60)  // 重启时间       20240904

/* 存储相关 */
#define SAVE_OTHER_PARAM   (0) // 其余数据
#define SAVE_LOCAL_NETWORK (1) // 本地网络信息
#define SAVE_REMOTE_IP     (2) // 远端网络信息
#define SAVE_COMPARISION   (3) // 外设相关数据
#define SAVE_DEVICE_PARAM  (4) // 系统数据
#define SAVE_COM_PARAMETER (5) // 通信数据
#define SAVE_UPDATE		     (6) // 更新参数
#define SAVE_REPORT_SW	   (7) // 上报开关参数
#define SAVE_ONLY_SEND_IP  (8) // 只发送服务器
#define SAVE_CAREMA        (9) // 摄像机参数
#define SAVE_THRESHOLD     (10) // 阈值

/* 函数声明 */
void save_init_function(void);
void save_clear_file_function(uint8_t mode);

// 存储
int8_t save_stroage_local_network(struct local_ip_t *local);
int8_t save_stroage_remote_ip_function(struct remote_ip *remote);
int8_t save_stroage_comparision_parameter(comparision_parameter_t *param);
int8_t save_storage_device_parameter_function(struct device_param *param);
int8_t save_stroage_com_param_function(com_param_t *param);
int8_t save_stroage_update_addr(uint8_t *ip,uint32_t port) ;
int8_t save_stroage_report_switch(struct report_status *param);

// 读取
int8_t save_read_local_network(struct local_ip_t *local);
int8_t save_read_remote_ip_function(struct remote_ip *remote);
int8_t save_read_comparision_parameter(comparision_parameter_t *param);
int8_t save_read_device_paramter_function(struct device_param *param);
int8_t save_read_com_param_function(com_param_t *param);
int8_t save_read_update_addr(uint8_t *ip,uint32_t *port);
int8_t save_read_report_switch(struct report_status *param);

// 默认参数
void save_read_default_local_network(struct local_ip_t *local);
void save_read_default_comparision_parameter(comparision_parameter_t *param);
void save_read_default_device_paramter_function(struct device_param *param);
void save_read_default_remote_ip(struct remote_ip *remote);
void save_read_default_com_param_function(com_param_t *param);

// 20230712 摄像机相关参数
int8_t save_read_carema_parameter(carema_t *param);
void save_read_default_carema_parameter(carema_t *param);
int8_t save_stroage_carema_parameter(carema_t *param);

// 20230723 阈值
int8_t save_stroage_threshold_parameter(struct threshold_params *param);
void save_read_default_threshold_parameter(struct threshold_params *param);
int8_t save_read_threshold_parameter(struct threshold_params *param);	

// 20231022 备份信息
int8_t save_stroage_backups_function(sys_backups_t *param);
int8_t save_read_backups_function(sys_backups_t *param);
void save_read_default_backups(sys_backups_t *param);


#endif
