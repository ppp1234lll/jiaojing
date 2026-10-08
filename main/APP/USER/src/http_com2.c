#include "http_com.h"
#include "app.h"
#include "det.h"
#include "relay.h"
#include "eth.h"
#include "rtc.h"
#include "malloc.h"
#include "save.h"
#include "lwip_comm.h"
#include "cJSON.h"
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

//客户端2处理
sys_json_t			sg_json_t2 			= {0};
queue_s 				sg_queue_http2	=	{0};	// 队列2	
com_http_cmd_t 	com_http_cmd2 	=	{0};	
com_websocket_data_t sg_websocket2 = {0};
sys_event_t     sg_event_param2 = {0};
uint8_t http_buff2[1000];	

/************************************************************
*
* Function name	: http_com_buff_init
* Description	: HTTP队列初始化
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_com_buff_init2(void)
{
	Init_Queue(&sg_queue_http2);
}

/************************************************************
*
* Function name	: http_com_stroage_data2
* Description	: 将数据存储到缓存区2
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_com_stroage_data2(uint8_t *buff,uint16_t len)
{
	Enqueue_Bytes_From_Buffer(buff,&sg_queue_http2,len);
}

/************************************************************
*
* Function name	: com_deal_http_info_function2
* Description	: HTTP数据处理2
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t com_deal_http_info_function2(void)
{
	struct local_ip_t  *local = app_get_local_network_function();	
	int size	= 0;
	char *str = NULL;
	char local_ipbuf[20] = {0};
	
//	memset(http_buff2,0,1000);
	size = com_http_queue_find_info2(http_buff2,1000);
	if((size != 0)&&(com_http_cmd2.data_recving == 0))
	{	
		if(lwipdev.client_websocket_id == 2)
		{
			http_websocket_analy_data((char*)http_buff2,0,&sg_websocket2);
			if(HTTP_COM_DEBUG) printf("websocket_analy_data:%s\n",sg_websocket2.buf);
			
			if(sg_websocket2.opcode == PAYLOAD_PING)
				http_data_send_function(HTTP_WEBSOCKET_PING,2);
			else
			{
				http_websocket_event_param((char*)sg_websocket2.buf,&sg_event_param2); // 处理事件信息
				http_event_anay_param(&sg_event_param2,2);
			}
		}
		else
		{
			memset(local_ipbuf,0,20);
			sprintf(local_ipbuf,"%d.%d.%d.%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
			str = strstr((char *)http_buff2,HTTP_UPGRATE_WEBSOCKET1); // 判断是否是协议转换
			if(str != NULL)
			{
				sprintf(local_ipbuf,"%d.%d.%d.%d:%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3],local->port);
				str = strstr((char *)http_buff2,"\r\nHost");
				sscanf((char*)str,"%*[^: ]: %[^\r\n]",com_http_cmd2.host_ip); 
				str = strstr((char *)str,"Sec-WebSocket-Key:");
				sscanf((char*)str,"%*[^: ]: %[^\r\n]",web_key_data.key); 
				com_http_cmd2.http_cmd = 1;
				if((strcmp(com_http_cmd2.host_ip,local_ipbuf) == 0)|| // 判断IP是否正确
					 (strcmp(com_http_cmd2.host_ip,TEST_SERVER) == 0))
				{
					if(HTTP_COM_DEBUG) printf("HTTP_WEBSOCKET\n");
					if(lwipdev.client_websocket_id == 0)
						lwipdev.client_websocket_id = 2; // 判断客户端编号
					com_http_cmd2.http_cmd = 0;
					http_data_send_function(HTTP_WEBSOCKET,2);			
					return 0;
				}
			}
			else
			{
				str = strstr((char *)http_buff2,HTTP_CERTIFIED_HANDSHAKE); // 摘要认证
				if(str != NULL)
				{
					str = strstr((char *)http_buff2,HTTP_AUTHORIZATION); // 摘要认证
					if(str != NULL)
						com_http_cmd2.http_cmd = 3; 
					else
						com_http_cmd2.http_cmd = 2; // 首次返回
				}
				http_com_data_deal((char *)http_buff2,&com_http_cmd2);
			}
			if(HTTP_COM_DEBUG) printf("com_http_cmd2:%s......%s\n",com_http_cmd2.cmd,com_http_cmd2.host_ip);
			if(strcmp(com_http_cmd2.host_ip,local_ipbuf) == 0) // 判断IP是否正确
			{
				switch(com_http_cmd2.http_cmd)
				{
					case 1: // 协议转换
						com_http_cmd2.http_cmd = 0;
						if(lwipdev.client_websocket_id == 0)
							lwipdev.client_websocket_id = 2; // 判断客户端编号
						http_data_send_function(HTTP_WEBSOCKET,2);						
						break;

					case 2: // 首次认证
						com_http_cmd2.http_cmd = 0;
						http_data_send_function(HTTP_ATTESTATION,2);				
						break;	

					case 3: // 认证结果
						com_http_cmd2.http_cmd = 0;
						if(http_ack_certification_calculations(&com_http_cmd2) == 1)
							http_data_send_function(HTTP_CALCULATION_OK,2);
						else
							http_data_send_function(HTTP_CALCULATION_ERROR,2);		
						break;		
					default:
						if(http_ack_certification_calculations(&com_http_cmd2) == 1)
						{
							if(HTTP_COM_DEBUG) printf("验证成功....\n");
							if(strcmp(com_http_cmd2.cmd ,BoxSubModelMgr_GetSmartBoxDevList)==0 ) //获取机箱管理的设备列表
								http_data_send_function(BoxDevList,2);
							else if(strcmp(com_http_cmd2.cmd ,DynamicCapability_GetDomains)==0 ) // 水浸
								http_data_send_function(DOMINS,2);
							else if(strcmp(com_http_cmd2.cmd ,WaterOutSense_WaterOutStatus)==0 ) // 水浸
								http_data_send_function(WATER_OUT,2);
							else if(strcmp(com_http_cmd2.cmd ,InfoMgr_DeviceDescription)==0 ) // 设备信息
								http_data_send_function(DEV_INFO,2);
							else if(strcmp(com_http_cmd2.cmd ,InfoMgr_DeviceServiceDescription)==0 ) // 设备生产信息
								http_data_send_function(DEV_PRODUCE,2);
							else if(strcmp(com_http_cmd2.cmd ,InfoMgr_DeviceVersion)==0 ) // 设备版本信息
								http_data_send_function(DEV_VERSION,2);
							else if(strcmp(com_http_cmd2.cmd ,DeviceTiltDetection_DevRealtimeTiltData)==0 ) // 倾斜度
								http_data_send_function(ANGLE_DATA,2);
							else if(strcmp(com_http_cmd2.cmd ,Humiture_Temperature)==0 ) // 温度
								http_data_send_function(TEMPERATURE_DATA,2);
							else if(strcmp(com_http_cmd2.cmd ,Humiture_Humidity)==0 ) // 湿度
								http_data_send_function(HUMIDITY_DATA,2);
							else if(strcmp(com_http_cmd2.cmd ,Fan_FanStatus)==0 ) // 风扇状态
								http_data_send_function(FAN_TWO_STATUS,2);
							else if(strcmp(com_http_cmd2.cmd ,BoxDoorMgr_BoxDoorStatus)==0 ) // 机箱门状态
								http_data_send_function(BOX_STATUS,2);
							else if(strcmp(com_http_cmd2.cmd ,NetworkAddress_IPAddressCfgList)==0 ) // 网络信息
								http_data_send_function(NETWORK_VERSION,2);
							else if(strcmp(com_http_cmd2.cmd ,PowerMgr_DeviceRealTimePowerParam)==0 ) // 电力参数
								http_data_send_function(POWER_DATA,2);	
							else if(strcmp(com_http_cmd2.cmd ,PowerMgr_PowerPortSwitchTimePlan)==0 ) // 电口开关时间
								http_data_send_function(POWER_PLAN_TIME,2);	
							else if(strcmp(com_http_cmd2.cmd ,PowerMgr_GetPowerPortStatusList)==0 ) // 获取供电口状态列表
								http_data_send_function(POWER_PORT_STATUS,2);	
							else if(strcmp(com_http_cmd2.cmd ,DeviceTiltDetection_DevTiltDetectionParam)==0 ) // 设备倾斜检测参数
							{
								http_deal_json_param(&sg_json_t2,(char*)http_buff2);							
								if(http_com_deal_configure_angle(sg_json_t2.buf)<0)
									http_data_send_function(OPERATE_ERROR,2);	
								else
									http_data_send_function(OPERATE_SUCCESS,2);	
							}
							else if(strcmp(com_http_cmd2.cmd ,Humiture_HumidityAlarmThreshold)==0 ) // 湿度告警阈值
							{
								http_deal_json_param(&sg_json_t2,(char*)http_buff2);							
								if(http_com_deal_configure_humiture(sg_json_t2.buf)<0)
									http_data_send_function(OPERATE_ERROR,2);	
								else
									http_data_send_function(OPERATE_SUCCESS,2);				
							}
							else if(strcmp(com_http_cmd2.cmd ,Humiture_TemperatureAlarmThreshold)==0 ) // 温度告警阈值
							{
								http_deal_json_param(&sg_json_t2,(char*)http_buff2);							
								if(http_com_deal_configure_temperature(sg_json_t2.buf)<0)
									http_data_send_function(OPERATE_ERROR,2);	
								else
									http_data_send_function(OPERATE_SUCCESS,2);	
							}
							else if(strcmp(com_http_cmd2.cmd ,PowerMgr_ModifyPowerPortWorkParamList)==0 ) // 修改供电口工作参数列表
							{
								http_deal_json_param(&sg_json_t2,(char*)http_buff2);							
								if(http_com_deal_configure_powerport(sg_json_t2.buf)<0)
									http_data_send_function(OPERATE_ERROR,2);	
								else
									http_data_send_function(OPERATE_SUCCESS,2);	
							}
							else if(strcmp(com_http_cmd2.cmd ,SystemMaintenance_SystemReset)==0 ) // 设备系统恢复出厂设置
							{
								W25QXX_Erase_Chip();
								set_reboot_time_function(1000);
							}
							else if(strcmp(com_http_cmd2.cmd ,SystemMaintenance_SystemBasicReset)==0 ) // 设备系统恢复默认设置
								app_set_reset_function();
							else if(strcmp(com_http_cmd2.cmd ,SystemMaintenance_SystemReboot)==0 ) // 设备系统重启
								set_reboot_time_function(1000);		
							else
								http_data_send_function(NO_SUPPORT,2);
						}
						else
							http_data_send_function(SERVER_ERROR,2);	
						break;				
				}	
				return 0;
			}
			else
			{
				if(HTTP_COM_DEBUG) printf("SERVER_ERROR1\n");
				http_data_send_function(SERVER_ERROR,2);	
				return -1;
			}
		}
	}
	else if(size < 0)
	{
		if(HTTP_COM_DEBUG) printf("SERVER_ERROR2\n");
		http_data_send_function(SERVER_ERROR,2);	
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
* Function name	: com_http_queue_find_info2
* Description	: 获取queue2一包数据
* Parameter		: 
* Return		: 
*	
************************************************************/
int com_http_queue_find_info2(uint8_t *msg,uint16_t size)
{
	char * line_start = NULL;
	char * line_end 	= NULL;
	char * line_str 	= NULL;
	uint16_t length 	= 0;
	int buf_length 	= 0;
	static uint16_t http_length2 = 0;
	
	if(Get_Queue_Count(&sg_queue_http2) > 0 ) // 有数据接收
	{	
		if(com_http_cmd2.data_recving == 0)
		{
			memset(msg,0,size);
			memset(&com_http_cmd2, 0, sizeof(com_http_cmd_t));
			if(lwipdev.client_websocket_id == 2)
			{
				length = sg_queue_http2.count;
				if(sg_queue_http2.front > sg_queue_http2.rear)
				{
					length = QUEUE_BUF_SIZE-sg_queue_http2.front;
					Dequeue_Bytes_To_Buffer(&sg_queue_http2,msg,length);
					memset(sg_queue_http2.buf+sg_queue_http2.front,0,length);	
					
					Dequeue_Bytes_To_Buffer(&sg_queue_http2,msg+length,sg_queue_http2.rear);
					memset(sg_queue_http2.buf,0,sg_queue_http2.rear);	
					return length;		
				}
				else	
				{
					Dequeue_Bytes_To_Buffer(&sg_queue_http2,msg,sg_queue_http2.count);
					memset(sg_queue_http2.buf+sg_queue_http2.front,0,sg_queue_http2.count);	
					return length;		
				}
			}
			else
			{
				line_start = strstr((char *)(sg_queue_http2.buf+sg_queue_http2.front), "GET");
				if (line_start == NULL)
				{
					line_start = strstr((char *)(sg_queue_http2.buf+sg_queue_http2.front), "SET");
					if (line_start == NULL)
					{
						line_start = strstr((char *)(sg_queue_http2.buf+sg_queue_http2.front), "POST");
						if (line_start == NULL)
						{					
							Clear_Queue(&sg_queue_http2);  // 清空队列
							return -1;
						}
						else
							sprintf(com_http_cmd2.method,"%s","POST"); 	
					}	
					else
						sprintf(com_http_cmd2.method,"%s","SET"); 			
				}
				else
					sprintf(com_http_cmd2.method,"%s","GET"); 
				
				/* check http end flag */
				if(sg_queue_http2.front > sg_queue_http2.rear)
				{
					line_end = strstr((char *)sg_queue_http2.buf, "\r\n\r\n");
				}
				else	
				{
					line_end = strstr((char *)(sg_queue_http2.buf+sg_queue_http2.front), "\r\n\r\n");
				}
				line_str = strstr((char *)(sg_queue_http2.buf+sg_queue_http2.front), "Length:");
				if(line_str != NULL)
					sscanf((char*)line_str,"Length:%d\r\n",&buf_length); 
//				else
//					printf("length error\n");
				if((line_end == NULL)||(line_end<line_start))
				{
					Clear_Queue(&sg_queue_http2);  // 清空队列
					return -1;
				}
				
				if(buf_length !=0)  // 判断后面是否有数据
				{
					sg_json_t2.len = buf_length;
					line_str = strstr((char *)line_end, "{");
					if(line_str == NULL) // 后面无数据
					{
						com_http_cmd2.data_recving = 1;
						buf_length = 0;
					}
					else
						com_http_cmd2.data_recving = 0;
				}
				else
					com_http_cmd2.data_recving = 0;
				
				line_end = line_end+4+buf_length;
				if(line_start > line_end)
				{
					length = QUEUE_BUF_SIZE-(line_start-(char *)sg_queue_http2.buf);
					Dequeue_Bytes_To_Buffer(&sg_queue_http2,msg,length);
					memset(line_start,0,length);	
					Dequeue_Bytes_To_Buffer(&sg_queue_http2,msg+length,(line_end-(char *)sg_queue_http2.buf));
					memset(sg_queue_http2.buf,0,(line_end-(char *)sg_queue_http2.buf));	
					if(sg_queue_http2.count == 0)
						Clear_Queue(&sg_queue_http2);  // 清空队列
					http_length2 = line_end+QUEUE_BUF_SIZE-line_start;
					return http_length2;
				}
				else
				{
					Dequeue_Bytes_To_Buffer(&sg_queue_http2,msg,(line_end-line_start));
					memset(line_start,0,(line_end-line_start));	
					if(sg_queue_http2.count == 0)
						Clear_Queue(&sg_queue_http2);  // 清空队列
					http_length2 = line_end-line_start;
					return http_length2;				
				}	
			}
		}
		else if(com_http_cmd2.data_recving == 1)
		{
			com_http_cmd2.data_recving = 0;
			if(sg_queue_http2.front > sg_queue_http2.rear)
			{
				length = QUEUE_BUF_SIZE-sg_queue_http2.front;
				Dequeue_Bytes_To_Buffer(&sg_queue_http2,msg+http_length2,length);
				memset(sg_queue_http2.buf+sg_queue_http2.front,0,length);	
				
				Dequeue_Bytes_To_Buffer(&sg_queue_http2,msg+http_length2+length,sg_queue_http2.rear);
				memset(sg_queue_http2.buf,0,sg_queue_http2.rear);	
				return (sg_queue_http2.count+http_length2);		
			}
			else	
			{
				Dequeue_Bytes_To_Buffer(&sg_queue_http2,msg+http_length2,sg_queue_http2.count);
				memset(sg_queue_http2.buf+sg_queue_http2.front,0,sg_queue_http2.count);	
				return (sg_queue_http2.count+http_length2);		
			}		
		}
	}
	return 0;
}

