#include "app.h"
#include "includes.h"
#include "bsp.h"
#include "led.h"
#include "save.h"
#include "det.h"
#include "appconfig.h"
#include "relay.h"
#include "malloc.h"
#include "com.h"
#include "eth.h"
#include "tcp_client.h"
#include "tcp_server.h"
#include "rtc.h"
#include "gsm.h"
#include "update.h"
#include "bootload.h"
#include "lfs_port.h"
#include "onvif.h"
#include "IWDG.h"
#include "fan.h"
#include "http_com.h"
#include "start.h"

#include "GPRS.h"

/* 发送状态 */
#define SEND_STATUS_NO		(0) // 当前没有发送
#define SEND_STATUS_SENGING (1) // 当前正在发送
#define SEND_STATUS_RESULT  (2) // 当前发送有了结果

#define SERVER_LINK_TIME  (30000) // 服务器连接时间  5min = 30000ms/10ms

typedef struct
{
	struct
	{
		send_result_e send_result;   // 发送结果
		uint32_t 	  heart_time;    // 心跳计时
		uint32_t	  report_time;	 // 上报时间
		uint16_t 	  send_time;	 // 发送计时
		uint8_t  	  send_mode;	 // send mode:0-LWIP 1-GPRS
		uint8_t  	  repeat;		 // 重复次数: 针对发送
		uint8_t  	  send_status;   // 发送状态: 0-当前没有发送 1-当前有发送 2-发送有结果
		uint8_t		  send_cmd;  	 // 发送内容，直接使用对应命令字
		
		uint8_t 	  return_cmd; 	 // 回复标志
		uint8_t 	  return_error;	 // 回复内容
		
		uint16_t 	  fault_code;	 // 故障码
	} com;
	struct
	{
		uint8_t save_comparision;	 		// 存储外设相关数据
		uint8_t save_other_param; 	 	// 存储其余参数
		uint8_t save_device_param;   	// 存储设备参数
		uint8_t save_local_network;  	// 保存本地网络参数
		uint8_t save_remote_network; 	// 保存远端网络参数
		uint8_t save_update_addr;    	// 保存更新地址
		uint8_t com_parameter;			 	// 通信相关参数
		uint8_t save_report_sw;		 		// 存储上报开关参数
    uint8_t save_carema;       		// 摄像头参数 20230712
		uint8_t save_threshold;       // 阈值
		uint8_t save_reset;		     		// 恢复出厂化
		
		uint8_t erase_local_network; 	// 将本地网络参数恢复默认值
		uint8_t erase_local_mac; 			// 将mac地址恢复默认值
		uint8_t erase_report_time;	 	// 将上报实际恢复默认值
		uint8_t erase_camera_ip;	 		// 删除摄像机IP
		uint8_t erase_main_ip;		 		// 删除主机检测ip
		uint8_t erase_server_ip;     	// 擦除服务器IP
		uint8_t erase_ping_time;	 		// 还原ping间隔时间
		uint8_t erase_tran_mode;     	// 还原传输模式
		uint8_t erase_netdelay_time;	// 还原网络延时时间  20220308
		uint8_t erase_ipc_login;	  	// 还原IPC登录信息  20220329
	} save_flag;
	struct
	{
		uint8_t report_normally;	   // 正常上报
		uint8_t query_configuration; // 查询配置上传
		uint8_t heart_pack;			     // 心跳包
		uint8_t version;			       // 版本信息
		uint8_t config_return;		   // 配置回复
		uint8_t ipc_ip;              // IP查询			20220329
		uint8_t ipc_info;					   // 参数查询   20220329
		uint8_t lbs_info;					   // 参数查询   20220329
	} com_flag;
	struct
	{
		uint32_t ack_msg;	 	// 回复报文 
		uint8_t http_to_web;	// http转websocket
		uint8_t web_status;  // 转换结果
		uint8_t proactive;   // 主动发起websocket
		uint8_t client1_id;  // 当前通信的客户端编号
		uint8_t client2_id;
		uint8_t client3_id;
	} http_com_flag;
	struct
	{
		uint8_t caramer_num;      // 摄像机编号  20220329
		uint8_t adapter_num;		  // 适配器 - 编号
		uint8_t current_protection;	  // 电流保护
		uint8_t volt_protection;	  // 电压保护
	} sys;
	struct
	{
		uint8_t lwip_reset;         // 网络重启标志
		uint8_t adapter_reset;		  // 平台
		uint8_t relay_reset[8];		    // HTTP通信用
	} sys_flag;
	struct
	{
		char uuid[40];         // 设备UUID
	} uuid_t;
}sys_operate_t;

struct switch_control_t {
	uint8_t adapter1; // 适配器1
	uint8_t adapter2; // 适配器2
	uint8_t adapter3; // 适配器3
	uint8_t adapter4; // 适配器4
	uint8_t adapter5; // 适配器5
	uint8_t adapter6; // 适配器6
	uint8_t adapter7; // 适配器7
	uint8_t adapter8; // 适配器8
};

/* 参数定义 */
sys_operate_t 	sg_sysoperate_t ;// 系统操作参数：包括通信、存储、计时 
comparision_parameter_t sg_comparisionparam_t = {0}; // 摄像头
sys_backups_t 	sg_backups_t   	= {0}; 	// 备份信息 20231022
sys_param_t   	sg_sysparam_t   = {0}; 	// 系统参数：本地、远端、设备、其它（电话）参数、上报相关参数
carema_t   			sg_carema_param_t;   		// onvif 摄像头信息

com_param_t	 		sg_comparam_t	 	= {
	90000,
	60000,
	60000,
	60000,
	200,    // 网络延时时间  20220308
	0,      // 风扇
	172800, // 重启时间       20240904
}; 				// 通信参数：心跳、上报、ping间隔时间
uint8_t				  sg_send_buff[1024]  = {0}; // 发送缓存区
uint16_t				sg_send_size		  	= 0;	 // 发送数据长度

uint8_t				  sg_websocket_send_buff[1024]  = {0}; // 发送缓存区
uint16_t				sg_websocket_send_size		  	= 0;	 // 发送数据长度
uint8_t				  sg_http_send_buff[4096]  = {0}; // 发送缓存区
uint16_t				sg_http_send_size		  	= 0;	 // 发送数据长度
rtc_time_t		  sg_rtctime_t		  	= {0}; // rtc采集间隔时间
struct switch_control_t sg_swcontrol_t = {0};
uint32_t fan_current_time =0;  // 风扇当前运行时间

/************************************************************
*
* Function name	: app_task_function
* Description	: 应用程序主任务
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_task_function(void)
{
	uint8_t get_time_cnt = 0;
	
	/* 开启系统指示灯 */
	led_control_function(LD_STATE,LD_FLICKER);
	start_pack_device_uuid_str(sg_sysoperate_t.uuid_t.uuid);
	for(;;)
	{
//		app_detection_collection_param(); 	// 采集数据监测任务
		app_task_save_function();   				// 存储相关任务
		com_deal_http_info_function();		// 单服务器: 仅 1 路 HTTP/WebSocket 处理
		com_deal_main_function(); 					// 处理接收数据
		app_com_send_function();						// 通信发送
		app_server_link_status_function();
		app_open_exec_task_function();
		app_sys_operate_relay();
		get_time_cnt++;
		if(get_time_cnt>100)
		{
			get_time_cnt = 0;
			RTC_Get_Time(&sg_rtctime_t);		/* 时间获取 */
			my_modem_detcet_update_status_function();
			sg_sysparam_t.mem = mem_perused(SRAMIN);
		}
		IWDG_Feed();	
		OSTimeDly(2);  			/* 2 tick = 10ms @200Hz, 按节拍显式延时, 避免 ms->tick 换算陷阱 */
	}
}

/************************************************************
*
* Function name	: app_set_switch_control_function
* Description	: 设置开关状态
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_switch_control_function(uint8_t mode, uint8_t cmd)
{
	switch(mode) 
	{
		case 1:	sg_swcontrol_t.adapter1 = cmd;	break;
		case 2:	sg_swcontrol_t.adapter2 = cmd;	break;
		case 3:	sg_swcontrol_t.adapter3 = cmd;	break;
		case 4:	sg_swcontrol_t.adapter4 = cmd;	break;
		case 5:	sg_swcontrol_t.adapter5 = cmd;	break;
		case 6:	sg_swcontrol_t.adapter6 = cmd;	break;
		case 7:	sg_swcontrol_t.adapter7 = cmd;	break;
		case 8:	sg_swcontrol_t.adapter8 = cmd;	break;
	}
}

/************************************************************
*
* Function name	: app_detection_collection_param
* Description	: 检测采集数据: 市电电压、市电电流、适配器1-3 、 防雷模块 、 箱门、 箱体姿态
* Parameter		: 
* Return		: 
*	status_error -> 0:高压  1:低压  2:电流  3:姿态  4:箱门 5:断电  6:适配器  7:SPD
8:SIM   9:电池  10:网口  11:浸水
************************************************************/
void app_detection_collection_param(void)
{
	static uint32_t status_error	= 0;   // 故障上报状态
	static uint32_t status_normal	= 0;   // 正常上报状态：
	
	/* 适配器、断电上报 */
	if(det_get_220v_in_function() == 0) // 12V断电
	{
		if(det_get_vin220v_handler(0) < 50)  // 市电电压 < 50V，说明断电
		{
			if( (status_error & 0x20) == 0) 
			{
				status_error |= 0x20;
				status_normal &=~0x20;
				if(DIRECT_REP_DEBUG) printf("report_duandian.....1 \n");
				app_report_information_immediately();
				if((sg_sysparam_t.report.report_allowed & FLAG_AC_STATUS) == FLAG_AC_STATUS)
				{
					http_websocket_pack_data((char*)sg_websocket_send_buff,&sg_websocket_send_size,FLAG_AC_STATUS,1);// 断电
					tcp_cilent_send_buff(sg_websocket_send_buff,sg_websocket_send_size,1);	
				}
			}				
		}
		else 					 // 市电电压 > 50V，说明适配器故障
		{
			if((status_error & 0x40) == 0) 
			{
				OSTimeDlyHMSM(0,0,0,100);
				status_error  |= 0x40;
				status_normal &=~0x40;
				if(DIRECT_REP_DEBUG) printf("report_shipeiqi.....1 \n");
				app_report_information_immediately();
			}
		}
	}		
	else   // 12V有电，220V存在 
	{
		if(((status_normal & 0x20) == 0) || ((status_normal&0x40) == 0))
		{
			if((status_error&0x20) || (status_error&0x40))// 适配器故障、220断电
			{
				/* 适配器重新上电 - 重启设备*/
				lfs_unmount(&g_lfs_t);
				OSTimeDlyHMSM(0,0,0,100);
				System_SoftReset();
			}
			OSTimeDlyHMSM(0,0,0,100);
			status_normal |= 0x20;
			status_error  &=~0x20;
			
			status_normal |= 0x40;
			status_error &=~ 0x40;
			if(DIRECT_REP_DEBUG) printf("report_immediately.....2 \n");
			app_report_information_immediately();
			app_power_open_protection_function();  // 打开继电器
		}		
	}

	/* 运维网口 */
	if(eth_get_network_cable_status() == 0) 
	{
		if((status_error & 0x0400) == 0) 
		{
			status_error  |= 0x0400;
			status_normal &=~0x0400;
			if(DIRECT_REP_DEBUG) printf("report_lan.....18 \n");
			app_report_information_immediately();
		}
	} 
	else
	{
		if((status_normal & 0x0400) == 0) 
		{
			status_normal |= 0x0400;
			status_error  &=~0x0400;
			if(DIRECT_REP_DEBUG) printf("report_lan.....19 \n");
			app_report_information_immediately();
		}	
	}		
	
	/* 检测主网与摄像头是否发送状态变化 */
	if(det_main_network_and_camera_network() == 1) 
	{
		if(DIRECT_REP_DEBUG) printf("report_network.....3 \n");
		app_report_information_immediately();
	}
		
	// 高压报警
	if( sg_sysparam_t.threshold.volt_max == 0 )  // 阈值为0，不作处理
	{	}
	else
	{
		if(det_get_vin220v_handler(0) >= sg_sysparam_t.threshold.volt_max) 
		{	
			if((status_error & 0x01) == 0)  // 故障上报标志位是0
			{
				OSTimeDlyHMSM(0,0,0,100);
				status_error |= 0x01;        // 标志位置1，表示已上报
				status_normal &=~ 0x01;      // 正常上报标志位清0		
				sg_sysoperate_t.sys.volt_protection = 2;
				if(DIRECT_REP_DEBUG) printf("report_volt_max.....4 \n");
				app_report_information_immediately();	
				sg_sysparam_t.threshold.overvoltageTimes++; // 过压次数
				if((sg_sysparam_t.report.report_allowed & FLAG_POWER) == FLAG_POWER)
				{
					http_websocket_pack_data((char*)sg_websocket_send_buff,&sg_websocket_send_size,FLAG_POWER,1);// 电压高
					tcp_cilent_send_buff(sg_websocket_send_buff,sg_websocket_send_size,1);	
				}
			}
		}
		else if(det_get_vin220v_handler(0) >= 50) // 市电有电情况下
		{
			if((status_normal & 0x01) == 0)  // 正常上报标志位是0
			{
				OSTimeDlyHMSM(0,0,0,100);
				status_normal |= 0x01;  // 标志位置1，表示已上报
				status_error &=~ 0x01; // 故障上报标志位清0
				sg_sysoperate_t.sys.volt_protection = 0;
				if(DIRECT_REP_DEBUG) printf("report_volt_max.....5 \n");
				app_report_information_immediately();
				app_power_open_protection_function();  // 打开继电器
			}
		}	
	}

	// 低压报警
	if( sg_sysparam_t.threshold.volt_min == 0 )  // 阈值为0，不作处理
	{	}
	else
	{
		if((det_get_vin220v_handler(0) <= sg_sysparam_t.threshold.volt_min) && \
			  det_get_vin220v_handler(0) >= 20)
		{	
			if((status_error & 0x02) == 0)  // 故障上报标志位是0
			{
				OSTimeDlyHMSM(0,0,0,100);
				status_error |= 0x02;        // 标志位置1，表示已上报
				status_normal &=~ 0x02;      // 正常上报标志位清0		
				if(DIRECT_REP_DEBUG) printf("report_volt_min.....12 \n");
				sg_sysparam_t.threshold.undervoltageTimes++; // 欠压次数
				if((sg_sysparam_t.report.report_allowed & FLAG_POWER) == FLAG_POWER)
				{
					http_websocket_pack_data((char*)sg_websocket_send_buff,&sg_websocket_send_size,FLAG_POWER,0);// 电压低
					tcp_cilent_send_buff(sg_websocket_send_buff,sg_websocket_send_size,1);		
				}
			}
		}
		else if(det_get_vin220v_handler(0) >= sg_sysparam_t.threshold.volt_min) // 市电有电情况下
		{
			if((status_normal & 0x02) == 0)  // 正常上报标志位是0
			{
				OSTimeDlyHMSM(0,0,0,100);
				status_normal |= 0x02;  // 标志位置1，表示已上报
				status_error &=~ 0x02; // 故障上报标志位清0
				sg_sysoperate_t.sys.volt_protection = 0;
				if(DIRECT_REP_DEBUG) printf("report_volt_min.....13 \n");
				app_power_open_protection_function();  // 打开继电器
			}
		}	
	}
	
	/* 检测市电电流使用情况 */
	if( sg_sysparam_t.threshold.current == 0 )  // 阈值为0，不作处理
	{	}
	else
	{
		if(det_get_vin220v_handler(1) >= sg_sysparam_t.threshold.current)
		{
			if( (status_error & 0x04) == 0) 
			{
				status_error |= 0x04;
				status_normal &=~ 0x04;
				sg_sysoperate_t.sys.current_protection = 1;
				if(DIRECT_REP_DEBUG) printf("report_current.....6 \n");
				app_report_information_immediately();
				if((sg_sysparam_t.report.report_allowed & FLAG_POWER) == FLAG_POWER)
				{
					http_websocket_pack_data((char*)sg_websocket_send_buff,&sg_websocket_send_size,FLAG_POWER,2);// 电流高
					tcp_cilent_send_buff(sg_websocket_send_buff,sg_websocket_send_size,1);	
				}
			}
		} 
		else if(det_get_vin220v_handler(1) >= 30)
		{
			if( (status_normal & 0x04) == 0)
			{
				status_normal |= 0x04;
				status_error &=~ 0x04;
				sg_sysoperate_t.sys.current_protection = 0;
				if(DIRECT_REP_DEBUG) printf("report_current.....7 \n");
				app_report_information_immediately();
			}
		}
	}
	
	/* 箱体姿态 */
	if( det_get_cabinet_posture() >= sg_sysparam_t.threshold.angle) 
	{
		if( (status_error & 0x08) == 0) 
		{
			status_error  |= 0x08;
			status_normal &=~0x08;
			if(DIRECT_REP_DEBUG) printf("report_angle.....8 \n");
			app_report_information_immediately();
			if((sg_sysparam_t.report.report_allowed & FLAG_ANGLE) == FLAG_ANGLE)
			{
				http_websocket_pack_data((char*)sg_websocket_send_buff,&sg_websocket_send_size,FLAG_ANGLE,0);
				tcp_cilent_send_buff(sg_websocket_send_buff,sg_websocket_send_size,1);	
			}
		}
	} 
	else 
	{
		if((status_normal&0x08) == 0) 
		{
			status_normal |= 0x08;
			status_error  &=~0x08;
			if(DIRECT_REP_DEBUG) printf("report_angle.....9 \n");
			app_report_information_immediately();
		}
	}

	/* 箱门	*/
	if( det_get_open_door() == 1) 
	{
		if((status_error & 0x10) == 0) 
		{
			status_error  |= 0x10;
			status_normal &=~0x10;
			if(DIRECT_REP_DEBUG) printf("report_door.....10 \n");
			app_report_information_immediately();
			if((sg_sysparam_t.report.report_allowed & FLAG_DOOR) == FLAG_DOOR)
			{
				http_websocket_pack_data((char*)sg_websocket_send_buff,&sg_websocket_send_size,FLAG_DOOR,0);
				tcp_cilent_send_buff(sg_websocket_send_buff,sg_websocket_send_size,1);	
			}
		}
	} 
	else 
	{
		if((status_normal & 0x10) == 0) 
		{
			status_normal |= 0x10;
			status_error  &=~0x10;
			if(DIRECT_REP_DEBUG) printf("report_door.....11 \n");
			app_report_information_immediately();
		}
	}

	/* 防雷模块 */
#ifdef SPD_ENABLE
	if( det_get_spd_status() == 2 ) 
	{
		if((status_error & 0x80) == 0) 
		{
			status_error  |= 0x80;
			status_normal &=~0x80;
			if(DIRECT_REP_DEBUG) printf("report_spd.....14 \n");
			app_report_information_immediately();
			if((sg_sysparam_t.report.report_allowed & FLAG_SPD) == FLAG_SPD)
			{
				http_websocket_pack_data((char*)sg_http_send_buff,&sg_http_send_size,FLAG_SPD,0);
				tcp_server_set_send_buff(sg_http_send_buff,sg_http_send_size);	
			}
		}
	} 
	else if( det_get_spd_status() == 1)
	{
		if((status_normal & 0x80) == 0) 
		{
			status_normal  |= 0x80;
			status_error &=~0x80;
			if(DIRECT_REP_DEBUG) printf("report_spd.....15 \n");
			app_report_information_immediately();
			if((sg_sysparam_t.report.report_allowed & FLAG_SPD) == FLAG_SPD)
			{
				http_websocket_pack_data((char*)sg_http_send_buff,&sg_http_send_size,FLAG_SPD,0);
				tcp_server_set_send_buff(sg_http_send_buff,sg_http_send_size);			
			}
		}	
	}	
#endif
	
	/* 浸水检测模块 */
#ifdef WATER_ENABLE
	if(det_get_water_status() == 2)
	{
		if((status_error & 0x0800) == 0) 
		{
			status_error  |= 0x0800;
			status_normal &=~0x0800;
			if(DIRECT_REP_DEBUG) printf("report_water.....18 \n");
			app_report_information_immediately();
			if((sg_sysparam_t.report.report_allowed & FLAG_WATER_ERROR) == FLAG_WATER_ERROR)
			{
				http_websocket_pack_data((char*)sg_websocket_send_buff,&sg_websocket_send_size,FLAG_WATER_ERROR,0);
				tcp_cilent_send_buff(sg_websocket_send_buff,sg_websocket_send_size,1);		
			}
		}
	} 
	else if( det_get_water_status() == 1)
	{
		if((status_normal & 0x0800) == 0) 
		{
			status_normal |= 0x0800;
			status_error  &=~0x0800;
			if(DIRECT_REP_DEBUG) printf("report_water.....19 \n");
			app_report_information_immediately();
			if((sg_sysparam_t.report.report_allowed & FLAG_WATER_NOEMAL) == FLAG_WATER_NOEMAL)
			{
				http_websocket_pack_data((char*)sg_websocket_send_buff,&sg_websocket_send_size,FLAG_WATER_NOEMAL,0);
				tcp_cilent_send_buff(sg_websocket_send_buff,sg_websocket_send_size,1);	
			}
		}	
	}	
#endif		
	
	if(det_get_inside_temp() < sg_sysparam_t.threshold.temp_low)
	{
		if((status_error & 0x1000) == 0) 
		{
			status_error  |= 0x1000;
			status_normal &=~0x1000;
			status_error  &=~0x2000;
			if(DIRECT_REP_DEBUG) printf("report_temp.....30 \n");
			if((sg_sysparam_t.report.report_allowed & FLAG_TEMP_LOW) == FLAG_TEMP_LOW)
			{
				http_websocket_pack_data((char*)sg_websocket_send_buff,&sg_websocket_send_size,FLAG_TEMP_LOW,0);
				tcp_cilent_send_buff(sg_websocket_send_buff,sg_websocket_send_size,1);	
			}
		}			
	}
	else if(det_get_inside_temp() > sg_sysparam_t.threshold.temp_high)
	{
		if((status_error & 0x2000) == 0) 
		{
			status_error  |= 0x2000;
			status_normal &=~0x1000;
			status_error  &=~0x1000;
			if(DIRECT_REP_DEBUG) printf("report_temp.....31 \n");
			if((sg_sysparam_t.report.report_allowed & FLAG_TEMP_HIGH) == FLAG_TEMP_HIGH)
			{
				http_websocket_pack_data((char*)sg_websocket_send_buff,&sg_websocket_send_size,FLAG_TEMP_HIGH,0);
				tcp_cilent_send_buff(sg_websocket_send_buff,sg_websocket_send_size,1);	
			}
		}			
	}
	else
	{
		if((status_normal & 0x1000) == 0) 
		{
			status_normal |= 0x1000;
			status_error  &=~0x1000;
			status_error  &=~0x2000;
		}			
	}

	if(det_get_inside_humi() < sg_sysparam_t.threshold.humi_low)
	{
		if((status_error & 0x4000) == 0) 
		{
			status_error  |= 0x4000;
			status_normal &=~0x4000;
			status_error  &=~0x8000;
			if(DIRECT_REP_DEBUG) printf("report_temp.....30 \n");
			if((sg_sysparam_t.report.report_allowed & FLAG_HUMI_LOW) == FLAG_HUMI_LOW)
			{
				http_websocket_pack_data((char*)sg_websocket_send_buff,&sg_websocket_send_size,FLAG_HUMI_LOW,0);
				tcp_cilent_send_buff(sg_websocket_send_buff,sg_websocket_send_size,1);	
			}
		}			
	}
	else if(det_get_inside_humi() > sg_sysparam_t.threshold.humi_high)
	{
		if((status_error & 0x8000) == 0) 
		{
			status_error  |= 0x8000;
			status_normal &=~0x4000;
			status_error  &=~0x4000;
			if(DIRECT_REP_DEBUG) printf("report_temp.....31 \n");
			if((sg_sysparam_t.report.report_allowed & FLAG_HUMI_HIGH) == FLAG_HUMI_HIGH)
			{
				http_websocket_pack_data((char*)sg_websocket_send_buff,&sg_websocket_send_size,FLAG_HUMI_HIGH,0);
				tcp_cilent_send_buff(sg_websocket_send_buff,sg_websocket_send_size,1);	
			}
		}			
	}
	else
	{
		if((status_normal & 0x4000) == 0) 
		{
			status_normal |= 0x4000;
			status_error  &=~0x4000;
			status_error  &=~0x8000;
		}			
	}
	if((fan_get_status_function(FAN_1) == FAN_ON)||(fan_get_status_function(FAN_2) == FAN_ON))
	{
		if((status_error & 0x10000) == 0) 
		{
			status_error  |= 0x10000;
			status_normal &=~0x10000;
			if(DIRECT_REP_DEBUG) printf("report_fan..\n");
			if((sg_sysparam_t.report.report_allowed & FLAG_FAN) == FLAG_FAN)
			{
				http_websocket_pack_data((char*)sg_websocket_send_buff,&sg_websocket_send_size,FLAG_FAN,0);
				tcp_cilent_send_buff(sg_websocket_send_buff,sg_websocket_send_size,1);		
			}
		}
	}
	else
	{
		if((status_normal & 0x10000) == 0) 
		{
			status_normal |= 0x10000;
			status_error  &=~0x10000;
			if(DIRECT_REP_DEBUG) printf("report_fan..\n");
			app_set_save_infor_function(SAVE_COM_PARAMETER);
			if((sg_sysparam_t.report.report_allowed & FLAG_FAN) == FLAG_FAN)
			{
				http_websocket_pack_data((char*)sg_websocket_send_buff,&sg_websocket_send_size,FLAG_FAN,0);
				tcp_cilent_send_buff(sg_websocket_send_buff,sg_websocket_send_size,1);		
			}
		}	
	}	
}

/************************************************************
*
* Function name	: app_set_com_send_flag_function
* Description	: 设置发送函数
* Parameter		: 
* Return		: 
*	修改：增加摄像机ID号  20220329	
************************************************************/
void app_set_com_send_flag_function(uint8_t cmd, uint8_t data)
{
	switch(cmd)
	{
		case CR_QUERY_CONFIG:
			sg_sysoperate_t.com_flag.query_configuration = 1;
			break;
		case CR_QUERY_INFO:
			sg_sysoperate_t.com_flag.report_normally = 1;
			break;
		case CR_QUERY_SOFTWARE_VERSION:
			sg_sysoperate_t.com_flag.version = 1;
			break;
	
		case CR_QUERY_IPC_IP:          // 查询IPC IP地址 20220329
			sg_sysoperate_t.com_flag.ipc_ip = 1;
			break;
		case CR_QUERY_IPC_INFO:          // 查询IPC信息 20220329
			app_set_camera_id_num_function(data);
			sg_sysoperate_t.com_flag.ipc_info = 1;
			break;
		case CR_QUERY_LBS_INFO:          // 查询LBS信息
			sg_sysoperate_t.com_flag.lbs_info = 1;
			break;
	}
}

/************************************************************
*
* Function name	: app_set_reply_parameters_function
* Description	: 设置回复参数
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_reply_parameters_function(uint8_t cmd, uint8_t error)
{
	sg_sysoperate_t.com.return_cmd 		     = cmd;
	sg_sysoperate_t.com.return_error       = error;
	sg_sysoperate_t.com_flag.config_return = 1;
}

/************************************************************
*
* Function name	: app_report_information_immediately
* Description	: 立即上报信息 - 主要针对于出现异常数据时的上报
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_report_information_immediately(void) 
{
	if ( eth_get_tcp_status() != 2 && gsm_get_network_connect_status_function() == 0)
	{
		/* 网络异常，存储本次数据到本地 */
		sg_sysoperate_t.com_flag.report_normally = 1;
		return;
	}
	/* 发送正常上报数据 */
	memset(sg_send_buff,0,sizeof(sg_send_buff));
	com_report_normally_function(sg_send_buff,&sg_send_size,CR_QUERY_INFO);
	/* 设置发送参数 */
	sg_sysoperate_t.com.send_cmd = CR_QUERY_INFO;
	sg_sysoperate_t.com.repeat   = 0; 		// 重启一次正常上报计时   
	
	/* 数据发送 */
	if(sg_sysoperate_t.com.send_cmd != 0)
	{
		app_send_data_task_function();	
		sg_sysoperate_t.com.send_status = SEND_STATUS_SENGING;
		return ;								 // 如果检测到需要重复发送，则不进行下一次发送
	}
}

/************************************************************
*
* Function name	: app_deal_com_flag_function
* Description	: 用来处理通信发送标志
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_deal_com_flag_function(void)
{
	/* 数据发送 */
	if(sg_sysoperate_t.com.send_cmd != 0)
	{
		app_send_data_task_function();
		sg_sysoperate_t.com.send_status = SEND_STATUS_SENGING;		/* 转换为正在发送 */
		return ;								 // 如果检测到需要重复发送，则不进行下一次发送
	}
	
	if(sg_sysoperate_t.com_flag.heart_pack == 1)	/* 检测心跳发送 */
	{
		sg_sysoperate_t.com_flag.heart_pack = 0;
		com_heart_pack_function(sg_send_buff,&sg_send_size);		/* 发送心跳 */
		sg_sysoperate_t.com.send_cmd    = COM_HEART_UPDATA;		/* 设置发送参数 */
		sg_sysoperate_t.com.heart_time  = 0; 	 // 重启一次心跳计时
	}
	
	/* 立即上报设备状态 */
	if(sg_sysoperate_t.com_flag.report_normally == 1)
	{
		sg_sysoperate_t.com_flag.report_normally = 0;
		memset(sg_send_buff,0,sizeof(sg_send_buff));
		com_report_normally_function(sg_send_buff,&sg_send_size,CR_QUERY_INFO);		/* 发送正常上报数据 */
		/* 设置发送参数 */
		sg_sysoperate_t.com.send_cmd = CR_QUERY_INFO;
		sg_sysoperate_t.com.repeat   = 0; 		// 重启一次正常上报计时   
	}
	
	/* 直接发送，不需要检测回传 */
	/* 查询配置当前参数设置 */
	if(sg_sysoperate_t.com_flag.query_configuration == 1)
	{
		sg_sysoperate_t.com_flag.query_configuration = 0;
		/* 发送查询配置 */
		memset(sg_send_buff,0,sizeof(sg_send_buff));
		com_query_configuration_function(sg_send_buff,&sg_send_size);
		app_send_data_task_function();
	}
	
	/* 上报软件版本信息 */
	if(sg_sysoperate_t.com_flag.version == 1)
	{
		sg_sysoperate_t.com_flag.version = 0;
		/* 发送正常上报数据 */
		com_version_information(sg_send_buff,&sg_send_size);
		app_send_data_task_function();
	}
	/* 回传信号 */
	if(sg_sysoperate_t.com_flag.config_return == 1)
	{
		sg_sysoperate_t.com_flag.config_return = 0;
		com_ack_function(sg_send_buff,&sg_send_size,sg_sysoperate_t.com.return_cmd,sg_sysoperate_t.com.return_error);
		app_send_data_task_function();
	}
}

/************************************************************
*
* Function name	: app_get_com_send_status_function
* Description	: 获取当前通信状态
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t app_get_com_send_status_function(void)
{
	return sg_sysoperate_t.com.send_status;
}

/************************************************************
*
* Function name	: app_deal_com_send_wait_function
* Description	: 发送等待处理任务
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_deal_com_send_wait_function(void)
{
	switch(sg_sysoperate_t.com.send_result)
	{
		case SR_TIMEOUT:											// 发送超时							
			/* 本次发送超时 */
			sg_sysoperate_t.com.repeat++;
			if(sg_sysoperate_t.com.repeat >= COM_SEND_MAX_NUM)
			{
				/* 发送响应超时 */
				sg_sysoperate_t.com.repeat = 0;
				/* 发送超时，服务器无响应或者网络已断开 */
				eth_set_tcp_connect_reset();						// 重启TCP连接
				gsm_set_network_reset_function();					// 重启GRPS连接
				
				/* 数据清空 */
				sg_sysoperate_t.com.send_status = SEND_STATUS_NO; 	// 进行下一次发送
				sg_sysoperate_t.com.send_cmd = 0;
				sg_sysoperate_t.com.heart_time  = 0; 				// 重启一次心跳计时
				sg_sysoperate_t.com.repeat   = 0; 					// 重启一次正常上报计时   
			}
			else
			{
				sg_sysoperate_t.com.send_status = SEND_STATUS_NO;
			}
			break;
		case SR_OK:													// 发送成功
			/* 发送成功 */
			sg_sysoperate_t.com.send_status = SEND_STATUS_NO; 		// 进行下一次发送
			sg_sysoperate_t.com.send_cmd = 0;
			/* 清空数据 */
			memset(sg_send_buff,0,sizeof(sg_send_buff));
			sg_send_size = 0;
			break;
		case SR_ERROR:												// 响应错误
		case SR_SEND_ERROR:											// 发送错误
			sg_sysoperate_t.com.repeat++;
			if(sg_sysoperate_t.com.repeat >= COM_SEND_MAX_NUM)
			{
				/* 发送次数到达上限 */
				sg_sysoperate_t.com.repeat = 0;
				sg_sysoperate_t.com.send_status = SEND_STATUS_NO; 	// 进行下一次发送
				sg_sysoperate_t.com.send_cmd = 0;
				/* 清空数据 */
				memset(sg_send_buff,0,sizeof(sg_send_buff));
				sg_send_size = 0;
			}
			else
			{
				sg_sysoperate_t.com.send_status = SEND_STATUS_NO; 	// 进行下一次发送
			}
			break;
		default:
			sg_sysoperate_t.com.send_cmd = 0;
			sg_sysoperate_t.com.send_status = SEND_STATUS_NO; 		// 进行下一次发送
			/* 清空数据 */
			memset(sg_send_buff,0,sizeof(sg_send_buff));
			sg_send_size = 0;
			break;
	}	
	/* 结果清空 */
	sg_sysoperate_t.com.send_result = SR_WAIT;
}

/************************************************************
*
* Function name	: app_com_send_function
* Description	: 通信发送函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_com_send_function(void)
{
	static uint8_t netlink_error = 0;
	/* 检测无线网络状态 */
	if(gsm_get_network_connect_status_function() == 0 )
		return ;

	if(sg_sysoperate_t.com.send_mode == 1)
	{
		if(gsm_get_network_connect_status_function() == 0)	/* 网络错误，准备重新连接，清除本次发送参数 */
		{
			memset(&sg_sysoperate_t.com_flag,0,sizeof(sg_sysoperate_t.com_flag));
			sg_sysoperate_t.com.send_cmd = 0;
			gsm_set_network_reset_function(); // 重启GPRS
		}
	}
	else if(sg_sysoperate_t.com.send_mode == 3)
	{
//		if(eth_get_tcp_status() != 2)	/* 网络错误，准备重新连接，清除本次发送参数 */
//		{
//			if((netlink_error&0x01) == 0)
//			{
//				netlink_error |= 0x01;
//				eth_set_tcp_connect_reset();
//			}
//		}
//		else
//			netlink_error &=~0x01;

		if(gsm_get_network_connect_status_function() == 0)	/* 网络错误，准备重新连接，清除本次发送参数 */
		{
			if((netlink_error&0x02) == 0)
			{
				netlink_error |= 0x02;
				gsm_set_network_reset_function(); // 重启GPRS	
			}
		}
		else
			netlink_error &=~0x02;
	}
	
	/* 检测是否能发送数据 */
	if(sg_sysoperate_t.com.send_status == SEND_STATUS_NO)
	{
		app_deal_com_flag_function();
	}
	/* 检测到发送有结果了 */
	else if(sg_sysoperate_t.com.send_status == SEND_STATUS_RESULT)
	{
		app_deal_com_send_wait_function();
	}
}

/************************************************************
*
* Function name	: app_set_send_result_function
* Description	: 设置发送结果
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_send_result_function(send_result_e data)
{
	sg_sysoperate_t.com.send_result = data;
	sg_sysoperate_t.com.send_status = SEND_STATUS_RESULT;
}
	
/************************************************************
*
* Function name	: app_com_time_function
* Description	: 通信计时函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_com_time_function(void)
{
	/* 心跳计时 */
	sg_sysoperate_t.com.heart_time++;
	if(sg_sysoperate_t.com.heart_time > sg_comparam_t.heart)
	{
		sg_sysoperate_t.com.heart_time = 0;
		/* 发送一次心跳 */
		sg_sysoperate_t.com_flag.heart_pack = 1;
	}
	
	/* 正常上报 */
	sg_sysoperate_t.com.report_time++;
	if(sg_sysoperate_t.com.report_time > (sg_comparam_t.report))
	{
		sg_sysoperate_t.com.report_time = 0;
		/* 进行一次上报 */
		sg_sysoperate_t.com_flag.report_normally = 1;
	}
	
	/* 发送计时 */
	if(sg_sysoperate_t.com.send_status == 1)
	{
		sg_sysoperate_t.com.send_time++;
		if(sg_sysoperate_t.com.send_time > COM_SEND_MAX_TIME)
		{
			sg_sysoperate_t.com.send_time = 0;
			
			sg_sysoperate_t.com.send_status = SEND_STATUS_RESULT;
			sg_sysoperate_t.com.send_result = SR_TIMEOUT;
		}
	}
	else
	{
		sg_sysoperate_t.com.send_time = 0;
	}
}

/************************************************************
*
* Function name	: app_set_peripheral_switch
* Description	: 外设开关控制
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_peripheral_switch(uint8_t cmd, uint8_t data)
{
	switch (cmd) 
	{
		case CONTROL_FAN:
			if (data == 1)
			{
				fan_control(FAN_1,FAN_ON); // 开风扇1
				fan_control(FAN_2,FAN_ON); // 开风扇2
			}
			else if(data == 2)
			{
				fan_control(FAN_1,FAN_OFF); // 开风扇1
				fan_control(FAN_2,FAN_OFF); // 开风扇2
			}
			break;
		
		default:
			break;
	}
	
	app_set_reply_parameters_function(cmd,0x01);
}
/************************************************************
*
* Function name	: app_opeare_relay_function
* Description	: 操作继电器
* Parameter		: 
* Return		: 
*	
************************************************************/
int app_opeare_relay_function(uint8_t num, uint8_t relay,uint8_t status)
{
	if(sg_sysoperate_t.sys_flag.relay_reset[relay-1] == 0)
	{
		sg_sysoperate_t.sys_flag.relay_reset[relay-1] = 1;
		return 0;
	}
	else
		return -1;
}

/************************************************************
*
* Function name	: app_set_sys_opeare_function
* Description	: 设置操作任务 - 立即回发
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_sys_opeare_function(uint8_t cmd, uint8_t data)
{
	int8_t ret = 0;
	int8_t cnt = 0;
	
	switch(cmd)
	{
		case CR_SINGLE_CAMERA_CONTROL:
			if(data>=0x01 && data<=0x08)
			{
				if(sg_sysoperate_t.sys_flag.adapter_reset == 0)
				{
					sg_sysoperate_t.sys_flag.adapter_reset = 1;
					sg_sysoperate_t.sys.adapter_num = data;
//					app_set_switch_control_function(data,1);   // 重启
					app_set_reply_parameters_function(CR_SINGLE_CAMERA_CONTROL,0x01);
				}
				else
				{
					app_set_reply_parameters_function(CR_SINGLE_CAMERA_CONTROL,0x77);
				}
			}
			else
			{
				app_set_reply_parameters_function(CR_SINGLE_CAMERA_CONTROL,0x74);
			}
			break;

		case CR_POWER_RESETART:
			sg_sysoperate_t.com.return_error = 1;
			/* 数据回传 */
			com_ack_function(sg_send_buff,&sg_send_size,sg_sysoperate_t.com.return_cmd,sg_sysoperate_t.com.return_error);
			cnt = 0;
REPEAT1:
			/* 立即发送 */
			app_send_data_task_function();	
			if(ret < 0)
			{
				cnt++;
				if(cnt<=3)
				{
					OSTimeDlyHMSM(0,0,0,100); 
					goto REPEAT1;
				}
			}
				
			lfs_unmount(&g_lfs_t);
			OSTimeDlyHMSM(0,0,0,100); 
			/* 重启设备 */
			System_SoftReset();
			break;
		case CR_LWIP_NETWORK_RESET:	
			sg_sysoperate_t.com.return_error = 1;
			/* 数据回传 */
			com_ack_function(sg_send_buff,&sg_send_size,sg_sysoperate_t.com.return_cmd,sg_sysoperate_t.com.return_error);
			cnt = 0;
REPEAT2:
			app_send_data_task_function();	/* 立即发送 */
			if(ret < 0)
			{
				cnt++;
				if(cnt<=3)
				{
					OSTimeDlyHMSM(0,0,0,100); 
					goto REPEAT2;
				}
			}
			OSTimeDlyHMSM(0,0,0,100); 
			eth_set_network_reset();
			break;
		case CR_GPRS_NETWORK_RESET:
			sg_sysoperate_t.com.return_error = 1;
			/* 数据回传 */
			com_ack_function(sg_send_buff,&sg_send_size,sg_sysoperate_t.com.return_cmd,sg_sysoperate_t.com.return_error);
			cnt = 0;
REPEAT3:
			/* 立即发送 */
			app_send_data_task_function();	
			if(ret < 0)
			{
				cnt++;
				if(cnt<=3)
				{
					OSTimeDlyHMSM(0,0,0,100); 
					goto REPEAT3;
				}
			}			
			OSTimeDlyHMSM(0,0,0,100); 
			/* 重启GPRS */
			gsm_set_module_reset_function();
			break;
			case CR_GPRS_NETWORK_V_RESET:
			sg_sysoperate_t.com.return_error = 1;
			/* 数据回传 */
			com_ack_function(sg_send_buff,&sg_send_size,sg_sysoperate_t.com.return_cmd,sg_sysoperate_t.com.return_error);
			cnt = 0;
REPEAT4:
			/* 立即发送 */
			app_send_data_task_function();	
			if(ret < 0)
			{
				cnt++;
				if(cnt<=3)
				{
					OSTimeDlyHMSM(0,0,0,100); 
					goto REPEAT4;
				}
			}			
			OSTimeDlyHMSM(0,0,0,100); 
			/* 重启GPRS */
			gprs_v_reset_function();
			gsm_set_module_reset_function();
			break;
			
	}
}

/************************************************************
*
* Function name	: app_sys_operate_timer_function
* Description	: 操作命令的时间处理函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_sys_operate_timer_function(void)
{
	static uint16_t time = 0;
		
	/* 重启指定适配器 */
	if(sg_sysoperate_t.sys_flag.adapter_reset == 1)
	{
		/* 关闭指的适配器 */
		if(sg_sysoperate_t.sys.adapter_num <= 8)
		{
			switch(sg_sysoperate_t.sys.adapter_num)
			{
				case 1:	relay_control(RELAY_1,RELAY_OFF);	break;
				case 2:	relay_control(RELAY_2,RELAY_OFF);	break;
				case 3:	relay_control(RELAY_3,RELAY_OFF);	break;
				case 4:	relay_control(RELAY_4,RELAY_OFF);	break;
				case 5:	relay_control(RELAY_5,RELAY_OFF);	break;
				case 6:	relay_control(RELAY_6,RELAY_OFF);	break;
				case 7:	relay_control(RELAY_7,RELAY_OFF);	break;
				case 8:	relay_control(RELAY_8,RELAY_OFF);	break;
				default:	break;
			}
			sg_sysoperate_t.sys_flag.adapter_reset = 2;
			time = 15*1000; // 15s重启
		}
		else
		{
			sg_sysoperate_t.sys_flag.adapter_reset = 0;
		}
	}	
	else if(sg_sysoperate_t.sys_flag.adapter_reset == 2)
	{
		time--;
		if(time == 0)
		{
			switch(sg_sysoperate_t.sys.adapter_num)
			{
				case 1:	relay_control(RELAY_1,RELAY_ON);	break;
				case 2:	relay_control(RELAY_2,RELAY_ON);	break;
				case 3:	relay_control(RELAY_3,RELAY_ON);	break;
				case 4:	relay_control(RELAY_4,RELAY_ON);	break;
				case 5:	relay_control(RELAY_5,RELAY_ON);	break;
				case 6:	relay_control(RELAY_6,RELAY_ON);	break;
				case 7:	relay_control(RELAY_7,RELAY_ON);	break;
				case 8:	relay_control(RELAY_8,RELAY_ON);	break;
				default:
					break;
			}
			sg_sysoperate_t.sys_flag.adapter_reset = 0;
		}
	}
}

/************************************************************
*
* Function name	: app_sys_operate_relay
* Description	: 继电器重启
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_sys_operate_relay(void)
{
	static uint16_t relay_time[8] = {0};
		
	for(uint8_t i=0;i<8;i++)
	{
		if(sg_sysoperate_t.sys_flag.relay_reset[i] == 1)
		{
			relay_control((RELAY_DEV)i,RELAY_OFF);
			relay_time[i] = 10*100;
			sg_sysoperate_t.sys_flag.relay_reset[i] = 2;
		}
		else if(sg_sysoperate_t.sys_flag.relay_reset[i] == 2)
		{		
			relay_time[i]--;
			if(relay_time[i] == 0)
			{		
				relay_control((RELAY_DEV)i,RELAY_ON);
				sg_sysoperate_t.sys_flag.relay_reset[i]	= 0;			
			}
		}	
	}
}

/***********************************************************************************
					参数的配置与获取
***********************************************************************************/

/************************************************************
*
* Function name	: app_get_storage_param_function
* Description	: 用于获取存储的参数的函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_get_storage_param_function(void)
{
	save_read_local_network(&sg_sysparam_t.local);
	save_read_remote_ip_function(&sg_sysparam_t.remote);
	save_read_comparision_parameter(&sg_comparisionparam_t);
	save_read_device_paramter_function(&sg_sysparam_t.device);
	save_read_com_param_function(&sg_comparam_t);
	save_read_report_switch(&sg_sysparam_t.report);
	save_read_carema_parameter(&sg_carema_param_t);   //20230712
	save_read_threshold_parameter(&sg_sysparam_t.threshold); // 20230720
	update_read_addr();
	
	save_read_backups_function(&sg_backups_t); // 20231022
	// 判断设备重启时间，防止时间读取为0
	if(sg_comparam_t.reload < 86400)
		sg_comparam_t.reload = 86400;
	else if(sg_comparam_t.reload > 604800)
		sg_comparam_t.reload = 604800;
}

/************************************************************
*
* Function name	: app_task_save_function
* Description	: 用于存储本机的设置参数
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_task_save_function(void)
{
	uint32_t data = 0;
	if(sg_sysoperate_t.save_flag.save_device_param == 1)
	{
		sg_sysoperate_t.save_flag.save_device_param = 0;
		save_storage_device_parameter_function(&sg_sysparam_t.device);	/* 存储设备参数信息 */
		if(SAVE_DEBUG)  printf("save_storage_device_parameter_function\r\n");
	}	
	
	if(sg_sysoperate_t.save_flag.save_local_network == 1)
	{
		sg_sysoperate_t.save_flag.save_local_network = 0;
		save_stroage_local_network(&sg_sysparam_t.local);	/* 存储本地网络参数信息 */
		if(SAVE_DEBUG)  printf("save_stroage_local_network\r\n");
	}
	
	if(sg_sysoperate_t.save_flag.save_remote_network == 1)
	{
		sg_sysoperate_t.save_flag.save_remote_network = 0;
		save_stroage_remote_ip_function(&sg_sysparam_t.remote);
		if(SAVE_DEBUG)  printf("save_stroage_remote_ip_function\r\n");
		
		OSTimeDlyHMSM(0,0,0,100);
		set_reboot_time_function(1000); // 配置服务器后系统重启，防止设备未传到新平台
	}
	
	if(sg_sysoperate_t.save_flag.save_comparision == 1)
	{
		sg_sysoperate_t.save_flag.save_comparision = 0;
		save_stroage_comparision_parameter(&sg_comparisionparam_t);	/* 存储外设相关参数：摄像头等 */
		if(SAVE_DEBUG)  printf("save_stroage_comparision_parameter\r\n");
	}
	
	if(sg_sysoperate_t.save_flag.save_carema == 1)
	{
		sg_sysoperate_t.save_flag.save_carema = 0;
		save_stroage_carema_parameter(&sg_carema_param_t);	/* 存储摄像头参数 */
		if(SAVE_DEBUG)  printf("save_stroage_carema_parameter\r\n");
	}
	
	if(sg_sysoperate_t.save_flag.com_parameter == 1)
	{
		sg_sysoperate_t.save_flag.com_parameter = 0;
		save_stroage_com_param_function(&sg_comparam_t);		/* 存储通信相关参数 */
		if(SAVE_DEBUG)  printf("save_stroage_com_param_function\r\n");
	}
	
	if(sg_sysoperate_t.save_flag.save_report_sw == 1) {
		sg_sysoperate_t.save_flag.save_report_sw = 0;
		save_stroage_report_switch(&sg_sysparam_t.report);
		if(SAVE_DEBUG)  printf("save_stroage_report_switch\r\n");
	}
	
	if(sg_sysoperate_t.save_flag.save_update_addr == 1) 
	{
		if(SAVE_DEBUG)  printf("update_save_addr\r\n");
		sg_sysoperate_t.save_flag.save_update_addr = 0;
		update_save_addr();		/* 存储更新地址信息 */
	}
	if(sg_sysoperate_t.save_flag.save_threshold == 1)
	{
		sg_sysoperate_t.save_flag.save_threshold = 0;
		save_stroage_threshold_parameter(&sg_sysparam_t.threshold);
		if(SAVE_DEBUG)  printf("save_stroage_carema_parameter\r\n");
	}

	/* 恢复出厂化：产品序列号不变 */
	if(sg_sysoperate_t.save_flag.save_reset == 1)
	{
		sg_sysoperate_t.save_flag.save_reset = 0;
		if(SAVE_DEBUG)  printf("save_clear_file_function\r\n");		
		save_clear_file_function(0);
		app_get_storage_param_function();
		
		eth_set_network_reset(); 			 // 重启网络
		gsm_set_module_reset_function(); 	// 重启GPRS
	}
	
	/* 还原操作 */
	/** 还原本地网络信息 - 需要重启本地网络 **/
	if(sg_sysoperate_t.save_flag.erase_local_network == 1)
	{
		sg_sysoperate_t.save_flag.erase_local_network = 0;

		/* 本机IP */
		sg_sysparam_t.local.ip[0] = DEFALUT_LOCAL_IP0;
		sg_sysparam_t.local.ip[1] = DEFALUT_LOCAL_IP1;
		sg_sysparam_t.local.ip[2] = DEFALUT_LOCAL_IP2;
		sg_sysparam_t.local.ip[3] = DEFALUT_LOCAL_IP3;
		
		/* 本机子网掩码 */
		sg_sysparam_t.local.netmask[0]=DEFALUT_NETMASK0;	
		sg_sysparam_t.local.netmask[1]=DEFALUT_NETMASK1;
		sg_sysparam_t.local.netmask[2]=DEFALUT_NETMASK2;
		sg_sysparam_t.local.netmask[3]=DEFALUT_NETMASK3;
		/* 本机默认网关 */
		sg_sysparam_t.local.gateway[0]=DEFALUT_GATEWAY0;	
		sg_sysparam_t.local.gateway[1]=DEFALUT_GATEWAY1;
		sg_sysparam_t.local.gateway[2]=DEFALUT_GATEWAY2;
		sg_sysparam_t.local.gateway[3]=DEFALUT_GATEWAY3;	
			
		/* 获取默认数据 */
		save_stroage_local_network(&sg_sysparam_t.local);
		if(SAVE_DEBUG)  printf("erase_local_network\r\n");	
		eth_set_network_reset(); 			// 重启网络
	}
	/** 还原mac地址 - 需要重启本地网络 **/
	if(sg_sysoperate_t.save_flag.erase_local_mac == 1)
	{
		sg_sysoperate_t.save_flag.erase_local_mac = 0;
		data = *(vu32*)(0X1FFF7A10);
		/* 本机IP */
		sg_sysparam_t.local.mac[0]=2;//高三字节(IEEE称之为组织唯一ID,OUI)地址固定为:2.0.0
		sg_sysparam_t.local.mac[1]=0;
		sg_sysparam_t.local.mac[2]=0;
		sg_sysparam_t.local.mac[3]=(data>>16)&0XFF;//低三字节用STM32的唯一ID
		sg_sysparam_t.local.mac[4]=(data>>8)&0XFF;
		sg_sysparam_t.local.mac[5]=data&0XFF; 	
		
		STMFLASH_Write_SAVE(DEVICE_FLASH_STORE,DEVICE_MAC_ADDR,(uint32_t *)&sg_sysparam_t.local.mac,2);
//	  STMFLASH_Write(DEVICE_MAC_ADDR,(uint32_t *)&sg_sysparam_t.local.mac,2);
		/* 获取默认数据 */
		save_stroage_local_network(&sg_sysparam_t.local);
		if(SAVE_DEBUG)  printf("erase_local_mac\r\n");	
		eth_set_network_reset(); 			// 重启网络
	}

	
	/** 还原上报时间 **/
	if(sg_sysoperate_t.save_flag.erase_report_time == 1)
	{
		sg_sysoperate_t.save_flag.erase_report_time = 0;
		sg_comparam_t.report = DEFALUT_REPORT; 
		if(SAVE_DEBUG)  printf("erase_report_time\r\n");
		save_stroage_com_param_function(&sg_comparam_t);
	}
	
	/** 还原PING间隔时间 **/
	if(sg_sysoperate_t.save_flag.erase_ping_time == 1) {
		sg_sysoperate_t.save_flag.erase_ping_time = 0;
		
		sg_comparam_t.ping = DEFALUT_PING;
		sg_comparam_t.dev_ping = DEFALUT_DEV_PING;
		if(SAVE_DEBUG)  printf("erase_ping_time\r\n");
		save_stroage_com_param_function(&sg_comparam_t);
	}
	
	/** 还原网络延时时间  20220308**/
	if(sg_sysoperate_t.save_flag.erase_netdelay_time == 1)
	{
		sg_sysoperate_t.save_flag.erase_netdelay_time = 0;
		sg_comparam_t.network_time = DEFALUT_NETWORK_DELAY; 
		if(SAVE_DEBUG)  printf("erase_netdelay_time\r\n");
		save_stroage_com_param_function(&sg_comparam_t);
	}
	
	/** 还原传输模式 **/
	if(sg_sysoperate_t.save_flag.erase_tran_mode == 1) {
		sg_sysoperate_t.save_flag.erase_tran_mode = 0;
		if(SAVE_DEBUG)  printf("erase_tran_mode\r\n");
		sg_sysparam_t.local.server_mode = 4;
		save_stroage_local_network(&sg_sysparam_t.local);
	}
	
	/** 清除服务器 - 还原默认 **/
	if(sg_sysoperate_t.save_flag.erase_server_ip == 1) {
		sg_sysoperate_t.save_flag.erase_server_ip = 0;
		if(SAVE_DEBUG)  printf("erase_server_ip\r\n");
		save_read_default_remote_ip(&sg_sysparam_t.remote);
		save_stroage_remote_ip_function(&sg_sysparam_t.remote);
		
		/* 重启连接 */
		eth_set_tcp_connect_reset();
		gsm_set_network_reset_function();
	}
	
	/** 清除摄像头ip **/
	if(sg_sysoperate_t.save_flag.erase_camera_ip == 1)
	{
		sg_sysoperate_t.save_flag.erase_camera_ip = 0;
		memset(sg_comparisionparam_t.ip,0,sizeof(sg_comparisionparam_t.ip));
		if(SAVE_DEBUG)  printf("erase_camera_ip\r\n");
		save_stroage_comparision_parameter(&sg_comparisionparam_t);
	}
	/** 清除摄像头的用户名、密码  20220329**/
	if(sg_sysoperate_t.save_flag.erase_ipc_login == 1)
	{
		sg_sysoperate_t.save_flag.erase_ipc_login = 0;
		memset(sg_carema_param_t.name,0,sizeof(sg_carema_param_t.name));
		memset(sg_carema_param_t.pwd,0,sizeof(sg_carema_param_t.pwd));
		save_stroage_carema_parameter(&sg_carema_param_t);
	}
	/** 清除主机IP **/
	if(sg_sysoperate_t.save_flag.erase_main_ip == 1)
	{
		sg_sysoperate_t.save_flag.erase_main_ip = 0;
		if(SAVE_DEBUG)  printf("erase_main_ip\r\n");
		memset(sg_sysparam_t.local.ping_ip,0,sizeof(sg_sysparam_t.local.ping_ip));
		memset(sg_sysparam_t.local.ping_sub_ip,0,sizeof(sg_sysparam_t.local.ping_sub_ip));
		save_stroage_local_network(&sg_sysparam_t.local);
	}
	
}

/************************************************************
*
* Function name	: app_set_save_infor_function
* Description	: 设置存储标志位
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_save_infor_function(uint8_t mode)
{
	switch(mode)
	{
		case SAVE_OTHER_PARAM:
			sg_sysoperate_t.save_flag.save_other_param = 1;
			break;
		case SAVE_LOCAL_NETWORK:
			sg_sysoperate_t.save_flag.save_local_network = 1;
			break;
		case SAVE_REMOTE_IP:
			sg_sysoperate_t.save_flag.save_remote_network = 1;
			break;
		case SAVE_COMPARISION:
			sg_sysoperate_t.save_flag.save_comparision = 1;
			break;
		case SAVE_DEVICE_PARAM:
			sg_sysoperate_t.save_flag.save_device_param = 1;
			break;
		case SAVE_COM_PARAMETER:
			sg_sysoperate_t.save_flag.com_parameter = 1;
			break;
		case SAVE_UPDATE:
			sg_sysoperate_t.save_flag.save_update_addr = 1;
			break;
		case SAVE_REPORT_SW:
			sg_sysoperate_t.save_flag.save_report_sw = 1;
			break;
		case SAVE_CAREMA:
			sg_sysoperate_t.save_flag.save_carema = 1;
			break;
		case SAVE_THRESHOLD:
			sg_sysoperate_t.save_flag.save_threshold = 1;
			break;
		default:
			break;
	}
}

/************************************************************
*
* Function name	: app_set_erase_infor_function
* Description	: 设置需要清除的信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_erase_infor_function(uint8_t mode)
{
	switch(mode)
	{
		case CONFIGURE_SERVER_DOMAIN_NAME:
			sg_sysoperate_t.save_flag.erase_server_ip = 1;
			break;
		case CONFIGURE_PING_INTERVAL:
			sg_sysoperate_t.save_flag.erase_ping_time = 1;
			break;
		case CONFIGURE_NETWORK_DELAY:          // 网络延时时间		  20220308
			sg_sysoperate_t.save_flag.erase_netdelay_time = 1;
			break;
		case CONFIGURE_SERVER_IP:
			sg_sysoperate_t.save_flag.erase_tran_mode = 1;
			break;
		case CONFIGURE_LOCAL_NETWORK:
			sg_sysoperate_t.save_flag.erase_local_network = 1;
			break;
		case CONFIGURE_HEART_TIME:
			sg_sysoperate_t.save_flag.erase_report_time = 1;
			break;
		case CONFIGURE_CAMERA_CONFIG:
			sg_sysoperate_t.save_flag.erase_camera_ip = 1;
			break;
		case CONFIGURE_MAIN_NETWORK_IP:
			sg_sysoperate_t.save_flag.erase_main_ip = 1;
			break;
		case CONFIGURE_IPC_LOGIN_INFO:       // 用户名、密码  20220329
			sg_sysoperate_t.save_flag.erase_ipc_login = 1;
			break;
		default:
			break;
	}
}

/************************************************************
*
* Function name	: app_get_local_network_function
* Description	: 获取本机网络信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void *app_get_local_network_function(void)
{
	return (&sg_sysparam_t.local);
}

/************************************************************
*
* Function name	: app_set_local_network_function
* Description	: 设置本地网络参数
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_local_network_function(struct local_ip_t param)
{
	uint8_t mac[6] = {0};
	uint8_t main_ip[4] = {0};
	
	memcpy(mac,sg_sysparam_t.local.mac,6); 			// 备份
	memcpy(main_ip,sg_sysparam_t.local.ping_ip,4); 	// 备份
	memset((uint8_t*)&sg_sysparam_t.local,0,sizeof(struct local_ip_t));
	memcpy((uint8_t*)&sg_sysparam_t.local,&param,sizeof(struct local_ip_t));
	memcpy(sg_sysparam_t.local.mac,mac,6); 			// 还原
	memcpy(sg_sysparam_t.local.ping_ip,main_ip,4); 	// 还原
	/* 保存 */
	app_set_save_infor_function(SAVE_LOCAL_NETWORK);
}

/************************************************************
*
* Function name	: app_set_local_network_function_two
* Description	: 存储部分网络参数
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_local_network_function_two(struct local_ip_t param)
{
	memcpy(sg_sysparam_t.local.ip,param.ip,4);
	memcpy(sg_sysparam_t.local.gateway,param.gateway,4);
	memcpy(sg_sysparam_t.local.netmask,param.netmask,4);
	memcpy(sg_sysparam_t.local.dns,param.dns,4);
	memcpy(sg_sysparam_t.local.mac,param.mac,6);
	memcpy(sg_sysparam_t.local.ping_ip,param.ping_ip,4);
	memcpy(sg_sysparam_t.local.ping_sub_ip,param.ping_sub_ip,4);
	
	memcpy(sg_sysparam_t.local.multicast_ip,param.multicast_ip,4);
	sg_sysparam_t.local.multicast_port = param.multicast_port;
	sg_sysparam_t.local.port = param.port;

	STMFLASH_Write_SAVE(DEVICE_FLASH_STORE,DEVICE_MAC_ADDR,(uint32_t *)&sg_sysparam_t.local.mac,2);
//	STMFLASH_Write(DEVICE_MAC_ADDR,(uint32_t *)&sg_sysparam_t.local.mac,2);	
	sprintf(sg_sysparam_t.local.ip_str,"%d.%d.%d.%d",sg_sysparam_t.local.ip[0],sg_sysparam_t.local.ip[1],
																			sg_sysparam_t.local.ip[2],sg_sysparam_t.local.ip[3]);
	
	app_set_save_infor_function(SAVE_LOCAL_NETWORK);	/* 保存 */
}

/************************************************************
*
* Function name	: app_set_transfer_mode_function
* Description	: 存储传输模式信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_transfer_mode_function(uint8_t mode) 
{
	switch(mode) {
		case 0:
			sg_sysparam_t.local.server_mode = 1;
			break;
		case 1:
			sg_sysparam_t.local.server_mode = 2;
			break;
		case 2:
			sg_sysparam_t.local.server_mode = 3;
			break;
		case 3:
			sg_sysparam_t.local.server_mode = 4;
			break;
	}
	/* 保存 */
	app_set_save_infor_function(SAVE_LOCAL_NETWORK);
}
/************************************************************
*
* Function name	: app_set_carema_search_mode_function
* Description	: 设置摄像机搜索协议
* Parameter		: config_mode:配置方式 web/平台
* Return		: 
*	   20230810
************************************************************/
void app_set_carema_search_mode_function(uint8_t mode,uint8_t config_mode)  
{

}

/************************************************************
*
* Function name	: app_get_remote_network_function
* Description	: 获取远端网络信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void *app_get_remote_network_function(void)
{
	return (&sg_sysparam_t.remote);
}
/************************************************************
*
* Function name	: app_get_backups_function
* Description	: 获取备份信息
* Parameter		: 
* Return		: 
*	  20231022
************************************************************/
void *app_get_backups_function(void)
{
	return (&sg_backups_t.remote);
}
/************************************************************
*
* Function name	: app_set_remote_network_function
* Description	: 存储远端网络信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_remote_network_function(struct remote_ip param)
{

	memcpy(&sg_sysparam_t.remote,&param,sizeof(struct remote_ip));
	/* 存储 */
	app_set_save_infor_function(SAVE_REMOTE_IP);
}

/************************************************************
*
* Function name	: app_set_cilent_network_function
* Description	: 存储远端网络信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_cilent_network_function(struct remote_ip param)
{
	memcpy(&sg_sysparam_t.remote.client_iporname,&param.client_iporname,sizeof(param.client_iporname));
	sg_sysparam_t.remote.client_port = param.client_port;
}


/************************************************************
*
* Function name	: app_set_reset_function
* Description	: 恢复出厂化
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_reset_function(void)
{
	sg_sysoperate_t.save_flag.save_reset = 1;
}

/************************************************************
*
* Function name	: app_set_mac_reset_function
* Description	: mac地址初始化
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_mac_reset_function(void)
{
	sg_sysoperate_t.save_flag.erase_local_mac = 1;
}

/************************************************************
*
* Function name	: app_get_camera_function
* Description	: 获取相机IP信息
* Parameter		: 
*	@ip			: ip数据
*	@num		: 摄像机号数
* Return		: 
*	
************************************************************/
int8_t app_get_camera_function(uint8_t *ip, uint8_t num)
{	
	/* 验证是否有ip地址 */
	if( sg_comparisionparam_t.ip[num][0] == 0 && \
			sg_comparisionparam_t.ip[num][1] == 0 && \
			sg_comparisionparam_t.ip[num][2] == 0 && \
			sg_comparisionparam_t.ip[num][3] == 0)
	{
		return -1;
	}
	else
	{
		memcpy(ip,sg_comparisionparam_t.ip[num],4);
	}
	return 0;
}

/************************************************************
*
* Function name	: app_set_camera_function
* Description	: 设置摄像机ip地址
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_camera_function(uint8_t *ip)
{
	memset(sg_comparisionparam_t.ip,0,sizeof(sg_comparisionparam_t.ip));
	memcpy(sg_comparisionparam_t.ip,ip,sizeof(sg_comparisionparam_t.ip));
	
	/* 存储 */
	app_set_save_infor_function(SAVE_COMPARISION);
}
/************************************************************
*
* Function name	: app_set_camera_login_function
* Description	: 设置指定摄像机的用户名、密码
* Parameter		: 
* Return		: 
*								20220329
************************************************************/
void app_set_camera_login_function(char *name_buf,char *pwd_buf,int port,uint8_t num)
{
	memset(sg_carema_param_t.name[num],0,sizeof(sg_carema_param_t.name[num]));
	memset(sg_carema_param_t.pwd[num],0,sizeof(sg_carema_param_t.pwd[num]));
	sprintf(sg_carema_param_t.name[num],"%s",name_buf);
	sprintf(sg_carema_param_t.pwd[num],"%s",pwd_buf);
	sg_carema_param_t.port[num] = port;
	app_set_save_infor_function(SAVE_CAREMA);	/* 存储 */
}

/************************************************************
*
* Function name	: app_get_camera_function
* Description	: 获取指定摄像机用户名、密码
* Parameter		: 
*	@ip			: 
*	@num		: 摄像机号数
* Return		: 
*	             20220329
************************************************************/
int8_t app_get_camera_login_function(char *name_buf,char *pwd_buf,uint8_t num)
{	
	/* 验证信息是否为空 */
	if( strlen(sg_carema_param_t.name[num]) == 0 &&strlen(sg_carema_param_t.pwd[num]) == 0)
	{
		return -1;
	}
	else
	{
		sprintf(name_buf,"%s",sg_carema_param_t.name[num]);
		sprintf(pwd_buf,"%s",sg_carema_param_t.pwd[num]);
	}
	return 0;
}

/************************************************************
*
* Function name	: app_get_camera_port_function
* Description	: 获取指定摄像机端口号
* Parameter		: 
* Return		: 
*	             20220329
************************************************************/
int app_get_camera_port_function(uint8_t num)
{	
	return sg_carema_param_t.port[num];
}
/************************************************************
*
* Function name	: app_get_camera_num_function
* Description	: 获取指定摄像机编号
* Parameter		: 
* Return		: 
*	             20220329
************************************************************/
int8_t app_get_camera_num_function(void)
{	
	return  sg_sysoperate_t.sys.caramer_num;
}

/************************************************************
*
* Function name	: app_set_camera_id_num_function
* Description	: 设置摄像机的编号
* Parameter		: 
* Return		: 
*	             20220329
************************************************************/
void app_set_camera_id_num_function(uint8_t data)  // 设置摄像机的编号
{	
	sg_sysoperate_t.sys.caramer_num = data;
}

/************************************************************
*
* Function name	: app_set_camera_num_function
* Description	: 设置摄像机ip - 指定摄像机
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_camera_num_function(uint8_t *ip, uint8_t num)
{
	sg_comparisionparam_t.ip[num][0] = ip[0];
	sg_comparisionparam_t.ip[num][1] = ip[1];
	sg_comparisionparam_t.ip[num][2] = ip[2];
	sg_comparisionparam_t.ip[num][3] = ip[3];
	
	/* 存储 */
	app_set_save_infor_function(SAVE_COMPARISION);
}

/************************************************************
*
* Function name	: app_match_local_camera_ip
* Description	: 匹配本地IP
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t app_match_local_camera_ip(uint8_t *ip)
{
	int8_t  ret   = 0;
	uint8_t index = 0;
	
	for(index = 0; index<10; index++)
	{
		if(memcmp(sg_comparisionparam_t.ip[index],ip,4) == 0)
		{
			ret = -1;
			break;
		}
	}
	
	return ret;
}

/************************************************************
*
* Function name	: app_set_fan_humi_param_function
* Description	: 设置风扇湿度启动参数
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_fan_humi_param_function(uint8_t *data)
{
	sg_sysparam_t.threshold.humi_high = data[0];
	sg_sysparam_t.threshold.humi_low = data[1];
	app_set_save_infor_function(SAVE_THRESHOLD);	/* 存储 */
}

/************************************************************
*
* Function name	: app_set_fan_param_function
* Description	: 配置风扇温度参数
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_fan_param_function(int8_t *data)
{
	sg_sysparam_t.threshold.temp_high = data[0];
	sg_sysparam_t.threshold.temp_low = data[1];
	app_set_save_infor_function(SAVE_THRESHOLD);	/* 存储 */
}

/************************************************************
*
* Function name	: app_set_threshold_param_function
* Description	: 配置阈值
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_vol_current_param(uint16_t *data)
{
	sg_sysparam_t.threshold.volt_max = data[0];
	sg_sysparam_t.threshold.volt_min = data[1];
	sg_sysparam_t.threshold.current  = data[2];
	/* 保存 */
	app_set_save_infor_function(SAVE_THRESHOLD);	/* 存储 */
}

/************************************************************
*
* Function name	: app_get_device_param_function
* Description	: 获取设备参数
* Parameter		: 
* Return		: 
*	
************************************************************/
void *app_get_device_param_function(void)
{
	return (&sg_sysparam_t.device);
}

/************************************************************
*
* Function name	: app_match_password_function
* Description	: 密码比较函数
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t app_match_password_function(char *password)
{
	if(strcmp(password , (char*)sg_sysparam_t.device.password)==0)
	{
		return 0;
	}
	return -1;
}
/************************************************************
*
* Function name	: app_match_set_code_function
* Description	: 确认是否需要需要修改默认密码
* Parameter		: 
* Return		: 1-需要修改
*	
************************************************************/
int8_t app_match_set_code_function(void)
{
	return sg_sysparam_t.device.default_password;
}

/************************************************************
*
* Function name	: app_set_device_param_function
* Description	: 设置设备参数
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_device_param_function(struct device_param param)
{
	memset((uint8_t*)&sg_sysparam_t.device,0,sizeof(struct device_param));
	memcpy((uint8_t*)&sg_sysparam_t.device,&param,sizeof(struct device_param));
	
//	STMFLASH_Write(DEVICE_ID_ADDR,(uint32_t*)param.id.c,4);
//	STMFLASH_Write(DEVICE_ID_ADDR,(uint32_t*)param.id.c,1);	
	STMFLASH_Write_SAVE(DEVICE_FLASH_STORE,DEVICE_ID_ADDR,(uint32_t*)param.id.c,1);
	/* 保存 */
	app_set_save_infor_function(SAVE_DEVICE_PARAM);
}

/************************************************************
*
* Function name	: app_set_code_function
* Description	: 设置密码
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_code_function(struct device_param param)
{
	sg_sysparam_t.device.default_password = 0;
	memcpy(sg_sysparam_t.device.password,param.password,sizeof(param.password));
	
	/* 保存 */
	app_set_save_infor_function(SAVE_DEVICE_PARAM);
}

/************************************************************
*
* Function name	: app_get_com_heart_time
* Description	: 获取心跳参数
* Parameter		: 
* Return		: 
*	
************************************************************/
uint16_t app_get_com_heart_time(void)
{
	return sg_comparam_t.heart;
}

/************************************************************
*
* Function name	: app_set_next_report_time
* Description	: 设置上报间隔时间
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_next_report_time(uint16_t time)
{
	/* 将时间单位转换为ms */
	sg_comparam_t.report = time*1000;
		
	app_set_save_infor_function(SAVE_COM_PARAMETER);
}

/************************************************************
*
* Function name	: app_set_next_report_time_other
* Description	: 设置上报间隔时间-可选择
* Parameter		: 
*	@time		: 上报时间
*	@sel		: 设置编号
* Return		: 
*	
************************************************************/
void app_set_next_report_time_other(uint16_t time,uint8_t sel)
{
	if(sel == 1) {
		sg_comparam_t.report = time*1000;
		app_set_save_infor_function(SAVE_COM_PARAMETER);
	}
}

/************************************************************
*
* Function name	: app_set_next_ping_time
* Description	: 设置下一次ping的时间
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_next_ping_time(uint16_t time, uint8_t time_dev)
{
	/* 将时间单位转换为ms */
	sg_comparam_t.ping = time*1000;
	sg_comparam_t.dev_ping = time_dev*1000;
	
	app_set_save_infor_function(SAVE_COM_PARAMETER);
}
/************************************************************
*
* Function name	: app_set_network_delay_time
* Description	: // 设置网络延时时间  20220308
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_network_delay_time(uint8_t time_dev)
{
	/* 将时间单位转换为ms */
	sg_comparam_t.network_time = time_dev;
	app_set_save_infor_function(SAVE_COM_PARAMETER);
}

/************************************************************
*
* Function name	: app_get_current_time
* Description	: 获取当前时间
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_get_current_time(char *time)
{
	
	sprintf(time,"%04d/%02d/%02d %02d:%02d:%02d",sg_rtctime_t.year,\
												 sg_rtctime_t.month,\
												 sg_rtctime_t.data,\
												 sg_rtctime_t.hour,\
												 sg_rtctime_t.min,\
												 sg_rtctime_t.sec);
}
/************************************************************
*
* Function name	: app_get_current_times
* Description	: 获取当前时间
* Parameter		: 
* Return		: 
*	
************************************************************/
void *app_get_current_times(void)
{
	return &sg_rtctime_t;
}

/************************************************************
*
* Function name	: app_get_report_current_time
* Description	: 获取当前上报的实时时间
* Parameter		: 
*	@mode		: 0：秒 1：毫秒
* Return		: 
*	
************************************************************/
uint8_t* app_get_report_current_time(uint8_t mode)
{
	static uint8_t times[20];
	
	memset(times,0,sizeof(times));
	if(mode == 0) {
		sprintf((char*)times,"%04d%02d%02d%02d%02d%02d",sg_rtctime_t.year,
													sg_rtctime_t.month,
													sg_rtctime_t.data,
													sg_rtctime_t.hour,
													sg_rtctime_t.min,
													sg_rtctime_t.sec);
	} 
	else if(mode == 1) 
	{
		sprintf((char*)times,"%04d%02d%02d%02d%02d%02d",sg_rtctime_t.year,
													sg_rtctime_t.month,
													sg_rtctime_t.data,
													sg_rtctime_t.hour,
													sg_rtctime_t.min,
													sg_rtctime_t.sec);
		times[14] = sg_rtctime_t.hour%10+'0';
		times[15] = sg_rtctime_t.min%10+'0';
		times[16] = sg_rtctime_t.sec%10+'0';
	}
	else if(mode == 2) 
	{
		sprintf((char*)times,"%04d%02d%02d",sg_rtctime_t.year,
													sg_rtctime_t.month,
													sg_rtctime_t.data);
	} 
	return times;
	
}

/************************************************************
*
* Function name	: app_set_current_time
* Description	: 设置时间
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_current_time(int *time,uint8_t conv)
{
	rtc_time_t time_t;
	
	time_t.year  = time[0];
	time_t.month = time[1];
	time_t.data  = time[2];
	time_t.hour  = time[3];
	time_t.min   = time[4];
	time_t.sec   = time[5];
	
	if(conv) // 是否需要转换
	{
		rtc_time_t conv_time;
		local_to_utc_time(&conv_time,8,time_t);
		RTC_set_Time(conv_time);
	}
	else
	RTC_set_Time(time_t);
}

/************************************************************
*
* Function name	: app_get_next_ping_time
* Description	: 获取ping的间隔时间
* Parameter		: 
* Return		: 
*	
************************************************************/
uint32_t app_get_next_ping_time(void)
{
	return sg_comparam_t.ping;
}

/************************************************************
*
* Function name	: app_get_next_dev_ping_time
* Description	: 获取下一个设备的ping时间
* Parameter		: 
* Return		: 
*	
************************************************************/
uint32_t app_get_next_dev_ping_time(void)
{
	return sg_comparam_t.dev_ping;
}

/************************************************************
*
* Function name	: app_get_report_time
* Description	: 获取通信上报间隔时间
* Parameter		: 
* Return		: 
*	
************************************************************/
uint32_t app_get_report_time(void) 
{
	return sg_comparam_t.report;
}
/************************************************************
*
* Function name	: app_get_report_time
* Description	: 获取通信上报间隔时间
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t app_get_network_delay_time(void) 
{
	return sg_comparam_t.network_time;
}
/************************************************************
*
* Function name	: app_get_device_reload_time
* Description	: 获取重启时间
* Parameter		: 
* Return		: 
*	  20240903
************************************************************/
uint32_t app_get_device_reload_time(void) 
{
	return sg_comparam_t.reload;
}
/************************************************************
*
* Function name	: app_get_main_network_ping_ip_addr
* Description	: 获取主网络ping地址
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_get_main_network_ping_ip_addr(uint8_t* ip)
{
	ip[0] = sg_sysparam_t.local.ping_ip[0];
	ip[1] = sg_sysparam_t.local.ping_ip[1];
	ip[2] = sg_sysparam_t.local.ping_ip[2];
	ip[3] = sg_sysparam_t.local.ping_ip[3];
}

/************************************************************
*
* Function name	: app_get_main_network_sub_ping_ip_addr
* Description	: 获取主网pingip - 2
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_get_main_network_sub_ping_ip_addr(uint8_t* ip)
{
	ip[0] = sg_sysparam_t.local.ping_sub_ip[0];
	ip[1] = sg_sysparam_t.local.ping_sub_ip[1];
	ip[2] = sg_sysparam_t.local.ping_sub_ip[2];
	ip[3] = sg_sysparam_t.local.ping_sub_ip[3];
}
	
/************************************************************
*
* Function name	: app_set_main_network_ping_ip
* Description	: 设置主网检测IP
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_main_network_ping_ip(uint8_t *ip)
{
	sg_sysparam_t.local.ping_ip[0] = ip[0];
	sg_sysparam_t.local.ping_ip[1] = ip[1];
	sg_sysparam_t.local.ping_ip[2] = ip[2];
	sg_sysparam_t.local.ping_ip[3] = ip[3];
	
	sg_sysparam_t.local.ping_sub_ip[0] = ip[4];
	sg_sysparam_t.local.ping_sub_ip[1] = ip[5];
	sg_sysparam_t.local.ping_sub_ip[2] = ip[6];
	sg_sysparam_t.local.ping_sub_ip[3] = ip[7];
	
	/* 保存 */
	app_set_save_infor_function(SAVE_LOCAL_NETWORK);
}

/************************************************************
*
* Function name	: app_set_com_interface_selection_function
* Description	: 通信接口选择函数
* Parameter		: 
*	@mode		: 0:有线 1:外网
* Return		: 
*	
************************************************************/
void app_set_com_interface_selection_function(uint8_t mode)
{
	sg_sysoperate_t.com.send_mode = mode;
}

/************************************************************
*
* Function name	: app_send_once_heart_infor
* Description	: 立刻进行一次心跳发生
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_send_once_heart_infor(void)
{
	sg_sysoperate_t.com_flag.heart_pack = 1;
}

/************************************************************
*
* Function name	: app_get_com_time_infor
* Description	: 获取通信间隔时间
* Parameter		: 
* Return		: 
*	
************************************************************/
void *app_get_com_time_infor(void)
{
	return &sg_comparam_t;
}

/************************************************************
*
* Function name	: app_get_comparision_param
* Description	: 获取对比参数：加热、风扇、时间、摄像头
* Parameter		: 
* Return		: 
*	
************************************************************/
void *app_get_comparision_param(void)
{
	return &sg_comparisionparam_t;
}


/************************************************************
*
* Function name	: app_get_network_mode
* Description	: 获取网络模式
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t app_get_network_mode(void) 
{
	switch(sg_sysparam_t.local.server_mode) 
	{
			case 1:		return 1;
			case 2:		return 2;
		  case 3:		return 3;
			default:	return 4;
	}
}
/************************************************************
*
* Function name	: app_get_carema_search_mode
* Description	: 获取摄像机搜索协议
* Parameter		: 
* Return		: 
*	   20230810
************************************************************/
uint8_t app_get_carema_search_mode(void) 
{
	return sg_sysparam_t.local.search_mode;
}

/************************************************************
*
* Function name	: app_set_com_time_param_function
* Description	: 设置通信相关时间参数:ping、上报
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_com_time_param_function(uint32_t *time,uint8_t mode) 
{
	if(mode == 0) 
	{
		sg_comparam_t.dev_ping = time[1]*1000;
		sg_comparam_t.ping     = time[0]*1000;
		sg_comparam_t.reload     = time[3]*3600;    // 重启时间       20240904
	} 
	else 	if(mode == 1) 
	{
		sg_comparam_t.report  = time[0]*1000;
	}
	else 	if(mode == 2)   // 网络延时时间  20220308
	{
		sg_comparam_t.network_time  = time[0];
	}
	/* 保存 */
	app_set_save_infor_function(SAVE_COM_PARAMETER);
}

/************************************************************
*
* Function name	: app_get_report_switch_status
* Description	: 获取上报开关状态
* Parameter		: 
* Return		: 
*	  20230722 FLAG_WATER_ERROR|FLAG_WATER_NOEMAL|FLAG_SPD|FLAG_ANGLE|FLAG_TEMP_HIGH|FLAG_TEMP_LOW|\
														FLAG_HUMI_HIGH|FLAG_HUMI_LOW|FLAG_DOOR|FLAG_POWER|FLAG_AC_STATUS;
************************************************************/
void app_get_report_switch_status(uint8_t *sw)
{
	sw[0]=((sg_sysparam_t.report.report_allowed & FLAG_WATER_ERROR) == FLAG_WATER_ERROR)?1:0;
	sw[1]=((sg_sysparam_t.report.report_allowed & FLAG_WATER_NOEMAL)== FLAG_WATER_NOEMAL)?1:0;
	sw[2]=((sg_sysparam_t.report.report_allowed & FLAG_SPD)  				== FLAG_SPD)?1:0;
	sw[3]=((sg_sysparam_t.report.report_allowed & FLAG_ANGLE)  			== FLAG_ANGLE)?1:0;
	sw[4]=((sg_sysparam_t.report.report_allowed & FLAG_TEMP_HIGH)  	== FLAG_TEMP_HIGH)?1:0;
	sw[5]=((sg_sysparam_t.report.report_allowed & FLAG_TEMP_LOW)		== FLAG_TEMP_LOW)?1:0;
	sw[6]=((sg_sysparam_t.report.report_allowed & FLAG_HUMI_HIGH) 	== FLAG_HUMI_HIGH)?1:0;
	sw[7]=((sg_sysparam_t.report.report_allowed & FLAG_HUMI_LOW)  	== FLAG_HUMI_LOW)?1:0;
	sw[8]=((sg_sysparam_t.report.report_allowed & FLAG_DOOR)  			== FLAG_DOOR)?1:0;
	sw[9]=((sg_sysparam_t.report.report_allowed & FLAG_POWER)  			== FLAG_POWER)?1:0;
	sw[10]=((sg_sysparam_t.report.report_allowed & FLAG_AC_STATUS)  == FLAG_AC_STATUS)?1:0;	
	sw[11]=((sg_sysparam_t.report.report_allowed & FLAG_FAN)  == FLAG_FAN)?1:0;
}

/************************************************************
*
* Function name	: app_set_report_switch_status
* Description	: 设置上报开关状态
* Parameter		: 
* Return		: 
*	  20230722 
************************************************************/
void app_set_report_switch_status(uint8_t sw,REPORT_FLAG sel)
{
	switch(sel) 
	{
		case FLAG_W_E:
			if(sw)
				sg_sysparam_t.report.report_allowed |= FLAG_WATER_ERROR;
			else
				sg_sysparam_t.report.report_allowed &=~FLAG_WATER_ERROR;
			break;
		case FLAG_W_N:
			if(sw)
				sg_sysparam_t.report.report_allowed |= FLAG_WATER_NOEMAL;
			else
				sg_sysparam_t.report.report_allowed &=~FLAG_WATER_NOEMAL;
			break;
		case FLAG_S:
			if(sw)
				sg_sysparam_t.report.report_allowed |= FLAG_SPD;
			else
				sg_sysparam_t.report.report_allowed &=~FLAG_SPD;
			break;
		case FLAG_A:
			if(sw)
				sg_sysparam_t.report.report_allowed |= FLAG_ANGLE;
			else
				sg_sysparam_t.report.report_allowed &=~FLAG_ANGLE;
			break;
		case FLAG_T_H:
			if(sw)
				sg_sysparam_t.report.report_allowed |= FLAG_TEMP_HIGH;
			else
				sg_sysparam_t.report.report_allowed &=~FLAG_TEMP_HIGH;
			break;
		case FLAG_T_L:
			if(sw)
				sg_sysparam_t.report.report_allowed |= FLAG_TEMP_LOW;
			else
				sg_sysparam_t.report.report_allowed &=~FLAG_TEMP_LOW;
			break;
		case FLAG_H_H:
			if(sw)
				sg_sysparam_t.report.report_allowed |= FLAG_HUMI_HIGH;
			else
				sg_sysparam_t.report.report_allowed &=~FLAG_HUMI_HIGH;
			break;
		case FLAG_H_L:
			if(sw)
				sg_sysparam_t.report.report_allowed |= FLAG_HUMI_LOW;
			else
				sg_sysparam_t.report.report_allowed &=~FLAG_HUMI_LOW;
			break;
		case FLAG_D:
			if(sw)
				sg_sysparam_t.report.report_allowed |= FLAG_DOOR;
			else
				sg_sysparam_t.report.report_allowed &=~FLAG_DOOR;
			break;
		case FLAG_P:
			if(sw)
				sg_sysparam_t.report.report_allowed |= FLAG_POWER;
			else
				sg_sysparam_t.report.report_allowed &=~FLAG_POWER;
			break;
		case FLAG_AC:
			if(sw)
				sg_sysparam_t.report.report_allowed |= FLAG_AC_STATUS;
			else
				sg_sysparam_t.report.report_allowed &=~FLAG_AC_STATUS;
			break;
		case FLAG_FAN:
			if(sw)
				sg_sysparam_t.report.report_allowed |= FLAG_FAN;
			else
				sg_sysparam_t.report.report_allowed &=~FLAG_FAN;
			break;
	}
	/* 存储 */
	app_set_save_infor_function(SAVE_REPORT_SW);
}


const char cg_network_no[] = {0xe6,0x97,0xa0,0xe8,0xbf,0x9e,0xe6,0x8e,0xa5,0x00};									// 未连接
const char cg_network_lan[] = {0xe6,0x9c,0x89,0xe7,0xba,0xbf,0xe5,0xb7,0xb2,0xe8,0xbf,0x9e,0xe6,0x8e,0xa5,0x00};	// 有线已连接
const char cg_netwokr_gprs[] = {0xe6,0x97,0xa0,0xe7,0xba,0xbf,0xe5,0xb7,0xb2 ,0xe8,0xbf,0x9e,0xe6,0x8e,0xa5,0x00};	// 无线已连接
const char cg_network_all[] = {0xe5,0x90,0x8c,0xe6,0x97,0xb6,0xe5,0x9c,0xa8,0xe7,0xba,0xbf,0x00};	 // 同时在线
const char cg_network_accept[] = {0xe7,0x9b,0x91,0xe5,0x90,0xac,0xe6,0xa8,0xa1,0xe5,0xbc,0x8f};	// 监听模式
const char cg_netwokr_clientok[] = {0xe5,0xae,0xa2,0xe6,0x88,0xb7,0xe7,0xab,0xaf,0xe5,0xb7,0xb2,0xe8,0xbf,0x9e,0xe6,0x8e,0xa5};	// 客户端已连接

/************************************************************
*
* Function name	: app_get_network_connect_status
* Description	: 获取网络连接状态
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_get_network_connect_status(char *buff)
{
	if((gsm_get_network_connect_status_function()==1)&&(eth_get_tcp_status()==2))
		sprintf(buff,"%s",cg_network_all);
	else
	{
		if(eth_get_tcp_status() == 2 )
			sprintf(buff,"%s",cg_network_lan);
		else if( gsm_get_network_connect_status_function() == 1 )
			sprintf(buff,"%s",cg_netwokr_gprs);	
		else
			sprintf(buff,"%s",cg_network_no);
	}

}

/************************************************************
*
* Function name	: app_get_device_name
* Description	: 获取设备名称
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t *app_get_device_name(void)
{
	return sg_sysparam_t.device.name;
}

/************************************************************
*
* Function name	: 
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_detect_function(void)
{
}

/************************************************************
*
* Function name	: app_get_vlot_protec_status
* Description	: 获取电压保护状态
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t app_get_vlot_protec_status(void)
{
	return sg_sysoperate_t.sys.volt_protection;
}

/************************************************************
*
* Function name	: app_get_current_status
* Description	: 获取电流状态
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t app_get_current_status(void)
{
	return sg_sysoperate_t.sys.current_protection;
}

/************************************************************
*
* Function name	: app_set_threshold_param_function
* Description	: 设置阈值
* Parameter		: 
* Return		: 
*	   20230720
************************************************************/
void app_set_threshold_param_function(struct threshold_params param)
{
	sg_sysparam_t.threshold.volt_max = param.volt_max;
	sg_sysparam_t.threshold.volt_min = param.volt_min; 
	sg_sysparam_t.threshold.current = param.current;
	sg_sysparam_t.threshold.angle = param.angle; 
	sg_sysparam_t.threshold.humi_high = param.humi_high;
	sg_sysparam_t.threshold.humi_low = param.humi_low; 
	sg_sysparam_t.threshold.temp_high = param.temp_high;
	sg_sysparam_t.threshold.temp_low = param.temp_low; 
	save_stroage_threshold_parameter(&sg_sysparam_t.threshold); 
}
/************************************************************
*
* Function name	: app_get_threshold_param_function
* Description	: 获取阈值
* Parameter		: 
* Return		: 
*	   20230720
************************************************************/
void *app_get_threshold_param_function(void)
{
	return (&sg_sysparam_t.threshold);
}

/************************************************************
*
* Function name	: app_get_fault_code_function
* Description	: 获取故障码
* Parameter		: 
* Return		: 
*	   20230720
************************************************************/
uint16_t app_get_fault_code_function(void)
{
	return 0;
}

/************************************************************
*
* Function name	: app_get_backups_param_function
* Description	: 获取备份数据
* Parameter		: 
* Return		: 
*	   20231022
************************************************************/
void *app_get_backups_param_function(void)
{
	return (&sg_backups_t);
}
/************************************************************
*
* Function name	: app_server_link_status_function
* Description	: 服务器连接时间判断
* Parameter		: 
* Return		: 
*	   20231022
************************************************************/
void app_server_link_status_function(void)
{
	static uint32_t server_time_count = 0;

	server_time_count++;
	if(server_time_count >= SERVER_LINK_TIME)
	{
		server_time_count = 0;
		if(sg_backups_t.config_flag == 1)  // 配置过服务器
		{
			sg_backups_t.config_flag = 0;
			save_stroage_backups_function(&sg_backups_t);
			if((eth_get_tcp_status() != 2 ) && ( gsm_get_network_connect_status_function() != 1 ))
			{
				sg_sysoperate_t.save_flag.erase_server_ip = 1; // 擦除服务器配置
			}
		}
	}
}

/************************************************************
*
* Function name	: app_save_backups_remote_param_function
* Description	: 备份服务器信息
* Parameter		: 
* Return		: 
*	   20231022
************************************************************/
void app_save_backups_remote_param_function(void)
{
	memset(sg_backups_t.remote.outside_iporname,0,sizeof(sg_backups_t.remote.outside_iporname));
	strcpy((char*)sg_backups_t.remote.outside_iporname,(char*)sg_sysparam_t.remote.outside_iporname);	
	sg_backups_t.remote.outside_port = sg_sysparam_t.remote.outside_port;
	sg_backups_t.config_flag = 1;
	
	save_stroage_backups_function(&sg_backups_t); // 存储备份信息
}

/************************************************************
*
* Function name	: app_power_fail_protection_function
* Description	: 掉电保护
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_power_fail_protection_function(void)
{
	/* 开关全部关闭 */
	if(relay_get_status_function(RELAY_1) == RELAY_ON) 
		relay_control(RELAY_1,RELAY_OFF);
	
	if(relay_get_status_function(RELAY_2) == RELAY_ON) 
		relay_control(RELAY_2,RELAY_OFF);

	if(relay_get_status_function(RELAY_3) == RELAY_ON) 
		relay_control(RELAY_3,RELAY_OFF);
	
	if(relay_get_status_function(RELAY_4) == RELAY_ON) 
		relay_control(RELAY_4,RELAY_OFF);
	
	if(relay_get_status_function(RELAY_5) == RELAY_ON) 
		relay_control(RELAY_5,RELAY_OFF);
	
	if(relay_get_status_function(RELAY_6) == RELAY_ON) 
		relay_control(RELAY_6,RELAY_OFF);

	if(relay_get_status_function(RELAY_7) == RELAY_ON) 
		relay_control(RELAY_7,RELAY_OFF);
	
	if(relay_get_status_function(RELAY_8) == RELAY_ON) 
		relay_control(RELAY_8,RELAY_OFF);
}
/************************************************************
*
* Function name	: app_power_open_protection_function
* Description	: 打开继电器
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_power_open_protection_function(void)
{
	/* 开关全部打开 */
	relay_control(RELAY_1,RELAY_ON); // 开继电器
	OSTimeDlyHMSM(0,0,1,0);  			// 延时10ms
	relay_control(RELAY_2,RELAY_ON); // 开继电器
	OSTimeDlyHMSM(0,0,1,0);  			// 延时10ms
	relay_control(RELAY_3,RELAY_ON); // 开继电器
	OSTimeDlyHMSM(0,0,1,0);  			// 延时10ms
	relay_control(RELAY_4,RELAY_ON); // 开继电器
	OSTimeDlyHMSM(0,0,1,0);  			// 延时10ms
	relay_control(RELAY_5,RELAY_ON); // 开继电器
	OSTimeDlyHMSM(0,0,1,0);  			// 延时10ms
	relay_control(RELAY_6,RELAY_ON); // 开继电器
	OSTimeDlyHMSM(0,0,1,0);  			// 延时10ms
	relay_control(RELAY_7,RELAY_ON); // 开继电器
	OSTimeDlyHMSM(0,0,1,0);  			// 延时10ms
	relay_control(RELAY_8,RELAY_ON); // 开继电器
}
/************************************************************
*
* Function name	: app_open_exec_task_function
* Description	: 开关状态执行函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_open_exec_task_function(void)
{
	/* 检测设备是否需要散热 */
	if( det_get_inside_temp() < (sg_sysparam_t.threshold.temp_high-10))
	{
		fan_control(FAN_1,FAN_OFF);
		fan_control(FAN_2,FAN_OFF);
	}
	else if( det_get_inside_temp() > sg_sysparam_t.threshold.temp_high)
	{
		fan_control(FAN_1,FAN_ON);
		fan_control(FAN_2,FAN_ON);
	}
}

/************************************************************
*
* Function name	: app_send_data_task_function
* Description	: 发送数据
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_send_data_task_function(void)
{
	gsm_send_tcp_data(sg_send_buff,sg_send_size);
}

/************************************************************
*
* Function name	: app_set_http_com_status_function
* Description	: 设置当前http状态
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_http_websocket_status_function(uint8_t status)
{
	sg_sysoperate_t.http_com_flag.proactive = status;
}


/************************************************************
*
* Function name	: app_get_device_uuid
* Description	: 获取UUID
* Parameter		: 
* Return		: 
*	
************************************************************/
void *app_get_device_uuid(void)
{
	return sg_sysoperate_t.uuid_t.uuid;
}
/************************************************************
*
* Function name	: app_fan_timer_function
* Description	: 风扇记时相关函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_fan_timer_function(void)
{
	static uint16_t fan_count = 0;
	if((fan_get_status_function(FAN_1) == FAN_ON)||(fan_get_status_function(FAN_2) == FAN_ON))
	{
		fan_current_time++;
		sg_comparam_t.fan_time++;
		fan_count++;
	}
	if(fan_count >= 600) // 10min中保存一次
	{
		fan_count = 0;
		app_set_save_infor_function(SAVE_COM_PARAMETER);
	}
}

/************************************************************
*
* Function name	: app_get_fan_time
* Description	: 获取风扇运行时间
* Parameter		: 
* Return		: 
*	
************************************************************/
uint32_t app_get_fan_time(uint8_t id)
{
	if(id)
		return sg_comparam_t.fan_time;
	else
		return fan_current_time;
}

/************************************************************
*
* Function name	: my_app_run_param_init
* Description	: 程序运行参数
* Parameter		: 
* Return		: 
*	
************************************************************/
void my_app_run_param_init(void)
{
	run_result_t  sg_run_param = {0};
	save_read_run_param(&sg_run_param);
	sg_run_param.JumpResult = 1; // 跳转成功
	save_write_run_param(sg_run_param);
}



