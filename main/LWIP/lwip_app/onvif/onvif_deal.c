#include "onvif_agree.h"
#include "onvif_digest.h"
#include "malloc.h"
#include "onvif_deal.h"
#include <lwip/sockets.h>
#include "lwip/opt.h"
#include "onvif.h"
#include "onvif_tcp.h"
#include "onvif_prefixlen.h"

IPC_Param_t sg_ipc_param;
Onvif_OSD_Param_t osd_params;

int onvif_send(int sock_t,char *onvif_cmd,uint8_t sort,char *ip,int port)
{
	char *onvif_send_data;
	int str_len    = 0;

	onvif_send_data = (char *)mymalloc(SRAMIN,1700);  // 申请内存
	memset(onvif_send_data,0,1700);
	str_len = onvif_agreement_create(onvif_send_data,onvif_cmd,ip,port,sort);
	send(sock_t, onvif_send_data, str_len, 0);        // socket数据发送
	
//	printf("%d...........%s\r\n", str_len,onvif_send_data);
	
	myfree(SRAMIN,onvif_send_data);   // 释放内存
	return 0;
}

/*根据str 取出xml中相应的参数*/
int ONVIF_IPC_GetParam_FUN(char *data,char *sStr,char *param)
{
  char *str  = NULL;
	str = strstr(data,sStr);
	if(!str)
	    return -1;
	sscanf(str,"%*[^>]>%[^</]",param);
  return 0;
}

/*校验是否成功*/
int ONVIF_IPC_ChkData_FUN(char *data)
{
	char *str = NULL;
	char buf[100] = {0};
	
	str = strstr(data,"HTTP/1.1 200 OK");    /*SOAP-ENV:Sender*/
	if(str == NULL)      // 未找到指定字符串     
	{
//	  printf("ChkData failed ... data: %s \r\n ",data);
		Get_Str_Between(data,buf,"<env:Text xml:lang=\"en\">","</env:Text>");
//		printf("ChkData failed ... data: %s \r\n ",buf);
		return -1;
	}
  return 0;
}
/*读取两个字符之间的字符串*/
int Get_Str_Between(char *pcBuf, char *pcRes,char *begin,char *end)
{
	char *pcBegin = NULL;
	char *pcEnd = NULL;
 
	pcBegin = strstr(pcBuf, begin);
	pcEnd = strstr(pcBuf, end);
 
	if(pcBegin == NULL || pcEnd == NULL || pcBegin > pcEnd)
	{
//		printf("Mail name not found!\n");
		return -1;
	}
	else
	{
		pcBegin += strlen(begin);
		memcpy(pcRes, pcBegin, pcEnd-pcBegin);
	}
	return 0;
}

/*获取设备参数*/
int ONVIF_IPC_GetDevice_API(int socket_t,char *buff,char *ip,int port)
{
	int ret    = 0;
	char *rcvData;
	int  recv_len = 0;	

	onvif_send(socket_t,Get_DEVICE_INFO,0,ip,port);     // 发送数据
	rcvData = (char *)mymalloc(SRAMIN,TCP_RX_BUFSIZE);  // 申请内存
	memset(rcvData,0,TCP_RX_BUFSIZE);	
	
	OSTimeDlyHMSM(0,0,0,200);  //延时5ms
	recv_len = recv(socket_t,rcvData, TCP_RX_BUFSIZE, 0);	/* 接收并打印响应的数据，使用加密数据传输 */
	recv_len = recv(socket_t,rcvData+recv_len, TCP_RX_BUFSIZE-recv_len, 0);	/* 接收并打印响应的数据，使用加密数据传输 */	
	
//	printf("device_over.....  \n");	

	ret = Get_IPC_Device_Info(rcvData);             // 处理数据，获得设备参数
	myfree(SRAMIN,rcvData);   											// 释放内存
  return ret;
}

int Get_IPC_Device_Info(char *buf)
{
	int ret    = 0;
	if(ONVIF_IPC_ChkData_FUN(buf) == 0)
	{
		if(ONVIF_IPC_GetParam_FUN(buf,"<tds:Manufacturer>",sg_ipc_param.device.manufacturer) < 0) // 获取摄像机品牌名称
			ret  =  -2;

		if(ONVIF_IPC_GetParam_FUN(buf,"<tds:Model>",sg_ipc_param.device.models) < 0)
			ret  =  -3;
		
		if(ONVIF_IPC_GetParam_FUN(buf,"<tds:FirmwareVersion>",sg_ipc_param.device.fire_version) < 0)
			ret  =  -4;

		if(ONVIF_IPC_GetParam_FUN(buf,"<tds:SerialNumber>",sg_ipc_param.device.serial_num) < 0)
			ret  =  -5;

		if(ONVIF_IPC_GetParam_FUN(buf,"<tds:HardwareId>",sg_ipc_param.device.hardware_id) < 0)
			ret  =  -6;		
	}
	else
		ret = -1;	
	return ret;
}


/*获取网络接口参数*/
int ONVIF_IPC_Network_API(int socket_t,char *buff,char *ip,int port)
{
	int ret    = 0;
	char *rcvData;
	int  recv_len = 0;	
	
	onvif_send(socket_t,Get_NETWORK_INTER,0,ip,port);     // 发送数据
	rcvData = (char *)mymalloc(SRAMIN,TCP_RX_BUFSIZE);  // 申请内存
	memset(rcvData,0,TCP_RX_BUFSIZE);	
	
	OSTimeDlyHMSM(0,0,0,200);   //延时5ms
	recv_len = recv(socket_t,rcvData, TCP_RX_BUFSIZE, 0);	/* 接收并打印响应的数据，使用加密数据传输 */
	recv_len = recv(socket_t,rcvData+recv_len, TCP_RX_BUFSIZE-recv_len, 0);	/* 接收并打印响应的数据，使用加密数据传输 */
//	printf("network_over.....  \n");	
//	printf("...........%s\r\n",rcvData);
	ret = Get_IPC_Network_Info(rcvData);             // 处理数据，获得设备参数
	myfree(SRAMIN,rcvData);   											// 释放内存

  return ret;
}

int Get_IPC_Network_Info(char *buf)
{
	int ret    = 0;
	char prefixlen_str[5]={0};
	char ip_buffer[40] = {0};
	int    	temp[5] 	 = {0};
	char *str  = NULL;
	
	if(ONVIF_IPC_ChkData_FUN(buf) == 0)
	{
		if(ONVIF_IPC_GetParam_FUN(buf,"<tt:PrefixLength>",prefixlen_str) < 0) // 获取摄像机IP
			ret  =  -2;
		else
		{
			ipv4_prefixlen2str(atoi(prefixlen_str),sg_ipc_param.network.netmask);
		}
		if(ONVIF_IPC_GetParam_FUN(buf,"<tt:Address>",ip_buffer) < 0) // 获取摄像机IP
			ret  =  -3;
		else
		{
			sscanf(ip_buffer,"%d.%d.%d.%d",&temp[0],&temp[1],&temp[2],&temp[3]);
			sg_ipc_param.network.ip[0] = temp[0];
			sg_ipc_param.network.ip[1] = temp[1];
			sg_ipc_param.network.ip[2] = temp[2];
			sg_ipc_param.network.ip[3] = temp[3];	
		}
		if(strcmp(sg_ipc_param.device.manufacturer,"HIKVISION") == 0) // 海康
		{	
			str = strstr(buf,"<tt:LinkLocal>");
			memset(temp,0,sizeof(temp));
			if(ONVIF_IPC_GetParam_FUN(str,"<tt:Address>",ip_buffer) < 0) // 获取摄像机IP
				ret  =  -3;
			else
			{
				sscanf(ip_buffer,"%x::%x:%x:%x:%x",&temp[0],&temp[1],&temp[2],&temp[3],&temp[4]);
				sg_ipc_param.network.ip6[0] = temp[0];
				sg_ipc_param.network.ip6[1] = temp[1];
				sg_ipc_param.network.ip6[2] = temp[2];
				sg_ipc_param.network.ip6[3] = temp[3];	
				sg_ipc_param.network.ip6[4] = temp[4];	
			}
		}
		else
			memset(sg_ipc_param.network.ip6,0,sizeof(sg_ipc_param.network.ip6));
	}
	else
		ret = -1;	
	return ret;
}


/*获取网关参数*/
int ONVIF_IPC_Gateway_API(int socket_t,char *buff,char *ip,int port)
{	
	int ret    = 0;
	char *rcvData;
	int  recv_len = 0;	

	onvif_send(socket_t,Get_NETWORK_GATEWAY,0,ip,port);     // 发送数据
	rcvData = (char *)mymalloc(SRAMIN,TCP_RX_BUFSIZE);  // 申请内存
	memset(rcvData,0,TCP_RX_BUFSIZE);	
	OSTimeDlyHMSM(0,0,0,200);   //延时5ms
	recv_len = recv(socket_t,rcvData, TCP_RX_BUFSIZE, 0);	/* 接收并打印响应的数据，使用加密数据传输 */	
	recv_len = recv(socket_t,rcvData+recv_len, TCP_RX_BUFSIZE-recv_len, 0);
	
//	printf("gateway_over.....  \n");	
//	printf("...........%s\r\n", rcvData);
	ret = Get_IPC_Gateway_Info(rcvData);             // 处理数据，获得设备参数
	myfree(SRAMIN,rcvData);   											// 释放内存
  return ret;
}

int Get_IPC_Gateway_Info(char *buf)
{
	int ret    = 0;
	char gateway_str[20];
	int    			temp[4] 	= {0};
	if(ONVIF_IPC_ChkData_FUN(buf) == 0)
	{
		if(ONVIF_IPC_GetParam_FUN(buf,"<tt:IPv4Address>",gateway_str) < 0) // 获取网关
			ret  =  -2;
		else
		{
			sscanf(gateway_str,"%d.%d.%d.%d",&temp[0],&temp[1],&temp[2],&temp[3]);
			sg_ipc_param.network.gateway[0] = temp[0];
			sg_ipc_param.network.gateway[1] = temp[1];
			sg_ipc_param.network.gateway[2] = temp[2];
			sg_ipc_param.network.gateway[3] = temp[3];			
		}
	}
	else
		ret = -1;	
	return ret;
}

/* 获取Configurations token */
int ONVIF_IPC_Token_API(int socket_t,char *buff,char *ip,int port)
{
	int ret    = 0;
	char *rcvData;
	int  recv_len = 0;	

	onvif_send(socket_t,Get_CONFIG_TOKEN,1,ip,port);     // 发送数据
	
	rcvData = (char *)mymalloc(SRAMIN,TCP_RX_BUFSIZE);  // 申请内存
	memset(rcvData,0,TCP_RX_BUFSIZE);	
	OSTimeDlyHMSM(0,0,0,200);   //延时5ms
	recv_len = recv(socket_t,rcvData, TCP_RX_BUFSIZE, 0);	/* 接收并打印响应的数据，使用加密数据传输 */
	recv_len = recv(socket_t,rcvData+recv_len, TCP_RX_BUFSIZE-recv_len, 0);
//	printf("token_over.....  \n");	
//	printf("%d...........%s\r\n",ret,rcvData);
	ret = Get_IPC_Config_Token(rcvData);             // 处理数据，获得设备参数
	myfree(SRAMIN,rcvData);   											// 释放内存

  return ret;
}
int Get_IPC_Config_Token(char *buf)
{
	int ret    = 0;
	char token[30] = {0};
	char *str_data = NULL;
	
	if(ONVIF_IPC_ChkData_FUN(buf) == 0)
	{
		str_data = strstr(buf,"GetVideoSourceConfigurationsResponse");
		str_data = strstr(str_data,"token");		
		
		if(Get_Str_Between(str_data,token,"=\"","\">")<0)// 获取信息
			ret  =  -2;		
		else
		{
			sprintf((char*)sg_ipc_param.OSD_info.config_token,"%s",token);
//			printf("%s\n",sg_ipc_param.OSD_info.config_token);  // 16进制打印
		}
	}
	else
		ret = -1;	
	return ret;
}
/* 获取OSD标注 */
int ONVIF_IPC_OSD_API(int socket_t,char *buff,char *ip,int port)
{
	int ret    = 0;
	char *rcvData;
	int  recv_len = 0;	
	recv_len = recv_len;
	
	onvif_send(socket_t,Get_OSD_INFO,1,ip,port);     // 发送数据
	
	rcvData = (char *)mymalloc(SRAMIN,TCP_RX_BUFSIZE);  // 申请内存
	memset(rcvData,0,TCP_RX_BUFSIZE);	
	OSTimeDlyHMSM(0,0,0,200);   //延时5ms

	if(strcmp(sg_ipc_param.device.manufacturer,"Tiandy Tech") == 0) // 天地伟业摄像机需要添加TOKEN信息
	{
//		printf("tiandiweye........\n");
		recv_len = recv(socket_t,rcvData, TCP_RX_BUFSIZE, 0);	/* 接收并打印响应的数据，使用加密数据传输 */
		recv_len = recv(socket_t,rcvData+500, TCP_RX_BUFSIZE, 0);	
	}
	else
	{
		recv_len = recv(socket_t,rcvData, TCP_RX_BUFSIZE, 0);	/* 接收并打印响应的数据，使用加密数据传输 */
		recv_len = recv(socket_t,rcvData+1500, TCP_RX_BUFSIZE, 0);
	}
//	printf("osd_read_over.....  \n");	
//	printf("%d...........%s\r\n",ret,rcvData);
	ret = Get_IPC_OSD_Info(rcvData);             // 处理数据，获得设备参数
	myfree(SRAMIN,rcvData);   									 // 释放内存

  return ret;
}

int Get_IPC_OSD_Info(char *buf)
{
	int ret    = 0;
	char osd_name[30] = {0};
	char *str_buff = NULL;

	if(ONVIF_IPC_ChkData_FUN(buf) == 0)
	{
		if(strcmp(sg_ipc_param.device.manufacturer,"Tiandy Tech") == 0)  // 天地伟业 第一个<tt:PlainText>为空
		{	
			str_buff = strstr(buf,"<tt:PlainText>");
			str_buff = strstr(str_buff+20,"<tt:PlainText>");  // 搜索第二个字符串
		}
		else
			str_buff = strstr(buf,"<tt:PlainText>");
		if(Get_Str_Between(str_buff,osd_name,"<tt:PlainText>","</tt:PlainText>")<0)// 获取信息
//		if(ONVIF_IPC_GetParam_FUN(buf,"<tt:PlainText>",osd_name)<0)// 获取信息
			ret  =  -2;		
		else
		{
			sprintf((char*)sg_ipc_param.OSD_info.name,"%s",osd_name);
			sprintf((char*)osd_params.name,"%s",osd_name);
//			for(uint8_t i=0;i<20;i++)
//				printf("%x",(uint16_t)osd_name[i]);  // 16进制打印
//			printf("\r\n");
		}
	}
	else
		ret = -1;	
	return ret;
}

/*获取时间*/
int ONVIF_IPC_Time_API(int socket_t,char *buff,char *ip,int port)
{
	int ret    = 0;
	char *rcvData;
	int  recv_len = 0;	

	onvif_send(socket_t,Get_DATE_TIME,0,ip,port);     // 发送数据
	
	rcvData = (char *)mymalloc(SRAMIN,TCP_RX_BUFSIZE);  // 申请内存
	memset(rcvData,0,TCP_RX_BUFSIZE);	
	OSTimeDlyHMSM(0,0,0,200);  //延时5ms
	recv_len = recv(socket_t,rcvData, TCP_RX_BUFSIZE, 0);	/* 接收并打印响应的数据，使用加密数据传输 */	
	recv_len = recv(socket_t,rcvData+recv_len, TCP_RX_BUFSIZE-recv_len, 0);
//	printf("tim_read_over.....  \n");	
//	printf("%d...........%s\r\n", len,rcvData);
	ret = Get_IPC_Time_Info(rcvData);             // 处理数据，获得设备参数
	myfree(SRAMIN,rcvData);   											// 释放内存

  return ret;
}

int Get_IPC_Time_Info(char *buf)
{
	int ret    = 0;
	char time[10]= {0};
	if(ONVIF_IPC_ChkData_FUN(buf) == 0)
	{
		if(ONVIF_IPC_GetParam_FUN(buf,"<tt:Year>",time) < 0) // 获取时间	
			ret  =  -2;
		else
			sg_ipc_param.times.year = atoi(time);

		if(ONVIF_IPC_GetParam_FUN(buf,"<tt:Month>",time) < 0)
			ret  =  -3;
		else
			sg_ipc_param.times.mon = atoi(time);
		
		if(ONVIF_IPC_GetParam_FUN(buf,"<tt:Day>",time) < 0)
			ret  =  -4;
		else
			sg_ipc_param.times.day = atoi(time);

		if(ONVIF_IPC_GetParam_FUN(buf,"<tt:Hour>",time) < 0)
			ret  =  -5;
		else
			sg_ipc_param.times.hour = atoi(time);

		if(ONVIF_IPC_GetParam_FUN(buf,"<tt:Minute>",time) < 0)
			ret  =  -6;		
		else
			sg_ipc_param.times.min = atoi(time);

		if(ONVIF_IPC_GetParam_FUN(buf,"<tt:Second>",time) < 0)
			ret  =  -6;		
		else
			sg_ipc_param.times.sec = atoi(time);
	}
	else
		ret = -1;	
	return ret;
}

/*设备重启*/
int ONVIF_IPC_Reboot_API(int socket_t,char *buff,char *ip,int port)
{
	int ret    = 0;
	char *rcvData;

	onvif_send(socket_t,DEVICE_REBOOT,0,ip,port);     // 发送数据
	
	rcvData = (char *)mymalloc(SRAMIN,TCP_RX_BUFSIZE);  // 申请内存
	memset(rcvData,0,TCP_RX_BUFSIZE);	
	OSTimeDlyHMSM(0,0,0,200);  //延时5ms
	recv(socket_t,rcvData, TCP_RX_BUFSIZE, 0);	/* 接收并打印响应的数据，使用加密数据传输 */	
//	printf("reboot over.....2 \n");	
//	printf("%d...........%s\r\n", len,rcvData);
	ret = Get_IPC_Reboot_Info(rcvData);             // 处理数据，获得设备参数
	myfree(SRAMIN,rcvData);   											// 释放内存

  return ret;
}

int Get_IPC_Reboot_Info(char *buf)
{
	int ret    = 0;
	char message[100] = {0};
	if(ONVIF_IPC_ChkData_FUN(buf) == 0)
	{
		if(Get_Str_Between(buf,message,"<tds:Message>","</tds:Message>")<0)// 获取信息
			ret  =  -2;
		else
		{
			ret  =  0;
//			printf("reboot:%s\r\n",message);
		}
	}
	else
		ret = -1;	
	return ret;
}



/*时间同步*/
int ONVIF_IPC_SetTime_API(int socket_t,char *buff,char *ip,int port)
{	
	int ret    = 0;
	char *rcvData;

	onvif_send(socket_t,Set_DATE_TIME,0,ip,port);     // 发送数据

	rcvData = (char *)mymalloc(SRAMIN,TCP_RX_BUFSIZE);  // 申请内存
	memset(rcvData,0,TCP_RX_BUFSIZE);	
		
	OSTimeDlyHMSM(0,0,0,200);  //延时5ms
	recv(socket_t,rcvData, TCP_RX_BUFSIZE, 0);	/* 接收并打印响应的数据，使用加密数据传输 */
//	printf("settime over..... \n");	
//	printf("%d...........%s\r\n", len,rcvData);
	ret = Get_IPC_SetTime_Info(rcvData);             // 处理数据，获得设备参数
	myfree(SRAMIN,rcvData);   											// 释放内存

  return ret;
}

int Get_IPC_SetTime_Info(char *buf)
{
	int ret    = 0;
	if(ONVIF_IPC_ChkData_FUN(buf) == 0)
	{
	}
	else
		ret = -1;	
	return ret;
}

/****************************************************************************
* 名    称: onvif_get_device_param_function
* 功    能：返回IPC设备信息：
* 入口参数：
* 返回参数：sg_ipc_param.device
* 说    明： 
****************************************************************************/
void *onvif_get_device_param_function(void)
{
	return &sg_ipc_param.device;
}
void *onvif_get_network_param_function(void)
{
	return &sg_ipc_param.network;
}
void *onvif_get_times_param_function(void)
{
	return &sg_ipc_param.times;
}
void *onvif_get_osd_param_function(void)
{
	return &sg_ipc_param.OSD_info;
}

void onvif_get_token_function(char *buff)
{
	sprintf(buff,"%s",sg_ipc_param.OSD_info.config_token);
}

void onvif_set_times(uint32_t timess)
{
	sg_ipc_param.times.timess = timess;
}
uint32_t onvif_get_times(void)
{
	return sg_ipc_param.times.timess;
}
