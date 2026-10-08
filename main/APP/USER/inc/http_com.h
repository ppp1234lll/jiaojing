#ifndef __HTTP_COM_H
#define __HTTP_COM_H

#include "sys.h"
#include "rtc.h"

#define WEBSOCKET_GUID  ("258EAFA5-E914-47DA-95CA-C5AB0DC85B11")
#define TEST_SERVER  ("DESKTOP-1VKLVMK")


// websocket 数据解析
#define PAYLOAD_OTHER_DATA  	0x0   	// 附加数据帧
#define PAYLOAD_STRING   			0x1   	// 文本数据帧
#define PAYLOAD_BIN   				0x2    	// 二进制数据帧
#define PAYLOAD_CLOSE   			0x8			// 关闭连接帧
#define PAYLOAD_PING   				0x9    	// ping
#define PAYLOAD_PONG   				0xA  		// pong

#define SUCCESS   							0x00000001   	// 成功
#define NO_SUPPORT   						0x00000002   	// 功能不支持
#define WATER_OUT   						0x00000004    // 水浸状态
#define DEV_REBOOT   						0x00000008		// 设备重启
#define DEV_INFO   							0x00000010    // 设备信息
#define DEV_PRODUCE   					0x00000020  	// 设备生产信息
#define DEV_VERSION   					0x00000040 		// 设备版本信息
#define ANGLE_DATA   						0x00000080  	// 倾斜度
#define TEMPERATURE_DATA  			0x00000100  	// 温度
#define HUMIDITY_DATA   				0x00000200    // 湿度
#define FAN_TWO_STATUS  				0x00000400    // 风扇状态
#define BOX_STATUS   						0x00000800   	// 机箱门状态
#define NETWORK_VERSION   			0x00001000  	// 网络信息
#define POWER_DATA   						0x00002000  	// 电力参数
#define HTTP_ATTESTATION				0x00004000  	// 认证握手
#define HTTP_CALCULATION_OK			0x00008000  	// 认证计算正确
#define HTTP_CALCULATION_ERROR	0x00010000  	// 认证计算错误
#define SERVER_ERROR   					0x10010000   	// 
#define DOMINS   								0x00020000   
#define BoxDevList   						0x00040000  
#define POWER_PLAN_TIME					0x00080000  
#define POWER_PORT_STATUS				0x00100000  

#define OPERATE_SUCCESS   			0x10000001   	// 操作成功
#define OPERATE_ERROR     			0x10000002   	// 操作失败

#define ADDEVENT_WATERRESET		0x80000001
#define ADDEVENT_WATEROUT 		0x80000002
#define ADDEVENT_ANGLE    		0x80000003
#define ADDEVENT_HUMI_HIGH 		0x80000004
#define ADDEVENT_HUMI_LOW 		0x80000005
#define ADDEVENT_TEMP_HIGH 		0x80000006
#define ADDEVENT_TEMP_LOW 		0x80000007
#define ADDEVENT_DOOR   		  0x80000008
#define ADDEVENT_FAN   		    0x80000009
#define ADDEVENT_POWER_ERROR  0x8000000A
#define ADDEVENT_POWER_STATE  0x8000000B
#define ADDEVENT_NONE         0x8000000F
#define ADDEVENT_ALL          0xF0000001 // 全部添加

#define DELE_EVENT_WATERRESET		0x40000001
#define DELE_EVENT_WATEROUT 		0x40000002
#define DELE_EVENT_ANGLE    		0x40000003
#define DELE_EVENT_HUMI_HIGH 		0x40000004
#define DELE_EVENT_HUMI_LOW 		0x40000005
#define DELE_EVENT_TEMP_HIGH 		0x40000006
#define DELE_EVENT_TEMP_LOW 		0x40000007
#define DELE_EVENT_DOOR   		  0x40000008
#define DELE_EVENT_FAN   		    0x40000009
#define DELE_EVENT_POWER_ERROR  0x4000000A
#define DELE_EVENT_POWER_STATE  0x4000000B
#define DELE_EVENT_NONE         0x4000000F
#define DELE_EVENT_ALL          0xF0000002 // 全部删除

#define HTTP_WEBSOCKET				0x80000000   	// 转换协议
#define HTTP_WEBSOCKET_PING		0xC0000000   	// ping命令


/* 摘要认证 */
#define HTTP_CERTIFIED_HANDSHAKE	("Authentication/UserCheck") // 认证握手
#define HTTP_AUTHORIZATION	("Authorization") // 认证请求

/* 协议转换 */
#define HTTP_UPGRATE_WEBSOCKET 			("Upgrade: websocket\r\nConnection: Upgrade") 
#define HTTP_UPGRATE_WEBSOCKET1 		("Connection: Upgrade") 
#define FUNCTION_HTTP_TO_WEBSOCKET 	("websocket")

/* 操作 */
#define DynamicCapability_GetFunctions 	("GetFunctions") // 获取领域在的功能点
#define DynamicCapability_GetDomains 		("GetDomains") // 获取支持的领域

#define BoxSubModelMgr_GetSmartBoxDevList 	("GetSmartBoxDevList") // 获取机箱管理的设备列表

#define SystemMaintenance_ExportConfigurationFile 	("secretkey") // 导出配置文件
#define SystemMaintenance_SystemReset 							("SystemReset") // 设备系统恢复出厂设置
#define SystemMaintenance_SystemBasicReset 					("SystemBasicReset") // 设备系统恢复默认设置
#define SystemMaintenance_ImportConfigurationFile 	("configurationFile") // 导入配置文件
#define SystemMaintenance_SystemReboot 							("SystemReboot") // 设备系统重启

#define LogMgr_SearchLog 								("searchID") // 日志搜索
#define PortMgr_SearchPortStatus 				("SearchPortStatus") // 查询端口基础状态信息

#define EventSubscription_DeleteEventSubscribeCfg 	("DeleteEventSubscribeCfg") // 删除事件订阅配置
#define EventSubscription_GetEventSubscribeCfg 			("GetEventSubscribeCfg") // 获取事件订阅配置
#define EventSubscription_AddEventSubscribeCfg 			("AddEventSubscribeCfg") // 添加事件订阅配置
#define EventSubscription_ModifyEventSubscribeCfg 	("ModifyEventSubscribeCfg") // 修改事件订阅配置

#define PowerMgr_ModifyPowerPortWorkParamList 	("ModifyPowerPortWorkParamList") // 修改供电口工作参数列表
#define PowerMgr_GetPowerPortStatusList 				("GetPowerPortStatusList") // 获取供电口状态列表

#define UserMgr_GetUserInfoCfg 			("GetUserInfoCfg") // 获取指定用户信息配置
#define UserMgr_ModifyUserInfoCfg 	("ModifyUserInfoCfg") // 修改指定用户信息配置


/* 属性 */
#define WaterOutSense_WaterOutStatus	("WaterOutStatus") // 水浸状态
#define LightningProtectionMgr_LightningProtectionStatus 					("LightningProtectionStatus") // 防雷状态

#define InfoMgr_DeviceLanguage 						("DeviceLanguage") // 设备语言信息
#define InfoMgr_DeviceDescription 				("DeviceDescription") // 设备描述信息
#define InfoMgr_DeviceServiceDescription 	("DeviceServiceDescription") // 生产服务信息
#define InfoMgr_DeviceWebInfo 						("DeviceWebInfo") // 设备Web信息
#define InfoMgr_DeviceVersion 						("DeviceVersion") // 设备版本信息

#define TimeMgr_TimeZone 				("TimeZone") // 时区配置
#define TimeMgr_DST 						("DST") // 夏令时配置
#define TimeMgr_NTPCfg 					("NTPCfg") // NTP配置
#define TimeMgr_NTPServiceCfg 	("NTPServiceCfg") // NTP服务配置
#define TimeMgr_SystemDateTime 	("SystemDateTime") // 系统时间

#define DeviceTiltDetection_DevRealtimeTiltData 			("DevRealtimeTiltData") // 设备实时倾斜数据
#define DeviceTiltDetection_DevTiltDetectionParam 		("DevTiltDetectionParam") // 设备倾斜检测参数

#define Humiture_HumidityAlarmThreshold 			("HumidityAlarmThreshold") // 湿度告警阈值
#define Humiture_Temperature 									("Temperature") // 温度
#define Humiture_TemperatureAlarmThreshold 		("TemperatureAlarmThreshold") // 温度告警阈值
#define Humiture_Humidity 										("Humidity") // 湿度
#define Humiture_TemperatureUnit 							("TemperatureUnit") // 测温单位
#define Fan_FanStatus 									("FanStatus") // 风扇状态
#define BoxDoorMgr_BoxDoorStatus 				("BoxDoorStatus") // 机箱门状态
#define BoxDoorMgr_BoxDoorDetectionCfg 	("BoxDoorDetectionCfg") // 箱门状态检测配置

#define NetworkAddress_IPAddressCfgList 	("IPAddressCfgList") // 网络接口网络地址配置列表

#define PowerMgr_PowerPortSwitchTimePlan 				("PowerPortSwitchTimePlan") // 电口开关时间计划
#define PowerMgr_DeviceRealTimePowerParam 			("DeviceRealTimePowerParam") // 设备实时电力参数
#define PowerMgr_PowerPortDevLinkCfg 						("PowerPortDevLinkCfg") // 电口和设备关联配置

#define LocationMgr_LocateCfg 			("LocateCfg") // 设备定位规则参数
#define LocationMgr_LocateStatus 		("LocateStatus") // 设备定位运行状态


/* 事件 */
#define WaterOutSense_WaterOutReset 	("WaterOutReset") // 水浸恢复
#define WaterOutSense_WaterOut 				("WaterOut") // 水浸告警
#define LightningProtectionMgr_DevLightingProtectionStatusReport 	("DevLightingProtectionStatusReport") // 设备防雷状态上报

#define PortMgr_SwitchPortStatusReport 	("SwitchPortStatusReport") // 交换机端口状态上报

#define DeviceTiltDetection_DeviceTiltAlarm 					("DeviceTiltAlarm") // 设备倾斜报警

#define Humiture_HumidityTooHigh 							("HumidityTooHigh") // 湿度过高告警
#define Humiture_HumidityTooLow 							("HumidityTooLow") // 湿度过低告警
#define Humiture_TemperatureTooLow 						("TemperatureTooLow") // 温度过低告警
#define Humiture_TemperatureTooHigh 					("TemperatureTooHigh") // 温度过高告警

#define BoxDoorMgr_BoxDoorStatusReport 	("BoxDoorStatusReport") // 箱门状态上报

#define Fan_FanStatusReport 						("FanStatusReport") // 风扇状态上报

#define PowerMgr_DevicePowerParamAlarm 					("DevicePowerParamAlarm") // 设备电力参数报警
#define PowerMgr_DevPowerStatusReport 					("DevPowerStatusReport") // 设备电源状态上报


#define OTA_VERSION 					("ota/inform/operate") 					// 获取固件版本信息
#define OTA_UPGRADE_VERSION 	("ota/upgradePackageInfo/operateoperate") // 获取可升级版本信息
#define OTA_UPGRADE_CMD 			("ota/upgradeByBinary/operate") // 下发升级命令

#define SERVER_INFO 					("service/operate") 	// 领域
#define ATTRIBUTE_GET 				("attribute/get") 	// 获取属性
#define ATTRIBUTE_SET 				("attribute/set") 	// 设置属性
#define EVENT_REPORT 					("event/report") 	// 时间上报


#define COM_HTTP_MALLOC_SIZE	 (1024)	   // 内存数据申请
#define WEBSOCKET_MALLOC_SIZE	 (512)	   // 内存数据申请

typedef struct
{
	char buffer[COM_HTTP_MALLOC_SIZE];  // 缓存区大小 申请内存时赋值
	uint16_t  flag;
} com_http_t;

typedef struct
{
	uint8_t  method;	 // 方法
	uint8_t  *url;     // URL
	uint8_t  length;	 // 长度
	uint8_t  *type;	   // 数据类型
	uint8_t  *buff;	   // 数据内容
	uint8_t  *host;	   // 主机地址
} com_http_data_t;

typedef struct
{
	uint8_t  fin;	 		// 消息的最后一帧
	uint8_t  rsv;     // 扩展定义
	uint8_t  opcode;	// 解释 Payload 数据
	uint8_t  mask;	  // 掩码
	uint16_t  len;	  	// 数据内容
	uint8_t  masking_key[4];// 掩码解密密钥
	uint8_t  buf[WEBSOCKET_MALLOC_SIZE]; // 任意长度数据
} com_websocket_data_t;

typedef struct
{
	char  cmd[40];	 		// 命令
	char  host_ip[20];	// 主机地址
	char  realm[20];		// 限制域	
	char  nonce[64];		// 随机数		
	char  uri[100];			// URI		
	char  cnonce[50];		// 客户端随机数	
	uint32_t nc;        // 一个16进制的数值
	char  qop[10];			// 		
	char  response[40];	// 响应		
	char  algorithm[10];		// 加密方式
	char  opaque[10];		// 	
	uint8_t http_cmd;		// 命令
	char  method[5];		// 方法	
	uint8_t cilent_id;		// 客户端编号
	uint8_t data_recving;		// 是否接收完成
} com_http_cmd_t;

typedef struct
{
	char  key[30];	 	 // 随机值
	char  accept[30];	 // 验证值
} web_key_t;

typedef struct
{
	char  id[40];		// 事件ID
	uint8_t  uri; 	// 
	char  mode[8]; 	//
	char  list[64];
} sys_event_t;

typedef struct
{
	char  		buf[256];	// 缓存
	uint16_t  len;		 	// 长度
} sys_json_t;

extern web_key_t 		 web_key_data;
extern sys_event_t   sg_event_param2;
extern sys_event_t   sg_event_param3;

/* 函数声明 */
void http_com_buff_init(void);
void http_com_stroage_data(uint8_t *buff,uint16_t len);
int8_t com_deal_http_info_function(void);
int com_http_queue_find_info(uint8_t *msg,uint16_t size);

void http_com_buff_init2(void);
void http_com_stroage_data2(uint8_t *buff,uint16_t len);
int8_t com_deal_http_info_function2(void);
int com_http_queue_find_info2(uint8_t *msg,uint16_t size);

void http_com_buff_init3(void);
void http_com_stroage_data3(uint8_t *buff,uint16_t len);
int8_t com_deal_http_info_function3(void);
int com_http_queue_find_info3(uint8_t *msg,uint16_t size);

void http_com_buff_init4(void);
void http_com_stroage_data4(uint8_t *buff,uint16_t len);
int8_t com_deal_http_info_function4(void);
int com_http_queue_find_info4(uint8_t *msg,uint16_t size);

void http_com_ack_function(char *data, uint16_t *len, char *json_str,uint16_t jsonlen, uint8_t status_ID);
void http_json_ack_status(char* buf,uint16_t *size,uint8_t status_ID);
void http_json_ack_waterout(char* buf,uint16_t *size,uint8_t status_ID);
void http_json_ack_devicedescription(char* buf,uint16_t *size,uint8_t status_ID); // 设备信息
void http_json_ack_deviceservicedescription(char* buf,uint16_t *size,uint8_t status_ID);  // 设备生产信息
void http_json_ack_deviceversion(char* buf,uint16_t *size,uint8_t status_ID); // 设备版本信息
void http_json_ack_angle(char* buf,uint16_t *size,uint8_t status_ID);  // 倾斜度
void http_json_ack_tempature(char* buf,uint16_t *size,uint8_t status_ID); // 温度
void http_json_ack_humidity(char* buf,uint16_t *size,uint8_t status_ID);  // 湿度
void http_json_ack_fanstatus(char* buf,uint16_t *size,uint8_t status_ID);  // 风扇状态
void http_json_ack_doorstatus(char* buf,uint16_t *size,uint8_t status_ID);
void http_json_ack_networkaddress(char* buf,uint16_t *size,uint8_t status_ID);
void http_json_ack_devicerealtimepowerparam(char* buf,uint16_t *size,uint8_t status_ID);
void http_json_ack_domins(char* buf,uint16_t *size,uint8_t status_ID);
void http_json_ack_BoxSubModelMgr(char* buf,uint16_t *size,uint8_t status_ID);
void http_json_ack_PowerPortSwitchTime(char* buf,uint16_t *size,uint8_t status_ID);
void http_json_ack_GetPowerPortStatusList(char* buf,uint16_t *size,uint8_t status_ID);

void http_to_websocket_function(char* data,uint16_t *len);
void http_to_websocket_verification_ack(char* data,uint16_t *len);
void http_websocket_pack_data(char* data,uint16_t *len,uint16_t event_type,uint8_t power_type);  
int http_websocket_analy_data(char* data,uint16_t len,com_websocket_data_t *websocket_data) ;
int http_websocket_event_param(char* data,sys_event_t *sys_event) ;
void http_event_anay_param(sys_event_t *sys_event,uint8_t cliend_id);

void http_websocket_pong_data(char* data,uint16_t *len);
uint16_t websocket_event_alerts_wateroutreset(char* buf);
uint16_t websocket_event_alerts_waterout(char* buf);
uint16_t websocket_event_alerts_devlightingprotectionstatusreport(char* buf);
uint16_t websocket_event_alerts_devicetiltalarm(char* buf);
uint16_t websocket_event_alerts_humiditytoohigh(char* buf);
uint16_t websocket_event_alerts_humiditytoolow(char* buf);
uint16_t websocket_event_alerts_temperaturetoolow(char* buf);
uint16_t websocket_event_alerts_temperaturetoohigh(char* buf);
uint16_t websocket_event_alerts_BoxDoorStatusReport(char* buf);
uint16_t websocket_event_alerts_fan_status(char* buf);
uint16_t websocket_event_alerts_DevicePowerParamAlarm(char* buf,uint8_t typef);
uint16_t websocket_event_alerts_DevPowerStatusReport(char* buf);
void local_to_utc_time(rtc_time_t *utc_time, int8_t timezone, rtc_time_t local_time);

int http_com_deal_configure_angle(char* data);
int http_com_deal_configure_humiture(char* buf);
int http_com_deal_configure_temperature(char* buf);
int http_com_deal_configure_powerport(char* buf);

// HTTP 摘要认证
void http_ack_authentication_algorithms(char* buf,uint16_t *size,uint8_t id);
void http_com_certified_handshake_ack_function(char *data, uint16_t *len, char *json_str,uint16_t jsonlen,uint8_t flag);

void http_com_data_deal(char* data,com_http_cmd_t *com_cmd);
int http_ack_certification_calculations(com_http_cmd_t *com_cmd);
void http_ack_certification_status(char* buf,uint16_t *size,uint8_t flag);
void http_com_queue_time_function(void);

int http_deal_json_param(sys_json_t *json_d,char* data);
uint16_t http_websocket_event_add_all(char* buf);

void http_data_send_function(uint32_t status,uint8_t client_id);


#endif
