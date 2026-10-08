#ifndef _APP_H_
#define _APP_H_

#include "bsp.h"

struct local_ip_t				// 本地网络信息
{
	uint8_t ip[4];				// 本地IP
	uint8_t mac[6]; 			// MAC地址
	uint8_t netmask[4]; 	// 掩码
	uint8_t gateway[4]; 	// 网关
	uint8_t dns[4];				// 默认DNS
	uint32_t port;
	uint8_t ping_ip[4];				// 主机需要测试的ping ip
	uint8_t ping_sub_ip[4];		// 主机需要测试的ip 2
	uint8_t server_mode;			// 服务器模式:1-有线 2-无线 4-自动 3-同时在线
	uint8_t multicast_ip[4];	// 组播IP
	uint32_t multicast_port;	// 组播端口	
	uint8_t search_mode;			// 摄像机检测模式:1-PING 2-协议  20230810
	char ip_str[20];
};

struct remote_ip				
{
	uint8_t	 outside_iporname[128];	  // 外网IP-域名
	uint32_t outside_port;			  // 外网端口
	uint8_t  client_iporname[4];// 客户端IP-域名
	uint32_t client_port;			  // 客户端端口
};

struct device_param
{
	union i_c  id;		  	 // id
	uint8_t  	 name[52];     // 设备名称
	uint8_t    password[20]; // 设备密码
	uint8_t    default_password; // 0-已修改过默认密码 1-未修改默认密码
};

/* 参数 */
typedef enum
{
	FLAG_W_E  = 0, // 水浸告警
	FLAG_W_N  = 1, // 水浸恢复
	FLAG_S  	= 2, // 防雷
	FLAG_A   	= 3, // 倾斜
	FLAG_T_H 	= 4, // 温度过高
	FLAG_T_L  = 5, // 温度过低
	FLAG_H_H  = 6, // 湿度过高
	FLAG_H_L  = 7, // 湿度过低
	FLAG_D   	= 8, // 箱门
	FLAG_P   	= 9, // 电量
	FLAG_AC   = 10, // 电源状态
	FLAG_FAN  = 11,
} REPORT_FLAG;

#define FLAG_WATER_ERROR    ((uint16_t)0x0001)  // 水浸告警
#define FLAG_WATER_NOEMAL   ((uint16_t)0x0002)  // 水浸恢复 
#define FLAG_SPD      			((uint16_t)0x0004)  // 防雷 
#define FLAG_ANGLE      		((uint16_t)0x0008)  // 倾斜  
#define FLAG_TEMP_HIGH    	((uint16_t)0x0010)  // 温度过高 
#define FLAG_TEMP_LOW     	((uint16_t)0x0020)  // 温度过低  
#define FLAG_HUMI_HIGH      ((uint16_t)0x0040)  // 湿度过高 
#define FLAG_HUMI_LOW      	((uint16_t)0x0080)  // 湿度过低 
#define FLAG_DOOR      			((uint16_t)0x0100)  // 箱门 
#define FLAG_POWER    			((uint16_t)0x0200)  // 电量 
#define FLAG_AC_STATUS     	((uint16_t)0x0400)  // 电源状态 
#define FLAG_FAN			     	((uint16_t)0x0800)  // 风扇 

#define FLAG_ADD_ALL     	  ((uint16_t)0xF001)  //  
#define FLAG_DELE_ALL     	((uint16_t)0xF002)  // 


typedef struct
{
	int8_t   JumpResult;
	int8_t   reset_num;    // 重启次数
	uint32_t jump_addr;    // 跳转地址
}run_result_t;


struct report_status {
	uint32_t report_allowed;  // 是否允许上报 
};

// 阈值检测 20230720
struct threshold_params {
	uint16_t volt_max;  // 高压
	uint16_t volt_min;  // 低压
	uint16_t current;
	uint8_t  angle;
	int8_t   temp_high;		// 
	int8_t   temp_low;	// 
	int8_t   humi_high;		// 
	int8_t   humi_low;  // 

	uint32_t  overcurrentTimes;  //  过流次数
	uint32_t  undervoltageTimes;  //  欠压次数	
	uint32_t  overvoltageTimes;  //  过压次数
	uint32_t  electricLeakageTimes;  //  漏电次数		
};

/* 参数 */
typedef struct
{
	struct local_ip_t    local;  // 本机网络参数
	struct remote_ip     remote; // 远端网络参数
	struct device_param  device; // 设备参数
//	struct other_param   param;// 其余参数
	struct report_status report; // 上报控制参数
	struct threshold_params threshold; // 阈值 20230720
	uint8_t mem;			   	 // 内存利用率
} sys_param_t;
/* 参数 */
typedef struct
{
	uint8_t config_flag;
	struct remote_ip     remote; // 远端网络参数
} sys_backups_t;


typedef struct
{
	uint8_t ip[10][4];		// 摄像头IP信息
} comparision_parameter_t;  // 对比参数

typedef struct
{
	char    name[10][20];		// 摄像头用户名
	char    pwd[10][20];		// 摄像头密码
	int     port[10];		// 摄像头端口
} carema_t;  // 摄像机参数


typedef struct
{
	uint32_t heart;			// 心跳包
	uint32_t report;		// 上报时间
	uint32_t ping;			// ping的间隔时间
	uint32_t dev_ping;		// 设备间隔ping时间
	uint8_t  network_time;  // 网络延时时间  20220308
	uint32_t fan_time;			// 风扇运行时间
	uint32_t reload;       // 设备重启时间  20240904
} com_param_t;


/* 发送结果 */
typedef enum
{
	SR_WAIT       = 0, // 发送等待
	SR_OK         = 1, // 发送完成
	SR_TIMEOUT    = 2, // 发送超时
	SR_ERROR      = 3, // 发送结束到错误提示
	SR_SEND_ERROR = 4, // 发送错误
} send_result_e;

/* 函数声明 */

/** 执行函数 **/
void app_task_function(void);
void app_sw_control_function(void);			  // 开关控制
void app_detection_collection_param(void);	  // 检测采集数据: 市电电压、市电电流、适配器1-3 、 防雷模块 、 箱门、 箱体姿态
void app_task_save_function(void);			  // 存储任务
void app_com_send_function(void);			  // 通信发送函数
void app_com_time_function(void);  // 通信计时函数
void app_sys_operate_timer_function(void);  // 继电器处理函数
void app_set_switch_control_function(uint8_t mode, uint8_t cmd);			// 设置开关状态
void app_set_com_send_flag_function(uint8_t cmd, uint8_t data);				// 设置发送标志位
void app_set_reply_parameters_function(uint8_t cmd, uint8_t error);		// 设置回复参数
void app_set_send_result_function(send_result_e data);							// 设置发送结果函数
void app_set_peripheral_switch(uint8_t cmd, uint8_t data);						// 设置外设开关状态
void app_set_sys_opeare_function(uint8_t cmd, uint8_t data);					// 设置操作任务 - 立即回发
int app_opeare_relay_function(uint8_t num, uint8_t relay,uint8_t status);
void app_set_save_infor_function(uint8_t mode);									// 设置保存信息
void app_set_erase_infor_function(uint8_t mode);								// 设置需要清除的信息
void app_set_local_network_function(struct local_ip_t param);					// 设置本地网络参数
void app_set_local_network_function_two(struct local_ip_t param);				// 存储部分网络参数
void app_set_transfer_mode_function(uint8_t mode);								// 设置传输模式
void app_set_carema_search_mode_function(uint8_t mode,uint8_t config_mode) ;  // 20230810
void app_set_remote_network_function(struct remote_ip param);					// 设置远端网络参数
void app_set_reset_function(void);												// 设备重置
void app_set_mac_reset_function(void);
void app_set_camera_function(uint8_t *ip);										// 设置摄像头IP
void app_set_camera_num_function(uint8_t *ip, uint8_t num);						// 设置指定位数摄像头IP
void app_set_device_param_function(struct device_param param);					// 设置设备参数
void app_set_next_report_time(uint16_t time);									// 设置下一次上报时间
void app_set_next_report_time_other(uint16_t time,uint8_t sel);					// 设置上报间隔时间-可选择
void app_set_next_ping_time(uint16_t time, uint8_t time_dev);					// 设置下一次ping的时间
void app_set_network_delay_time(uint8_t time_dev);// 设置网络延时时间  20220308
void app_set_current_time(int *time,uint8_t conv);								// 设置当前时间
void app_set_main_network_ping_ip(uint8_t *ip);									// 设置主PING地址
void app_set_com_interface_selection_function(uint8_t mode);					// 通信接口选择函数
void app_set_com_time_param_function(uint32_t *time,uint8_t mode) ;				// 设置通信相关时间参数:ping、上报
void app_set_report_switch_status(uint8_t sw,REPORT_FLAG sel);						// 存储上报开关状态
void app_set_camera_login_function(char *name_buf,char *pwd_buf,int port,uint8_t num);// 设置用户名、密码  20220329
void app_set_camera_id_num_function(uint8_t data);                     // 设置摄像机编号 20220329
void app_set_code_function(struct device_param param);   // 设置密码


void app_report_information_immediately(void); // 立即上报数据
void app_deal_com_flag_function(void);   // 用来处理通信发送标志
void app_deal_com_send_wait_function(void);  // 发送等待处理任务
void app_sys_operate_timer_function(void);  // 继电器处理函数
void app_send_once_heart_infor(void);							// 立刻发送一次心跳信息


uint8_t app_get_com_send_status_function(void);  // 获取当前通信状态
void app_get_storage_param_function(void);	// 用于获取存储的参数的函数
void *app_get_local_network_function(void);  					// 获取本机网络信息
void *app_get_remote_network_function(void); 					// 获取远端网络信息
void *app_get_backups_function(void);             // 20231022
int8_t app_get_camera_function(uint8_t *ip, uint8_t num);		// 获取指定摄像头ip
void *app_get_device_param_function(void);						// 获取设备参数
uint16_t app_get_com_heart_time(void);							// 获取心跳
void app_get_current_time(char *time);							// 获取当前时间
void *app_get_current_times(void);									// 获取当前时间
uint8_t* app_get_report_current_time(uint8_t mode);
uint32_t app_get_next_ping_time(void);							// 下次ping的时间
uint32_t app_get_next_dev_ping_time(void);						// 获取下一个设备ping的时间
uint32_t app_get_report_time(void);								// 上报时间
uint8_t app_get_network_delay_time(void) ;  // 获取网络延时时间 20220308

void app_get_main_network_ping_ip_addr(uint8_t* ip);			// 获取主网ip1
void app_get_main_network_sub_ping_ip_addr(uint8_t* ip);		// 获取主网pingip地址 2
void *app_get_com_time_infor(void);								// 获取通信间隔时间
void *app_get_comparision_param(void);							// 获取摄像头参数
uint8_t app_get_network_mode(void);								// 获取网络模式
uint8_t app_get_carema_search_mode(void);         // 获取搜索协议  20230810
void app_get_report_switch_status(uint8_t *sw);					// 获取上报开关状态
void app_get_network_connect_status(char *buff);				// 获取网络连接状态
uint8_t *app_get_device_name(void);
uint8_t app_get_vlot_protec_status(void);
uint8_t app_get_current_status(void);							// 获取电流状态
int8_t app_get_camera_login_function(char *name_buf,char *pwd_buf,uint8_t num); //获取用户名、密码 20220329
int app_get_camera_port_function(uint8_t num);					//获取摄像机端口 20220329
int8_t app_get_camera_num_function(void); 							// 获取摄像机ID 20220329


void app_detect_function(void);
int8_t app_match_local_camera_ip(uint8_t *ip);					// 匹配摄像头地址
int8_t app_match_password_function(char *password);				// 密码校验函数


void app_set_lwip_reset_status(uint8_t sta); // 设置网络复位标志位
uint8_t app_get_lwip_reset_status(void);

// 20230720 阈值
void app_set_threshold_param_function(struct threshold_params param); 
void *app_get_threshold_param_function(void);        
uint16_t app_get_fault_code_function(void);

int8_t app_match_password_function(char *password); // 密码比较函数
int8_t app_match_set_code_function(void);  // 确认是否需要需要修改默认密码

void app_server_link_status_function(void); // 20231022
void app_save_backups_remote_param_function(void); // 20231024
void app_open_exec_task_function(void);
void app_power_fail_protection_function(void);
void app_power_open_protection_function(void);


void app_set_fan_humi_param_function(uint8_t *data);
void app_set_fan_param_function(int8_t *data);
void app_set_vol_current_param(uint16_t *data);
void app_send_data_task_function(void);

void app_set_http_websocket_status_function(uint8_t status);
void app_set_http_com_status_function(uint32_t status,uint8_t id);   // 设置当前http状态
void app_http_com_send_function(void);

void app_set_cilent_network_function(struct remote_ip param);
void *app_get_device_uuid(void);
void app_fan_timer_function(void);
uint32_t app_get_fan_time(uint8_t id);

void my_app_run_param_init(void);
void app_sys_operate_relay(void);

uint32_t app_get_device_reload_time(void);


#endif
