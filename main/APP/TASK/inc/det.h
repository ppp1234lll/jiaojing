#ifndef _DET_H_
#define _DET_H_
#include "bsp.h"

#define DET_ADAPTER_DATA (12)

typedef enum
{
	DO_CURRENT1 = 0, // 电流采集1
	DO_CURRENT2 = 1, // 电流采集2
	DO_CURRENT3 = 2, // 电流采集3
	DO_CURRENT4 = 3, // 电流采集4
	DO_LIGHT  	= 4, // 外部补光灯
} DET_OUTSIDE;    	 // 外部采集数据

typedef enum
{
	DW_LIGHT = 0, // 补光灯工作状态
	
} DET_WORK_STATUS;

typedef struct {
	int32_t AcceX;				/*acceleration x*/
	int32_t AcceY;				/*acceleration y*/
	int32_t AcceZ;				/*acceleration z*/
} MENS_XYZ_STATUS_T;

typedef struct
{
	float temp_inside; 	 	// 内部温度值
	float humi_inside; 	 	// 内部湿度值
	
	double attitude_acc; 	// 加速度

	float vin220v;		 		// 220V电压检测
	float current[8]; 		// 电流检测
	float total_current; 	// 总电流检测
	float total_power;  	// 总功率1
	float power[8]; 			// 功率
	float total_electricity;  	// 总用电量
	float electricity[8]; 			// 用电量
	
	uint8_t open_door;	// 开门检测
	uint8_t vinin;		 	// 市电输入监测
	uint8_t spd;	 			// 防雷检测
	uint8_t sim;	 			// SIM检测
	uint8_t bat;	 			// 电池检测
	uint8_t water;	 		// 浸水检测

	uint8_t camera[10];	 // 摄像机状态x3：0；离线 1：在线，2：延时严重
	uint8_t main_ip;	 // 主网络状态：0：离线 1：在线，2：延时严重
	uint8_t main_sub_ip; // 主网络状态sub: 0：离线 1：在线，2：延时严重
	uint8_t ping_status;     // ping结束标志位
} data_collection_t;


void det_task_function(void);

void det_get_key_status_function(void);
uint8_t det_main_network_and_camera_network(void);
void det_get_temphumi_function(void);
float lean_check(MENS_XYZ_STATUS_T *acc_xyz);
void det_get_attitude_state_value(void);
fp32 det_get_inside_temp(void);								// 获取内部温度
fp32 det_get_inside_humi(void); 							// 获取内部湿度
fp32 det_get_vin220v_handler(uint8_t num);					// 获取220v采集数据
fp32 det_get_power_handler(uint8_t num);

void det_set_open_door(uint8_t mode);  // 设置箱门状态
void det_set_220v_in_function(uint8_t status); 			    // 设置市电状态
void det_set_camera_status(uint8_t num,uint8_t status);		// 设置摄像机状态
void det_set_main_network_status(uint8_t status);			// 设置主网络状态
void det_set_main_network_sub_status(uint8_t status);		// 设置主网络状态 - 2
void det_set_total_energy(uint8_t num,float data);			// 设置电量参数
void det_set_ping_status(uint8_t status);
void det_set_spd_status(uint8_t mode);   //  设置防雷开关状态
void det_set_sim_status(uint8_t mode);   //  设置SIM卡状态
void det_set_bat_status(void);
void det_set_water_status(uint8_t mode);


uint8_t det_get_open_door(void);							// 获取箱门状态
uint16_t det_get_cabinet_posture(void);						// 获取箱体姿态
uint8_t det_get_220v_in_function(void);						// 获取市电状态参数
uint8_t det_get_camera_status(uint8_t num);					// 获取摄像机状态
uint8_t det_get_main_network_status(void);					// 获取主网络状态
uint8_t det_get_main_network_sub_status(void);				// 获取主网络状态 - 2
uint8_t det_get_spd_status(void);	   //  获取防雷开关状态
uint8_t det_get_sim_status(void);    //  获取SIM卡状态
uint8_t det_get_battery_status(void);
uint8_t det_get_water_status(void);
void *det_get_collect_data(void);

#endif
