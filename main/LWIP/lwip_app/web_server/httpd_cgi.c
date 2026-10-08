#include "httpd_cgi_ssi.h"
#include "lwip/debug.h"
#include "httpd.h"
#include "lwip/tcp.h"
#include "fs.h"
#include "lwip_comm.h"
#include "det.h"
#include "relay.h"
#include "app.h"
#include "error.h"
#include "start.h"
#include "eth.h"
#include "appconfig.h"
#include "gsm.h"
#include "includes.h"

#include "stdio.h"
#include "string.h"
#include <stdlib.h>
#include "update.h"
#include "timer.h"
#include "w25qxx.h"
#include "relay.h"
/************************************************************
*
* Function name	: 
* Description	: 网页登录操作
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t httpd_cgi_login_function(int iNumParams, char *pcParam[], char *pcValue[])
{
	uint8_t i 		  = 0;
	uint8_t login_cnt = 0;
	
	if (strcmp(pcValue[0], "login")==0)
	{
		for (i=1; i< (iNumParams); i++)
		{
			/* 监测名称 */
			if (strcmp(pcParam[i], "username")==0)
			{
				if(strcmp(pcValue[i], "root")==0)
				{
					login_cnt++;
				}
			}
			/* 监测密码 */
			if (strcmp(pcParam[i], "password")==0)
			{
				if(app_match_password_function(pcValue[i]) == 0)
				{
					login_cnt++;
				}
			}
		}
		/* 监测状态 */
		if(login_cnt == 2)
		{
			if( app_match_set_code_function() == 1 ) {
				set_return_status_function(0,(uint8_t*)"\"SUCCESS!\",\"1\"");
			} else {
				set_return_status_function(0,(uint8_t*)"\"SUCCESS!\",\"0\"");
			}
		}
		else
		{
			set_return_status_function( INCORRECT_ACCOUNT_OR_PASSWORD_NUM,\
																		(uint8_t*)INCORRECT_ACCOUNT_OR_PASSWORD_STR);
		}
		return 0;
	}
	return -1;
}
/************************************************************
*
* Function name	: httpd_cgi_login_mod_function
* Description	: 密码修改页面
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t httpd_cgi_login_mod_function(int iNumParams, char *pcParam[], char *pcValue[])
{
	static struct device_param param = {0};
	uint8_t i 		  = 0;
	uint8_t size = 0;
	
    /* 修改密码 */
	if (strcmp(pcValue[0], "changePassword")==0) {
		
		for (i=1; i< (iNumParams); i++){
			/* 监测密码 */
			if (strcmp(pcParam[i], "password")==0)
			{
				memset(param.password,0,sizeof(param.password));
				size = strlen(pcValue[i]);
				if(size > CODE_MAX_NUM) {
					size = 12;
				}
				memcpy(param.password,pcValue[i],size);
				
				set_return_status_function(0,(uint8_t*)"\"SUCCESS!\"");
				/* 存储密码 */
				app_set_code_function(param);
			}
		}
		
		return 0;
	}
	
	return -1;
}
/************************************************************
*
* Function name	: httpd_cgi_select_function
* Description	: 下拉框
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t httpd_cgi_select_function(char *pcValue[])
{
	if (strcmp(pcValue[0] , "select")==0 ) {	
		set_return_status_function(0,(uint8_t*)"\"\"");
		
		return 0;
	}
	
	return -1;
}	

/************************************************************
*
* Function name	: Setting_switch_status_function
* Description	: 设置开关状态
* Parameter		: 
* Return		: 
*	
************************************************************/
static void setting_switch_status_function(char *pcParam[], char *pcValue[],uint8_t i)
{	
	/* 适配器1的开关 */
	if (strcmp(pcParam[i] , "sw1_switch")==0) 
	{
		relay_control(RELAY_1,(RELAY_STATUS)(atoi(pcValue[i])));	return;
	}

	/* 适配器2的开关 */
	if (strcmp(pcParam[i] , "sw2_switch")==0) 
	{		
		relay_control(RELAY_2,(RELAY_STATUS)(atoi(pcValue[i])));	return;
	}

	if (strcmp(pcParam[i] , "sw3_switch")==0) 
	{
		relay_control(RELAY_3,(RELAY_STATUS)(atoi(pcValue[i])));	return;
	}

	if (strcmp(pcParam[i] , "sw4_switch")==0) 
	{
		relay_control(RELAY_4,(RELAY_STATUS)(atoi(pcValue[i])));	return;
	}

	if (strcmp(pcParam[i] , "sw5_switch")==0) 
	{
		relay_control(RELAY_5,(RELAY_STATUS)(atoi(pcValue[i])));	return;
	}

	if (strcmp(pcParam[i] , "sw6_switch")==0) 
	{
		relay_control(RELAY_6,(RELAY_STATUS)(atoi(pcValue[i])));	return;
	}

	if (strcmp(pcParam[i] , "sw7_switch")==0) 
	{
		relay_control(RELAY_7,(RELAY_STATUS)(atoi(pcValue[i])));	return;
	}

	if (strcmp(pcParam[i] , "sw8_switch")==0) 
	{
		relay_control(RELAY_8,(RELAY_STATUS)(atoi(pcValue[i])));	return;
	}
	
	/* 水浸告警 */
	if (strcmp(pcParam[i] , "r_watererror")==0)
	{
		app_set_report_switch_status(atoi(pcValue[i]),FLAG_W_E); return;
	}
	
	/* 水浸恢复 */
	if (strcmp(pcParam[i] , "r_watereset")==0)
	{
		app_set_report_switch_status(atoi(pcValue[i]),FLAG_W_N); return;
	}
	/* 防雷 */
	if (strcmp(pcParam[i] , "r_spd")==0)
	{
		app_set_report_switch_status(atoi(pcValue[i]),FLAG_S); return;
	}
	/* 倾斜 */
	if (strcmp(pcParam[i] , "r_angle")==0)
	{
		app_set_report_switch_status(atoi(pcValue[i]),FLAG_A); return;
	}
	/* 温度过高 */
	if (strcmp(pcParam[i] , "r_temphigh")==0)
	{
		app_set_report_switch_status(atoi(pcValue[i]),FLAG_T_H); return;
	}
	/* 温度过低 */
	if (strcmp(pcParam[i] , "r_templow")==0)
	{
		app_set_report_switch_status(atoi(pcValue[i]),FLAG_T_L); return;
	}
	/* 湿度过高 */
	if (strcmp(pcParam[i] , "r_humihigh")==0)
	{
		app_set_report_switch_status(atoi(pcValue[i]),FLAG_H_H); return;
	}
	/* 湿度过低 */
	if (strcmp(pcParam[i] , "r_humilow")==0)
	{
		app_set_report_switch_status(atoi(pcValue[i]),FLAG_H_L); return;
	}
	/* 箱门 */
	if (strcmp(pcParam[i] , "r_door")==0)
	{
		app_set_report_switch_status(atoi(pcValue[i]),FLAG_D); return;
	}
	/* 电量 */
	if (strcmp(pcParam[i] , "r_power")==0)
	{
		app_set_report_switch_status(atoi(pcValue[i]),FLAG_P); return;
	}
	/* 电源状态 */
	if (strcmp(pcParam[i] , "r_acstate")==0)
	{
		app_set_report_switch_status(atoi(pcValue[i]),FLAG_AC); return;	
	}
}

/************************************************************
*
* Function name	: httpd_cgi_switch_function
* Description	: 开关量
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t httpd_cgi_switch_function(int iNumParams, char *pcParam[], char *pcValue[])
{
	uint8_t i = 0;
	
	if (strcmp(pcValue[0] , "switch")==0 ) {
		for (i=1; i< (iNumParams); i++) {
			setting_switch_status_function(pcParam,pcValue,i);
			if(i == (iNumParams-1)) {
				set_return_status_function(0,(uint8_t*)"\"SUCCESS!\"");
			}
		}
		
		return 0;
	}
	
	return -1;
}

/************************************************************
*
* Function name	: Setting_threshold_parameter_function
* Description	: 阈值
* Parameter		: 
* Return		: 
*	   20230720
************************************************************/
static int8_t Setting_threshold_parameter_function(char *pcParam[], char *pcValue[],uint8_t i)
{
	static struct threshold_params param = {0};
	int8_t      ret		= 0;
	
	/* 系统设置 */
	if (strcmp(pcParam[i] , "aa")==0) // 电压
	{
		param.volt_max = atoi(pcValue[i]); return ret;
	}
	if (strcmp(pcParam[i] , "ab")==0) // 电压
	{
		param.volt_min = atoi(pcValue[i]); return ret;
	}	
	if (strcmp(pcParam[i] , "ac")==0) // 电流
	{
		param.current = atoi(pcValue[i]); return ret;
	}
	if (strcmp(pcParam[i] , "ad")==0) // 角度
	{
		param.angle = atoi(pcValue[i]); return ret;
	}
	if (strcmp(pcParam[i] , "ae")==0) // 风扇启动温度
	{
		param.temp_high = atoi(pcValue[i]); return ret;
	}	
	if (strcmp(pcParam[i] , "af")==0) // 风扇停止温度
	{
		param.temp_low = atoi(pcValue[i]); return ret;
	}		
	if (strcmp(pcParam[i] , "ag")==0) // 风扇启动湿度
	{
		param.humi_high = atoi(pcValue[i]); return ret; 
	}		
	if (strcmp(pcParam[i] , "ah")==0) // 风扇停止湿度
	{
		param.humi_low = atoi(pcValue[i]);
		app_set_threshold_param_function(param);
	}		
	return ret;
}

/************************************************************
*
* Function name	: httpd_cgi_set_threshold_function
* Description	: 阈值设置
* Parameter		: 
* Return		: 
*	   20230720
************************************************************/
int8_t httpd_cgi_set_threshold_function(int iNumParams, char *pcParam[], char *pcValue[])
{
	int8_t  ret = 0;
	uint8_t i   = 0;
		
	if (strcmp(pcValue[0] , "threshold_save")==0)
	{	
		for (i=1; i<iNumParams; i++)
		{
			ret = Setting_threshold_parameter_function(pcParam,pcValue,i);
			if(ret != 0)
			{
				set_return_status_function(PARAMETER_ERROR_NUM,(uint8_t*)PARAMETER_ERROR_STR);
				break;
			}
			else
			{
				if(i == (iNumParams-1)) {
					set_return_status_function(0,(uint8_t*)"\"SUCCESS!\"");
				}	
			}
		}
		return 0;
	}
	return -1;
}

/************************************************************
*
* Function name	: str_to_hex
* Description	: 字符转hex
* Parameter		: 
* Return		: 
*	
************************************************************/
static uint8_t str_to_hex(uint8_t *buff)
{
	uint8_t temp;
	
	if(buff[0] >= 'A' && buff[0] <= 'F') {
		temp = buff[0]-'A' + 0x0A;
	} else if(buff[0] >= '0' && buff[0] <= '9') {
		temp = buff[0]-'0';
	}
	temp = temp<<4;
	if(buff[1] >= 'A' && buff[1] <= 'F') {
		temp |= (buff[1]-'A' + 0x0A);
	} else if(buff[1] >= '0' && buff[1] <= '9') {
		temp |= (buff[1]-'0');
	}
	return temp;
}

/************************************************************
*
* Function name	: Setting_device_parameter_function
* Description	: 设置设备参数
* Parameter		: 
* Return		: 
*	
************************************************************/
static int8_t Setting_device_parameter_function(char *pcParam[], char *pcValue[],uint8_t i)
{
	int8_t ret				  		 = 0;
	static struct device_param param = {0};
	static int    time[6]			 = {0};
	uint8_t 	  mode 				 = 0;
	uint8_t 	  index   		  	 = 0;
	uint8_t  	  num				 = 0;
	uint8_t		  size				 = 0;
	
	/* 系统设置 */
	if (strcmp(pcParam[i] , "a")==0) // 同步时间
	{
		sscanf(pcValue[i],"%d/%d/%d%%20%d:%d:%d",&time[0],&time[1],&time[2],&time[3],&time[4],&time[5]);
		app_set_current_time(time,0);
	}
	else if (strcmp(pcParam[i] , "b")==0) // 本地ID - 存数字
	{
		param.id.i = 0;
//		sscanf(pcValue[i],"%d",&param.id);
		sscanf(pcValue[i],"%X",&param.id.i);
	}
	else if (strcmp(pcParam[i] , "c")==0) // 设备名称
	{
		memset(param.name,0,sizeof(param.name));
		size = strlen(pcValue[i]);
		num = 0;
		for(index=0; index<size; index++) 
		{
			if(pcValue[i][index] == '%') 
			{
				if(pcValue[i][index+3] == '%' && pcValue[i][index+6] == '%') {
					param.name[num++] = str_to_hex((uint8_t*)&pcValue[i][index+1]);
					param.name[num++] = str_to_hex((uint8_t*)&pcValue[i][index+4]);
					param.name[num++] = str_to_hex((uint8_t*)&pcValue[i][index+7]);
					index+=8;
				} 
				else {
					param.name[num++] = pcValue[i][index];
				}
			} 
			else {
				param.name[num++] = pcValue[i][index];
			}
		}
	}
	else if (strcmp(pcParam[i] , "d")==0) // 登录密码
	{
		memset(param.password,0,sizeof(param.password));
		memcpy(param.password,pcValue[i],strlen(pcValue[i]));
	}
	if (strcmp(pcParam[i] , "tran") == 0) { // 传输模式
	
		mode = atoi(pcValue[i]);
		app_set_transfer_mode_function(mode);
		app_set_device_param_function(param);			
	}
	return ret;
}

/************************************************************
*
* Function name	: httpd_cgi_set_system_function
* Description	: 系统设置
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t httpd_cgi_set_system_function(int iNumParams, char *pcParam[], char *pcValue[])
{
	int8_t  ret = 0;
	uint8_t i   = 0;
	
	if (strcmp(pcValue[0] , "set_save")==0)
	{
		for (i=1; i< (iNumParams); i++)
		{
			ret = Setting_device_parameter_function(pcParam,pcValue,i);
			if(ret != 0)
			{
				set_return_status_function(PARAMETER_ERROR_NUM,(uint8_t*)PARAMETER_ERROR_STR);
				break;
			}
			else
			{
				if(i == (iNumParams-1)) {
					set_return_status_function(0,(uint8_t*)"\"SUCCESS!\"");
					OSTimeDlyHMSM(0,0,0,100);
					eth_set_tcp_connect_reset();				/* 重启TCP连接 */
					gsm_set_module_reset_function();		 /* 重启无线连接 */
				}
			}
		}
		return 0;
	}
	return -1;
}

/************************************************************
*
* Function name	: Setting_local_network_parameter_function
* Description	: 设置网络参数
* Parameter		: 
* Return		: 
*	
************************************************************/
static int8_t Setting_local_network_parameter_function(char *pcParam[], char *pcValue[],uint8_t i)
{
	int8_t 		  			 ret     = 0;
	static struct local_ip_t param   = {0};
	int	 		  			 temp[6] = {0};
	uint8_t          time[1]  = {0};
	
	if (strcmp(pcParam[i] , "e")==0)  // IP
	{
		ret = sscanf(pcValue[i],"%d.%d.%d.%d",&temp[0],&temp[1],&temp[2],&temp[3]);
		if(ret == 4) {
			param.ip[0] = temp[0];
			param.ip[1] = temp[1];
			param.ip[2] = temp[2];
			param.ip[3] = temp[3];
			ret = 0;
		}
	}
	else if (strcmp(pcParam[i] , "g")==0) // 网关
	{
		ret = sscanf(pcValue[i],"%d.%d.%d.%d",&temp[0],&temp[1],&temp[2],&temp[3]);
		if(ret == 4) {
			param.gateway[0] = temp[0];
			param.gateway[1] = temp[1];
			param.gateway[2] = temp[2];
			param.gateway[3] = temp[3];
			ret = 0;
		}
	}
	
	else if(strcmp(pcParam[i] , "f")==0) // 子网掩码
	{
		ret = sscanf(pcValue[i],"%d.%d.%d.%d",&temp[0],&temp[1],&temp[2],&temp[3]);
		if(ret == 4) {
			param.netmask[0] = temp[0];
			param.netmask[1] = temp[1];
			param.netmask[2] = temp[2];
			param.netmask[3] = temp[3];
			ret = 0;
		}
	}
	else if (strcmp(pcParam[i] , "ai")==0) // 本机端口
	{
		param.port = atoi(pcValue[i]);
	}	
	else if(strcmp(pcParam[i] , "h")==0) // DNS
	{
		ret = sscanf(pcValue[i],"%d.%d.%d.%d",&temp[0],&temp[1],&temp[2],&temp[3]);
		if(ret == 4) {
			param.dns[0] = temp[0];
			param.dns[1] = temp[1];
			param.dns[2] = temp[2];
			param.dns[3] = temp[3];
			ret = 0;
		}
	}
		
	else if (strcmp(pcParam[i] , "L")==0) // MAC 
	{
		ret = sscanf(pcValue[i],"%x-%x-%x-%x-%x-%x",&temp[0],&temp[1],&temp[2],&temp[3],&temp[4],&temp[5]);
		if(ret == 6) {
			param.mac[0] = temp[0];
			param.mac[1] = temp[1];
			param.mac[2] = temp[2];
			param.mac[3] = temp[3];
			param.mac[4] = temp[4];
			param.mac[5] = temp[5];
			ret = 0;
		}
	}

	else if (strcmp(pcParam[i] , "i")==0) { // 主网ping地址1
		ret = sscanf(pcValue[i],"%d.%d.%d.%d",&temp[0],&temp[1],&temp[2],&temp[3]);
		if(ret == 4) {
			param.ping_ip[0] = temp[0];
			param.ping_ip[1] = temp[1];
			param.ping_ip[2] = temp[2];
			param.ping_ip[3] = temp[3];
			ret = 0;
		}
	}
	
	else if (strcmp(pcParam[i] , "K")==0) { // 主网ping地址1
		ret = sscanf(pcValue[i],"%d.%d.%d.%d",&temp[0],&temp[1],&temp[2],&temp[3]);
		if(ret == 4) {
			ret = 0;
			param.ping_sub_ip[0] = temp[0];
			param.ping_sub_ip[1] = temp[1];
			param.ping_sub_ip[2] = temp[2];
			param.ping_sub_ip[3] = temp[3];
		}
	}
	
  // 网络延时时间  20220308
	else if(strcmp(pcParam[i] , "N") == 0) {
		time[0] = atoi(pcValue[i]);
		app_set_com_time_param_function((uint32_t*)time,2); // 网络延时时间 
	}

	else if (strcmp(pcParam[i] , "O")==0)  // IP
	{
		ret = sscanf(pcValue[i],"%d.%d.%d.%d",&temp[0],&temp[1],&temp[2],&temp[3]);
		if(ret == 4) 
		{
			param.multicast_ip[0] = temp[0];
			param.multicast_ip[1] = temp[1];
			param.multicast_ip[2] = temp[2];
			param.multicast_ip[3] = temp[3];
			ret = 0;
		}
	}
	else if (strcmp(pcParam[i] , "P")==0) // 内外端口
	{
		param.multicast_port = atoi(pcValue[i]);
		app_set_local_network_function_two(param);
	}

	return ret;
}


/************************************************************
*
* Function name	: httpd_cgi_set_network_function
* Description	: 网络设置
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t httpd_cgi_set_network_function(int iNumParams, char *pcParam[], char *pcValue[])
{
	int8_t ret;
	uint8_t i;
	
	if (strcmp(pcValue[0] , "local_save")==0)
	{
		for (i=1; i< (iNumParams); i++)
		{
			ret = Setting_local_network_parameter_function(pcParam,pcValue,i);
			if(ret != 0)
			{
				set_return_status_function(PARAMETER_ERROR_NUM,(uint8_t*)PARAMETER_ERROR_STR);
				break;
			}
			else
			{
				if(i == (iNumParams-1)) {
					set_return_status_function(0,(uint8_t*)"\"SUCCESS!\"");
					OSTimeDlyHMSM(0,0,0,100);
					/* 设置网络重启 */
					eth_set_network_reset();
				}
			}
		}
		
		return 0;
	}
	
	return -1;
}

/************************************************************
*
* Function name	: Setting_camera_function
* Description	: 设置相机参数
* Parameter		: 
* Return		: 
*	
************************************************************/
static int8_t Setting_camera_function(char *pcParam[], char *pcValue[],uint8_t i)
{
	static uint32_t time[4]     = {0};
	int8_t	 		ret 		= 0;
	static uint8_t  ip[6][4]	= {0};
	int    			temp[4] 	= {0};
	
	if (strcmp(pcParam[i] , "m")==0) // 摄像头1
	{
		sscanf(pcValue[i],"%d.%d.%d.%d",&temp[0],&temp[1],&temp[2],&temp[3]);
		ip[0][0] = temp[0];
		ip[0][1] = temp[1];
		ip[0][2] = temp[2];
		ip[0][3] = temp[3];
		return ret;
	}
	
	if (strcmp(pcParam[i] , "n")==0) // 摄像头2
	{
		sscanf(pcValue[i],"%d.%d.%d.%d",&temp[0],&temp[1],&temp[2],&temp[3]);
		ip[1][0] = temp[0];
		ip[1][1] = temp[1];
		ip[1][2] = temp[2];
		ip[1][3] = temp[3];
		return ret;
	}
	
	if (strcmp(pcParam[i] , "o")==0) // 摄像头3
	{
		sscanf(pcValue[i],"%d.%d.%d.%d",&temp[0],&temp[1],&temp[2],&temp[3]);
		ip[2][0] = temp[0];
		ip[2][1] = temp[1];
		ip[2][2] = temp[2];
		ip[2][3] = temp[3];
		return ret;
	}
	
	if (strcmp(pcParam[i] , "p")==0) // 摄像头2
	{
		sscanf(pcValue[i],"%d.%d.%d.%d",&temp[0],&temp[1],&temp[2],&temp[3]);
		ip[3][0] = temp[0];
		ip[3][1] = temp[1];
		ip[3][2] = temp[2];
		ip[3][3] = temp[3];
		return ret;
	}
	
	if (strcmp(pcParam[i] , "q")==0) // 摄像头2
	{
		sscanf(pcValue[i],"%d.%d.%d.%d",&temp[0],&temp[1],&temp[2],&temp[3]);
		ip[4][0] = temp[0];
		ip[4][1] = temp[1];
		ip[4][2] = temp[2];
		ip[4][3] = temp[3];
		return ret;
	}
	
	if (strcmp(pcParam[i] , "r")==0) // 摄像头2
	{
		sscanf(pcValue[i],"%d.%d.%d.%d",&temp[0],&temp[1],&temp[2],&temp[3]);
		ip[5][0] = temp[0];
		ip[5][1] = temp[1];
		ip[5][2] = temp[2];
		ip[5][3] = temp[3];
		return ret;
	}
	/* 每轮ping间隔时间 */
	if(strcmp(pcParam[i] , "s") == 0) {
		time[0] = atoi(pcValue[i]);  return ret;
	}
	
	/* 下一次ping的间隔时间 */
	if(strcmp(pcParam[i] , "t") == 0) {
		time[1] = atoi(pcValue[i]);  return ret;
	}

	/* ONVIF时间 */
	if(strcmp(pcParam[i] , "Q") == 0) {
		time[2] = atoi(pcValue[i]);
	}

	/* 重启时间 */
	else if(strcmp(pcParam[i] , "ae") == 0) {
		time[3] = atoi(pcValue[i]);
		// 判断设备重启时间，防止时间读取为0
		if(time[3] < 24)
			time[3]= 24;
		else if(time[3] > 168)
			time[3]= 168;	
		app_set_com_time_param_function(time,0);
		app_set_camera_function((uint8_t*)ip);
	}
	return ret;
}

/************************************************************
*
* Function name	: httpd_cgi_set_camera_ip_function
* Description	: 摄像头ip设置
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t httpd_cgi_set_camera_ip_function(int iNumParams, char *pcParam[], char *pcValue[])
{
	int8_t ret;
	uint8_t i;
	
	if (strcmp(pcValue[0] , "camera_save")==0)
	{
		for (i=1; i< (iNumParams); i++)
		{
			ret = Setting_camera_function(pcParam,pcValue,i);
			if(ret != 0)
			{
				set_return_status_function(PARAMETER_ERROR_NUM,(uint8_t*)PARAMETER_ERROR_STR);
				break;
			}
			else
			{
				if(i == iNumParams-1) {
					set_return_status_function(0,(uint8_t*)"\"SUCCESS!\"");
				}
			}
		}
		
		return 0;
	}
	
	return -1;
}
/************************************************************
*
* Function name	: Setting_remote_network_function
* Description	: 设置服务器参数
* Parameter		: 
* Return		: 
*	
************************************************************/
static int8_t Setting_remote_network_function(char *pcParam[], char *pcValue[],uint8_t i)
{
	static struct remote_ip param 	  = {0};
	static uint32_t			time	  = 0;
	static uint8_t 			ip[4]     = {0};
	static uint32_t			port	  = 0;
	int	   					temp[4]   = {0};
	int8_t 					ret 	  = 0;
	if (strcmp(pcParam[i] , "F")==0) // 外网Ip
	{
		memset(param.outside_iporname,0,sizeof(param.outside_iporname));
		memcpy(param.outside_iporname,pcValue[i],strlen((const char*)pcValue[i]));
		return ret;
	}
	
	if (strcmp(pcParam[i] , "G")==0) // 外网端口
	{
		param.outside_port = atoi(pcValue[i]);  return ret;
		
	}
	if (strcmp(pcParam[i] , "J")==0) // 上报时间间隔
	{
		time = atoi(pcValue[i]);  return ret;
	}
	
	if (strcmp(pcParam[i] , "H")==0) // 升级地址
	{
		sscanf(pcValue[i],"%d.%d.%d.%d",&temp[0],&temp[1],&temp[2],&temp[3]);
		ip[0] = temp[0];
		ip[1] = temp[1];
		ip[2] = temp[2];
		ip[3] = temp[3];
		return ret;
	}
	
	if (strcmp(pcParam[i] , "I") == 0) { // 端口
		port = atoi(pcValue[i]);
		
		update_set_update_addr(ip,port);
		app_set_com_time_param_function(&time,1);
		app_set_remote_network_function(param);
	}

	return ret;
}

/************************************************************
*
* Function name	: httpd_cgi_set_remote_ip_function
* Description	: 远端网络设置
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t httpd_cgi_set_remote_ip_function(int iNumParams, char *pcParam[], char *pcValue[])
{
	uint8_t i   = 0;
	int8_t  ret = 0;
	
	if (strcmp(pcValue[0] , "remote_save")==0)
	{
		for (i=1; i< (iNumParams); i++)
		{
			ret = Setting_remote_network_function(pcParam,pcValue,i);
			if(ret != 0)
			{
				set_return_status_function(PARAMETER_ERROR_NUM,(uint8_t*)PARAMETER_ERROR_STR);
				break;
			}
			else
			{
				if(i == iNumParams-1) {
					set_return_status_function(0,(uint8_t*)"\"SUCCESS!\"");
					OSTimeDlyHMSM(0,0,0,100);	
				}
			}
		}
		
		return 0;
	}
	
	return -1;
	
}

/************************************************************
*
* Function name	: httpd_cgi_update_function
* Description	: 更新函数
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t httpd_cgi_update_function(int iNumParams, char *pcParam[], char *pcValue[])
{
	if (strcmp(pcValue[0] , "updatelwip")==0) 
	{
		if( update_detection_status_function() != UPDATE_MODE_NULL) {
			set_return_status_function(0,(uint8_t*)"\"UPDATING!\"");
		} 
		else 
		{
			update_set_update_mode(UPDATE_MODE_LWIP);
			set_return_status_function(0,(uint8_t*)"\"SUCCESS!\"");
		}
		return 0;
	}
	
	return -1;
}

/************************************************************
*
* Function name	: httpd_cgi_system_function
* Description	: 系统相关设置
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t httpd_cgi_system_function(int iNumParams, char *pcParam[], char *pcValue[])
{
	/* 恢复出厂设置 */
	if (strcmp(pcValue[0] , "reset")==0)
	{
		app_set_reset_function();
		set_return_status_function(0,(uint8_t*)"\"SUCCESS!\"");
		OSTimeDlyHMSM(0,0,0,100);
		return 0;
	}
	
	/* 系统重启 */
	if (strcmp(pcValue[0] , "reboot")==0)
	{
		set_return_status_function(0,(uint8_t*)"\"SUCCESS!\"");
		set_reboot_time_function(1000);
		return 0;
	}
	
	/* 擦除W25Q128 */
	if (strcmp(pcValue[0] , "eacres")==0)
	{
		set_return_status_function(0,(uint8_t*)"\"SUCCESS!\"");
		W25QXX_Erase_Chip();
		set_reboot_time_function(1000);
		return 0;
	}

	/* 读取基站定位信息 */
	if (strcmp(pcValue[0] , "lbsgps")==0)
	{
		set_return_status_function(0,(uint8_t*)"\"SUCCESS!\"");
		gsm_run_gps_task_function();
		return 0;
	}
	
	if ( strcmp(pcValue[0] , "loginInit")==0 ) {
		set_return_status_function(0,(uint8_t*)"[\"root\",\" \"]");
		return 0;
	}
	
	return -1;
}

/************************************************************
*
* Function name	: httpd_cgi_show_function
* Description	: 显示内容
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t httpd_cgi_show_function(char *pcValue[], uint16_t *data, uint8_t *buff)
{
	if ( strcmp(pcValue[0] , "VACollection")==0 ) { // 电压 电流
		*data = 0;
		httpd_ssi_volt_cur_data_collection_function((char*)buff);
		return 0;
	}
	
	if ( strcmp(pcValue[0] , "PowerCollection")==0 ) {  // 功率
		*data = 0;
		httpd_ssi_power_data_collection_function((char*)buff);
		return 0;
	}
	
	if ( strcmp(pcValue[0] , "otherCollection")==0 ) {  // 其他参数
		*data = 0;
		httpd_ssi_other_data_collection_function((char*)buff);
		return 0;
	}
	
	if ( strcmp(pcValue[0] , "ThresholdData")==0 ) {  // 阈值
		*data = 0;
		httpd_ssi_threshold_seting_function((char*)buff);
		return 0;
	}		
	
	/* 更新开关状态 */
	if ( strcmp(pcValue[0] , "switchStatus")==0 ) {
		*data = 0;
		httpd_ssi_switch_status_function((char*)buff);
		return 0;
	}
	/* 系统状态 */
	if ( strcmp(pcValue[0] , "systemStatus")==0 ) {
		*data = 0;
		httpd_ssi_system_status_function((char*)buff);
		return 0;
	}
	/* 系统设置 */
	if ( strcmp(pcValue[0] , "systemSetting")==0 ) {
		*data = 0;
		httpd_ssi_system_seting_function((char*)buff);
		return 0;
	}
	/* 无线网络信息 */
	if ( strcmp(pcValue[0] , "gprsSetting")==0 ) {
		*data = 0;
		httpd_ssi_nework_gprs_show_function((char*)buff);
		return 0;
	}
	/* 网络信息 */
	if ( strcmp(pcValue[0] , "networkSetting")==0 ) {
		*data = 0;
		httpd_ssi_network_setting_function((char*)buff);
		return 0;
	}
	/* 摄像头IP */
	if ( strcmp(pcValue[0] , "otherSetting")==0 ) {
		*data = 0;
		httpd_ssi_other_setting_function((char*)buff);
		return 0;
	}
	/* 服务器信息 */
	if ( strcmp(pcValue[0] , "serverset")==0 ) {
		*data = 0;
		http_ssi_server_setting_function((char*)buff);
		return 0;
	}
	
	return -1;
		
}

