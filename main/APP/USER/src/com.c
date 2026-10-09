#include "com.h"
#include "app.h"
#include "det.h"
#include "relay.h"
#include "eth.h"
#include "crc.h"
#include "timer.h"
#include "malloc.h"
#include "save.h"
#include "lwip_comm.h"
#include "stdio.h"
#include "string.h"
#include "stdlib.h"
#include "my_json.h"
#include "appconfig.h"
#include "includes.h"
#include "rtc.h"
#include "update.h"
#include "gsm.h"
#include "start.h"
#include "mycJSON.h"
#include "onvif.h"
#include "onvif_tcp.h"
#include "onvif_deal.h"
#include "httpd_cgi_ssi.h"

#define COM_HEAD_HEX (0x0F0F) 		// 数据头
#define COM_TAIL_HEX (0xFFFF) 		// 数据尾

#define COM_REC_HAED_HEX (0xF0F0) 	// 接收数据头

#ifdef COMDATA_PROCESS_MODE1
#define COM_NUM_VAERSION (0x10) 	// 数据版本
#endif
#ifdef COMDATA_PROCESS_MODE2
#define COM_NUM_VAERSION (0x11) 	// 数据版本
#endif


#ifdef COMDATA_PROCESS_MODE2
static struct com_qn_t sg_comqn_t; // 请求标识码
#endif
	
/************************************************************
*
* Function name	: com_report_get_adapter_status
* Description	: 获取适配器状态
* Parameter		: 
*	@adapter	: 适配器编号
* Return		: 0-不存在 1-通电 2-断电
*	
************************************************************/
uint8_t com_report_get_adapter_status(uint8_t adapter)
{
	uint8_t status = 0;
	
	switch(adapter)
	{
		case 0:
			if(relay_get_status_function(RELAY_1) == RELAY_ON) 
				status = 1;
			else 	/* 摄像头断电 */
				status = 2;
			
			break;
		case 1:
//			status = 0;
			if(relay_get_status_function(RELAY_2) == RELAY_ON) 
				status = 1;
			else 			/* 摄像头断电 */
				status = 2;
			break;
		case 2:
			status = 0;
			break;
	}
	return status;
}

/************************************************************
*
* Function name	: com_report_get_camera_status
* Description	: 获取摄像机工作状态
* Parameter		: 
*	@camera		: 摄像机编号
* Return		: 0-不存在 1-网络正常 2-网络断开
*	
************************************************************/
uint8_t com_report_get_camera_status(uint8_t camera)
{
	uint8_t ip[4]  = {0};
	uint8_t status = 0;
	
	if(app_get_camera_function(ip,camera) < 0)
	{
		status = 0;			// 不存在
	}
	else
	{
		if(eth_get_network_cable_status() == 0)
		{
			status = 2; 			// 网络断开
		}
		else 
		{
			if ( det_get_camera_status(camera) == 1 ) 
				status = 1;		// 网络正常
			else if ( det_get_camera_status(camera) == 2 )   // 网络延时时间  20220308
				status = 4;		// 网络延时严重
			else 
				status = 2;		// 网络异常
		}
	}
	return status;
}

/************************************************************
*
* Function name	: com_report_get_main_network_status
* Description	: 获取主网络状态
* Parameter		: 
*	@main		: 0-主网络1 1-主网络2
* Return		: 0-未指定IP 1-网络正常 2-网络断开 3-设备网络断开
*	
************************************************************/
uint8_t com_report_get_main_network_status(uint8_t main)
{
	uint8_t ip[4]  = {0};
	uint8_t status = 0;
	
	switch(main) 
	{
		case 0:			/* 主网络状态1 */
			if(eth_get_network_cable_status() == 0) 
			{
				status = 3;
			} 
			else 
			{
				app_get_main_network_ping_ip_addr(ip);
				if(ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) 
				{
					status = 0;				// 不存在
				} 
				else 
				{
					if( det_get_main_network_status() == 1)
						status = 1;			// 网络正常
					else if( det_get_main_network_status() == 2)  // 网络延时时间  20220308
						status = 4;			// 网络延时严重
					else
						status = 2;			// 网络断开
				}
			}
			break;
		/* 主网络状态2 */
		case 1:
			app_get_main_network_sub_ping_ip_addr(ip); 
			if(ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) 
			{
				status = 0;					// 不存在
			} 
			else 
			{
				if( det_get_main_network_sub_status() == 1) 
					status = 1;				// 网络正常
				else if( det_get_main_network_sub_status() == 2)  // 网络延时时间  20220308
					status = 4;			// 网络延时严重
				else 
					status = 2;				// 网络断开
			}
			break;
	}
	return status;
}


/************************************************************
*
* Function name	: com_report_normally_function
* Description	: 正常上报
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_report_normally_function(uint8_t *data, uint16_t *len, uint8_t cmd)
{
	struct device_param *device = app_get_device_param_function();
	uint16_t 		index   = 0;
	uint16_t		size    = 0;
	uint8_t			crc			= 0;
	uint8_t  		str[45] ={0};
	uint8_t			*p			= 0;
	uint16_t		buff[4] ={0};
	fp32				temp 		= 0;
	char 			  str_buff[8][8] = {0};
#ifdef COM_GPS_ENABLE
	double				fpdata  = 0;
  uint32_t			dtemp   = 0;
#endif
	
	/* 数据头 */
	data[index++] = '#';
	data[index++] = '#';
	/* 数据长度 */
	data[index++] = '0';
	data[index++] = '0';
	data[index++] = '0';
	data[index++] = '0';
	/* 请求编码QN */
	memset(str,0,sizeof(str));
	
	if(sg_comqn_t.flag == 1) {
		/* 请求上报 */
		sg_comqn_t.flag = 0;
		sprintf((char*)str,"QN=%08d%09d;",sg_comqn_t.qn1,sg_comqn_t.qn2);
		strcat((char*)data,(char*)str);
		sg_comqn_t.qn1 = 0;
		sg_comqn_t.qn2 = 0;
	} else {
		/* 主动上报 */
		sprintf((char*)str,"QN=0;");
		strcat((char*)data,(char*)str);
	}
	
	/* 设备唯一标识TID */
	memset(str,0,sizeof(str));
	sprintf((char*)str,"TID=%d;",device->id.i);
	strcat((char*)data,(char*)str);
	/* 版本号 */
	memset(str,0,sizeof(str));
	sprintf((char*)str,"VER=%d;",COM_DATA_VERSION);
	strcat((char*)data,(char*)str);
	/* 指令参数CP */
	strcat((char*)data,"CP=&&");
	
	/** 系统时间 **/
	memset(str,0,sizeof(str));
	sprintf((char*)str,"DT=%s;",app_get_report_current_time(0));
	strcat((char*)data,(char*)str);
	/** 摄像机的电源状态 **/
	memset(str,0,sizeof(str));
	sprintf((char*)str,"CPS=%01d;", com_report_get_adapter_status(0));
	strcat((char*)data,(char*)str);
	/** 摄像机的网络状态 **/
	memset(str,0,sizeof(str));
	sprintf((char*)str,"CNS=%01d,%01d,%01d,%01d,%01d,%01d;",
											com_report_get_camera_status(0),
											com_report_get_camera_status(1),
											com_report_get_camera_status(2),
											com_report_get_camera_status(3),
											com_report_get_camera_status(4),
											com_report_get_camera_status(5));
	strcat((char*)data,(char*)str);
	/** 主网网络状态 **/
	memset(str,0,sizeof(str));
	sprintf((char*)str,"MN=%d,%d;", com_report_get_main_network_status(0),
																	com_report_get_main_network_status(1));
	strcat((char*)data,(char*)str);
	/** 电压、电流 **/
	memset(str,0,sizeof(str));
	temp = det_get_vin220v_handler(0);
	buff[0] = (uint16_t)temp;
	buff[1] = temp*100-buff[0]*100;
	temp = det_get_vin220v_handler(1);
	buff[2] = (uint16_t)temp;
	buff[3] = temp*100-buff[2]*100;
	sprintf((char*)str,"V=%d.%02d;A=%d.%02d;",buff[0],buff[1],buff[2],buff[3]);
	strcat((char*)data,(char*)str);
	/** 湿度、温度 **/
	memset(str,0,sizeof(str));
	temp = det_get_inside_humi();
	buff[0] = (uint16_t)temp;
	buff[1] = temp*100-buff[0]*100;
	temp = det_get_inside_temp();
	if(temp < 0) {
		temp = 0 - temp;
		buff[2] = (uint16_t)temp;
		buff[3] = temp*100-buff[2]*100;
		sprintf((char*)str,"H=%d.%02d;T=-%d.%02d;",buff[0],buff[1],buff[2],buff[3]);
	} else {
		buff[2] = (uint16_t)temp;
		buff[3] = temp*100-buff[2]*100;
		sprintf((char*)str,"H=%d.%02d;T=%d.%02d;",buff[0],buff[1],buff[2],buff[3]);
	}	
	strcat((char*)data,(char*)str);
	/** 门状态、箱体姿态、防雷状态 **/
	memset(str,0,sizeof(str));
	sprintf((char*)str,"DS=%01d;P=%d;SPD=%01d;",
											(det_get_open_door()?1:2),
											 det_get_cabinet_posture(),
											 det_get_spd_status());
	strcat((char*)data,(char*)str);
	/** 设备供电状态**/
	memset(str,0,sizeof(str));
	sprintf((char*)str,"PA=%01d;", det_get_220v_in_function());
	strcat((char*)data,(char*)str);

	/** 过压 欠压保护 **/
	memset(str,0,sizeof(str));
	sprintf((char*)str,"OV=%01d;",app_get_vlot_protec_status());
	strcat((char*)data,(char*)str);
	
	/** 过载电流保护 **/
	memset(str,0,sizeof(str));
	sprintf((char*)str,"OCPS=%01d;",app_get_current_status());
	strcat((char*)data,(char*)str);

	/** 电池、SIM检测、水浸 **/
	memset(str,0,sizeof(str));
	sprintf((char*)str,"BAT=%01d;SIM=%01d;WATER=%01d;",
											det_get_battery_status(),
											det_get_sim_status(),
											det_get_water_status());
	strcat((char*)data,(char*)str);

	/** 继电器状态 **/
	memset(str,0,sizeof(str));
	sprintf((char*)str,"RELAY=%01d,%01d,%01d,%01d,%01d,%01d,%01d,%01d;",
											relay_get_status(0),relay_get_status(1),
											relay_get_status(2),relay_get_status(3),
											relay_get_status(4),relay_get_status(5),
											relay_get_status(6),relay_get_status(7));
	strcat((char*)data,(char*)str);

	/** 功率 **/
	memset(str_buff,0,sizeof(str_buff));
	Vin220_Power_Handler(str_buff[0],1);
	Vin220_Power_Handler(str_buff[1],2);
	Vin220_Power_Handler(str_buff[2],3);
	Vin220_Power_Handler(str_buff[3],4);
	Vin220_Power_Handler(str_buff[4],5);
	Vin220_Power_Handler(str_buff[5],6);
	Vin220_Power_Handler(str_buff[6],7);
	Vin220_Power_Handler(str_buff[7],8);	
	sprintf((char*)str,"POWER=%s,%s,%s,%s,%s,%s,%s,%s;",
											str_buff[0],str_buff[1],str_buff[2],str_buff[3],
											str_buff[4],str_buff[5],str_buff[6],str_buff[7]);
	strcat((char*)data,(char*)str);
	
	/** GPS参数 **/
#ifdef COM_GPS_ENABLE
	fpdata = gsm_get_location_information_function(0);
	memset(str,0,sizeof(str));
	buff[0] = (uint16_t)fpdata;
	dtemp   = (fpdata - buff[0]) * 100000000;
	sprintf((char*)str,"LOC=%d.%08d,", buff[0], dtemp);
	strcat((char*)data,(char*)str);
	fpdata = gsm_get_location_information_function(1);
	memset(str,0,sizeof(str));
	buff[0] = (uint16_t)fpdata;
	dtemp   = (fpdata - buff[0]) * 100000000;
	sprintf((char*)str,"%d.%08d", buff[0], dtemp);
	strcat((char*)data,(char*)str);
#endif

	strcat((char*)data,"&&");
	
	/* 数据长度 */
	size = strlen((char*)data);
	sprintf((char*)str,"%04d",size-6);
	data[2] = str[0];
	data[3] = str[1];
	data[4] = str[2];
	data[5] = str[3];
	/* CRC校验 */
	p = (uint8_t*)strstr((char*)data,"&&");
	if(p == 0) {
		*len = 0;
	} else {
		crc = calc_crc8(p,strlen((char*)p));
		memset(str,0,sizeof(str));
		sprintf((char*)str,"%02x##",crc);
		strcat((char*)data,(char*)str);
		*len = strlen((char*)data);
		
//		printf("%s \r\n",data);
	}
}


/************************************************************
*
* Function name	: com_query_configuration_function
* Description	: 查询配置
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_query_configuration_function(uint8_t *pdata, uint16_t *len)
{
	struct device_param 	*device   	 		= app_get_device_param_function();
	struct remote_ip    	*remote  	 			= app_get_remote_network_function();
	struct local_ip_t   	*local    	 		= app_get_local_network_function();
	com_param_t						*comparam 	 		= app_get_com_time_infor();
	comparision_parameter_t *comparision 	= app_get_comparision_param();
	struct threshold_params *threshol 		= app_get_threshold_param_function();
	
	uint8_t name[5]     = {0};
	char  temp[128] 	= {0};
	char  crc_buff[20]	= {0};
	uint8_t crc		    = 0;
	uint8_t index 		= 0;
	
	/* 生成校验码 */
	sprintf(crc_buff,"%x%dE1",0x10,device->id.i);
	crc = calc_crc8((uint8_t*)crc_buff,strlen(crc_buff)-1);
	
	my_cjson_create_function(pdata,0); // 开始
	my_cjson_join_int_function(pdata,(uint8_t *)"code",0,1);
	my_cjson_data_create_function(pdata,0); // 开始
	my_cjson_join_string_function(pdata,(uint8_t *)"ver",(uint8_t *)"11",1);
	
	/* 添加QN */
	#ifdef COMDATA_PROCESS_MODE2 
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%08d%09d",sg_comqn_t.qn1,sg_comqn_t.qn2);
	my_cjson_join_string_function(pdata,(uint8_t *)"qn",(uint8_t *)temp,1);
    sg_comqn_t.qn1 = 0;
    sg_comqn_t.qn2 = 0;
    sg_comqn_t.flag = 0;
	#endif
	
	/* ID */
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d",device->id.i);
	my_cjson_join_string_function(pdata,(uint8_t *)"tid",(uint8_t *)temp,1);
	/* 通信命令 */
	my_cjson_join_string_function(pdata,(uint8_t *)"cmd",(uint8_t *)"E1",1);

	/* 设备名称 */
	my_cjson_join_string_function(pdata,(uint8_t *)"tn",(uint8_t *)app_get_device_name(),1);
	/* 终端序列号 */
	memset(temp,0,sizeof(temp));
	start_get_device_id_str((uint8_t*)temp);
	my_cjson_join_string_function(pdata,(uint8_t*)"tsn",(uint8_t*)temp,1);

	/* 内外服务器IP、域名+端口 */
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%s:%d","",0);
	my_cjson_join_string_function(pdata,(uint8_t*)"isi",(uint8_t*)temp,1);
	/* 外网服务器IP、域名+端口 */
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%s:%d",remote->outside_iporname,remote->outside_port);
	my_cjson_join_string_function(pdata,(uint8_t*)"osi",(uint8_t*)temp,1);
	/* 升级服务器IP、域名+端口 */
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%s:%d",update_addr_ip(),update_addr_port());
	my_cjson_join_string_function(pdata,(uint8_t*)"usi",(uint8_t*)temp,1);
	/* SIM卡序列号 */
	my_cjson_join_string_function(pdata,(uint8_t*)"iccid",(uint8_t*)gsm_get_sim_ccid_function(),1);
	/* 传输模式 */
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d",app_get_network_mode());
	my_cjson_join_string_function(pdata,(uint8_t*)"tm",(uint8_t*)temp,1);
	/* 网卡MAC地址 */
	my_cjson_join_string_function(pdata,(uint8_t*)"mac",lwip_get_mac_addr(),1);
	/* IP */
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d",local->ip[0],local->ip[1],local->ip[2],local->ip[3]);
	my_cjson_join_string_function(pdata,(uint8_t*)"ip",(uint8_t*)temp,1);
	/* 子网掩码 */
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d",local->netmask[0],local->netmask[1],local->netmask[2],local->netmask[3]);
	my_cjson_join_string_function(pdata,(uint8_t*)"nm",(uint8_t*)temp,1);
	/* 网关 */
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d",local->gateway[0],local->gateway[1],local->gateway[2],local->gateway[3]);
	my_cjson_join_string_function(pdata,(uint8_t*)"gw",(uint8_t*)temp,1);
	/* 主网检测IP */
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d.%d.%d.%d,%d.%d.%d.%d", local->ping_ip[0],local->ping_ip[1],local->ping_ip[2],local->ping_ip[3],
											local->ping_sub_ip[0],local->ping_sub_ip[1],local->ping_sub_ip[2],local->ping_sub_ip[3]);
	my_cjson_join_string_function(pdata,(uint8_t*)"mip",(uint8_t*)temp,1);
	/* 上报间隔 */
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d,%d,%d",comparam->report/1000,0,0);
	my_cjson_join_string_function(pdata,(uint8_t*)"rt",(uint8_t*)temp,1);
	/* ping每轮间隔时间 */
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d",comparam->ping/1000);
	my_cjson_join_string_function(pdata,(uint8_t*)"pl",(uint8_t*)temp,1);

	/* ping每个设备的间隔时间 */
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d",comparam->dev_ping/1000);
	my_cjson_join_string_function(pdata,(uint8_t*)"pn",(uint8_t*)temp,1);

	/* 风扇启动、停止温度 */
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d,%d",threshol->temp_high,threshol->temp_low);
	my_cjson_join_string_function(pdata,(uint8_t*)"ft",(uint8_t*)temp,1);

	/* 风扇启动湿度 */
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d,%d",threshol->humi_high,threshol->humi_low);
	my_cjson_join_string_function(pdata,(uint8_t*)"fh",(uint8_t*)temp,1);

	/* 网络延时时间设置  20220308*/  
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d",comparam->network_time);
	my_cjson_join_string_function(pdata,(uint8_t*)"pt",(uint8_t*)temp,1);

	/* 摄像机IP */
	for(index=0;index<10;index++)
	{
		memset(temp,0,sizeof(temp));
		sprintf(temp,"%d.%d.%d.%d", comparision->ip[index][0],comparision->ip[index][1],\
																comparision->ip[index][2],comparision->ip[index][3]);
		sprintf((char*)name,"c%d",index+1);
		my_cjson_join_string_function(pdata,name,(uint8_t*)temp,1);
	}

	/* 过压、欠压、电流 */
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d,%d,%d,%d",threshol->volt_max,threshol->volt_min,
														 threshol->current,threshol->angle);
  my_cjson_join_string_function(pdata,(uint8_t*)"opovc",(uint8_t*)temp,1);

	/* 搜索协议模式  20220908*/  
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d",local->search_mode);
	my_cjson_join_string_function(pdata,(uint8_t*)"sm",(uint8_t*)temp,1);
	
	/* 搜索协议时间设置  20220908*/  
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d",0);
	my_cjson_join_string_function(pdata,(uint8_t*)"smt",(uint8_t*)temp,1);
	
	/* 签名校验 */
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%x",crc);
	my_cjson_join_string_function(pdata,(uint8_t*)"crc",(uint8_t*)temp,0);
	
	my_cjson_data_create_function(pdata,1); // 结束
	my_cjson_create_function(pdata,1); // 结束
	
	*len = strlen((char*)pdata);
	
	if(COM_DEBUG)   printf("%s \r\n",pdata);
}
	
/************************************************************
*
* Function name	: com_heart_pack_function
* Description	: 心跳包
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_heart_pack_function(uint8_t *data, uint16_t *len)
{
	struct device_param *device = app_get_device_param_function();
	uint8_t 			index   = 0;
	uint16_t			crc     = 0;
	
	/* 数据头 */
	data[index++] = (COM_HEAD_HEX&0xff00)>>8; 
	data[index++] = COM_HEAD_HEX&0xff;
	/* 数据版本 */
	data[index++] = COM_NUM_VAERSION;
	/* 设备ID */
	data[index++] = (device->id.i>>16)&0xff;
	data[index++] = (device->id.i>>8)&0xff;
	data[index++] = (device->id.i>>0)&0xff;
	/* 命令 */
	data[index++] = COM_HEART_UPDATA;
	
	/* 请求标识码 */
	#ifdef COMDATA_PROCESS_MODE2
	data[index++] = 0;
	data[index++] = 0;
	data[index++] = 0;
	data[index++] = 0;
	
	data[index++] = 0;
	data[index++] = 0;
	data[index++] = 0;
	data[index++] = 0;
	#endif
	
	/* 数据长度 */
	data[index++] = 0x01;

	/* 数据内容 */
	data[index++] = 0x01;

	/* crc校验 */
	crc = calc_crc8(&data[2],index-2);
	data[index++] = crc;
	
	/* 数据尾 */
	data[index++] = (COM_TAIL_HEX>>8)&0xff;
	data[index++] = (COM_TAIL_HEX)&0xff;
	
	*len = index;
}

/************************************************************
*
* Function name	: com_ack_function
* Description	: 回复数据
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_ack_function(uint8_t *data, uint16_t *len, uint8_t ack, uint8_t error)
{
	struct device_param *device = app_get_device_param_function();
	uint8_t 			index   = 0;
	uint16_t			crc     = 0;
	
	/* 数据头 */
	data[index++] = (COM_HEAD_HEX&0xff00)>>8; 
	data[index++] = COM_HEAD_HEX&0xff;
	/* 数据版本 */
	data[index++] = COM_NUM_VAERSION;
	/* 设备ID */
	data[index++] = (device->id.i>>16)&0xff;
	data[index++] = (device->id.i>>8)&0xff;
	data[index++] = (device->id.i>>0)&0xff;
	/* 命令 */
	data[index++] = ack;
	/* 请求标识码QN */
	#ifdef COMDATA_PROCESS_MODE2
	if(sg_comqn_t.flag == 1) {
		sg_comqn_t.flag = 0;
	}
	data[index++] = (sg_comqn_t.qn1>>24) & 0xff;
	data[index++] = (sg_comqn_t.qn1>>16) & 0xff;
	data[index++] = (sg_comqn_t.qn1>>8) & 0xff;
	data[index++] = (sg_comqn_t.qn1>>0) & 0xff;
	data[index++] = (sg_comqn_t.qn2>>24) & 0xff;
	data[index++] = (sg_comqn_t.qn2>>16) & 0xff;
	data[index++] = (sg_comqn_t.qn2>>8) & 0xff;
	data[index++] = (sg_comqn_t.qn2>>0) & 0xff;
    
	sg_comqn_t.qn1 = 0;
	sg_comqn_t.qn2 = 0;
	#endif
	
	/* 数据长度 */
	data[index++] = 0x01;
	
	/* 错误码 */
	data[index++] = error;
	
	/* crc校验 */
	crc = calc_crc8(&data[2],index-2);
	data[index++] = crc;
	
	/* 数据尾 */
	data[index++] = (COM_TAIL_HEX>>8)&0xff;
	data[index++] = (COM_TAIL_HEX)&0xff;
	
	*len = index;
}

/************************************************************
*
* Function name	: com_version_information
* Description	: 上传软件、硬件版本号
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_version_information(uint8_t *pdata, uint16_t *size)
{
	struct device_param *device = app_get_device_param_function();
	char  temp[128] 	   = {0};
	char  crc_buff[50] = {0};
//	uint8_t index	   = 0;
	uint8_t crc		   = 0;
	my_json_t js;

	/* 获取crc校验结果 */
	sprintf(crc_buff,"%x%dE3",0x10,device->id.i);
	crc = calc_crc8((uint8_t*)crc_buff,strlen(crc_buff)-1);

	/* 自研组包: 直接拼接JSON字符串(不使用cJSON), pdata为 sg_send_buff(1024字节) */
	my_json_init(&js, (char*)pdata, 1024);
	my_json_object_begin(&js, NULL);						/* 根对象 */
	my_json_add_int(&js, "code", 0);

	my_json_object_begin(&js, "data");
	#ifdef COMDATA_PROCESS_MODE2
	memset(crc_buff,0,sizeof(crc_buff));
	sprintf(crc_buff,"%08d%09d",sg_comqn_t.qn1,sg_comqn_t.qn2);
	my_json_add_str(&js, "qn", crc_buff);
	sg_comqn_t.qn1 = 0;
	sg_comqn_t.qn2 = 0;
	sg_comqn_t.flag = 0;
	#endif
	my_json_add_str(&js, "ver", "10");
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d",device->id.i);
	my_json_add_str(&js, "tid", temp);
	my_json_add_str(&js, "cmd", "E3");
	my_json_add_str(&js, "mod", HARD_NO_STR);
	my_json_add_str(&js, "sysver", SOFT_NO_STR);
	/* 终端序列号 */
	memset(temp,0,sizeof(temp));
	start_get_device_id_str((uint8_t*)temp);
	my_json_add_str(&js, "tsn", temp);
	/* SIM卡序列号 */
	my_json_add_str(&js, "iccid", (char*)gsm_get_sim_ccid_function());
	/* 网卡MAC地址 */
	my_json_add_str(&js, "mac", (char*)lwip_get_mac_addr());
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%x",crc);
	my_json_add_str(&js, "crc", temp);
	my_json_object_end(&js);								/* data 结束 */

	my_json_object_end(&js);								/* 根对象结束 */

	*size = (uint16_t)my_json_len(&js);
}

/************************************************************
*
* Function name	: com_gprs_lbs_information
* Description	: 上传基站定位信息
* Parameter		: 
* Return		: 
*	              20220329
************************************************************/
void com_gprs_lbs_information(uint8_t *pdata, uint16_t *size)
{
	struct device_param 	*device   = app_get_device_param_function();
	double longi_data = 0;
	double lati_data = 0;
	uint32_t dtemp	 = 0;
	char  temp[50] 	= {0};
	char  crc_buff[20]	= {0};
	uint8_t crc		    = 0;

	/* 生成校验码 */
	sprintf(crc_buff,"%x%dE1",0x10,device->id.i);
	crc = calc_crc8((uint8_t*)crc_buff,strlen(crc_buff)-1);	
	
	my_cjson_create_function(pdata,0); // 开始
	my_cjson_join_int_function(pdata,(uint8_t *)"code",0,1);
	
	my_cjson_data_create_function(pdata,0); // 开始
	
	/* 添加QN */
	#ifdef COMDATA_PROCESS_MODE2 
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%08d%09d",sg_comqn_t.qn1,sg_comqn_t.qn2);
	my_cjson_join_string_function(pdata,(uint8_t *)"qn",(uint8_t *)temp,1);
	sg_comqn_t.qn1 = 0;
	sg_comqn_t.qn2 = 0;
	sg_comqn_t.flag = 0;
	#endif	
	my_cjson_join_string_function(pdata,(uint8_t *)"ver",(uint8_t *)"10",1);	
		
	/* ID */
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%d",device->id.i);
	my_cjson_join_string_function(pdata,(uint8_t *)"tid",(uint8_t *)temp,1);
	/* 通信命令 */
	my_cjson_join_string_function(pdata,(uint8_t *)"cmd",(uint8_t *)"E6",1);

	memset(temp,0,sizeof(temp));
	longi_data = gsm_get_location_information_function(0);
	temp[0] = (uint16_t)longi_data;
	dtemp   = (longi_data - temp[0]) * 100000000;
	sprintf((char*)temp,"%d.%08d,", temp[0], dtemp);
	my_cjson_join_string_function(pdata,(uint8_t*)"longitude",(uint8_t*)temp,1);
	
	lati_data	 = gsm_get_location_information_function(1);
	memset(temp,0,sizeof(temp));
	temp[0] = (uint16_t)lati_data;
	dtemp   = (lati_data - temp[0]) * 100000000;
	sprintf((char*)temp,"%d.%08d,", temp[0], dtemp);
	my_cjson_join_string_function(pdata,(uint8_t*)"latitude",(uint8_t*)temp,1);

	/* 签名校验 */
	memset(temp,0,sizeof(temp));
	sprintf(temp,"%x",crc);
	my_cjson_join_string_function(pdata,(uint8_t*)"crc",(uint8_t*)temp,0);
	
	my_cjson_data_create_function(pdata,1); // 结束
	my_cjson_create_function(pdata,1); // 结束	
	*size = strlen((char*)pdata);
	
//	printf("%s \r\n",pdata);
}

/************************************************************
*
* Function name	: com_deal_configure_server_domain_name
* Description	: 处理配置服务器域名函数
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t com_deal_configure_server_domain_name(com_rec_data_t *buff)
{
	struct remote_ip *remote     = app_get_remote_network_function();
	sys_backups_t    *param_back = app_get_backups_function();

	int8_t ret = 0;
	char *p1 = NULL;

	int32_t temp[4] = {0};
	uint8_t ip[4] = {0};
	uint32_t port = 0;
	uint8_t mode  = 0;
	
	p1 = strstr((char*)buff->buff,"outer##");
	if (p1 != NULL) {
		memset(remote->outside_iporname,0,sizeof(remote->outside_iporname));
		p1 += 7;
		sscanf((char*)p1,"%[^:]:%d",remote->outside_iporname,&remote->outside_port);
		mode = 1;
		goto EXIT;
	}
		
	p1 = strstr((char*)buff->buff,"all##");
	if (p1 != NULL) 
	{
		p1 += 5;

		p1 = strchr((char*)p1,',');
		if( p1 != NULL ) 
		{
			p1[0] = 0;
			p1	 += 1;
			
			app_save_backups_remote_param_function();  // 备份服务器信息
			memset(remote->outside_iporname,0,sizeof(remote->outside_iporname));
			sscanf((char*)p1,"%[^:]:%d",remote->outside_iporname,&remote->outside_port);
		}
		mode = 1;
		goto EXIT;
	}
	
	p1 = strstr((char*)buff->buff,"update##");
	if (p1 != NULL) {
		p1 += 8;
		sscanf((char*)p1,"%d.%d.%d.%d:%d",&temp[0],&temp[1],&temp[2],&temp[3],&port);
		ip[0] = temp[0];
		ip[1] = temp[1];
		ip[2] = temp[2];
		ip[3] = temp[3];
		update_set_update_addr(ip,port);
		
		/* 设置回传 */
		app_set_send_result_function(SR_OK);
		app_set_reply_parameters_function(buff->cmd,0x01);
		OSTimeDlyHMSM(0,0,0,100);  			// 延时10ms
		return ret;
	}
	
	p1 = strstr((char*)buff->buff,"outer2##");
	if (p1 != NULL) {

		mode = 2;
		goto EXIT;
	}
	
	p1 = strstr((char*)buff->buff,"outer3##");
	if (p1 != NULL) {
		mode = 2;
		goto EXIT;
	}
	
	if ( p1 == NULL ) {
		/* 设置回传 */
		app_set_reply_parameters_function(buff->cmd,0x74);
		
		return ret;
	}
	EXIT:
	/* 设置回传 */
	app_set_send_result_function(SR_OK);
	app_set_reply_parameters_function(buff->cmd,0x01);
	OSTimeDlyHMSM(0,0,0,100);  			// 延时10ms
	if(mode == 1) 
	{
		app_set_save_infor_function(SAVE_REMOTE_IP);
	} 
	else if(mode == 2) 
	{
		app_set_save_infor_function(SAVE_ONLY_SEND_IP);
	}
	return ret;
}

/************************************************************
*
* Function name	: com_deal_configure_server_mode
* Description	: 设置服务器模式
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_deal_configure_server_mode(com_rec_data_t *buff)
{
	struct local_ip_t *local = app_get_local_network_function();

	local->server_mode = buff->buff[0];
	app_set_send_result_function(SR_OK);
	app_set_reply_parameters_function(buff->cmd,0x01);
	OSTimeDlyHMSM(0,0,0,100);  			// 延时10ms
	app_set_save_infor_function(SAVE_LOCAL_NETWORK);
}

/************************************************************
*
* Function name	: com_deal_update_system_function
* Description	: 处理更新
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_deal_update_system_function(com_rec_data_t *buff)
{
	/* 设置回传 */
	if( update_detection_status_function() != UPDATE_MODE_NULL) 
	{
		app_set_reply_parameters_function(buff->cmd,0x77);  // 错误，正在更新
	} 
	else 
	{
		app_set_reply_parameters_function(buff->cmd,0x01);
		OSTimeDlyHMSM(0,0,0,200); 
		update_set_update_mode(UPDATE_MODE_GPRS);
	}	
}

/************************************************************
*
* Function name	: com_set_now_time_function
* Description	: 设置当前时间
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_set_now_time_function(com_rec_data_t *buff)
{
	uint32_t time = buff->buff[0];
	
	time = (time<<8)|buff->buff[1];
	time = (time<<8)|buff->buff[2];
	time = (time<<8)|buff->buff[3];
	
	TimeBySecond(time);
}

/************************************************************
*
* Function name	: com_deal_configure_local_network
* Description	: 配置设备IP、子网掩码、网关
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_deal_configure_local_network(com_rec_data_t *buff)
{
	struct local_ip_t *local = app_get_local_network_function();
	
	local->ip[0] = buff->buff[0];
	local->ip[1] = buff->buff[1];
	local->ip[2] = buff->buff[2];
	local->ip[3] = buff->buff[3];
	
	local->netmask[0] = buff->buff[4];
	local->netmask[1] = buff->buff[5];
	local->netmask[2] = buff->buff[6];
	local->netmask[3] = buff->buff[7];
	
	local->gateway[0] = buff->buff[8];
	local->gateway[1] = buff->buff[9];
	local->gateway[2] = buff->buff[10];
	local->gateway[3] = buff->buff[11];

	/* 设置回传 */
	app_set_send_result_function(SR_OK);
	app_set_reply_parameters_function(buff->cmd,0x01);
	OSTimeDlyHMSM(0,0,0,100);  			// 延时10ms
	
	/* 保存 */
	app_set_save_infor_function(SAVE_LOCAL_NETWORK);
	eth_set_network_reset();
	
}

/************************************************************
*
* Function name	: com_deal_configure_mac
* Description	: 配置设备mac
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_deal_configure_mac(com_rec_data_t *buff)
{
	struct local_ip_t *local = app_get_local_network_function();
	
	local->mac[0] = buff->buff[0];
	local->mac[1] = buff->buff[1];
	local->mac[2] = buff->buff[2];
	local->mac[3] = buff->buff[3];
	local->mac[4] = buff->buff[4];
	local->mac[5] = buff->buff[5]; 
	
	/* 设置回传 */
	app_set_send_result_function(SR_OK);
	app_set_reply_parameters_function(buff->cmd,0x01);
	OSTimeDlyHMSM(0,0,0,100);  			// 延时10ms
	
	/* 保存 */
	STMFLASH_Write_SAVE(DEVICE_FLASH_STORE,DEVICE_MAC_ADDR,(uint32_t *)&local->mac,2);
//	STMFLASH_Write(DEVICE_MAC_ADDR,(uint32_t *)&local->mac,2);
	app_set_save_infor_function(SAVE_LOCAL_NETWORK);
	eth_set_network_reset();
	
}

/************************************************************
*
* Function name	: com_deal_configure_device_id
* Description	: 配置设备ID
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_deal_configure_device_id(com_rec_data_t *buff)
{
	struct device_param  *param  = app_get_device_param_function();
		
	/* 获取ID */
	param->id.i   = (buff->buff[0]<<16)|(buff->buff[1]<<8)|(buff->buff[2]<<0);
	
	/* 设置回传 */
	app_set_send_result_function(SR_OK);
	app_set_reply_parameters_function(buff->cmd,0x01);
	OSTimeDlyHMSM(0,0,0,100);  			// 延时10ms
	
	/* 保存 */
	STMFLASH_Write_SAVE(DEVICE_FLASH_STORE,DEVICE_ID_ADDR,(uint32_t*)param->id.c,1);
//	STMFLASH_Write(DEVICE_ID_ADDR,(uint32_t*)param->id.c,1);
	app_set_save_infor_function(SAVE_DEVICE_PARAM);
	
	OSTimeDlyHMSM(0,0,0,100);
	eth_set_tcp_connect_reset();				/* 重启TCP连接 */
	gsm_set_module_reset_function();				/* 重启无线连接 */
}


/************************************************************
*
* Function name	: com_deal_camera_config
* Description	: 处理配置摄像头信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_deal_camera_config(com_rec_data_t *buff)
{
	uint8_t num   = buff->buff[0]-1;
	uint8_t ip[4] = {0};
	int8_t  ret   = 0;
	
	ip[0] = buff->buff[1];
	ip[1] = buff->buff[2];
	ip[2] = buff->buff[3];
	ip[3] = buff->buff[4];
	
	if( ip[0] == 0 &&ip[1] == 0 &&ip[2] == 0 &&ip[3] == 0)
	{
		/* 清除指定IP */
		app_set_camera_num_function(ip,num);
		/* 设置回传 */
		app_set_reply_parameters_function(buff->cmd,0x01);
	}
	else
	{
		ret = app_match_local_camera_ip(ip);
		if(ret != 0)
		{
			/* 设置回传 */
			app_set_reply_parameters_function(buff->cmd,0x74); // IP已存在
		}
		else
		{
			app_set_camera_num_function(ip,num);
			/* 设置回传 */
			app_set_reply_parameters_function(buff->cmd,0x01);
		}
	}
}

/************************************************************
*
* Function name	: com_del_device_name
* Description	: 
* Parameter		: 
* Return		: 
*	          20220416 新加配置设备名称
************************************************************/
void com_set_device_name_function(com_rec_data_t *buff)
{
	struct device_param *param = app_get_device_param_function();
	uint8_t		  size				 = 0;
	
	memset(param->name,0,sizeof(param->name));
	size = strlen((const char*)(buff->buff));
//	printf("zhongwen:");
//	for(uint8_t i=0;i<size;i++)
//		printf("%x",buff->buff[i]);
	
	if(size ==0)
	{
		app_set_reply_parameters_function(buff->cmd,0x74);		/* 错误 */
	}
	else
	{
		for(uint8_t index=0; index<size; index++) 
		{
			param->name[index] = buff->buff[index];
		}	
		app_set_save_infor_function(SAVE_DEVICE_PARAM);	/* 存储 */
		app_set_reply_parameters_function(buff->cmd,0x01);	/* 设置回传 */
	}
//	for(uint8_t i=0;i<size;i++)
//		printf("%x",param->name[i]);
}

/************************************************************
*
* Function name	: com_set_threshold_params_function
* Description	: 设置阈值
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_set_threshold_params_function(com_rec_data_t *buff)
{	
	uint16_t data[3];
	
	data[0] = (buff->buff[0]<<8|buff->buff[1]); // 前2个字节
	data[1] = (buff->buff[2]<<8|buff->buff[3]); // 前2个字节
	data[2] = (buff->buff[4]<<8|buff->buff[5]); // 前2个字节
	
	/* 保存参数 */
	app_set_vol_current_param(data);	/* 存储 */
	/* 设置回传 */
	app_set_reply_parameters_function(buff->cmd,0x01);
}

/************************************************************
*
* Function name	: com_deal_fan_temp_parmaeter
* Description	: 处理风扇温度
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_deal_fan_temp_parmaeter(com_rec_data_t *buff)
{
	int8_t data[2] = {0};
	
	data[0] = buff->buff[0];
	data[1] = buff->buff[1];
	
	app_set_fan_param_function(data);

	/* 设置回传 */
	app_set_reply_parameters_function(buff->cmd,0x01);
}

/************************************************************
*
* Function name	: com_deal_fan_humi_param
* Description	: 设置风扇启动湿度参数
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_deal_fan_humi_param(com_rec_data_t *buff)
{
	uint8_t data[2] = {0};
	
	data[0] = buff->buff[0];
	data[1] = buff->buff[1];
	
	app_set_fan_humi_param_function(data);
	
	/* 设置回传 */
	app_set_reply_parameters_function(buff->cmd,0x01);
}

/************************************************************
*
* Function name	: com_deal_erase_parameter
* Description	: 处理擦除信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_deal_erase_parameter(com_rec_data_t *buff)
{
	uint8_t cmd = buff->buff[0];
	
	switch(cmd)
	{
		case CONFIGURE_SERVER_DOMAIN_NAME:
		case CONFIGURE_PING_INTERVAL:
		case CONFIGURE_SERVER_IP:
		case CONFIGURE_LOCAL_NETWORK:
		case CONFIGURE_HEART_TIME:
		case CONFIGURE_FAN_PARAMETER:
		case CONFIGURE_CAMERA_CONFIG:
		case CONFIGURE_HEATING_PARAM:
		case CONFIGURE_FAN_HUMI:
		case CONFIGURE_MAIN_NETWORK_IP:
		case CONFIGURE_FILL_LIGHT_TIME:
		case CONFIGURE_NETWORK_DELAY:    // 网络延时时间  20220308
		case CONFIGURE_IPC_LOGIN_INFO:    // 用户名、密码  20220329
		case CONFIGURE_IPC_TIME_SYNC:    	// 同步时间  		 20220329
		case CONFIGURE_SET_TEL:
			app_set_erase_infor_function(cmd);
			break;
		default:
			break;
	}
	
	/* 设置回传 */
	app_set_reply_parameters_function(buff->cmd,0x01);
}

/************************************************************
*
* Function name	: com_set_next_report_time
* Description	: 设置上报间隔时间
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_set_next_report_time(com_rec_data_t *buff)
{
	uint8_t  sel  = buff->buff[0];
	uint16_t time = (buff->buff[1]<<8|buff->buff[2]);

	if(time == 0)
	{
		/* 设置回传 */
		app_set_reply_parameters_function(buff->cmd,0x74);
	}
	else
	{
		app_set_next_report_time_other(time,sel);
		/* 设置回传 */
		app_set_reply_parameters_function(buff->cmd,0x01);
	}
	
}

/************************************************************
*
* Function name	: com_set_next_ping_time
* Description	: 设置ping的时间间隔
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_set_next_ping_time(com_rec_data_t *buff)
{
	uint16_t time 	  = (buff->buff[0]<<8|buff->buff[1]); // 前2个字节
	uint8_t	 time_dev = buff->buff[2];					  // 后1个字节
	
	if(time == 0)
	{
		/* 设置回传 */
		app_set_reply_parameters_function(buff->cmd,0x74);
	}
	else
	{
		/* 保存参数 */
		app_set_next_ping_time(time, time_dev);
		
		/* 设置回传 */
		app_set_reply_parameters_function(buff->cmd,0x01);
	}
}

/************************************************************
*
* Function name	: com_set_network_delay_time
* Description	: // 配置网络延时时间		  20220308
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_set_network_delay_time(com_rec_data_t *buff)
{
	uint8_t	 time = buff->buff[0];				
	
	if(time == 0)
	{
		app_set_reply_parameters_function(buff->cmd,0x74);		/* 设置回传 */
	}
	else
	{
		app_set_network_delay_time(time);		/* 保存参数 */
		app_set_reply_parameters_function(buff->cmd,0x01);		/* 设置回传 */
	}
}

/************************************************************
*
* Function name	: com_set_device_password
* Description	:  设置密码
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_set_device_password(com_rec_data_t *buff)
{
  struct device_param 	*device = app_get_device_param_function();
	memset(device->password,0,sizeof(device->name));
	sprintf((char*)device->password,DEFALUT_PASSWORD);
	device->default_password = 1; // 默认开机需要修改密码
	
	app_set_save_infor_function(SAVE_DEVICE_PARAM);	/* 存储 */
	app_set_reply_parameters_function(buff->cmd,0x01);	/* 设置回传 */	
	
}

/************************************************************
*
* Function name	: com_set_main_ping_ip
* Description	: 设置主机pingip
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_set_main_ping_ip(com_rec_data_t *buff)
{
	uint8_t ip[8];
	
	ip[0] = buff->buff[0];
	ip[1] = buff->buff[1];
	ip[2] = buff->buff[2];
	ip[3] = buff->buff[3];
	
	ip[4] = buff->buff[4];
	ip[5] = buff->buff[5];
	ip[6] = buff->buff[6];
	ip[7] = buff->buff[7];
	
	app_set_main_network_ping_ip(ip);
	
	/* 设置回传 */
	app_set_reply_parameters_function(buff->cmd,0x01);	
}


/************************************************************
*
* Function name	: com_deal_ack_parameter
* Description	: 处理回复数据
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_deal_ack_parameter(com_rec_data_t *buff)
{
	uint8_t error = buff->buff[0];
	
	if(error == 0x01)
	{
		app_set_send_result_function(SR_OK);
	}
	else
	{
		app_set_send_result_function(SR_ERROR);
	}
}

/************************************************************
*
* Function name	: com_query_processing_function
* Description	: 查询处理函数
* Parameter		: 
* Return		: 
*	修改： 增加摄像机ID号 20220329
************************************************************/
void com_query_processing_function(uint8_t query, uint8_t data)
{
	app_set_com_send_flag_function(query,data);
}


/************************************************************
*
* Function name	: com_recevie_function_init
* Description	: 通信接收初始化函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_recevie_function_init(void)
{
	com_cache_initialization(COM_MALLOC_SIZE);
}

/************************************************************
*
* Function name	: com_deal_main_function
* Description	: 通信接收处理函数
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t com_deal_main_function(void)
{
	#ifdef COMDATA_PROCESS_MODE2
	struct device_param *device      	= app_get_device_param_function();
	com_rec_data_t      recdata_t    	= {0};
	uint32_t 						temp		 			= 0;
	uint8_t 	        	rec_buff[100] = {0};
	uint8_t 	        	size		 			= 0;
	uint8_t 	        	crc			 			= 0;
	uint8_t 		    		ret			 			= 0;
	
	size = com_queue_find_msg(rec_buff,100);
	if(size != 0)
	{

		/* 校验CRC:去除crc校验与数据尾 */
		crc = calc_crc8(&rec_buff[2],size-5);
		if(crc != rec_buff[size-3])
		{
			/* CRC校验错误 */
			ret = CR_CHECK_ERROR;
			app_set_reply_parameters_function(recdata_t.cmd,ret);
			goto __ERROR;
		}
		rec_buff[size-3] = 0; // 将CRC校验对应的数据直接转换为字符串尾
		
		/* 获取版本 */
		recdata_t.version = rec_buff[2];
		/* 获取ID */
		recdata_t.id      = (rec_buff[3]<<16)|(rec_buff[4]<<8)|(rec_buff[5]<<0);
		
		/* 数据版本 */
		if(recdata_t.version != COM_NUM_VAERSION)
		{
			/* 数据版本错误 */
			goto __ERROR;
		}
		
		/* 获取指令类型 */
		recdata_t.cmd     = rec_buff[6];
		
		/* ID验证 */
		if(recdata_t.id != device->id.i && recdata_t.cmd != CONFIGURE_NOW_TIME)
		{
			/* ID错误 */
			ret = CR_DEVICE_NUMBER_ERROR;
			app_set_reply_parameters_function(recdata_t.cmd,ret);
			goto __ERROR;
		}
		
		/* 获取请求标识码 */
		temp = rec_buff[7];
		temp = (temp<<8)|rec_buff[8];
		temp = (temp<<8)|rec_buff[9];
		temp = (temp<<8)|rec_buff[10];
		sg_comqn_t.qn1 = temp;
		temp = rec_buff[11];
		temp = (temp<<8)|rec_buff[12];
		temp = (temp<<8)|rec_buff[13];
		temp = (temp<<8)|rec_buff[14];
		sg_comqn_t.qn2 = temp;
		/* 获取长度 */
		recdata_t.size    = rec_buff[15];
		/* 获取内容 */
		recdata_t.buff    = &rec_buff[16];
		
		/* 根据命令解析数据 */
		switch(recdata_t.cmd)
		{
			/* 配置指令 */
			case CONFIGURE_SERVER_DOMAIN_NAME:  // 设置服务器信息
				com_deal_configure_server_domain_name(&recdata_t);
				break;
			case CONFIGURE_LOCAL_NETWORK: 		// 设置本地网络信息
				com_deal_configure_local_network(&recdata_t);
				break;
			case CONFIGURE_SET_MAC: 			// 设置MAC
				com_deal_configure_mac(&recdata_t);
				break;
			case CONFIGURE_CAMERA_CONFIG:		// 设置摄像头IP
				com_deal_camera_config(&recdata_t);
				break;
			case CONFIGURE_FAN_PARAMETER:		// 设置风扇温度
				com_deal_fan_temp_parmaeter(&recdata_t);
				break;
			case CONFIGURE_ERASE_PARAMETER:		// 擦除操作
				com_deal_erase_parameter(&recdata_t);
				break;
			case CONFIGURE_HEART_TIME:			// 设置上报时间
				com_set_next_report_time(&recdata_t);
				break;
			case CONFIGURE_PING_INTERVAL:		// 设置ping间隔时间
				com_set_next_ping_time(&recdata_t);
				break;
			case CONFIGURE_NETWORK_DELAY:		// 配置网络延时时间		  20220308
				com_set_network_delay_time(&recdata_t);
				break;
			case CONFIGURE_MAIN_NETWORK_IP:     // 设置主网络
				com_set_main_ping_ip(&recdata_t);
				break;

			case CONFIGURE_FAN_HUMI:			// 设置风扇湿度
				com_deal_fan_humi_param(&recdata_t);
				break;

			case CONFIGURE_SERVER_MODE:			// 配置网络连接模式
				com_deal_configure_server_mode(&recdata_t);
				break;
			case CONFIGURE_UPDATE_SYSTEM:		// 启用系统更新
				com_deal_update_system_function(&recdata_t);
				break;
			case CONFIGURE_NOW_TIME:			// 配置当前时间
				com_set_now_time_function(&recdata_t);
				break;
			case CONFIGURE_DEVICE_NAME:				// 设置设备名称  20220416
				com_set_device_name_function(&recdata_t);
				break;
			case CONFIGURE_THRESHOLD_PARAMS:				// 配置阈值  20230721
				com_set_threshold_params_function(&recdata_t);
				break;
			case CONFIGURE_DEVICE_PASSWORD:		// 密码恢复出厂
				com_set_device_password(&recdata_t);
				break;
			case CONFIGURE_DEVICE_ID: 			// 设置设备ID  20231026
				com_deal_configure_device_id(&recdata_t);
				break;

			
			/* 查询指令 */
			case CR_QUERY_CONFIG: 			// 查询设备当前参数设置 - 对应上传查询配置
			case CR_QUERY_INFO:   			// 立即上报设备状态	    - 正常上报
			case CR_QUERY_SOFTWARE_VERSION: // 查询设备软件版本号
			case CR_QUERY_IPC_IP:       		// 查询摄像机IP地址    20220329
			case CR_QUERY_IPC_INFO:					// 查询摄像机相关参数  20220329
			case CR_QUERY_LBS_INFO:					// 查询基站定位信息
				sg_comqn_t.flag = 1;
				com_query_processing_function(recdata_t.cmd,recdata_t.buff[0]-1);
				break;
			
			/* 操作指令 */
			case CR_SINGLE_CAMERA_CONTROL:
			case CR_POWER_RESETART:
			case CR_GPRS_NETWORK_RESET:
			case CR_LWIP_NETWORK_RESET:
			case CR_IPC_REBOOT:            // IPC重启  20220329
			case CR_GPRS_NETWORK_V_RESET:
				app_set_sys_opeare_function(recdata_t.cmd,recdata_t.buff[0]);
				break;
			
			/* 开关控制 */
			case CONTROL_FAN:
			case CONTROL_FILL_LIGHT:
			case CONTROL_HEATING:
				app_set_peripheral_switch(recdata_t.cmd,recdata_t.buff[0]);
				break;
			
			default:
				com_deal_ack_parameter(&recdata_t);
				break;
		}
	} 
	else 
		size = 0;
	
	return ret;
__ERROR:
	return ret;
	#endif
}

/*********************************************************
				数据缓存区
**********************************************************/
com_queue_t sg_comqueue_t = {0};

static void com_queue_init(com_queue_t *queue);

/************************************************************
*
* Function name	: com_cache_initialization
* Description	: 缓存区初始化
* Parameter		: 
* Return		: 
* 
************************************************************/
void com_cache_initialization(uint16_t size)
{
	sg_comqueue_t.data = mymalloc(SRAMIN,size);
	sg_comqueue_t.size = size;
	mymemset(sg_comqueue_t.data,0,size);
	com_queue_init(&sg_comqueue_t);
}

/************************************************************
*
* Function name	: 
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_queue_init(com_queue_t *queue)
{
	queue->front = 0;
	queue->rear  = 0;
}

/************************************************************
*
* Function name	: 
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t com_is_queue_empty(com_queue_t queue)
{
	return (queue.front == queue.rear?1:0);
}

/************************************************************
*
* Function name	: com_en_queue
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t com_en_queue(com_queue_t *queue,uint8_t data)
{
	uint16_t pos = (queue->front+1)%(queue->size);
	
	if(pos != queue->rear)
	{
		queue->data[queue->front] = data;
		queue->front = pos;
		return 1;
	}
	else
	{
//		mymemset(sg_comqueue_t.data,0,sg_comqueue_t.size);
//		com_queue_init(&sg_comqueue_t);
		return 0;
	}	
}

/************************************************************
*
* Function name	: 
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t com_de_queue(com_queue_t *queue,uint8_t *data)
{
	if(queue->rear == queue->front)
	{
//		mymemset(sg_comqueue_t.data,0,sg_comqueue_t.size);
//		com_queue_init(&sg_comqueue_t);
		return 0;
	}
	*data=queue->data[queue->rear];
	queue->rear=(queue->rear+1)%(queue->size);
	return 1;
}

/************************************************************
*
* Function name	: com_stroage_cache_data
* Description	: 将数据存储到缓存区
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_stroage_cache_data(uint8_t *buff,uint16_t len)
{
	uint16_t index = 0;
	uint8_t  res   = 0;
	uint8_t  data  = 0;
	
	for(index=0; index<len; index++)
	{
		res = com_en_queue(&sg_comqueue_t,buff[index]);
		if(res == 0)
		{
			com_de_queue(&sg_comqueue_t,&data);
			com_en_queue(&sg_comqueue_t,buff[index]);
		}
	}
}

/************************************************************
*
* Function name	: com_storage_cache_full_data
* Description	: 填充空数据
* Parameter		: 
* Return		: 
*	
************************************************************/
void com_storage_cache_full_data(void)
{
	uint8_t buff = 0xff;
	uint8_t index = 0;
	
	for(index=0; index<20; index++)
	{
		com_stroage_cache_data(&buff,1);
	}
}

/************************************************************
*
* Function name	: 
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
uint16_t com_size_queue(com_queue_t queue)
{
	return ((queue.front+queue.size-queue.rear)%(queue.size));
}

static uint16_t msg_pos   = 0;

#define CUTDOWN_TIME (1000) // 1000x1ms
uint32_t com_cutdown_time = 0;

void com_queue_time_function(void)
{
	if(com_cutdown_time != 0) {
		com_cutdown_time--;
		if(com_cutdown_time == 0) {
			msg_pos = 0;
		}
	}
}

/*
##0163QN=20200730160101008;TID=123456;CMD=F5;CP=&&INNER=192.168.0.1:52369;OUTER=abc.fnwlw.net:52369;OUTER2=b2.fnwlw.net:52369;OUTER3=b3.fnwlw.net:52369;UPDATE=192.168.0.100:54333&&be##
*/
/************************************************************
*
* Function name	: com_queue_find_msg
* Description	: 获取缓存区中的数据
* Parameter		: 
* Return		: 
*	
************************************************************/
uint16_t com_queue_find_msg(uint8_t *msg,uint16_t size)
{
	uint16_t msg_size = 0;
	#ifdef COMDATA_PROCESS_MODE2
	static uint16_t msg_state = 0;
	static uint16_t	msg_head  = 0;
	static uint8_t  buff_size = 0;
	static uint8_t  buff_cmd  = 0;
	uint8_t  		msg_data  = 0;
	OS_CPU_SR 		cpu_sr;
	buff_cmd  =  buff_cmd;
	
	/* 关中断: 本环形队列由串口空闲中断(com_stroage_cache_data)入队,
	   由本函数在任务中出队, 并发读写 front/rear 会破坏队列索引与帧,
	   故整段解析加临界区保护(纯内存操作, 时间极短)。 */
	OS_ENTER_CRITICAL();
	while(com_size_queue(sg_comqueue_t) > 0)
	{
		com_cutdown_time = CUTDOWN_TIME;
		com_de_queue(&sg_comqueue_t,&msg_data);
		
		msg_head = (msg_head<<8)|msg_data;
		if(msg_pos==0 && (msg_head != COM_REC_HAED_HEX))
		{
			continue;
		}
		/* 补齐第一个字节的数据头 */
		if(msg_pos==0)
		{
			msg[msg_pos++] = (COM_REC_HAED_HEX>>8)&0xff;
			buff_size = 0;
		}
		if(msg_pos == 6) {
			buff_cmd = msg_data;
		}

		if(msg_pos == (7+8))
		{
			buff_size = msg_data;
		}
		
		if(msg_pos < size)
		{
			msg[msg_pos++] = msg_data;
		} 
		else 
		{
			msg_size  = 0;
			msg_state = 0;						//重新检测帧尾巴
			msg_pos   = 0;					    //复位指令指针
			buff_size = 0;
			break;
		}
		
		msg_state = ((msg_state<<8)|msg_data); //拼接最后2个字节，组成一个16位整数
		
		/* 最后2个字节与帧尾匹配，得到完整帧 */
		if(msg_state == COM_TAIL_HEX && ((buff_size + 11 + 8) <= msg_pos))
		{
			msg_size  = msg_pos;				//指令字节长度
			msg_state = 0;						  //重新检测帧尾巴
			msg_pos   = 0;					    //复位指令指针
			buff_cmd  = 0;
			
			break;
		}
		
		/* 协议数据出错处理 */
		if((11+8+buff_size) <= msg_pos) {
			msg_size  = 0;
			msg_state = 0;						//重新检测帧尾巴
			msg_pos   = 0;					    //复位指令指针
			buff_size = 0;
			break;
		}
	}
	OS_EXIT_CRITICAL();
	#endif
	return msg_size; 
}
