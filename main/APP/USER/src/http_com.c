#include "http_com.h"
#include "app.h"
#include "det.h"
#include "relay.h"
#include "eth.h"
#include "rtc.h"
#include "timer.h"
#include "malloc.h"
#include "save.h"
#include "lwip_comm.h"
#include "my_json.h"
#include "appconfig.h"
#include "includes.h"
#include "error.h"
#include "queue.h"
#include "fan.h"
#include "lan8720.h"
#include "rng.h"
#include "w25qxx.h"
#include "MD5.h"
#include "myencrypt.h"
#include "cmox_crypto.h"
#include "tcp_client.h"

//客户端1处理
sys_json_t			sg_json_t 		= {0};
queue_s 				sg_queue_http =	{0};	// 队列1
web_key_t 			web_key_data 	=	{0};
com_http_cmd_t 	com_http_cmd 	=	{0};	
com_websocket_data_t sg_websocket = {0};
sys_event_t     sg_event_param  = {0};
uint8_t    http_buff[1000]      = {0};	
uint8_t	   http_send_buff[4096] = {0}; // 发送缓存区
uint16_t   http_send_length	  	=  0;	 // 发送数据长度

const uint16_t HTTP_STATUS[5] = {HTTP_OK,HTTP_BAD_REQUEST,HTTP_FORBIDDEN,HTTP_NOT_FOUND,HTTP_SERVICE_UNAVAILABLE};

const char HTTP_PUBLIC_ERROR[6][11]={PUBLIC_ERROR_OK,PUBLIC_ERROR_NOT_ACTIVATED,PUBLIC_ERROR_OPERATION_FAILED,
																		 PUBLIC_ERROR_NOT_SUPPORTED,PUBLIC_ERROR_RESOURCES,PUBLIC_ERROR_INCORRECT_PARAM};

const char HTTP_PUBLIC_ERRORMSG[6][40]={ERRORMSG_OK,ERRORMSG_NOT_ACTIVATED,ERRORMSG_OPERATION_FAILED,
																				ERRORMSG_NOT_SUPPORTED,ERRORMSG_RESOURCES,	ERRORMSG_INCORRECT_PARAM};

/************************************************************
*
* Function name	: http_com_buff_init
* Description	: HTTP队列初始化
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_com_buff_init(void)
{
	Init_Queue(&sg_queue_http);
}
/************************************************************
*
* Function name	: http_com_stroage_data
* Description	: 将数据存储到缓存区
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_com_stroage_data(uint8_t *buff,uint16_t len)
{
	Enqueue_Bytes_From_Buffer(buff,&sg_queue_http,len);
}

/************************************************************
*
* Function name	: com_deal_http_info_function
* Description	: HTTP数据处理
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t com_deal_http_info_function(void)
{
	struct local_ip_t  *local = app_get_local_network_function();	
	int size	= 0;
	char *str = NULL;
	char local_ipbuf[20] = {0};
	
	size = com_http_queue_find_info(http_buff,1000);
	if((size != 0)&&(com_http_cmd.data_recving == 0))
	{	
		if(lwipdev.client_websocket_id == 1)
		{
			http_websocket_analy_data((char*)http_buff,0,&sg_websocket);
			if(HTTP_COM_DEBUG) printf("websocket_analy_data:%s\n",sg_websocket.buf);
			
			if(sg_websocket.opcode == PAYLOAD_PING)
				http_data_send_function(HTTP_WEBSOCKET_PING,1);
			else
			{
				http_websocket_event_param((char*)sg_websocket.buf,&sg_event_param); // 处理事件信息
				http_event_anay_param(&sg_event_param,1);
			}
		}
		else
		{
			memset(local_ipbuf,0,20);
			sprintf(local_ipbuf,"%d.%d.%d.%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
			str = strstr((char *)http_buff,HTTP_UPGRATE_WEBSOCKET1); // 判断是否是协议转换
			if(str != NULL)
			{
				sprintf(local_ipbuf,"%d.%d.%d.%d:%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3],local->port);
				str = strstr((char *)http_buff,"\r\nHost");
				sscanf((char*)str,"%*[^: ]: %[^\r\n]",com_http_cmd.host_ip); 
				str = strstr((char *)str,"Sec-WebSocket-Key:");
				sscanf((char*)str,"%*[^: ]: %[^\r\n]",web_key_data.key); 
				com_http_cmd.http_cmd = 1;
				if((strcmp(com_http_cmd.host_ip,local_ipbuf) == 0)|| // 判断IP是否正确
					 (strcmp(com_http_cmd.host_ip,TEST_SERVER) == 0))
				{
					if(HTTP_COM_DEBUG) printf("HTTP_WEBSOCKET\n");
					if(lwipdev.client_websocket_id == 0)
						lwipdev.client_websocket_id = 1; // 判断客户端编号
					com_http_cmd.http_cmd = 0;
					http_data_send_function(HTTP_WEBSOCKET,1);			
					return 0;
				}
			}
			else
			{
				str = strstr((char *)http_buff,HTTP_CERTIFIED_HANDSHAKE); // 摘要认证
				if(str != NULL)
				{
					str = strstr((char *)http_buff,HTTP_AUTHORIZATION); // 摘要认证
					if(str != NULL)
						com_http_cmd.http_cmd = 3; 
					else
						com_http_cmd.http_cmd = 2; // 首次返回
				}
				http_com_data_deal((char *)http_buff,&com_http_cmd);
			}
			if(HTTP_COM_DEBUG) printf("com_http_cmd:%s......%s\n",com_http_cmd.cmd,com_http_cmd.host_ip);
			if(strcmp(com_http_cmd.host_ip,local_ipbuf) == 0) // 判断IP是否正确
			{
				switch(com_http_cmd.http_cmd)
				{
					case 1: // 协议转换
						com_http_cmd.http_cmd = 0;
						if(lwipdev.client_websocket_id == 0)
							lwipdev.client_websocket_id = 1; // 判断客户端编号
						http_data_send_function(HTTP_WEBSOCKET,1);						
						break;

					case 2: // 首次认证
						com_http_cmd.http_cmd = 0;
						http_data_send_function(HTTP_ATTESTATION,1);				
						break;	

					case 3: // 认证结果
						com_http_cmd.http_cmd = 0;
						if(http_ack_certification_calculations(&com_http_cmd) == 1)
							http_data_send_function(HTTP_CALCULATION_OK,1);
						else
							http_data_send_function(HTTP_CALCULATION_ERROR,1);		
						break;		
					default:
						if(http_ack_certification_calculations(&com_http_cmd) == 1)
						{
							if(HTTP_COM_DEBUG) printf("验证成功....\n");
							if(strcmp(com_http_cmd.cmd ,BoxSubModelMgr_GetSmartBoxDevList)==0 ) //获取机箱管理的设备列表
								http_data_send_function(BoxDevList,1);
							else if(strcmp(com_http_cmd.cmd ,DynamicCapability_GetDomains)==0 ) // 水浸
								http_data_send_function(DOMINS,1);
							else if(strcmp(com_http_cmd.cmd ,WaterOutSense_WaterOutStatus)==0 ) // 水浸
								http_data_send_function(WATER_OUT,1);
							else if(strcmp(com_http_cmd.cmd ,InfoMgr_DeviceDescription)==0 ) // 设备信息
								http_data_send_function(DEV_INFO,1);
							else if(strcmp(com_http_cmd.cmd ,InfoMgr_DeviceServiceDescription)==0 ) // 设备生产信息
								http_data_send_function(DEV_PRODUCE,1);
							else if(strcmp(com_http_cmd.cmd ,InfoMgr_DeviceVersion)==0 ) // 设备版本信息
								http_data_send_function(DEV_VERSION,1);
							else if(strcmp(com_http_cmd.cmd ,DeviceTiltDetection_DevRealtimeTiltData)==0 ) // 倾斜度
								http_data_send_function(ANGLE_DATA,1);
							else if(strcmp(com_http_cmd.cmd ,Humiture_Temperature)==0 ) // 温度
								http_data_send_function(TEMPERATURE_DATA,1);
							else if(strcmp(com_http_cmd.cmd ,Humiture_Humidity)==0 ) // 湿度
								http_data_send_function(HUMIDITY_DATA,1);
							else if(strcmp(com_http_cmd.cmd ,Fan_FanStatus)==0 ) // 风扇状态
								http_data_send_function(FAN_TWO_STATUS,1);
							else if(strcmp(com_http_cmd.cmd ,BoxDoorMgr_BoxDoorStatus)==0 ) // 机箱门状态
								http_data_send_function(BOX_STATUS,1);
							else if(strcmp(com_http_cmd.cmd ,NetworkAddress_IPAddressCfgList)==0 ) // 网络信息
								http_data_send_function(NETWORK_VERSION,1);
							else if(strcmp(com_http_cmd.cmd ,PowerMgr_DeviceRealTimePowerParam)==0 ) // 电力参数
								http_data_send_function(POWER_DATA,1);	
							else if(strcmp(com_http_cmd.cmd ,PowerMgr_PowerPortSwitchTimePlan)==0 ) // 电口开关时间
								http_data_send_function(POWER_PLAN_TIME,1);	
							else if(strcmp(com_http_cmd.cmd ,PowerMgr_GetPowerPortStatusList)==0 ) // 获取供电口状态列表
								http_data_send_function(POWER_PORT_STATUS,1);	
							else if(strcmp(com_http_cmd.cmd ,DeviceTiltDetection_DevTiltDetectionParam)==0 ) // 设备倾斜检测参数
							{
								http_deal_json_param(&sg_json_t,(char*)http_buff);							
								if(http_com_deal_configure_angle(sg_json_t.buf)<0)
									http_data_send_function(OPERATE_ERROR,1);	
								else
									http_data_send_function(OPERATE_SUCCESS,1);	
							}
							else if(strcmp(com_http_cmd.cmd ,Humiture_HumidityAlarmThreshold)==0 ) // 湿度告警阈值
							{
								http_deal_json_param(&sg_json_t,(char*)http_buff);							
								if(http_com_deal_configure_humiture(sg_json_t.buf)<0)
									http_data_send_function(OPERATE_ERROR,1);	
								else
									http_data_send_function(OPERATE_SUCCESS,1);				
							}
							else if(strcmp(com_http_cmd.cmd ,Humiture_TemperatureAlarmThreshold)==0 ) // 温度告警阈值
							{
								http_deal_json_param(&sg_json_t,(char*)http_buff);							
								if(http_com_deal_configure_temperature(sg_json_t.buf)<0)
									http_data_send_function(OPERATE_ERROR,1);	
								else
									http_data_send_function(OPERATE_SUCCESS,1);	
							}
							else if(strcmp(com_http_cmd.cmd ,PowerMgr_ModifyPowerPortWorkParamList)==0 ) // 修改供电口工作参数列表
							{
								http_deal_json_param(&sg_json_t,(char*)http_buff);							
								if(http_com_deal_configure_powerport(sg_json_t.buf) < 0 )
									http_data_send_function(OPERATE_ERROR,1);	
								else
									http_data_send_function(OPERATE_SUCCESS,1);										
							}
							else if(strcmp(com_http_cmd.cmd ,SystemMaintenance_SystemReset)==0 ) // 设备系统恢复出厂设置
							{
								W25QXX_Erase_Chip();
								set_reboot_time_function(1000);
							}
							else if(strcmp(com_http_cmd.cmd ,SystemMaintenance_SystemBasicReset)==0 ) // 设备系统恢复默认设置
								app_set_reset_function();
							else if(strcmp(com_http_cmd.cmd ,SystemMaintenance_SystemReboot)==0 ) // 设备系统重启
								set_reboot_time_function(1000);		
							else
								http_data_send_function(NO_SUPPORT,1);
						}
						else
							http_data_send_function(SERVER_ERROR,1);	
						break;				
				}	
				return 0;
			}
			else
			{
				if(HTTP_COM_DEBUG) printf("IP error\n"); // ip错误
				http_data_send_function(SERVER_ERROR,1);	
				return -1;
			}
		}
	}
	else if(size < 0)
	{
		if(HTTP_COM_DEBUG) printf("http size error\n"); // http数据解析错误
		http_data_send_function(SERVER_ERROR,1);	
	}	
	return -1;
}

/*
POST /iot/global/0-global/model/service/operate/BoxSubModelMgr/GetSmartBoxDevList HTTP/1.1
Content-Length: 0
Content-Type: application/json
Authorization: Digest username="admin",realm="DS-2CD2520F",
nonce="4d6a553452444d30525441364e6d4d304e6a68684e47553d",
uri="/iot/global/0-global/model/service/operate/BoxSubModelMgr/GetSmartBoxDevList",
algorithm="MD5",cnonce="bde145af6c8aa404ee63ed21ccdf9ca6",
nc=00000001,qop="auth",response="260148a8cd3ef2cf3076023470268f54"
Host: 37.10.168.39
*/
/************************************************************
*
* Function name	: com_http_queue_find_info
* Description	: 获取一包数据
* Parameter		: 
* Return		: 
*	
************************************************************/
int com_http_queue_find_info(uint8_t *msg,uint16_t size)
{
	char * line_start = NULL;
	char * line_end 	= NULL;
	char * line_str 	= NULL;
	uint16_t length 	= 0;
	int buf_length 	= 0;
	static uint16_t http_length = 0;
	
	if(Get_Queue_Count(&sg_queue_http) > 0 ) // 有数据接收
	{	
		if(com_http_cmd.data_recving == 0)
		{
			memset(msg,0,size);
			memset(&com_http_cmd, 0, sizeof(com_http_cmd_t));
			if(lwipdev.client_websocket_id == 1)
			{
				length = sg_queue_http.count;
				if(sg_queue_http.front > sg_queue_http.rear)
				{
					length = QUEUE_BUF_SIZE-sg_queue_http.front;
					Dequeue_Bytes_To_Buffer(&sg_queue_http,msg,length);
					memset(sg_queue_http.buf+sg_queue_http.front,0,length);	
					
					Dequeue_Bytes_To_Buffer(&sg_queue_http,msg+length,sg_queue_http.rear);
					memset(sg_queue_http.buf,0,sg_queue_http.rear);	
					return length;		
				}
				else	
				{
					Dequeue_Bytes_To_Buffer(&sg_queue_http,msg,sg_queue_http.count);
					memset(sg_queue_http.buf+sg_queue_http.front,0,sg_queue_http.count);	
					return length;		
				}
			}
			else
			{
				line_start = strstr((char *)(sg_queue_http.buf+sg_queue_http.front), "GET");
				if (line_start == NULL)
				{
					line_start = strstr((char *)(sg_queue_http.buf+sg_queue_http.front), "SET");
					if (line_start == NULL)
					{
						line_start = strstr((char *)(sg_queue_http.buf+sg_queue_http.front), "POST");
						if (line_start == NULL)
						{					
							Clear_Queue(&sg_queue_http);  // 清空队列
							return -1;
						}
						else
							sprintf(com_http_cmd.method,"%s","POST"); 	
					}	
					else
						sprintf(com_http_cmd.method,"%s","SET"); 			
				}
				else
					sprintf(com_http_cmd.method,"%s","GET"); 
				
				/* check http end flag */
				if(sg_queue_http.front > sg_queue_http.rear)
					line_end = strstr((char *)sg_queue_http.buf, "\r\n\r\n");
				else	
					line_end = strstr((char *)(sg_queue_http.buf+sg_queue_http.front), "\r\n\r\n");

				line_str = strstr((char *)(sg_queue_http.buf+sg_queue_http.front), "Length:");
				if(line_str != NULL)
					sscanf((char*)line_str,"Length:%d\r\n",&buf_length); 
//				else
//					printf("length error\n");
				if((line_end == NULL)||( line_end < line_start))// 未找到包尾 或 包头包尾错误
				{
					Clear_Queue(&sg_queue_http);  // 清空队列
					return -1;
				}
				if(buf_length !=0)  // 判断后面是否有数据
				{
					sg_json_t.len = buf_length;
					line_str = strstr((char *)line_end, "{");
					if(line_str == NULL) // 后面无数据
					{
						com_http_cmd.data_recving = 1; // 继续接收
						buf_length = 0;
					}
					else
						com_http_cmd.data_recving = 0; // 已经接收完成
				}
				else
					com_http_cmd.data_recving = 0;
			
				line_end = line_end+4+buf_length; // 获取当前所有的数据
				
				if(line_start > line_end)
				{
					length = QUEUE_BUF_SIZE-(line_start-(char *)sg_queue_http.buf);
					Dequeue_Bytes_To_Buffer(&sg_queue_http,msg,length);
					memset(line_start,0,length);	
					Dequeue_Bytes_To_Buffer(&sg_queue_http,msg+length,(line_end-(char *)sg_queue_http.buf));
					memset(sg_queue_http.buf,0,(line_end-(char *)sg_queue_http.buf));
					if(sg_queue_http.count == 0)
						Clear_Queue(&sg_queue_http);  // 清空队列			
					http_length = line_end+QUEUE_BUF_SIZE-line_start;
					return http_length;
				}
				else
				{
					Dequeue_Bytes_To_Buffer(&sg_queue_http,msg,(line_end-line_start));
					if(sg_queue_http.count == 0)
						Clear_Queue(&sg_queue_http);  // 清空队列
					memset(line_start,0,(line_end-line_start));	
					http_length = line_end-line_start;
					return http_length;				
				}	
			}
		}
		else if(com_http_cmd.data_recving == 1)
		{
			com_http_cmd.data_recving = 0;
			if(sg_queue_http.front > sg_queue_http.rear)
			{
				length = QUEUE_BUF_SIZE-sg_queue_http.front;
				Dequeue_Bytes_To_Buffer(&sg_queue_http,msg+http_length,length);
				memset(sg_queue_http.buf+sg_queue_http.front,0,length);	
				
				Dequeue_Bytes_To_Buffer(&sg_queue_http,msg+http_length+length,sg_queue_http.rear);
				memset(sg_queue_http.buf,0,sg_queue_http.rear);	
				return (sg_queue_http.count+http_length);		
			}
			else	
			{
				Dequeue_Bytes_To_Buffer(&sg_queue_http,msg+http_length,sg_queue_http.count);
				memset(sg_queue_http.buf+sg_queue_http.front,0,sg_queue_http.count);	
				return (sg_queue_http.count+http_length);		
			}		
		}
	}
	return 0; 
}


/* HTTP 格式
HTTP/1.1 404 Not Found
Content-Type: application/json; charset="UTF-8"
Content-Length: 0000000079

{"status":404,"code":"0x00100003","errorMsg":"This function is not supported."}
*/
/************************************************************
*
* Function name	: http_com_ack_function
* Description	: HTTP回复数据
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_com_ack_function(char *data, uint16_t *len, char *json_str,uint16_t jsonlen, uint8_t status_ID)
{
	uint16_t str_len = 0;
  str_len = sprintf(data,"%s","HTTP/1.1 "); 	
	switch(status_ID)
	{
		case 0:
			str_len += sprintf(data+str_len,"%s","200 OK\r\nContent-Type: application/json; charset=\"UTF-8\"\r\nContent-Length: ");
		break;
		
		case 1:
			str_len += sprintf(data+str_len,"%s","400 Bad Request\r\nContent-Type: application/json; charset=\"UTF-8\"\r\nContent-Length: ");	
		break;
		
		case 2:
			str_len += sprintf(data+str_len,"%s","403 Forbidden\r\nContent-Type: application/json; charset=\"UTF-8\"\r\nContent-Length: ");	
		break;

		case 3:
			str_len += sprintf(data+str_len,"%s","404 Not Found\r\nContent-Type: application/json; charset=\"UTF-8\"\r\nContent-Length: ");	
		break;

		case 4:
			str_len += sprintf(data+str_len,"%s","500 Internal Server Error\r\nContent-Type: application/json; charset=\"UTF-8\"\r\nContent-Length: ");	
		break;		
	}
	str_len += sprintf(data+str_len,"%010d\r\n\r\n",jsonlen);
	str_len += sprintf(data+str_len,"%s",json_str);
	*len = str_len;
}

/************************************************************
*
* Function name	: http_json_ack_status
* Description	: HTTP回复数据:操作状态
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_json_ack_status(char* buf,uint16_t *size,uint8_t status_ID)
{
	my_json_t js;

	my_json_init(&js, buf, 4000);		/* buf = http_data_send_function 中 mymalloc(4000) 的 ack_json_buf */
	my_json_object_begin(&js, NULL);
	my_json_add_int(&js, "status", HTTP_STATUS[status_ID]);
	my_json_add_str(&js, "code", HTTP_PUBLIC_ERROR[status_ID]);
	my_json_add_str(&js, "errorMsg", HTTP_PUBLIC_ERRORMSG[status_ID]);
	my_json_object_end(&js);

	*size = (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: http_json_ack_waterout
* Description	: 水浸状态
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_json_ack_waterout(char* buf,uint16_t *size,uint8_t status_ID)
{
	my_json_t js;
	uint8_t stauts = 0;

	if(HTTP_COM_DEBUG) printf("http_json_ack_waterout \n");

	if(det_get_water_status() == 1)
		stauts = 2;
	else if(det_get_water_status() == 2)
		stauts = 1;

	my_json_init(&js, buf, 4000);
	my_json_object_begin(&js, NULL);
	my_json_add_int(&js, "status", HTTP_STATUS[status_ID]);
	my_json_add_str(&js, "code", HTTP_PUBLIC_ERROR[status_ID]);
	my_json_add_str(&js, "errorMsg", HTTP_PUBLIC_ERRORMSG[status_ID]);
	my_json_object_begin(&js, "data");
	my_json_add_int(&js, "Value", stauts);
	my_json_object_end(&js);
	my_json_object_end(&js);

	*size = (uint16_t)my_json_len(&js);
}


/************************************************************
*
* Function name	: http_json_ack_devicedescription
* Description	: 设备信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_json_ack_devicedescription(char* buf,uint16_t *size,uint8_t status_ID)
{
	struct device_param 	*device  = app_get_device_param_function();
	struct local_ip_t   	*local   = app_get_local_network_function();
	char temp[20] = {0};
	my_json_t js;

	my_json_init(&js, buf, 4000);
	my_json_object_begin(&js, NULL);
	my_json_add_int(&js, "status", HTTP_STATUS[status_ID]);
	my_json_add_str(&js, "code", HTTP_PUBLIC_ERROR[status_ID]);
	my_json_add_str(&js, "errorMsg", HTTP_PUBLIC_ERRORMSG[status_ID]);

	my_json_object_begin(&js, "data");
	my_json_object_begin(&js, "Value");
	my_json_add_str(&js, "deviceName", "");					// 设备名称
	my_json_add_str(&js, "deviceDescription", "");			// 设备描述
	my_json_add_str(&js, "deviceLocation", "");				// 设备位置
	my_json_add_str(&js, "model", HARD_NO_STR);				// 设备型号

	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d",device->id.i);
	my_json_add_str(&js, "serialNumber", temp);				// 设备序列号
	my_json_add_str(&js, "subSerialNumber", "");			// 子序列号

	memset(temp,0,sizeof(temp));
	sprintf(temp,"%02x:%02x:%02x:%02x:%02x:%02x",
										local->mac[0],local->mac[1],local->mac[2],
										local->mac[3],local->mac[4],local->mac[5]);
	my_json_add_str(&js, "macAddress", temp);				// MAC地址
	my_json_add_str(&js, "ARCID", "");						// ARC编号
	my_json_add_str(&js, "boardModel", "");					// 板卡型号
	my_json_add_str(&js, "mainboardModel", "");				// 主板型号
	my_json_add_str(&js, "chipPlatform", "GD32F107");		// 芯片平台
	my_json_add_str(&js, "deviceType", "MCU");				// 设备类型
	my_json_object_end(&js);								// Value
	my_json_object_end(&js);								// data
	my_json_object_end(&js);								// root

	*size = (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: http_json_ack_deviceservicedescription
* Description	: 设备生产信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_json_ack_deviceservicedescription(char* buf,uint16_t *size,uint8_t status_ID)
{
	my_json_t js;

	my_json_init(&js, buf, 4000);
	my_json_object_begin(&js, NULL);
	my_json_add_int(&js, "status", HTTP_STATUS[status_ID]);
	my_json_add_str(&js, "code", HTTP_PUBLIC_ERROR[status_ID]);
	my_json_add_str(&js, "errorMsg", HTTP_PUBLIC_ERRORMSG[status_ID]);

	my_json_object_begin(&js, "data");
	my_json_object_begin(&js, "Value");
	my_json_add_str(&js, "systemContact", "fengniao");		// 生产商
	my_json_add_str(&js, "companyName", "fengniao");		// 生产公司简称
	my_json_add_str(&js, "copyright", "");					// 版权信息
	my_json_add_str(&js, "supportUrl", "undefined");		// 服务门户网站
	my_json_object_end(&js);								// Value
	my_json_object_end(&js);								// data
	my_json_object_end(&js);								// root

	*size = (uint16_t)my_json_len(&js);
}


/************************************************************
*
* Function name	: http_json_ack_deviceversion
* Description	: 设备版本信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_json_ack_deviceversion(char* buf,uint16_t *size,uint8_t status_ID)
{
	my_json_t js;

	my_json_init(&js, buf, 4000);
	my_json_object_begin(&js, NULL);
	my_json_add_int(&js, "status", HTTP_STATUS[status_ID]);
	my_json_add_str(&js, "code", HTTP_PUBLIC_ERROR[status_ID]);
	my_json_add_str(&js, "errorMsg", HTTP_PUBLIC_ERRORMSG[status_ID]);

	my_json_object_begin(&js, "data");
	my_json_object_begin(&js, "Value");
	my_json_add_str(&js, "firmwareVersion", SOFT_NO_STR);			// 主控版本号
	my_json_add_str(&js, "firmwareReleasedDate", SOFT_NO_DAT);		// 主控版本日期
	my_json_add_str(&js, "firmwareVersionInfo", "");				// 主控版本描述信息
	my_json_add_str(&js, "softwareVersion", SOFT_NO_STR);			// 软件版本信息
	my_json_add_str(&js, "playbackLibraryVersion", "");				// 播放库版本
	my_json_add_str(&js, "kernelVersion", "");						// 内核版本
	my_json_add_str(&js, "DSPVersion", "");							// DSP版本
	my_json_add_str(&js, "BSPVersion", "");							// BSP版本
	my_json_add_str(&js, "FPGAVersion", "");						// FPGA版本
	my_json_add_str(&js, "hardwareVersion", "V1.2");				// 硬件版本
	my_json_object_end(&js);										// Value
	my_json_object_end(&js);										// data
	my_json_object_end(&js);										// root

	*size = (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: http_json_ack_angle
* Description	: 倾斜度
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_json_ack_angle(char* buf,uint16_t *size,uint8_t status_ID)
{
	uint16_t angle_data = det_get_cabinet_posture();
	struct threshold_params *threshol = app_get_threshold_param_function();
	char tiltStatus[10] = {0};
	my_json_t js;

	if(angle_data < threshol->angle)
		sprintf(tiltStatus,"%s","normal");
	else
		sprintf(tiltStatus,"%s","abnormal");

	my_json_init(&js, buf, 4000);
	my_json_object_begin(&js, NULL);
	my_json_add_int(&js, "status", HTTP_STATUS[status_ID]);
	my_json_add_str(&js, "code", HTTP_PUBLIC_ERROR[status_ID]);
	my_json_add_str(&js, "errorMsg", HTTP_PUBLIC_ERRORMSG[status_ID]);

	my_json_object_begin(&js, "data");
	my_json_object_begin(&js, "Value");
	my_json_add_int(&js, "inclination", angle_data);		// 倾斜角度
	my_json_add_str(&js, "tiltStatus", tiltStatus);			// 倾斜状态
	my_json_object_end(&js);								// Value
	my_json_object_end(&js);								// data
	my_json_object_end(&js);								// root

	*size = (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: http_json_ack_tempature
* Description	: 温度
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_json_ack_tempature(char* buf,uint16_t *size,uint8_t status_ID)
{
	my_json_t js;

	my_json_init(&js, buf, 4000);
	my_json_object_begin(&js, NULL);
	my_json_add_int(&js, "status", HTTP_STATUS[status_ID]);
	my_json_add_str(&js, "code", HTTP_PUBLIC_ERROR[status_ID]);
	my_json_add_str(&js, "errorMsg", HTTP_PUBLIC_ERRORMSG[status_ID]);

	my_json_object_begin(&js, "data");
	my_json_add_double(&js, "Value", det_get_inside_temp());	// 温度
	my_json_object_end(&js);								// data
	my_json_object_end(&js);								// root

	*size = (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: http_json_ack_humidity
* Description	: 湿度
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_json_ack_humidity(char* buf,uint16_t *size,uint8_t status_ID)
{
	my_json_t js;

	my_json_init(&js, buf, 4000);
	my_json_object_begin(&js, NULL);
	my_json_add_int(&js, "status", HTTP_STATUS[status_ID]);
	my_json_add_str(&js, "code", HTTP_PUBLIC_ERROR[status_ID]);
	my_json_add_str(&js, "errorMsg", HTTP_PUBLIC_ERRORMSG[status_ID]);

	my_json_object_begin(&js, "data");
	my_json_add_double(&js, "Value", det_get_inside_humi());	// 湿度
	my_json_object_end(&js);								// data
	my_json_object_end(&js);								// root

	*size = (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: http_json_ack_fanstatus
* Description	: 风扇状态
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_json_ack_fanstatus(char* buf,uint16_t *size,uint8_t status_ID)
{
	my_json_t js;
	uint8_t fan_1 = fan_get_status_function(FAN_1);

	my_json_init(&js, buf, 4000);
	my_json_object_begin(&js, NULL);
	my_json_add_int(&js, "status", HTTP_STATUS[status_ID]);
	my_json_add_str(&js, "code", HTTP_PUBLIC_ERROR[status_ID]);
	my_json_add_str(&js, "errorMsg", HTTP_PUBLIC_ERRORMSG[status_ID]);

	my_json_object_begin(&js, "data");
	my_json_object_begin(&js, "Value");
	my_json_array_begin(&js, "fanStatusList");
	my_json_object_begin(&js, NULL);						/* 数组元素对象 */
	my_json_add_int(&js, "ID", FAN_1+1);
	if(fan_1 == 1)
	{
		my_json_add_int(&js, "speed", 3600);
		my_json_add_str(&js, "fanStatus", "normal");
	}
	else
	{
		my_json_add_int(&js, "speed", 0);
		my_json_add_str(&js, "fanStatus", "notPower");
	}
	my_json_add_int(&js, "curRunningTime", app_get_fan_time(0));
	my_json_add_int(&js, "totalRunningTime", app_get_fan_time(1));
	my_json_object_end(&js);								/* 数组元素对象结束 */
	my_json_array_end(&js);									/* fanStatusList */
	my_json_object_end(&js);								/* Value */
	my_json_object_end(&js);								/* data */
	my_json_object_end(&js);								/* root */

	*size = (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: http_json_ack_doorstatus
* Description	: 们状态
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_json_ack_doorstatus(char* buf,uint16_t *size,uint8_t status_ID)
{
	my_json_t js;
	uint8_t door = det_get_open_door();

	my_json_init(&js, buf, 4000);
	my_json_object_begin(&js, NULL);
	my_json_add_int(&js, "status", HTTP_STATUS[status_ID]);
	my_json_add_str(&js, "code", HTTP_PUBLIC_ERROR[status_ID]);
	my_json_add_str(&js, "errorMsg", HTTP_PUBLIC_ERRORMSG[status_ID]);

	my_json_object_begin(&js, "data");
	my_json_object_begin(&js, "Value");
	my_json_array_begin(&js, "doorStatusList");
	my_json_object_begin(&js, NULL);						/* 数组元素对象 */
	my_json_add_int(&js, "doorID", 1);
	if(door == 1)
		my_json_add_str(&js, "doorStatus", "opened");
	else
		my_json_add_str(&js, "doorStatus", "closed");
	my_json_object_end(&js);								/* 数组元素对象结束 */
	my_json_array_end(&js);									/* doorStatusList */
	my_json_object_end(&js);								// Value
	my_json_object_end(&js);								// data
	my_json_object_end(&js);								// root

	*size = (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: http_json_ack_networkaddress
* Description	: 网络地址管理
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_json_ack_networkaddress(char* buf,uint16_t *size,uint8_t status_ID)
{
	struct local_ip_t  *local = app_get_local_network_function();
	char temp[20] = {0};
	my_json_t js;

	my_json_init(&js, buf, 4000);
	my_json_object_begin(&js, NULL);
	my_json_add_int(&js, "status", HTTP_STATUS[status_ID]);
	my_json_add_str(&js, "code", HTTP_PUBLIC_ERROR[status_ID]);
	my_json_add_str(&js, "errorMsg", HTTP_PUBLIC_ERRORMSG[status_ID]);

	my_json_object_begin(&js, "data");
	my_json_object_begin(&js, "Value");
	my_json_array_begin(&js, "ipAddressCfgList");

	my_json_object_begin(&js, NULL);						/* 数组元素对象 */
	my_json_add_int(&js, "id", 1);							// 网口索引

	my_json_object_begin(&js, "ipAddressCfg");
	my_json_add_str(&js, "ipVersion", "ipv4");				// ip地址类型
	my_json_add_str(&js, "ipv4addressingMethod", "static");	// ipv4地址获取方法
	my_json_add_str(&js, "ipv6addressMethod", "undefined");

	my_json_object_begin(&js, "ipAddress");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
	my_json_add_str(&js, "ipv4Address", temp);
	my_json_add_str(&js, "ipv6Address", "undefined");
	my_json_object_end(&js);

	my_json_object_begin(&js, "subnetMask");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d",local->netmask[0],local->netmask[1],local->netmask[2],local->netmask[3]);
	my_json_add_str(&js, "ipv4Address", temp);
	my_json_add_str(&js, "ipv6Address", "undefined");
	my_json_object_end(&js);

	my_json_object_begin(&js, "defaultGateway");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d",local->gateway[0],local->gateway[1],local->gateway[2],local->gateway[3]);
	my_json_add_str(&js, "ipv4Address", temp);
	my_json_add_str(&js, "ipv6Address", "undefined");
	my_json_object_end(&js);

	my_json_object_begin(&js, "primaryDNS");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d",local->dns[0],local->dns[1],local->dns[2],local->dns[3]);
	my_json_add_str(&js, "ipv4Address", temp);
	my_json_add_str(&js, "ipv6Address", "undefined");
	my_json_object_end(&js);

	my_json_object_begin(&js, "secondaryDNS");
	my_json_add_str(&js, "ipv4Address", "undefined");
	my_json_add_str(&js, "ipv6Address", "undefined");
	my_json_object_end(&js);

	my_json_add_bool(&js, "dnsEnable", 0);				// 原 cJSON_AddFalseToObject
	my_json_object_end(&js);							// ipAddressCfg

	my_json_object_begin(&js, "link");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%02x:%02x:%02x:%02x:%02x:%02x",
										local->mac[0],local->mac[1],local->mac[2],
										local->mac[3],local->mac[4],local->mac[5]);
	my_json_add_str(&js, "macAddress", temp);
	my_json_add_bool(&js, "autoNegotiation", 1);		// 原 cJSON_AddTrueToObject
	my_json_add_int(&js, "linkSpeed", LAN8720_Get_Speed());	// 连接速率
	if(LAN8720_Get_Duplex() == 1)
		my_json_add_str(&js, "linkDuplex", "half");
	else if(LAN8720_Get_Duplex() == 2)
		my_json_add_str(&js, "linkDuplex", "full");
	my_json_add_int(&js, "mtu", TCP_MSS);
	my_json_object_end(&js);							// link

	my_json_object_end(&js);							// 数组元素对象
	my_json_array_end(&js);								// ipAddressCfgList
	my_json_object_end(&js);							// Value
	my_json_object_end(&js);							// data
	my_json_object_end(&js);							// root

	*size = (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: http_json_ack_devicerealtimepowerparam
* Description	: 电力实时参数
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_json_ack_devicerealtimepowerparam(char* buf,uint16_t *size,uint8_t status_ID)
{
	data_collection_t *det_param = det_get_collect_data();
	struct threshold_params *shold_param = app_get_threshold_param_function();
	my_json_t js;
	uint8_t i;

	my_json_init(&js, buf, 4000);
	my_json_object_begin(&js, NULL);
	my_json_add_int(&js, "status", HTTP_STATUS[status_ID]);
	my_json_add_str(&js, "code", HTTP_PUBLIC_ERROR[status_ID]);
	my_json_add_str(&js, "errorMsg", HTTP_PUBLIC_ERRORMSG[status_ID]);

	my_json_object_begin(&js, "data");
	my_json_object_begin(&js, "Value");
	my_json_array_begin(&js, "powerList");

	for(i=0;i<8;i++)
	{
		my_json_object_begin(&js, NULL);					/* 数组元素对象 */
		my_json_add_int(&js, "powerID", i+1);				// 索引

		if(relay_get_status_function((RELAY_DEV)i) == 0)	// 继电器关闭
			my_json_add_str(&js, "powerStatus", "abnormal");
		else if(relay_get_status_function((RELAY_DEV)i) == 1)
			my_json_add_str(&js, "powerStatus", "normal");

		if(det_param->vin220v < 50)							// 市电电压 < 50V,说明断电
			my_json_add_str(&js, "status", "notPower");
		else if(det_param->vin220v > 200)
			my_json_add_str(&js, "status", "normal");
		else
			my_json_add_str(&js, "status", "abnormal");

		my_json_add_str(&js, "powerType", "AC");

		my_json_add_int(&js, "ratedPower", 1000);								// 额定功率
		my_json_add_double(&js, "totalPower", det_param->power[i]);				// 总功率
		my_json_add_double(&js, "powerConsumption", det_param->electricity[i]);	// 总耗电量

		if(relay_get_status_function((RELAY_DEV)i) == 0)	// 继电器关闭
			my_json_add_int(&js, "voltage", 0);				// 电压
		else if(relay_get_status_function((RELAY_DEV)i) == 1)
			my_json_add_double(&js, "voltage", det_param->vin220v);	// 电压

		my_json_add_double(&js, "electricCurrent", det_param->current[i]/1000.000f);	// 电流

		my_json_add_str(&js, "reclosingStatus", "closed");			// 重合闸状态
		my_json_add_str(&js, "electricLeakageStatus", "notSupport");	// 漏电状态

		if(shold_param->current !=0)
		{
			if(det_param->current[i] < shold_param->current)	// 电流状态
				my_json_add_str(&js, "electricCurrentStatus", "normal");
			else if(det_param->current[i] > shold_param->current)
				my_json_add_str(&js, "electricCurrentStatus", "high");
		}
		else
			my_json_add_str(&js, "electricCurrentStatus", "normal");

		if(shold_param->volt_max !=0 && shold_param->volt_min !=0)
		{
			if((det_param->vin220v < shold_param->volt_min)&& (det_param->vin220v > 30))	// 电压状态
				my_json_add_str(&js, "voltageStatus", "low");
			else if((det_param->vin220v < shold_param->volt_max)&& (det_param->vin220v > shold_param->volt_min))
				my_json_add_str(&js, "voltageStatus", "normal");
			else if(det_param->vin220v > shold_param->volt_max)
				my_json_add_str(&js, "voltageStatus", "high");
		}
		else
			my_json_add_str(&js, "voltageStatus", "normal");

		my_json_add_int(&js, "undercurrentTimes", 0);
		my_json_add_int(&js, "overcurrentTimes", shold_param->overcurrentTimes);		// 过流次数
		my_json_add_int(&js, "electricLeakageTimes", 0);								// 漏电次数
		my_json_add_int(&js, "undervoltageTimes", shold_param->undervoltageTimes);		// 欠压次数
		my_json_add_int(&js, "overvoltageTimes", shold_param->overvoltageTimes);		// 过压次数
		my_json_object_end(&js);							/* 数组元素对象结束 */
	}

	my_json_array_end(&js);									// powerList
	my_json_object_end(&js);								// Value
	my_json_object_end(&js);								// data
	my_json_object_end(&js);								// root

	*size = (uint16_t)my_json_len(&js);

//	uint16_t  jsonlen= 0;
//	
//	jsonlen += sprintf(buf+jsonlen,"%s","{\"status\":200,\"code\":\"0x00000000\",\"errorMsg\":\"Succeeded.\",\"data\":{\"Value\":{\"powerList\":[{\"powerID\":1,\"powerSta\
//tus\":\"normal\",\"status\":\"normal\",\"powerType\":\"AC\",\"ratedPower\":0,\"totalPower\":100.90939,\"powerConsumption\":333.06\
//1,\"voltage\":234.673,\"electricCurrent\":0.43,\"reclosingStatus\":\"closed\",\"electricLeakageStatus\":\"notSupport\",\"electric\
//CurrentStatus\":\"normal\",\"voltageStatus\":\"normal\",\"undercurrentTimes\":0,\"overcurrentTimes\":0,\"electricLeakageTimes\":\
//0,\"undervoltageTimes\":0,\"overvoltageTimes\":0},{\"powerID\":2,\"powerStatus\":\"normal\",\"status\":\"normal\",\"powerTyp\
//e\":\"AC\",\"ratedPower\":0,\"totalPower\":30.50749,\"powerConsumption\":87.697,\"voltage\":234.673,\"electricCurrent\":0.13,\"re\
//closingStatus\":\"closed\",\"electricLeakageStatus\":\"notSupport\",\"electricCurrentStatus\":\"normal\",\"voltageStatus\":\"no\
//rmal\",\"undercurrentTimes\":0,\"overcurrentTimes\":0,\"electricLeakageTimes\":0,\"undervoltageTimes\":0,\"overvoltageTimes\":\
//0},{\"powerID\":3,\"powerStatus\":\"normal\",\"status\":\"normal\",\"powerType\":\"AC\",\"ratedPower\":0,\"totalPower\":0,\"pow\
//erConsumption\":0,\"voltage\":234.673,\"electricCurrent\":0,\"reclosingStatus\":\"closed\",\"electricLeakageStatus\":\"notSuppo\
//rt\",\"electricCurrentStatus\":\"normal\",\"voltageStatus\":\"normal\",\"undercurrentTimes\":0,\"overcurrentTimes\":0,\"electric\
//LeakageTimes\":0,\"undervoltageTimes\":0,\"overvoltageTimes\":0},{\"powerID\":4,\"powerStatus\":\"normal\",\"status\":\"norm\
//al\",\"powerType\":\"AC\",\"ratedPower\":0,\"totalPower\":0,\"powerConsumption\":0,\"voltage\":234.673,\"electricCurrent\":0,\"re\
//closingStatus\":\"closed\",\"electricLeakageStatus\":\"notSupport\",\"electricCurrentStatus\":\"normal\",\"voltageStatus\":\"norm\
//al\",\"undercurrentTimes\":0,\"overcurrentTimes\":0,\"electricLeakageTimes\":0,\"undervoltageTimes\":0,\"overvoltageTimes\":0},\
//{\"powerID\":5,\"powerStatus\":\"normal\",\"status\":\"normal\",\"powerType\":\"AC\",\"ratedPower\":0,\"totalPower\":0,\"powerCon\
//sumption\":0,\"voltage\":234.673,\"electricCurrent\":0,\"reclosingStatus\":\"closed\",\"electricLeakageStatus\":\"notSupport\",\"e\
//lectricCurrentStatus\":\"normal\",\"voltageStatus\":\"normal\",\"undercurrentTimes\":0,\"overcurrentTimes\":0,\"electricLeakageTi\
//mes\":0,\"undervoltageTimes\":0,\"overvoltageTimes\":0},{\"powerID\":6,\"powerStatus\":\"normal\",\"status\":\"normal\",\"powerTy\
//pe\":\"AC\",\"ratedPower\":0,\"totalPower\":0,\"powerConsumption\":0,\"voltage\":234.673,\"electricCurrent\":0,\"reclosingStat\
//us\":\"closed\",\"electricLeakageStatus\":\"notSupport\",\"electricCurrentStatus\":\"normal\",\"voltageStatus\":\"normal\",\"un\
//dercurrentTimes\":0,\"overcurrentTimes\":0,\"electricLeakageTimes\":0,\"undervoltageTimes\":0,\"overvoltageTimes\":0},{\"power\
//ID\":7,\"powerStatus\":\"normal\",\"status\":\"normal\",\"powerType\":\"AC\",\"ratedPower\":0,\"totalPower\":9.38692,\"powerCo\
//nsumption\":29.617,\"voltage\":234.673,\"electricCurrent\":0.04,\"reclosingStatus\":\"closed\",\"electricLeakageStatus\":\"notS\
//upport\",\"electricCurrentStatus\":\"normal\",\"voltageStatus\":\"normal\",\"undercurrentTimes\":0,\"overcurrentTimes\":0,\"ele\
//ctricLeakageTimes\":0,\"undervoltageTimes\":0,\"overvoltageTimes\":0},{\"powerID\":8,\"powerStatus\":\"normal\",\"status\":\"nor\
//mal\",\"powerType\":\"AC\",\"ratedPower\":0,\"totalPower\":28.16076,\"powerConsumption\":87.553,\"voltage\":234.673,\"electricCu\
//rrent\":0.12,\"reclosingStatus\":\"closed\",\"electricLeakageStatus\":\"notSupport\",\"electricCurrentStatus\":\"normal\",\"volt\
//ageStatus\":\"normal\",\"undercurrentTimes\":0,\"overcurrentTimes\":0,\"electricLeakageTimes\":0,\"undervoltageTimes\":0,\"overv\
//oltageTimes\":0}]}}}");
//	*size = jsonlen;
}

/************************************************************
*
* Function name	: http_json_ack_domins
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_json_ack_domins(char* buf,uint16_t *size,uint8_t status_ID)
{
	my_json_t js;

	if(HTTP_COM_DEBUG) printf("http_json_ack_waterout22 \n");

	my_json_init(&js, buf, 4000);
	my_json_object_begin(&js, NULL);
	my_json_add_int(&js, "status", HTTP_STATUS[status_ID]);
	my_json_add_str(&js, "code", HTTP_PUBLIC_ERROR[status_ID]);
	my_json_add_str(&js, "errorMsg", HTTP_PUBLIC_ERRORMSG[status_ID]);

	my_json_object_begin(&js, "data");
	my_json_array_begin(&js, "domains");
	my_json_add_str(&js, NULL, "WaterOutSense");
	my_json_add_str(&js, NULL, "SystemMaintenance");
	my_json_add_str(&js, NULL, "InfoMgr");
	my_json_add_str(&js, NULL, "DeviceTiltDetection");
	my_json_add_str(&js, NULL, "Humiture");
	my_json_add_str(&js, NULL, "Fan");
	my_json_add_str(&js, NULL, "BoxDoorMgr");
	my_json_add_str(&js, NULL, "NetworkAddress");
	my_json_add_str(&js, NULL, "PowerMgr");
	my_json_array_end(&js);
	my_json_object_end(&js);			// data
	my_json_object_end(&js);			// root

	*size = (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: http_json_ack_BoxSubModelMgr
* Description	: 获取机箱管理的设备列表
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_json_ack_BoxSubModelMgr(char* buf,uint16_t *size,uint8_t status_ID)
{
	my_json_t js;
	char dataid[3] = {0};
	uint8_t i;

	my_json_init(&js, buf, 4000);
	my_json_object_begin(&js, NULL);
	my_json_add_int(&js, "status", HTTP_STATUS[status_ID]);
	my_json_add_str(&js, "code", HTTP_PUBLIC_ERROR[status_ID]);
	my_json_add_str(&js, "errorMsg", HTTP_PUBLIC_ERRORMSG[status_ID]);

	my_json_object_begin(&js, "data");
	my_json_array_begin(&js, "deviceList");

	for(i=0;i<8;i++)
	{
		my_json_object_begin(&js, NULL);					/* 数组元素对象 */
		my_json_add_str(&js, "deviceType", "encoder");
		my_json_add_str(&js, "model", "");
		my_json_add_str(&js, "ipV4Address", "");
		my_json_add_str(&js, "ipV6Address", "");
		my_json_add_str(&js, "macAddress", "");

		if(relay_get_status_function((RELAY_DEV)i) == 0)	// 继电器关闭
			my_json_add_str(&js, "powerStatus", "abnormal");
		else if(relay_get_status_function((RELAY_DEV)i) == 1)
			my_json_add_str(&js, "powerStatus", "normal");

		my_json_add_str(&js, "netStatus", "notSupport");
		sprintf(dataid,"%d",i+1);
		my_json_add_str(&js, "electricPortCode", dataid);

		my_json_add_int(&js, "SDKPort", 1);
		my_json_add_int(&js, "httpPort", 1);
		my_json_add_int(&js, "portID", 1);					// 交换机端口索引
		my_json_add_str(&js, "portName", "undefined");
		my_json_object_end(&js);							/* 数组元素对象结束 */
	}

	my_json_array_end(&js);									// deviceList
	my_json_object_end(&js);								// data
	my_json_object_end(&js);								// root

	*size = (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: http_json_ack_PowerPortSwitchTime
* Description	: 电口开关时间计划
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_json_ack_PowerPortSwitchTime(char* buf,uint16_t *size,uint8_t status_ID)
{
	my_json_t js;
	char data_id[3] = {0};
	uint8_t i;

	my_json_init(&js, buf, 4000);
	my_json_object_begin(&js, NULL);
	my_json_add_int(&js, "status", HTTP_STATUS[status_ID]);
	my_json_add_str(&js, "code", HTTP_PUBLIC_ERROR[status_ID]);
	my_json_add_str(&js, "errorMsg", HTTP_PUBLIC_ERRORMSG[status_ID]);

	my_json_object_begin(&js, "data");
	my_json_object_begin(&js, "Value");
	my_json_array_begin(&js, "powerPortDevList");

	for(i=0;i<8;i++)
	{
		my_json_object_begin(&js, NULL);					/* 数组元素对象 */
		sprintf(data_id,"%d",i+1);
		my_json_add_str(&js, "portCode", data_id);			// 供电口编码
		my_json_add_bool(&js, "planEnabled", 1);			// 原 cJSON_AddTrueToObject

		my_json_array_begin(&js, "timeSpanList");
		my_json_object_begin(&js, NULL);					/* 时间对象 */
		my_json_add_str(&js, "startTime", "00:00:00");		// 开始时间
		my_json_add_str(&js, "endTime", "23:59:59");		// 结束时间
		my_json_object_end(&js);
		my_json_array_end(&js);

		my_json_object_end(&js);							/* 数组元素对象结束 */
	}

	my_json_array_end(&js);									// powerPortDevList
	my_json_object_end(&js);								// Value
	my_json_object_end(&js);								// data
	my_json_object_end(&js);								// root

	*size = (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: http_json_ack_GetPowerPortStatusList
* Description	: 获取供电口状态列表
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_json_ack_GetPowerPortStatusList(char* buf,uint16_t *size,uint8_t status_ID)
{
	data_collection_t *det_param = det_get_collect_data();
	my_json_t js;
	char dataid[3] = {0};
	uint8_t i;

	my_json_init(&js, buf, 4000);
	my_json_object_begin(&js, NULL);
	my_json_add_int(&js, "status", HTTP_STATUS[status_ID]);
	my_json_add_str(&js, "code", HTTP_PUBLIC_ERROR[status_ID]);
	my_json_add_str(&js, "errorMsg", HTTP_PUBLIC_ERRORMSG[status_ID]);

	my_json_object_begin(&js, "data");
	my_json_array_begin(&js, "powerPortStatusList");

	for(i=0;i<8;i++)
	{
		my_json_object_begin(&js, NULL);					/* 数组元素对象 */
		sprintf(dataid,"%d",i+1);
		my_json_add_str(&js, "portCode", dataid);

		if(relay_get_status_function((RELAY_DEV)i) == 0)	// 继电器关闭
			my_json_add_int(&js, "voltage", 0);				// 电压
		else if(relay_get_status_function((RELAY_DEV)i) == 1)
			my_json_add_double(&js, "voltage", det_param->vin220v);	// 电压

		my_json_add_double(&js, "electricCurrent", det_param->current[i]/1000.000f);	// 电流
		my_json_add_double(&js, "totalPower", det_param->power[i]);						// 总功率
		my_json_add_double(&js, "powerConsumption", det_param->electricity[i]);			// 总耗电量

		if(relay_get_status_function((RELAY_DEV)i) == 0)	// 继电器关闭
			my_json_add_str(&js, "powerPortStatus", "abnormal");
		else if(relay_get_status_function((RELAY_DEV)i) == 1)
			my_json_add_str(&js, "powerPortStatus", "normal");

		if(det_param->power[i] < 1)							// 功率小于1W,判断为未插入
			my_json_add_str(&js, "powerPortStatus", "notInsert");
		my_json_object_end(&js);							/* 数组元素对象结束 */
	}

	my_json_array_end(&js);									// powerPortStatusList
	my_json_object_end(&js);								// data
	my_json_object_end(&js);								// root

	*size = (uint16_t)my_json_len(&js);
}


/* HTTP 转 webSocket
GET /chat HTTP/1.1
Host: server.example.com
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==
Sec-WebSocket-Protocol: chat, superchat
Sec-WebSocket-Version: 13
Origin: http://example.com
*/
/************************************************************
*
* Function name	: http_to_websocket_function
* Description	: HTTP升级为webSocket
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_to_websocket_function(char* data,uint16_t *len)
{
	struct remote_ip	*remoteparam = app_get_remote_network_function(); 
	uint16_t str_len = 0;
	char temp[30] = {0};	
	uint8_t random_key[18] = {0};
	
	str_len = sprintf(data,"%s","GET /chat HTTP/1.1\r\nHost: "); 	
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d:%d",remoteparam->client_iporname[0],remoteparam->client_iporname[1],remoteparam->client_iporname[2],
																remoteparam->client_iporname[3],remoteparam->client_port);
	str_len += sprintf(data+str_len,"%s\r\n",temp);
	
	str_len += sprintf(data+str_len,"%s","Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: "); 	
	
	for(uint8_t k=0;k<18;k++)
		random_key[k] = RNG_Get_RandomRange(1,255);
	memset(&web_key_data,0,sizeof(web_key_t));
	base64_encode(random_key,web_key_data.key,18);
	str_len += sprintf(data+str_len,"%s\r\n",web_key_data.key); 	
	str_len += sprintf(data+str_len,"%s","Origin: http://"); 		
	str_len += sprintf(data+str_len,"%s\r\n",temp);
	str_len += sprintf(data+str_len,"%s","Sec-WebSocket-Version: 13\r\n\r\n"); 

	if(HTTP_COM_DEBUG) printf("websocket:%s\n",data);
	
	*len = str_len;
}

/* HTTP 转 webSocket
HTTP/1.1 101 Switching Protocols
Upgrade: WebSocket
Connection: Upgrade
Sec-WebSocket-Accept: G/cEt4HtsYEnP0MnSVkKRk459gM=
Sec-WebSocket-Protocol: ws-protocol-example\r\n\r\n
*/
/************************************************************
*
* Function name	: http_to_websocket_verification
* Description	: 验证服务器回复消息是否正确
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_to_websocket_verification_ack(char* data,uint16_t *len)
{
	char key_append[100] = {0};
	uint8_t sha1_encode[50] = {0};
	uint16_t str_len = 0;
	
	str_len = sprintf(key_append,"%s%s",web_key_data.key,WEBSOCKET_GUID); 		
	sha1(key_append,sha1_encode,str_len);
	base64_encode(sha1_encode,web_key_data.accept,strlen((char*)sha1_encode));
	str_len = 0;
	str_len  = sprintf(data,"%s","HTTP/1.1 101 Switching Protocols\r\nUpgrade: webSocket\r\n");
	str_len += sprintf(data+str_len,"%s","Connection: Upgrade\r\nSec-WebSocket-Accept: ");	
	str_len += sprintf(data+str_len,"%s\r\n",web_key_data.accept);
	str_len += sprintf(data+str_len,"%s","Sec-WebSocket-Protocol: ws-protocol-example\r\n\r\n");
	if(HTTP_COM_DEBUG) printf("websocket:%s\n",data);
	*len = str_len;	
}

/*
Frame format:  
      0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 ......
     +-+-+-+-+-------+-+-------------+-------------------------------+
     |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
     |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
     |N|V|V|V|       |S|             |   (if payload len==126/127)   |
     | |1|2|3|       |K|             |                               |
     +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
     |     Extended payload length continued, if payload len == 127  |
     + - - - - - - - - - - - - - - - +-------------------------------+
     |                               |Masking-key, if MASK set to 1  |
     +-------------------------------+-------------------------------+
     | Masking-key (continued)       |          Payload Data         |
     +-------------------------------- - - - - - - - - - - - - - - - +
     :                     Payload Data continued ...                :
     + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
     |                     Payload Data continued ...                |
     +---------------------------------------------------------------+
*/
/************************************************************
*
* Function name	: http_websocket_analy_data
* Description	: websocket数据解析
* Parameter		: 
* Return		: 
*	
************************************************************/
int http_websocket_analy_data(char* data,uint16_t len,com_websocket_data_t *websocket_data)  
{   
	char temp[8]; 

	if (strlen(data) < 2)  return 0;  

	websocket_data->fin = data[0]&0x80;   // 表示是消息的最后一帧
	if (websocket_data->fin != 0x80) return 0;

	websocket_data->rsv = data[0]&0x70; // 扩展定义
	if (websocket_data->rsv != 0x00) return 0;

	websocket_data->opcode = data[0]&0x0F; // 解释 Payload 数据

	websocket_data->mask = (data[1] & 0x80);  // 掩码 

	websocket_data->len = data[1] & 0x7F; // Payload 数据的长度  
	if (websocket_data->len == 126)  
	{
		websocket_data->len = (data[2]<<8)| data[3];   
		if(websocket_data->mask == 0x80)
		{
			memcpy(websocket_data->masking_key, data+4, 4);   
			memset(websocket_data->buf,0, WEBSOCKET_MALLOC_SIZE);  
			memcpy(websocket_data->buf,data+8,websocket_data->len);
		}
		else
		{
			memset(websocket_data->buf,0, WEBSOCKET_MALLOC_SIZE);  
			memcpy(websocket_data->buf,data+4,websocket_data->len);			
		}
	}  
	else if (websocket_data->len == 127)  
	{  
		for (uint8_t i = 0; i < 8; i++)  
		{  
			temp[i] = data[2+i];  
		}   
		memcpy(&websocket_data->len,temp,8);    
		if(websocket_data->mask == 0x80)
		{
			memcpy(websocket_data->masking_key, data+10, 4);   
			memset(websocket_data->buf,0, WEBSOCKET_MALLOC_SIZE);  
			memcpy(websocket_data->buf,data+14,websocket_data->len);
		}
		else
		{
			memset(websocket_data->buf,0, WEBSOCKET_MALLOC_SIZE);  
			memcpy(websocket_data->buf,data+10,websocket_data->len);			
		}
	}  
	else  
	{     
		if(websocket_data->mask == 0x80)
		{
			memcpy(websocket_data->masking_key, data+2, 4);   
			memset(websocket_data->buf,0, WEBSOCKET_MALLOC_SIZE);  
			memcpy(websocket_data->buf,data+6,websocket_data->len);
		}
		else
		{
			memset(websocket_data->buf,0, WEBSOCKET_MALLOC_SIZE);  
			memcpy(websocket_data->buf,data+2,websocket_data->len);			
		}
	}  
	if(websocket_data->mask == 0x80)
	{
		for (uint16_t i = 0; i < websocket_data->len; i++)  
		{  
			websocket_data->buf[i] = (char)(websocket_data->buf[i] ^ websocket_data->masking_key[i % 4]);
		}
	}	
	return 1; 
}  

/************************************************************
*
* Function name	: http_websocket_pong_data
* Description	: 打包pong数据
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_websocket_pong_data(char* data,uint16_t *len)  
{ 
	data[0] = 0x80;  // FIN
	data[0] |= PAYLOAD_PONG;  // OPCODE
	
	data[1] = 0x00;  // MASK=0
	data[1] |= sg_websocket.len;
	memcpy(data + 2, sg_websocket.buf, sg_websocket.len);  
	
	*len = sg_websocket.len + 2; 
}  

/************************************************************
*
* Function name	: http_websocket_pack_data
* Description	: 打包websocket数据
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_websocket_pack_data(char* data,uint16_t *len,uint16_t event_type,uint8_t power_type)  
{   
	char event_buf[512] = {0};
	uint16_t event_len = 0;
	switch(event_type)
	{
		case FLAG_WATER_ERROR: // 水浸告警
		event_len = websocket_event_alerts_waterout(event_buf);
		break;
		case FLAG_WATER_NOEMAL: // 水浸恢复
		event_len = websocket_event_alerts_wateroutreset(event_buf);
		break;
		case FLAG_SPD: // 防雷
		event_len = websocket_event_alerts_devlightingprotectionstatusreport(event_buf);
		break;
		case FLAG_ANGLE: // 倾斜
		event_len = websocket_event_alerts_devicetiltalarm(event_buf);
		break;
		case FLAG_TEMP_HIGH: // 温度过高
		event_len = websocket_event_alerts_temperaturetoohigh(event_buf);
		break;
		case FLAG_TEMP_LOW: // 温度过低
		event_len = websocket_event_alerts_temperaturetoolow(event_buf);
		break;
		case FLAG_HUMI_HIGH: // 湿度过高
		event_len = websocket_event_alerts_humiditytoohigh(event_buf);
		break;
		case FLAG_HUMI_LOW: // 湿度过低
		event_len = websocket_event_alerts_humiditytoolow(event_buf);
		break;
		case FLAG_DOOR: // 箱门
		event_len = websocket_event_alerts_BoxDoorStatusReport(event_buf);
		break;
		case FLAG_POWER: // 电量
		event_len = websocket_event_alerts_DevicePowerParamAlarm(event_buf,power_type);
		break;
		case FLAG_AC_STATUS: // 电源状态
		event_len = websocket_event_alerts_DevPowerStatusReport(event_buf);
		break;
		case FLAG_ADD_ALL: // 添加事件
		event_len = http_websocket_event_add_all(event_buf);
		break;
		case FLAG_FAN: // 风扇
		event_len = websocket_event_alerts_fan_status(event_buf);
		break;
//		case FLAG_DELE_ALL: // 删除事件
//		event_len = websocket_event_alerts_waterout(event_buf);
//		break;		
	}
  if (event_len <= 0) *len = 0;  
	else if ((event_len > 0)&&(event_len < 126))  
	{       
		data[0] = 0x81;  
		data[1] = event_len;  
		memcpy(data + 2, event_buf, event_len);  
		*len = event_len + 2;  
	}  
	else if ((event_len >= 126)&&(event_len <= 0xFFFF)) 
	{    
		data[0] = 0x81;  
		data[1] = 126;  
		data[2] = ((event_len >> 8) & 0xFF);  
		data[3] = (event_len & 0xFF);  
		memcpy(data + 4, event_buf, event_len);      
		*len = event_len + 4;  
	}  
	else  
	{  
		data[0] = 0x81;  
		data[1] = 127;  
		data[2] = (((uint64_t)event_len >> 56) & 0xFF);
		data[3] = (((uint64_t)event_len >> 48) & 0xFF);
		data[4] = (((uint64_t)event_len >> 40) & 0xFF);
		data[5] = (((uint64_t)event_len >> 32) & 0xFF);
		data[6] = (((uint64_t)event_len >> 24) & 0xFF);
		data[7] = (((uint64_t)event_len >> 16) & 0xFF);
		data[8] = (((uint64_t)event_len >> 8) & 0xFF);
		data[9] = (((uint64_t)event_len >> 0) & 0xFF);
		memcpy(data + 10, event_buf, event_len);      
		*len  = event_len + 10;  
	} 
}  

/************************************************************
*
* Function name	: websocket_event_alerts_wateroutreset
* Description	: 水浸恢复事件
* Parameter		: 
* Return		: 
*	
************************************************************/
uint16_t websocket_event_alerts_wateroutreset(char* buf)
{
	struct local_ip_t  *local = app_get_local_network_function();
	char temp[30] = {0};
	rtc_time_t local_rtc,utc_time;
	char *uuid_buf = app_get_device_uuid();
	my_json_t js;

	my_json_init(&js, buf, 512);			/* buf = http_websocket_pack_data 的 event_buf[512] */
	my_json_object_begin(&js, NULL);
	my_json_add_str(&js, "topic", "/iot/global/0-global/model/event/report/WaterOutSense/WaterOutReset");

	my_json_object_begin(&js, "basic");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
	my_json_add_str(&js, "ipV4Address", temp);
	my_json_add_str(&js, "ipV6Address", "");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%02x-%02x-%02x-%02x-%02x-%02x",
										local->mac[0],local->mac[1],local->mac[2],
										local->mac[3],local->mac[4],local->mac[5]);
	my_json_add_str(&js, "macAddress", temp);
	RTC_Get_Time(&local_rtc);
	local_to_utc_time(&utc_time,-8,local_rtc);
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%04d-%02d-%02dT%02d:%02d:%02d+08:00",
					utc_time.year,utc_time.month,utc_time.data,
					utc_time.hour,utc_time.min,utc_time.sec);
	my_json_add_str(&js, "dateTime", temp);
	my_json_add_str(&js, "UUID", uuid_buf);
	my_json_object_end(&js);						// basic

	my_json_object_begin(&js, "notification");
	my_json_add_int(&js, "status", 1);
	my_json_add_int(&js, "action", 0);
	my_json_add_str(&js, "location", "undefined");
	my_json_add_str(&js, "relationId", "undefined");
	my_json_array_begin(&js, "pictures");
	my_json_object_begin(&js, NULL);
	my_json_add_int(&js, "cloudtype", 0);
	my_json_add_str(&js, "bucket", "undefined");
	my_json_add_str(&js, "type", "undefined");
	my_json_add_int(&js, "length", 0);
	my_json_add_int(&js, "crypt", 0);
	my_json_add_str(&js, "fileid", "undefined");
	my_json_add_int(&js, "tinyvideo", 0);
	my_json_add_str(&js, "checksum", "undefined");
	my_json_add_int(&js, "lifecycle", 0);
	my_json_object_end(&js);
	my_json_array_end(&js);
	my_json_object_end(&js);						// notification
	my_json_object_end(&js);						// root

	if(HTTP_COM_DEBUG) printf("wateroutreset:%s\n",buf);
	return (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: websocket_event_alerts_waterout
* Description	: 水浸告警事件
* Parameter		: 
* Return		: 
*	
************************************************************/
uint16_t websocket_event_alerts_waterout(char* buf)
{
	struct local_ip_t  *local = app_get_local_network_function();
	char temp[30] = {0};
	rtc_time_t local_rtc,utc_time;
	char *uuid_buf = app_get_device_uuid();
	my_json_t js;

	my_json_init(&js, buf, 512);			/* buf = http_websocket_pack_data 的 event_buf[512] */
	my_json_object_begin(&js, NULL);
	my_json_add_str(&js, "topic", "/iot/global/0-global/model/event/report/WaterOutSense/WaterOut");

	my_json_object_begin(&js, "basic");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
	my_json_add_str(&js, "ipV4Address", temp);
	my_json_add_str(&js, "ipV6Address", "");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%02x-%02x-%02x-%02x-%02x-%02x",
										local->mac[0],local->mac[1],local->mac[2],
										local->mac[3],local->mac[4],local->mac[5]);
	my_json_add_str(&js, "macAddress", temp);
	RTC_Get_Time(&local_rtc);
	local_to_utc_time(&utc_time,-8,local_rtc);
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%04d-%02d-%02dT%02d:%02d:%02d+08:00",
					utc_time.year,utc_time.month,utc_time.data,
					utc_time.hour,utc_time.min,utc_time.sec);
	my_json_add_str(&js, "dateTime", temp);
	my_json_add_str(&js, "UUID", uuid_buf);
	my_json_object_end(&js);						// basic

	my_json_object_begin(&js, "notification");
	my_json_add_int(&js, "status", 1);
	my_json_add_int(&js, "action", 0);
	my_json_add_str(&js, "location", "undefined");
	my_json_add_str(&js, "relationId", "undefined");
	my_json_array_begin(&js, "pictures");
	my_json_object_begin(&js, NULL);
	my_json_add_int(&js, "cloudtype", 0);
	my_json_add_str(&js, "bucket", "undefined");
	my_json_add_str(&js, "type", "undefined");
	my_json_add_int(&js, "length", 0);
	my_json_add_int(&js, "crypt", 0);
	my_json_add_str(&js, "fileid", "undefined");
	my_json_add_int(&js, "tinyvideo", 0);
	my_json_add_str(&js, "checksum", "undefined");
	my_json_add_int(&js, "lifecycle", 0);
	my_json_object_end(&js);
	my_json_array_end(&js);
	my_json_object_end(&js);						// notification
	my_json_object_end(&js);						// root

	if(HTTP_COM_DEBUG) printf("waterout:%s\n",buf);
	return (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: websocket_event_alerts_devlightingprotectionstatusreport
* Description	: 设备防雷事件
* Parameter		: 
* Return		: 
*	
************************************************************/
uint16_t websocket_event_alerts_devlightingprotectionstatusreport(char* buf)
{
	struct local_ip_t  *local = app_get_local_network_function();
	char temp[30] = {0};
	rtc_time_t local_rtc,utc_time;
	char *uuid_buf = app_get_device_uuid();
	my_json_t js;

	my_json_init(&js, buf, 512);
	my_json_object_begin(&js, NULL);
	my_json_add_str(&js, "topic", "/iot/global/0-global/model/event/report/LightningProtectionMgr/DevLightingProtectionStatusReport");

	my_json_object_begin(&js, "basic");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
	my_json_add_str(&js, "ipV4Address", temp);
	my_json_add_str(&js, "ipV6Address", "");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%02x-%02x-%02x-%02x-%02x-%02x",
									local->mac[0],local->mac[1],local->mac[2],
									local->mac[3],local->mac[4],local->mac[5]);
	my_json_add_str(&js, "macAddress", temp);
	RTC_Get_Time(&local_rtc);
	local_to_utc_time(&utc_time,-8,local_rtc);
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%04d-%02d-%02dT%02d:%02d:%02d+08:00",
					utc_time.year,utc_time.month,utc_time.data,
					utc_time.hour,utc_time.min,utc_time.sec);
	my_json_add_str(&js, "dateTime", temp);
	my_json_add_str(&js, "UUID", uuid_buf);
	my_json_object_end(&js);						// basic

	my_json_object_begin(&js, "payload");
	my_json_add_str(&js, "devName", "");
	my_json_add_str(&js, "model", "");
	if( det_get_spd_status() == 2 )
		my_json_add_str(&js, "lightningProtectionStatus", "abnormal");
	else
		my_json_add_str(&js, "lightningProtectionStatus", "normal");
	my_json_object_end(&js);						// payload

	my_json_object_end(&js);						// root

	if(HTTP_COM_DEBUG) printf("devlightingprotectionstatusreport:%s\n",buf);
	return (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: websocket_event_alerts_devicetiltalarm
* Description	: 倾斜事件
* Parameter		: 
* Return		: 
*	
************************************************************/
uint16_t websocket_event_alerts_devicetiltalarm(char* buf)
{
	struct local_ip_t  *local = app_get_local_network_function();
	struct threshold_params  *threshold = app_get_threshold_param_function();
	data_collection_t *det_param = det_get_collect_data();
	char temp[30] = {0};
	rtc_time_t local_rtc,utc_time;
	char *uuid_buf = app_get_device_uuid();
	my_json_t js;

	my_json_init(&js, buf, 512);
	my_json_object_begin(&js, NULL);
	my_json_add_str(&js, "topic", "/iot/global/0-global/model/event/report/DeviceTiltDetection/DeviceTiltAlarm");

	my_json_object_begin(&js, "basic");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
	my_json_add_str(&js, "ipV4Address", temp);
	my_json_add_str(&js, "ipV6Address", "");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%02x-%02x-%02x-%02x-%02x-%02x",
									local->mac[0],local->mac[1],local->mac[2],
									local->mac[3],local->mac[4],local->mac[5]);
	my_json_add_str(&js, "macAddress", temp);
	RTC_Get_Time(&local_rtc);
	local_to_utc_time(&utc_time,-8,local_rtc);
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%04d-%02d-%02dT%02d:%02d:%02d+08:00",
					utc_time.year,utc_time.month,utc_time.data,
					utc_time.hour,utc_time.min,utc_time.sec);
	my_json_add_str(&js, "dateTime", temp);
	my_json_add_str(&js, "UUID", uuid_buf);
	my_json_object_end(&js);						// basic

	my_json_object_begin(&js, "payload");
	my_json_add_str(&js, "alarmSourceType", "box");
	if( det_param->attitude_acc >= threshold->angle)
		my_json_add_str(&js, "exceptionType", "high");
	my_json_object_begin(&js, "inclinationParam");
	my_json_add_double(&js, "inclination", det_param->attitude_acc);
	my_json_object_end(&js);						// inclinationParam
	my_json_object_end(&js);						// payload

	my_json_object_end(&js);						// root

	if(HTTP_COM_DEBUG) printf("devicetiltalarm:%s\n",buf);
	return (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: websocket_event_alerts_humiditytoohigh
* Description	: 湿度过高
* Parameter		: 
* Return		: 
*	
************************************************************/
uint16_t websocket_event_alerts_humiditytoohigh(char* buf)
{
	struct local_ip_t  *local = app_get_local_network_function();
	data_collection_t *det_param = det_get_collect_data();
	char temp[30] = {0};
	rtc_time_t local_rtc,utc_time;
	char *uuid_buf = app_get_device_uuid();
	my_json_t js;

	my_json_init(&js, buf, 512);
	my_json_object_begin(&js, NULL);
	my_json_add_str(&js, "topic", "/iot/global/0-global/model/event/report/Humiture/HumidityTooHigh");

	my_json_object_begin(&js, "basic");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
	my_json_add_str(&js, "ipV4Address", temp);
	my_json_add_str(&js, "ipV6Address", "");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%02x-%02x-%02x-%02x-%02x-%02x",
									local->mac[0],local->mac[1],local->mac[2],
									local->mac[3],local->mac[4],local->mac[5]);
	my_json_add_str(&js, "macAddress", temp);
	RTC_Get_Time(&local_rtc);
	local_to_utc_time(&utc_time,-8,local_rtc);
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%04d-%02d-%02dT%02d:%02d:%02d+08:00",
					utc_time.year,utc_time.month,utc_time.data,
					utc_time.hour,utc_time.min,utc_time.sec);
	my_json_add_str(&js, "dateTime", temp);
	my_json_add_str(&js, "UUID", uuid_buf);
	my_json_object_end(&js);						// basic

	my_json_object_begin(&js, "notification");
	my_json_add_int(&js, "action", 0);
	my_json_add_str(&js, "relationId", "undefined");
	my_json_object_end(&js);						// notification

	my_json_object_begin(&js, "payload");
	my_json_add_double(&js, "humidity", det_param->humi_inside);
	my_json_object_end(&js);						// payload

	my_json_object_end(&js);						// root

	if(HTTP_COM_DEBUG) printf("humiditytoohigh:%s\n",buf);
	return (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: websocket_event_alerts_humiditytoolow
* Description	: 湿度过低告警
* Parameter		: 
* Return		: 
*	
************************************************************/
uint16_t websocket_event_alerts_humiditytoolow(char* buf)
{
	struct local_ip_t  *local = app_get_local_network_function();
	data_collection_t *det_param = det_get_collect_data();
	char temp[30] = {0};
	rtc_time_t local_rtc,utc_time;
	char *uuid_buf = app_get_device_uuid();
	my_json_t js;

	my_json_init(&js, buf, 512);
	my_json_object_begin(&js, NULL);
	my_json_add_str(&js, "topic", "/iot/global/0-global/model/event/report/Humiture/HumidityTooLow");

	my_json_object_begin(&js, "basic");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
	my_json_add_str(&js, "ipV4Address", temp);
	my_json_add_str(&js, "ipV6Address", "");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%02x-%02x-%02x-%02x-%02x-%02x",
									local->mac[0],local->mac[1],local->mac[2],
									local->mac[3],local->mac[4],local->mac[5]);
	my_json_add_str(&js, "macAddress", temp);
	RTC_Get_Time(&local_rtc);
	local_to_utc_time(&utc_time,-8,local_rtc);
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%04d-%02d-%02dT%02d:%02d:%02d+08:00",
					utc_time.year,utc_time.month,utc_time.data,
					utc_time.hour,utc_time.min,utc_time.sec);
	my_json_add_str(&js, "dateTime", temp);
	my_json_add_str(&js, "UUID", uuid_buf);
	my_json_object_end(&js);						// basic

	my_json_object_begin(&js, "notification");
	my_json_add_int(&js, "action", 0);
	my_json_add_str(&js, "relationId", "undefined");
	my_json_object_end(&js);						// notification

	my_json_object_begin(&js, "payload");
	my_json_add_double(&js, "humidity", det_param->humi_inside);
	my_json_object_end(&js);						// payload

	my_json_object_end(&js);						// root

	if(HTTP_COM_DEBUG) printf("humiditytoolow:%s\n",buf);
	return (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: websocket_event_alerts_humiditytoolow
* Description	: 温度过低告警
* Parameter		: 
* Return		: 
*	
************************************************************/
uint16_t websocket_event_alerts_temperaturetoolow(char* buf)
{
	struct local_ip_t  *local = app_get_local_network_function();
	data_collection_t *det_param = det_get_collect_data();
	char temp[30] = {0};
	rtc_time_t local_rtc,utc_time;
	char *uuid_buf = app_get_device_uuid();
	my_json_t js;

	my_json_init(&js, buf, 512);
	my_json_object_begin(&js, NULL);
	my_json_add_str(&js, "topic", "/iot/global/0-global/model/event/report/Humiture/TemperatureTooLow");

	my_json_object_begin(&js, "basic");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
	my_json_add_str(&js, "ipV4Address", temp);
	my_json_add_str(&js, "ipV6Address", "");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%02x-%02x-%02x-%02x-%02x-%02x",
									local->mac[0],local->mac[1],local->mac[2],
									local->mac[3],local->mac[4],local->mac[5]);
	my_json_add_str(&js, "macAddress", temp);
	RTC_Get_Time(&local_rtc);
	local_to_utc_time(&utc_time,-8,local_rtc);
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%04d-%02d-%02dT%02d:%02d:%02d+08:00",
					utc_time.year,utc_time.month,utc_time.data,
					utc_time.hour,utc_time.min,utc_time.sec);
	my_json_add_str(&js, "dateTime", temp);
	my_json_add_str(&js, "UUID", uuid_buf);
	my_json_object_end(&js);						// basic

	my_json_object_begin(&js, "notification");
	my_json_add_int(&js, "action", 0);
	my_json_add_str(&js, "relationId", "undefined");
	my_json_object_end(&js);						// notification

	my_json_object_begin(&js, "payload");
	my_json_add_double(&js, "temperature", det_param->temp_inside);
	my_json_add_str(&js, "unit", "celsius");
	my_json_object_end(&js);						// payload

	my_json_object_end(&js);						// root

	if(HTTP_COM_DEBUG) printf("temperaturetoolow:%s\n",buf);
	return (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: websocket_event_alerts_temperaturetoohigh
* Description	: 温度过高告警
* Parameter		: 
* Return		: 
*	
************************************************************/
uint16_t websocket_event_alerts_temperaturetoohigh(char* buf)
{
	struct local_ip_t  *local = app_get_local_network_function();
	data_collection_t *det_param = det_get_collect_data();
	char temp[30] = {0};
	rtc_time_t local_rtc,utc_time;
	char *uuid_buf = app_get_device_uuid();
	my_json_t js;

	my_json_init(&js, buf, 512);
	my_json_object_begin(&js, NULL);
	my_json_add_str(&js, "topic", "/iot/global/0-global/model/event/report/Humiture/TemperatureTooHigh");

	my_json_object_begin(&js, "basic");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
	my_json_add_str(&js, "ipV4Address", temp);
	my_json_add_str(&js, "ipV6Address", "");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%02x-%02x-%02x-%02x-%02x-%02x",
									local->mac[0],local->mac[1],local->mac[2],
									local->mac[3],local->mac[4],local->mac[5]);
	my_json_add_str(&js, "macAddress", temp);
	RTC_Get_Time(&local_rtc);
	local_to_utc_time(&utc_time,-8,local_rtc);
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%04d-%02d-%02dT%02d:%02d:%02d+08:00",
					utc_time.year,utc_time.month,utc_time.data,
					utc_time.hour,utc_time.min,utc_time.sec);
	my_json_add_str(&js, "dateTime", temp);
	my_json_add_str(&js, "UUID", uuid_buf);
	my_json_object_end(&js);						// basic

	my_json_object_begin(&js, "notification");
	my_json_add_int(&js, "action", 0);
	my_json_add_str(&js, "relationId", "undefined");
	my_json_object_end(&js);						// notification

	my_json_object_begin(&js, "payload");
	my_json_add_double(&js, "temperature", det_param->temp_inside);
	my_json_add_str(&js, "unit", "celsius");
	my_json_object_end(&js);						// payload

	my_json_object_end(&js);						// root

	if(HTTP_COM_DEBUG) printf("temperaturetoohigh:%s\n",buf);
	return (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: websocket_event_alerts_BoxDoorStatusReport
* Description	: 箱门状态上报
* Parameter		: 
* Return		: 
*	
************************************************************/
uint16_t websocket_event_alerts_BoxDoorStatusReport(char* buf)
{
//	uint16_t  jsonlen= 0;
//	struct local_ip_t  *local = app_get_local_network_function();
//	struct threshold_params  *threshold = app_get_threshold_param_function();
//	data_collection_t *det_param = det_get_collect_data();
//	rtc_time_t local_rtc,utc_time;
//	char *uuid_buf = app_get_device_uuid();	
//	jsonlen += sprintf(buf+jsonlen,"%s","{\n\t\"topic\":\t");
//	jsonlen += sprintf(buf+jsonlen,"%s","\"/iot/global/0-global/model/event/report/BoxDoorMgr/BoxDoorStatusReport\",\n");
//	jsonlen += sprintf(buf+jsonlen,"%s","\t\"basic\":\t{\n\t\t\"ipV4Address\":\t");
//	
//	jsonlen += sprintf(buf+jsonlen,"\"%d.%d.%d.%d\",\n",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
//	jsonlen += sprintf(buf+jsonlen,"%s","\t\t\"ipV6Address\":\t\"\",\n");
//	jsonlen += sprintf(buf+jsonlen,"%s","\t\t\"macAddress\":\t");
//	jsonlen += sprintf(buf+jsonlen,"\"%02x-%02x-%02x-%02x-%02x-%02x\",\n",
//																	local->mac[0],local->mac[1],local->mac[2],
//																	local->mac[3],local->mac[4],local->mac[5]);	
//	// dateTime		
//	RTC_Get_Time(&local_rtc);
//	local_to_utc_time(&utc_time,-8,local_rtc);
//	jsonlen += sprintf(buf+jsonlen,"%s","\t\t\"dateTime\":\t");
//	jsonlen += sprintf(buf+jsonlen,"\"%04d-%02d-%02dT%02d:%02d:%02d+08:00\",\n",
//														utc_time.year,utc_time.month,utc_time.data,
//														utc_time.hour,utc_time.min,utc_time.sec);		
//	
//		// UUID	
//	jsonlen += sprintf(buf+jsonlen,"%s","\t\t\"UUID\":\t");	
//	jsonlen += sprintf(buf+jsonlen,"\"%s\"\n",uuid_buf);
//	jsonlen += sprintf(buf+jsonlen,"%s","\t},\n");
//	jsonlen += sprintf(buf+jsonlen,"%s","\t\"payload\":\t{\n");
//	jsonlen += sprintf(buf+jsonlen,"%s","\t\t\"doorStatusList\":\t[{\n");
//	jsonlen += sprintf(buf+jsonlen,"%s","\t\t\t\t\"doorID\":\t1,\n");
//	if(det_param->open_door == 0)
//		jsonlen += sprintf(buf+jsonlen,"%s","\t\t\t\t\"doorStatus\":\t\"closed\"\n");
//	else if(det_param->open_door == 1)
//		jsonlen += sprintf(buf+jsonlen,"%s","\t\t\t\t\"doorStatus\":\t\"opened\"\n");
//	jsonlen += sprintf(buf+jsonlen,"%s","\t\t\t}]\n\t}\n}");
//	return jsonlen;


	my_json_t js;
	struct local_ip_t  *local = app_get_local_network_function();
	data_collection_t *det_param = det_get_collect_data();
	char temp[30] = {0};
	rtc_time_t local_rtc,utc_time;
	char *uuid_buf = app_get_device_uuid();

	my_json_init(&js, buf, 512);
	my_json_object_begin(&js, NULL);
	my_json_add_str(&js, "topic", "/iot/global/0-global/model/event/report/BoxDoorMgr/BoxDoorStatusReport");

	my_json_object_begin(&js, "basic");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
	my_json_add_str(&js, "ipV4Address", temp);
	my_json_add_str(&js, "ipV6Address", "");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%02x-%02x-%02x-%02x-%02x-%02x",
									local->mac[0],local->mac[1],local->mac[2],
									local->mac[3],local->mac[4],local->mac[5]);
	my_json_add_str(&js, "macAddress", temp);
	RTC_Get_Time(&local_rtc);
	local_to_utc_time(&utc_time,-8,local_rtc);
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%04d-%02d-%02dT%02d:%02d:%02d+08:00",
					utc_time.year,utc_time.month,utc_time.data,
					utc_time.hour,utc_time.min,utc_time.sec);
	my_json_add_str(&js, "dateTime", temp);
	my_json_add_str(&js, "UUID", uuid_buf);
	my_json_object_end(&js);						// basic

	my_json_object_begin(&js, "payload");
	my_json_array_begin(&js, "doorStatusList");
	my_json_object_begin(&js, NULL);
	my_json_add_int(&js, "doorID", 1);
	if(det_param->open_door == 0)
		my_json_add_str(&js, "doorStatus", "closed");
	else if(det_param->open_door == 1)
		my_json_add_str(&js, "doorStatus", "opened");
	my_json_object_end(&js);
	my_json_array_end(&js);
	my_json_object_end(&js);						// payload

	my_json_object_end(&js);						// root

	if(HTTP_COM_DEBUG) printf("BoxDoorStatusReport:%s\n",buf);
	return (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: websocket_event_alerts_DevicePowerParamAlarm
* Description	: 设备电力参数报警
* Parameter		: 
* Return		: 
*	
************************************************************/
uint16_t websocket_event_alerts_DevicePowerParamAlarm(char* buf,uint8_t typef)
{
	struct local_ip_t  *local = app_get_local_network_function();
	data_collection_t *det_param = det_get_collect_data();
	char temp[30] = {0};
	rtc_time_t local_rtc,utc_time;
	char *uuid_buf = app_get_device_uuid();
	my_json_t js;

	my_json_init(&js, buf, 512);
	my_json_object_begin(&js, NULL);
	my_json_add_str(&js, "topic", "/iot/global/0-global/model/event/report/PowerMgr/DevicePowerParamAlarm");

	my_json_object_begin(&js, "basic");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
	my_json_add_str(&js, "ipV4Address", temp);
	my_json_add_str(&js, "ipV6Address", "");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%02x-%02x-%02x-%02x-%02x-%02x",
									local->mac[0],local->mac[1],local->mac[2],
									local->mac[3],local->mac[4],local->mac[5]);
	my_json_add_str(&js, "macAddress", temp);
	RTC_Get_Time(&local_rtc);
	local_to_utc_time(&utc_time,-8,local_rtc);
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%04d-%02d-%02dT%02d:%02d:%02d+08:00",
					utc_time.year,utc_time.month,utc_time.data,
					utc_time.hour,utc_time.min,utc_time.sec);
	my_json_add_str(&js, "dateTime", temp);
	my_json_add_str(&js, "UUID", uuid_buf);
	my_json_object_end(&js);						// basic

	my_json_object_begin(&js, "payload");
	my_json_add_str(&js, "alarmSourceType", "powerPort");
	switch(typef)
	{
		case 0: // 电压低
			my_json_add_str(&js, "paramType", "voltage");
			my_json_add_str(&js, "exceptionType", "low");
			break;
		case 1: // 电压高
			my_json_add_str(&js, "paramType", "voltage");
			my_json_add_str(&js, "exceptionType", "high");
			break;
		case 2: // 电流高
			my_json_add_str(&js, "paramType", "current");
			my_json_add_str(&js, "exceptionType", "high");
			break;
		default:
			my_json_add_str(&js, "paramType", "");
			my_json_add_str(&js, "exceptionType", "");
			break;
	}
	my_json_object_begin(&js, "voltageparam");
	my_json_add_double(&js, "voltage", det_param->vin220v);
	my_json_object_end(&js);
	my_json_object_begin(&js, "currentparam");
	my_json_add_double(&js, "electricCurrent", det_param->total_current);
	my_json_object_end(&js);
	my_json_object_end(&js);						// payload

	my_json_object_end(&js);						// root

	if(HTTP_COM_DEBUG) printf("DevicePowerParamAlarm:%s\n",buf);
	return (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: websocket_event_alerts_DevPowerStatusReport
* Description	: 设备电源状态上报
* Parameter		: 
* Return		: 
*	
************************************************************/
uint16_t websocket_event_alerts_DevPowerStatusReport(char* buf)
{
	struct local_ip_t  *local = app_get_local_network_function();
	char temp[30] = {0};
	rtc_time_t local_rtc,utc_time;
	char *uuid_buf = app_get_device_uuid();
	my_json_t js;

	my_json_init(&js, buf, 512);
	my_json_object_begin(&js, NULL);
	my_json_add_str(&js, "topic", "/iot/global/0-global/model/event/report/PowerMgr/DevPowerStatusReport");

	my_json_object_begin(&js, "basic");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
	my_json_add_str(&js, "ipV4Address", temp);
	my_json_add_str(&js, "ipV6Address", "");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%02x-%02x-%02x-%02x-%02x-%02x",
									local->mac[0],local->mac[1],local->mac[2],
									local->mac[3],local->mac[4],local->mac[5]);
	my_json_add_str(&js, "macAddress", temp);
	RTC_Get_Time(&local_rtc);
	local_to_utc_time(&utc_time,-8,local_rtc);
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%04d-%02d-%02dT%02d:%02d:%02d+08:00",
					utc_time.year,utc_time.month,utc_time.data,
					utc_time.hour,utc_time.min,utc_time.sec);
	my_json_add_str(&js, "dateTime", temp);
	my_json_add_str(&js, "UUID", uuid_buf);
	my_json_object_end(&js);						// basic

	my_json_object_begin(&js, "payload");
	my_json_add_str(&js, "devName", "");
	my_json_add_str(&js, "model", HARD_NO_STR);
	my_json_add_str(&js, "powerStatus", "abnormal");
	my_json_add_str(&js, "reclosingStatus", "closed");
	my_json_add_str(&js, "electricLeakageStatus", "normal");
	my_json_object_end(&js);						// payload

	my_json_object_end(&js);						// root

	if(HTTP_COM_DEBUG) printf("DevPowerStatusReport:%s\n",buf);
	return (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: websocket_event_alerts_fan_status
* Description	: 风扇状态上报
* Parameter		: 
* Return		: 
*	
************************************************************/
uint16_t websocket_event_alerts_fan_status(char* buf)
{
	struct local_ip_t  *local = app_get_local_network_function();
	char temp[30] = {0};
	rtc_time_t local_rtc,utc_time;
	char *uuid_buf = app_get_device_uuid();
	my_json_t js;

	my_json_init(&js, buf, 512);
	my_json_object_begin(&js, NULL);
	my_json_add_str(&js, "topic", "/iot/global/0-global/model/event/report/Fan/FanStatusReport");

	my_json_object_begin(&js, "basic");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
	my_json_add_str(&js, "ipV4Address", temp);
	my_json_add_str(&js, "ipV6Address", "");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%02x-%02x-%02x-%02x-%02x-%02x",
									local->mac[0],local->mac[1],local->mac[2],
									local->mac[3],local->mac[4],local->mac[5]);
	my_json_add_str(&js, "macAddress", temp);
	RTC_Get_Time(&local_rtc);
	local_to_utc_time(&utc_time,-8,local_rtc);
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%04d-%02d-%02dT%02d:%02d:%02d+08:00",
					utc_time.year,utc_time.month,utc_time.data,
					utc_time.hour,utc_time.min,utc_time.sec);
	my_json_add_str(&js, "dateTime", temp);
	my_json_add_str(&js, "UUID", uuid_buf);
	my_json_object_end(&js);						// basic

	my_json_object_begin(&js, "payload");
	my_json_add_str(&js, "devName", "");
	my_json_add_str(&js, "model", HARD_NO_STR);
	my_json_array_begin(&js, "fanStatusList");
	my_json_object_begin(&js, NULL);
	my_json_add_int(&js, "ID", FAN_1+1);
	if(fan_get_status_function(FAN_1) == 1)
	{
		my_json_add_int(&js, "speed", 3600);
		my_json_add_str(&js, "fanStatus", "normal");
	}
	else
	{
		my_json_add_int(&js, "speed", 0);
		my_json_add_str(&js, "fanStatus", "notPower");
	}
	my_json_add_int(&js, "curRunningTime", app_get_fan_time(0));
	my_json_add_int(&js, "totalRunningTime", app_get_fan_time(1));
	my_json_object_end(&js);
	my_json_array_end(&js);
	my_json_object_end(&js);						// payload

	my_json_object_end(&js);						// root

	if(HTTP_COM_DEBUG) printf("BoxDoorStatusReport:%s\n",buf);
	return (uint16_t)my_json_len(&js);
}



/* HTTP 格式
HTTP/1.1 401 Unauthorized
Content-Type: text/html
Date: Mon, 01 Feb 2021 14:06:08 GMT
Content-Length: 126
Connection: Keep-Alive
WWW-Authenticate: Digest realm="3521781c29acb312330dd668", qop="auth",
nonce="05a5f52a199db1b8:3521781c29acb312330dd668:1775dea3f75:85", algorithm="SHA-256"
*/
const char MONTH[12][4] = {"Jan","Feb","Mar","Ari","May","Jun","Jul","Aut","Sep","Oct","Nov","Dec"};
const char WEEK[7][5] = {"Mon","Tue","Wed","Thur","Fri","Sat","Sun"};
/************************************************************
*
* Function name	: http_ack_authentication
* Description	: http认证算法生成
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_ack_authentication_algorithms(char* buf,uint16_t *size,uint8_t id)
{
	uint16_t str_len = 0;		
  str_len = sprintf(buf,"%s","WWW-Authenticate: Digest realm="); 		
//	str_len += sprintf(buf+str_len,"\"%s\"",HARD_NO_STR);	
	str_len += sprintf(buf+str_len,"\"%s\"","DS-2CD2520F");
	str_len += sprintf(buf+str_len,"%s",",qop=\"auth\",nonce=\"");	
	str_len += sprintf(buf+str_len,"%08x%08x%08x%08x%08x%08x",
										 RNG_Get_RandomNum(),RNG_Get_RandomNum(),RNG_Get_RandomNum(),
										 RNG_Get_RandomNum(),RNG_Get_RandomNum(),RNG_Get_RandomNum());
	if(id == 0)
		str_len += sprintf(buf+str_len,"\"%s",",algorithm=\"MD5\"");		
	else
		str_len += sprintf(buf+str_len,"%s",",algorithm=\"SHA-256\"");		
	
	*size = str_len;
}

/************************************************************
*
* Function name	: http_com_certified_handshake_ack_function
* Description	: HTTP摘要认证
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_com_certified_handshake_ack_function(char *data, uint16_t *len, char *json_str,uint16_t jsonlen,uint8_t flag)
{
	uint16_t str_len = 0;
	rtc_time_t local_rtc,utc_time = {0};
	
	if(flag == 0)
	{
		str_len = sprintf(data,"%s","HTTP/1.1 401 Unauthorized\r\nContent-Type: text/html\r\nDate: "); 	
		RTC_Get_Time(&local_rtc);
		local_to_utc_time(&utc_time,-8,local_rtc);
		str_len += sprintf(data+str_len,"%s, ",WEEK[utc_time.week]);	
		str_len += sprintf(data+str_len,"%02d ",utc_time.data);		
		str_len += sprintf(data+str_len,"%s ",MONTH[utc_time.month]);	
		str_len += sprintf(data+str_len,"%02d %02d:%02d:%02d GMT\r\n",utc_time.year,utc_time.hour,utc_time.min,utc_time.sec);		
		str_len += sprintf(data+str_len,"%s","Content-Length: 0\r\nConnection: Keep-Alive\r\n"); 	
		str_len += sprintf(data+str_len,"%s\r\n\r\n",json_str);
	}
	else if(flag == 1)
	{
		str_len = sprintf(data,"%s","HTTP/1.1 200 OK\r\nContent-Type: application/json; charset=\"UTF-8\"\r\nDate: "); 	
		RTC_Get_Time(&local_rtc);
		local_to_utc_time(&utc_time,-8,local_rtc);
		str_len += sprintf(data+str_len,"%s, ",WEEK[utc_time.week]);	
		str_len += sprintf(data+str_len,"%02d ",utc_time.data);		
		str_len += sprintf(data+str_len,"%s ",MONTH[utc_time.month]);	
		str_len += sprintf(data+str_len,"%02d %02d:%02d:%02d GMT\r\n",utc_time.year,utc_time.hour,utc_time.min,utc_time.sec);		
		str_len += sprintf(data+str_len,"Content-Length: %d",jsonlen+4);
		str_len += sprintf(data+str_len,"%s","\r\nConnection: Keep-Alive\r\n\r\n");
		str_len += sprintf(data+str_len,"%s\r\n\r\n",json_str);		
	}
	else if(flag == 2)
	{
		str_len = sprintf(data,"%s","HTTP/1.1 401 Unauthorized\r\nContent-Type: application/json; charset=\"UTF-8\"\r\nDate: "); 	
		RTC_Get_Time(&local_rtc);
		local_to_utc_time(&utc_time,-8,local_rtc);
		str_len += sprintf(data+str_len,"%s, ",WEEK[utc_time.week]);	
		str_len += sprintf(data+str_len,"%02d ",utc_time.data);		
		str_len += sprintf(data+str_len,"%s ",MONTH[utc_time.month]);	
		str_len += sprintf(data+str_len,"%02d %02d:%02d:%02d GMT\r\n",utc_time.year,utc_time.hour,utc_time.min,utc_time.sec);		
		str_len += sprintf(data+str_len,"Content-Length: %d",jsonlen+4);
		str_len += sprintf(data+str_len,"%s","\r\nConnection: Keep-Alive\r\n\r\n");
		str_len += sprintf(data+str_len,"%s\r\n\r\n",json_str);		
	}
	*len = str_len;
}

/************************************************************
*
* Function name	: http_ack_certification_calculations
* Description	: http认证计算，验证回复信息
* Parameter		: 
* Return		: 
*	
************************************************************/
int http_ack_certification_calculations(com_http_cmd_t *com_cmd)
{
	struct device_param *device = app_get_device_param_function();
	uint8_t HA1[32] = {0};
	uint8_t HA2[32] = {0};
	uint8_t HA3[32] = {0};
	char response[40] = {0};	
	uint8_t buff[256] = {0};	
	uint16_t str_len = 0;
	
	if(strcmp(com_cmd->algorithm ,"MD5")==0 ) // 加密方式
	{
		if(HTTP_COM_DEBUG) printf("...MD5...\n");
		str_len = sprintf((char*)buff,"%s:%s:%s","root",com_cmd->realm,device->password); 
		MD5_Encode(buff,str_len,HA1);
		memset(buff, 0, 256);
		str_len = sprintf((char*)buff,"%s:%s",com_cmd->method,com_cmd->uri); 
		MD5_Encode(buff,str_len,HA2);	
		memset(buff, 0, 256);
		str_len = 0;
		for(uint8_t i=0;i<16;i++)
			str_len += sprintf((char*)(buff+str_len),"%02x",HA1[i]);
		
		str_len += sprintf((char*)(buff+str_len),":%s:%08x:%s:%s:",com_cmd->nonce,com_cmd->nc,com_cmd->cnonce,com_cmd->qop);

		for(uint8_t i=0;i<16;i++)
			str_len += sprintf((char*)(buff+str_len),"%02x",HA2[i]);

		MD5_Encode(buff,str_len,HA3);
		str_len = 0;
		for(uint8_t i=0;i<16;i++)
			str_len += sprintf((char*)(response+str_len),"%02x",HA3[i]);		
	}
	else if(strcmp(com_cmd->algorithm ,"SHA-256")==0 ) // 加密方式
	{
		if(HTTP_COM_DEBUG) printf("...SHA-256...\n");
		str_len = sprintf((char*)buff,"%s:%s:%s","root",com_cmd->realm,device->password); 
		SHA_Encrypt(CMOX_SHA256_ALGO,buff,str_len,HA1);
		memset(buff, 0, 256);
		str_len = sprintf((char*)buff,"%s:%s",com_cmd->method,com_cmd->uri); 
		SHA_Encrypt(CMOX_SHA256_ALGO,buff,str_len,HA2);	
		memset(buff, 0, 256);
		str_len = 0;
		for(uint8_t i=0;i<32;i++)
			str_len += sprintf((char*)(buff+str_len),"%02x",HA1[i]);
		
		str_len += sprintf((char*)(buff+str_len),":%s:%08x:%s:%s:",com_cmd->nonce,com_cmd->nc,com_cmd->cnonce,com_cmd->qop);

		for(uint8_t i=0;i<32;i++)
			str_len += sprintf((char*)(buff+str_len),"%02x",HA2[i]);
		
		SHA_Encrypt(CMOX_SHA256_ALGO,buff,str_len,HA3);	
		str_len = 0;
		for(uint8_t i=0;i<32;i++)
			str_len += sprintf((char*)(response+str_len),"%02x",HA3[i]);		
	}

	if(strcmp(response,com_cmd->response)== 0 ) // 判断应答数据
		return 1;
	else
		return 0;
}

/************************************************************
*
* Function name	: http_ack_certification_status
* Description	: HTTP认证状态
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_ack_certification_status(char* buf,uint16_t *size,uint8_t flag)
{
	my_json_t js;

	my_json_init(&js, buf, 4000);		/* buf = http_com_certified_handshake 调用时的 ack_json_buf */
	my_json_object_begin(&js, NULL);
	if(flag == 0)
	{
		my_json_add_int(&js, "status", 200);
		my_json_add_str(&js, "code", "0x00000000");
		my_json_add_str(&js, "errorMsg", "Succeeded.");
	}
	else
	{
		my_json_add_int(&js, "status", 401);
		my_json_add_str(&js, "code", "0x00100001");
		my_json_add_str(&js, "errorMsg", "The device is not activated.");
	}

	my_json_object_begin(&js, "data");
	if(flag == 0)
	{
		my_json_add_int(&js, "statusValue", 200);
		my_json_add_str(&js, "statusString", "OK");
	}
	else
	{
		my_json_add_int(&js, "statusValue", 401);
		my_json_add_str(&js, "statusString", "Unauthorized");
	}
	my_json_add_bool(&js, "isDefaultPassword", 0);		// 原 cJSON_AddFalseToObject
	my_json_add_bool(&js, "isRiskPassword", 0);			// 原 cJSON_AddFalseToObject
	my_json_add_bool(&js, "isActivated", 1);			// 原 cJSON_AddTrueToObject
	my_json_add_int(&js, "residualValidity", 9999);
	my_json_object_end(&js);							// data
	my_json_object_end(&js);							// root

	*size = (uint16_t)my_json_len(&js);
}


/************************************************************
*
* Function name	: local_to_utc_time
* Description	: 本地时间转UTC时间
* Parameter		: 
* Return		: 
*	
************************************************************/
void local_to_utc_time(rtc_time_t *utc_time, int8_t timezone, rtc_time_t local_time)
{
	int year,month,day,hour,week;
	int lastday = 0;			//last day of this month 本月天数
	int lastlastday = 0;		//last day of last month 上个月天数

	year	= local_time.year;	//utc time
	month = local_time.month;
	day 	= local_time.data;
	hour 	= local_time.hour + timezone; 
	week  = local_time.week;
	
	//1月大，2月小，3月大，4月小，5月大，6月小，7月大，8月大，9月小，10月大，11月小，12月大
	if(month==1 || month==3 || month==5 || month==7 || month==8 || month==10 || month==12)
	{
		lastday = 31;//本月天数
		lastlastday = 30;//这里应该补上上个月的天数
		
		if(month == 3)
		{
			if((year%400 == 0)||(year%4 == 0 && year%100 != 0))//if this is lunar year
				lastlastday = 29;
			else
				lastlastday = 28;
		}
		
		if(month == 8 || month == 1)//这里应该是8月和1月，因为8月和1月的上一个月（7月和12月）的天数是31天的
			lastlastday = 31;
	}
	else if(month == 4 || month == 6 || month == 9 || month == 11)
	{
		lastday = 30;
		lastlastday = 31;
	}
	else
	{
		lastlastday = 31;
		
		if((year%400 == 0)||(year%4 == 0 && year%100 != 0))
			lastday = 29;
		else
			lastday = 28;
	}

	if(hour >= 24)// if >24, day+1
	{					
		hour -= 24;
		day += 1;
		week+= 1;		// 日期加1
		if(week > 7)
			week = 1;  
		if(day > lastday)// next month, day-lastday of this month
		{ 		
			day -= lastday;
			month += 1;
			if(month > 12)// next year, month-12
			{
				month -= 12;
				year += 1;
			}
		}
	}
	
	if(hour < 0)// if <0, day-1
	{
		hour += 24;
		day -= 1;
		week -= 1;
		if(week < 1)
			week = 7;
		if(day < 1)// month-1, day=last day of last month
		{
			day = lastlastday;
			month -= 1;
			if(month < 1)// last year, month=12
			{
				month = 12;
				year -= 1;
			}
		}
	}
	utc_time->year  = year;
	utc_time->month = month;
	utc_time->data  = day;
	utc_time->week 	= week;
	utc_time->hour  = hour;
	utc_time->min	 	= local_time.min;
	utc_time->sec	 	= local_time.sec;
}

/************************************************************
*
* Function name	: app_deal_http_json_param
* Description	: 获取JSON数据
* Parameter		: 
* Return		: 
*	
************************************************************/
int http_deal_json_param(sys_json_t *json_d,char* data)
{
	char  *str  = NULL;	
	if(json_d->len > 0)
	{
		str = strstr(data,"\r\n\r\n{");
		if(str == NULL)  return -1;
		strncpy(json_d->buf, str+4,json_d->len);
	}
	return 0;
}

/************************************************************
*
* Function name	: http_com_data_deal
* Description	: 处理http数据
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_com_data_deal(char* data,com_http_cmd_t *com_cmd)
{
	char *str1 = NULL;
	char *str_start = NULL;
	char *str_end = NULL;

	str1 = strstr(data,"model/");
	str1 = strstr((str1+6),"/");
	str1 = strstr((str1+1),"/");
	str_start = strstr((str1+1),"/");
	str_end = strstr((str1+1)," ");
	strncpy(com_cmd->cmd, str_start+1,(str_end-str_start-1));
	str1 = strstr(str1,"\r\nHost");
	sscanf(str1,"%*[^: ]: %[^\r\n]",com_cmd->host_ip); 	
	
	if((com_cmd->http_cmd != 1)||(com_cmd->http_cmd != 2))
	{
		str1 = strstr(data,"realm="); // 获取限制域
		sscanf(str1,"%*[^=\"]=\"%[^\"]",com_cmd->realm); 		

		str1 = strstr(data,"nonce="); // 随机数
		sscanf(str1,"%*[^=\"]=\"%[^\"]",com_cmd->nonce); 
		
		str1 = strstr(data,"uri="); // URI
		sscanf(str1,"%*[^=\"]=\"%[^\"]",com_cmd->uri);

		str1 = strstr(data,"algorithm="); //加密方式
		sscanf(str1,"%*[^=\"]=\"%[^\"]",com_cmd->algorithm); 
		
		str1 = strstr(data,"cnonce="); // 客户端密码
		sscanf(str1,"%*[^=\"]=\"%[^\"]",com_cmd->cnonce); 
		
		str1 = strstr(data,"nc=");
		sscanf(str1+3,"%08x",&com_cmd->nc); 

		str1 = strstr(data,"qop=");
		sscanf(str1,"%*[^=\"]=\"%[^\"]",com_cmd->qop); 
		
		str1 = strstr(data,"response=");
		sscanf(str1,"%*[^=\"]=\"%[^\"]",com_cmd->response); 
		
		str1 = strstr(data,"opaque=");
		sscanf(str1,"%*[^=\"]=\"%[^\"]",com_cmd->opaque); 
	}
}

/************************************************************
*
* Function name	: app_set_http_json_length
* Description	: 设置JSON数据长度
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_set_http_json_length(uint16_t data)
{
	sg_json_t.len = data;
}


/************************************************************
*
* Function name	: http_com_deal_configure_angle
* Description	: 配置倾斜度
* Parameter		: 
* Return		: 
*	
************************************************************/
int http_com_deal_configure_angle(char* buf)
{
	struct threshold_params *param = app_get_threshold_param_function();
	static jsmntok_t tokens[MY_JSON_MAX_TOKENS];
	jsmn_parser parser;
	int ntok, t_data, t_value, t_en, t_high;
	int ret = -1;

	if(HTTP_COM_DEBUG) printf("http_com_deal_configure_angle....\n");
	if(buf == NULL) return -1;

	/* jsmn 解析(不使用 cJSON) */
	jsmn_init(&parser);
	ntok = jsmn_parse(&parser, buf, strlen(buf), tokens, MY_JSON_MAX_TOKENS);
	if(ntok < 0) return -1;

	t_data  = my_json_find(buf, tokens, ntok, 0, "data");
	t_value = (t_data  >= 0) ? my_json_find(buf, tokens, ntok, t_data, "Value") : -1;
	t_en    = (t_value >= 0) ? my_json_find(buf, tokens, ntok, t_value, "enabled") : -1;
	t_high  = (t_value >= 0) ? my_json_find(buf, tokens, ntok, t_value, "tiltAngleHignThreshold") : -1;

	if(t_en >= 0)
		app_set_report_switch_status(my_json_is_true(buf, &tokens[t_en]) ? 1 : 0, FLAG_A);

	if(t_high >= 0)
	{
		int angle = my_json_to_int(buf, &tokens[t_high]);
		if((angle > 0) && (angle < 180))
		{
			param->angle = angle;
			app_set_threshold_param_function(*param);
			ret = 0;
		}
	}
	return ret;
}

/************************************************************
*
* Function name	: http_com_deal_configure_humiture
* Description	: 配置湿度
* Parameter		: 
* Return		: 
*	
************************************************************/
int http_com_deal_configure_humiture(char* buf)
{
	struct threshold_params *param = app_get_threshold_param_function();
	static jsmntok_t tokens[MY_JSON_MAX_TOKENS];
	jsmn_parser parser;
	int ntok, t_data, t_value, t_en, t_low, t_high;
	int error1 = -1, error2 = -1;

	if(HTTP_COM_DEBUG) printf("http_com_deal_configure_humiture....\n");
	if(buf == NULL) return -1;

	jsmn_init(&parser);
	ntok = jsmn_parse(&parser, buf, strlen(buf), tokens, MY_JSON_MAX_TOKENS);
	if(ntok < 0) return -1;

	t_data  = my_json_find(buf, tokens, ntok, 0, "data");
	t_value = (t_data  >= 0) ? my_json_find(buf, tokens, ntok, t_data, "Value") : -1;
	if(t_value >= 0)
	{
		t_en   = my_json_find(buf, tokens, ntok, t_value, "enabled");
		t_low  = my_json_find(buf, tokens, ntok, t_value, "minHumidity");
		t_high = my_json_find(buf, tokens, ntok, t_value, "maxHumidity");

		if(t_en >= 0)
		{
			if(my_json_is_true(buf, &tokens[t_en]))
			{
				app_set_report_switch_status(1,FLAG_H_L);
				app_set_report_switch_status(1,FLAG_H_H);
			}
			else
			{
				app_set_report_switch_status(0,FLAG_H_L);
				app_set_report_switch_status(0,FLAG_H_H);
			}
		}

		if(t_high >= 0)
		{
			int v = my_json_to_int(buf, &tokens[t_high]);
			if((v > 0) && (v < 100))
			{
				param->temp_high = v;
				error1 = 0;
			}
		}

		/* 高值无效时与原逻辑一致: 跳过低值处理 */
		if((error1 == 0) && (t_low >= 0))
		{
			int v = my_json_to_int(buf, &tokens[t_low]);
			if((v > 0) && (v < 100))
			{
				param->temp_low = v;
				app_set_threshold_param_function(*param);
				error2 = 0;
			}
		}
	}

	if((error1 == 0) && (error2 == 0))
		return 0;
	else
		return -1;
}

/************************************************************
*
* Function name	: http_com_deal_configure_temperature
* Description	: 配置温度
* Parameter		: 
* Return		: 
*	
************************************************************/
int http_com_deal_configure_temperature(char* buf)
{
	struct threshold_params *param = app_get_threshold_param_function();
	static jsmntok_t tokens[MY_JSON_MAX_TOKENS];
	jsmn_parser parser;
	int ntok, t_data, t_value, t_en, t_low, t_high;
	int error1 = -1, error2 = -1;

	if(HTTP_COM_DEBUG) printf("http_com_deal_configure_temperature....\n");
	if(buf == NULL) return -1;

	jsmn_init(&parser);
	ntok = jsmn_parse(&parser, buf, strlen(buf), tokens, MY_JSON_MAX_TOKENS);
	if(ntok < 0) return -1;

	t_data  = my_json_find(buf, tokens, ntok, 0, "data");
	t_value = (t_data  >= 0) ? my_json_find(buf, tokens, ntok, t_data, "Value") : -1;
	if(t_value >= 0)
	{
		t_en   = my_json_find(buf, tokens, ntok, t_value, "enabled");
		t_low  = my_json_find(buf, tokens, ntok, t_value, "minTemperature");
		t_high = my_json_find(buf, tokens, ntok, t_value, "maxTemperature");

		if(t_en >= 0)
		{
			if(my_json_is_true(buf, &tokens[t_en]))
			{
				app_set_report_switch_status(1,FLAG_T_L);
				app_set_report_switch_status(1,FLAG_T_H);
			}
			else
			{
				app_set_report_switch_status(0,FLAG_T_L);
				app_set_report_switch_status(0,FLAG_T_H);
			}
		}

		if(t_high >= 0)
		{
			int v = my_json_to_int(buf, &tokens[t_high]);
			if((v > -100) && (v < 1000))
			{
				param->temp_high = v;
				error1 = 0;
			}
		}

		/* 高值无效时与原逻辑一致: 跳过低值处理 */
		if((error1 == 0) && (t_low >= 0))
		{
			int v = my_json_to_int(buf, &tokens[t_low]);
			if((v > -100) && (v < 1000))
			{
				param->temp_low = v;
				app_set_threshold_param_function(*param);
				error2 = 0;
			}
		}
	}

	if((error1 == 0) && (error2 == 0))
		return 0;
	else
		return -1;
}

/************************************************************
*
* Function name	: http_com_deal_configure_powerport
* Description	: 配置供电口
* Parameter		: 
* Return		: 
*	
************************************************************/
int http_com_deal_configure_powerport(char* buf)
{
	static jsmntok_t tokens[MY_JSON_MAX_TOKENS];
	jsmn_parser parser;
	int ntok, t_data, t_list, t_item, t_code, t_en;
	int count, idx, error = 0;
	uint8_t port_id[8]={0};
	uint8_t port_status[8]={0};

	if(HTTP_COM_DEBUG) printf("http_com_deal_configure_powerport....\n");
	if(buf == NULL) return -1;

	jsmn_init(&parser);
	ntok = jsmn_parse(&parser, buf, strlen(buf), tokens, MY_JSON_MAX_TOKENS);
	if(ntok < 0)
	{
		printf("json error\n");
		return -1;
	}

	t_data = my_json_find(buf, tokens, ntok, 0, "data");
	t_list = (t_data >= 0) ? my_json_find(buf, tokens, ntok, t_data, "powerPortCfgList") : -1;
	if(!my_json_is_array((t_list >= 0) ? &tokens[t_list] : NULL))
		return -1;

	count = my_json_arr_size(tokens, t_list);
	if(count > 8) count = 8;			/* 防止 port_id[]/port_status[] 越界 */
	for(idx = 0; idx < count; idx++)
	{
		t_item = my_json_arr_item(tokens, t_list, idx);
		if(t_item < 0) break;

		t_code = my_json_find(buf, tokens, ntok, t_item, "portCode");
		t_en   = my_json_find(buf, tokens, ntok, t_item, "powerEnabled");
		if(t_code >= 0)
			port_id[idx] = (uint8_t)my_json_to_int(buf, &tokens[t_code]);
		if(t_en >= 0)
			port_status[idx] = my_json_is_true(buf, &tokens[t_en]) ? RELAY_ON : RELAY_OFF;

		if((port_id[idx] > 0) && (port_id[idx] <= 8))
		{
			if(app_opeare_relay_function(idx, port_id[idx], port_status[idx]) < 0)
				error = -1;
			else
				error = 0;
		}
		else
		{
			error = -1;
			break;			/* 原 goto POWERPORT_ERROR */
		}
	}
	return error;
}

/*
{
"id":"w2dsdwdwdwdwdwr33qzxadswfrwfwdd",
"URI":"POST /iot/{$CHILDID}/{$LOCALINDEX-$RESOURCETYPE}/model/service/operate/EventSubscription/AddEventSubscribeCfg",
"body":{
"eventMode": "list",
"EventList": [{
"eventType": "VehicleEnterExit.GateStaticInfo"
"filePathType": "binary",
"channels": [
					1,
					2
]}]}}
*/
/************************************************************
*
* Function name	: http_websocket_event_param
* Description	: 解析事件订阅
* Parameter		: 
* Return		: 
*	
************************************************************/
int http_websocket_event_param(char* data,sys_event_t *sys_event)  
{   
	char *str = NULL; 
	char *str_start = NULL; 
	char *str_end = NULL; 
	str = strstr((char *)data,"\"URI\"");
	if(strstr(str,EventSubscription_DeleteEventSubscribeCfg) != NULL) // 删除事件订阅
		sys_event->uri = 1;
	else if(strstr(str,EventSubscription_GetEventSubscribeCfg) != NULL) // 获取事件订阅配置
		sys_event->uri = 2;
	else if(strstr(str,EventSubscription_AddEventSubscribeCfg) != NULL) // 添加事件订阅配置
		sys_event->uri = 3;	
	else if(strstr(str,EventSubscription_ModifyEventSubscribeCfg) != NULL) // 修改事件订阅配置
		sys_event->uri = 4;	
	
	str = strstr((char *)data,"\"id\"");
	if(str != NULL)
	{
		str_start = strstr((char *)str,": ");
		str_end   = strstr((char *)str,"\n");
		if((str_start != NULL)&&(str_end != NULL))
			memcpy(sys_event->id,str_start+3,str_end-str_start-4);
	}
	str = strstr((char *)data,"\"eventMode\"");
	if(str != NULL)
	{
		str_start = strstr((char *)str,": ");
		str_end   = strstr((char *)str,"\n");
		if((str_start != NULL)&&(str_end != NULL))
			memcpy(sys_event->mode,str_start+3,str_end-str_start-4);
	}
	str = strstr((char *)data,"\"eventType\"");
	if(str != NULL)
	{
		str_start = strstr((char *)str,": ");
		str_end   = strstr((char *)str,",");
		if((str_start != NULL)&&(str_end != NULL))
			memcpy(sys_event->list,str_start+3,str_end-str_start-4);
	}
	return 0;
}  

/************************************************************
*
* Function name	: http_event_anay_param
* Description	: 事件订阅设置
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_event_anay_param(sys_event_t *sys_event,uint8_t cliend_id)  
{   
	switch(sys_event->uri)
	{
		case 1:  // 删除事件订阅
			if(strstr(sys_event->mode,"all") != NULL) // 全部添加
			{
				http_data_send_function(DELE_EVENT_ALL ,cliend_id);	
			}
			else
			{
				http_data_send_function(DELE_EVENT_NONE ,cliend_id);
			}
			break;
	
		case 3:  // 添加事件订阅
			if(strstr(sys_event->mode,"all") != NULL) // 全部添加
			{
				http_data_send_function(ADDEVENT_ALL,cliend_id);	
			}
			else
			{
				http_data_send_function(ADDEVENT_NONE,cliend_id);
			}
			break;	

		default: break;
	}
}  

/************************************************************
*
* Function name	: http_websocket_event_add_all
* Description	: 添加事件
* Parameter		: 
* Return		: 
*	
************************************************************/
uint16_t http_websocket_event_add_all(char* buf)
{
//	uint16_t  jsonlen= 0;
//	char *uuid_buf = app_get_device_uuid();
//	jsonlen += sprintf(buf+jsonlen,"%s","{\n\t\"id\":\t");
//	jsonlen += sprintf(buf+jsonlen,"\"%s\",\n",sg_event_param.id);
//	jsonlen += sprintf(buf+jsonlen,"%s","\t\"body\":\t{\n");
//	jsonlen += sprintf(buf+jsonlen,"%s","\t\t\"subscribeEventID\":\t");
//	jsonlen += sprintf(buf+jsonlen,"\"%s\"\n",uuid_buf);
//	jsonlen += sprintf(buf+jsonlen,"%s","\t}\n");
//	jsonlen += sprintf(buf+jsonlen,"%s","}");
//	return jsonlen;

	my_json_t js;
	char *uuid_buf = app_get_device_uuid();

	my_json_init(&js, buf, 512);
	my_json_object_begin(&js, NULL);
	if(lwipdev.client_websocket_id == 1)
		my_json_add_str(&js, "id", sg_event_param.id);

	my_json_object_begin(&js, "body");
	my_json_add_str(&js, "subscribeEventID", uuid_buf);
	my_json_object_end(&js);
	my_json_object_end(&js);

	if(HTTP_COM_DEBUG) printf("event_add:%s\n",buf);
	return (uint16_t)my_json_len(&js);
}


/************************************************************
*
* Function name	: http_data_send_function
* Description	: HTTP通信发送函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_data_send_function(uint32_t status,uint8_t client_id)
{
	char *ack_json_buf = NULL;
	uint16_t json_len = 0;
	
	ack_json_buf = (char *)mymalloc(SRAMIN,4000);  // 申请内存
	memset(ack_json_buf,0,4000);		
	memset(http_send_buff,0,sizeof(http_send_buff));
	switch(client_id)
	{
		case 1: lwipdev.client1_id = 1; break;
		case 2: lwipdev.client2_id = 1; break;
		case 3: lwipdev.client3_id = 1; break;
		case 4: lwipdev.client4_id = 1; break;
	}
	switch(status)
	{
		case NO_SUPPORT: // 不支持该功能
			http_json_ack_status(ack_json_buf,&json_len,3);
			http_com_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,3);
			tcp_cilent_send_buff(http_send_buff,http_send_length,0); 
		break;
		
		case DOMINS: // 设备支持的功能参数
			http_json_ack_domins(ack_json_buf,&json_len,0);
			http_com_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,0);
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);
		break;
			
		case BoxDevList:
			http_json_ack_BoxSubModelMgr(ack_json_buf,&json_len,0);
			http_com_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,0);
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);
		break;
			
		case WATER_OUT:
			http_json_ack_waterout(ack_json_buf,&json_len,0);
			http_com_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,0);
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);

		break;
		case DEV_INFO:
			http_json_ack_devicedescription(ack_json_buf,&json_len,0);
			http_com_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,0);
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);

		break;
		case DEV_PRODUCE:
			http_json_ack_deviceservicedescription(ack_json_buf,&json_len,0);
			http_com_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,0);
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);

		break;
		case DEV_VERSION:
			http_json_ack_deviceversion(ack_json_buf,&json_len,0);
			http_com_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,0);
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);

			break;
		case ANGLE_DATA:
			http_json_ack_angle(ack_json_buf,&json_len,0);
			http_com_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,0);
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);

			break;
		case TEMPERATURE_DATA:
			http_json_ack_tempature(ack_json_buf,&json_len,0);
			http_com_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,0);
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);

			break;
		case HUMIDITY_DATA:
			http_json_ack_humidity(ack_json_buf,&json_len,0);
			http_com_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,0);
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);

			break;
		case FAN_TWO_STATUS:
			http_json_ack_fanstatus(ack_json_buf,&json_len,0);
			http_com_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,0);
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);

			break;
		case BOX_STATUS:
			http_json_ack_doorstatus(ack_json_buf,&json_len,0);
			http_com_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,0);
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);

			break;
		case NETWORK_VERSION:
			http_json_ack_networkaddress(ack_json_buf,&json_len,0);
			http_com_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,0);
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);

			break;
		case POWER_DATA:
			http_json_ack_devicerealtimepowerparam(ack_json_buf,&json_len,0);
			http_com_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,0);
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);

			break;
		case POWER_PLAN_TIME:
			http_json_ack_PowerPortSwitchTime(ack_json_buf,&json_len,0);
			http_com_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,0);
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);

			break;
		case POWER_PORT_STATUS:
			http_json_ack_GetPowerPortStatusList(ack_json_buf,&json_len,0);
			http_com_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,0);
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);

			break;			
		case HTTP_ATTESTATION:
			if(HTTP_COM_DEBUG) printf("...HTTP_ATTESTATION...\n");
			http_ack_authentication_algorithms(ack_json_buf,&json_len,0);
			http_com_certified_handshake_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,0);
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);

			break;
		case HTTP_CALCULATION_OK:
			if(HTTP_COM_DEBUG) printf("...HTTP_CALCULATION_OK...\n");
			http_ack_certification_status(ack_json_buf,&json_len,0);
			http_com_certified_handshake_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,1);	
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);

			break;
		case HTTP_CALCULATION_ERROR:
			if(HTTP_COM_DEBUG) printf("...HTTP_CALCULATION_ERROR...\n");
			http_ack_certification_status(ack_json_buf,&json_len,1);
			http_com_certified_handshake_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,2);	
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);

			break;
		case SERVER_ERROR:
			http_json_ack_status(ack_json_buf,&json_len,4);
			http_com_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,4);
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);

			break;
		case OPERATE_SUCCESS:
			http_json_ack_status(ack_json_buf,&json_len,0);
			http_com_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,0);
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);

			break;
		case OPERATE_ERROR:
			http_json_ack_status(ack_json_buf,&json_len,1);
			http_com_ack_function((char*)http_send_buff,&http_send_length,ack_json_buf,json_len,1);
			tcp_cilent_send_buff(http_send_buff,http_send_length,0);
			break;

		case HTTP_WEBSOCKET:
			http_to_websocket_verification_ack((char*)http_send_buff,&http_send_length);
			tcp_cilent_send_buff(http_send_buff,http_send_length,1);
			break;
		case HTTP_WEBSOCKET_PING:
			http_websocket_pong_data((char*)http_send_buff,&http_send_length);
			tcp_cilent_send_buff(http_send_buff,http_send_length,1);
			break;
		
		case ADDEVENT_ALL: // 添加事件
			http_websocket_pack_data((char*)http_send_buff,&http_send_length,FLAG_ADD_ALL,0);
			tcp_cilent_send_buff(http_send_buff,http_send_length,1);
			break;
		case DELE_EVENT_ALL: // 删除事件
			http_websocket_pack_data((char*)http_send_buff,&http_send_length,FLAG_DELE_ALL,0);
			tcp_cilent_send_buff(http_send_buff,http_send_length,1);
			break;			
	}
//		printf("http_send_buff:%s\n",http_send_buff);
	myfree(SRAMIN,ack_json_buf);   // 释放内存
}




