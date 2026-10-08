#include "onvif.h"
#include "onvif_tcp.h"
#include "lwip_comm.h"
#include "led.h"
#include "com.h"
#include "start.h"
#include "malloc.h"
#include "string.h"
#include "app.h"
#include <lwip/sockets.h>
#include "onvif_digest.h"
#include "onvif_agree.h"
#include "onvif_deal.h"

#define ONVIF_TCP_PRIO		14 				//	TCP客户端任务
#define ONVIF_TCP_STK_SIZE	256 	//任务堆栈
__align(8) static OS_STK ONVIF_TCP_TASK_STK[ONVIF_TCP_STK_SIZE];//任务堆栈

int onvif_sockt = -1;	
onvif_tcp_t sg_onvif_tcp;  // TCP状态标志位
ipc_flag_t sg_ipc_flag;
char ip_buff[20] = {0};

//udp任务函数
static void onvif_tcp_thread(void *arg)
{

	struct sockaddr_in server_addr;
	struct timeval tv_out;
	char onvif_ipbuf[20] ={0};
	int  onvif_port  = 0;

	sg_ipc_flag.error_flag = 0xFF;
	if(ONVIF_DEBUG) printf("create onvif_tcp_thread \r\n");
	onvif_get_ipc_ip_str(onvif_ipbuf);          // 获取地址
	onvif_port = onvif_get_ipc_port_str();  // 获取端口
		
	if(onvif_get_ipc_net_status() == 1) // 判断当前摄像机网络状态，网络正常时才进行连接
		sg_onvif_tcp.tcp_status = 1;  		// 允许连接
	else
		sg_onvif_tcp.tcp_status = 0;  		// 

	memset(&sg_ipc_param,0,sizeof(IPC_Param_t));  // 清除上次信息	
	while(1)
	{
		if(sg_onvif_tcp.tcp_status == 0)
		{
			sg_ipc_flag.status = 4;  // 信息读取失败，网络不通
		}			
		else if(sg_onvif_tcp.tcp_status == 1)
		{
			onvif_sockt = socket(AF_INET, SOCK_STREAM, 0); // SOCK_STREAM:提供面向连接的稳定数据传输，即TCP协议 
			if (onvif_sockt < 0)
			{
				if(ONVIF_DEBUG) printf("Socket error\n");
				close(onvif_sockt);
			}		
			else
			{
				/* 设置要访问的服务器的信息 */
				server_addr.sin_family = AF_INET;                			// IPv4
				server_addr.sin_addr.s_addr = inet_addr(onvif_ipbuf); // 服务器IP
				server_addr.sin_port = htons(onvif_port);         		// 端口
				memset(&(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));
				
				/* 连接到服务端 */
				if (connect(onvif_sockt, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in)))
				{
					if(ONVIF_DEBUG) printf("Unable to connect\n");
					close(onvif_sockt);
					return;
				}			
				tv_out.tv_sec = 5;
				tv_out.tv_usec = 0;
				setsockopt(onvif_sockt, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof(tv_out));
				
				sg_onvif_tcp.tcp_status = 2;
				if(ONVIF_DEBUG) printf("connect...success... \n");
			}
		}
		else if(sg_onvif_tcp.tcp_status == 2)			
		{
			if(sg_ipc_flag.cmd == READ_DEVICE_INFO)  // 读取设备参数
			{
				for(uint8_t i=0; i< ERROR_MAX_NUM;i++)			// 获取设备相关信息
				{
					if(ONVIF_DEBUG) printf("ONVIF_IPC_GetDevice_API... \n");
					if(ONVIF_IPC_GetDevice_API(onvif_sockt,ip_buff,onvif_ipbuf,onvif_port)<0)
					{
						sg_ipc_flag.error_flag |= DEVICE_ERROR;
						sg_ipc_flag.error_count[ONVIF_DEVICE] ++;
					}
					else
					{
						sg_ipc_flag.error_flag &=~DEVICE_ERROR;	
						break;
					}
				}
				//获取网关信息
				for(uint8_t i=0; i< ERROR_MAX_NUM;i++)
				{
					if(ONVIF_DEBUG) printf("ONVIF_IPC_Gateway_API... \n");
					if(ONVIF_IPC_Gateway_API(onvif_sockt,ip_buff,onvif_ipbuf,onvif_port)<0)
					{
						sg_ipc_flag.error_flag |= GATEWAY_ERROR;
						sg_ipc_flag.error_count[ONVIF_GATEWAY] ++;
					}
					else
					{
						sg_ipc_flag.error_flag &=~GATEWAY_ERROR;
						break;
					}
				}			
				//获取网络接口信息
				for(uint8_t i=0; i< ERROR_MAX_NUM;i++)
				{
					if(ONVIF_DEBUG) printf("ONVIF_IPC_Network_API... \n");
					if(ONVIF_IPC_Network_API(onvif_sockt,ip_buff,onvif_ipbuf,onvif_port)<0)
					{
						sg_ipc_flag.error_flag |= NETWORK_ERROR;
						sg_ipc_flag.error_count[ONVIF_NETWORK] ++;
					}
					else
					{
						sg_ipc_flag.error_flag &=~NETWORK_ERROR;	
						break;
					}
				}					
				for(uint8_t i=0; i< ERROR_MAX_NUM;i++)// 获取OSD相关信息
				{
					if(ONVIF_DEBUG) printf("ONVIF_IPC_Token_API... \n");
					if(ONVIF_IPC_Token_API(onvif_sockt,ip_buff,onvif_ipbuf,onvif_port)<0)
					{
						sg_ipc_flag.error_flag |= TOKEN_ERROR;
						sg_ipc_flag.error_count[ONVIF_TOKEN] ++;
					}
					else
					{
						sg_ipc_flag.error_flag &=~TOKEN_ERROR;	
						break;
					}
				}	
				for(uint8_t i=0; i< ERROR_MAX_NUM;i++)// 获取OSD相关信息
				{
					if(ONVIF_DEBUG) printf("ONVIF_IPC_OSD_API... \n");
					if(ONVIF_IPC_OSD_API(onvif_sockt,ip_buff,onvif_ipbuf,onvif_port)<0)
					{
						sg_ipc_flag.error_flag |= OSD_ERROR;
						sg_ipc_flag.error_count[ONVIF_OSD] ++;
					}
					else
					{
						sg_ipc_flag.error_flag &=~OSD_ERROR;	
						break;
					}
				}				

				for(uint8_t i=0; i< ERROR_MAX_NUM;i++)			// 获取时间
				{
					if(ONVIF_DEBUG) printf("ONVIF_IPC_Time_API... \n");
					if(ONVIF_IPC_Time_API(onvif_sockt,ip_buff,onvif_ipbuf,onvif_port)<0)
					{
						sg_ipc_flag.error_flag |= GET_TIME_ERROR;
						sg_ipc_flag.error_count[ONVIF_TIMEGET] ++;
					}
					else
					{
						sg_ipc_flag.error_flag &=~GET_TIME_ERROR;	
						break;
					}
				}	
				if((sg_ipc_flag.error_flag &0x1F)== 0)
				{
					if(ONVIF_DEBUG) printf("success \r\n");
					close(onvif_sockt);
					sg_ipc_flag.cmd = 0;
					sg_ipc_flag.status = 2;  // 信息读取成功
				}
				else
				{
					if(ONVIF_DEBUG) printf("fault \r\n");
					close(onvif_sockt);
					sg_ipc_flag.cmd = 0;
					sg_ipc_flag.status = 3;  // 信息读取失败
				}	
			}	
			
		}
		OSTimeDlyHMSM(0,0,0,200);  //延时5ms
	}
}

//创建UDP线程
//返回值:0 UDP创建成功
//		其他 UDP创建失败
INT8U onvif_tcp_client_init(void)
{
	INT8U res;
	OS_CPU_SR cpu_sr;
	
	OS_ENTER_CRITICAL();	//关中断
	res = OSTaskCreate(onvif_tcp_thread,(void*)0,(OS_STK*)&ONVIF_TCP_TASK_STK[ONVIF_TCP_STK_SIZE-1],ONVIF_TCP_PRIO); //创建UDP线程
	OS_EXIT_CRITICAL();		//开中断
	
	return res;
}

/************************************************************
*
* Function name	: onvif_tcp_stop
* Description	: tcp客户端停止函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void onvif_tcp_client_stop(void)
{
	OS_CPU_SR cpu_sr;
	OS_ENTER_CRITICAL();		// 关中断
//	close(onvif_sockt);
	sg_onvif_tcp.tcp_status = 0;  // 状态清零
	OSTaskDel(ONVIF_TCP_PRIO);	 // 删除TCP任务
	OS_EXIT_CRITICAL();			// 开中断
}


/****************************************************************************
* 名    称: set_ipc_device_cmd
* 功    能：设置 IPC 命令：
* 入口参数：cmd 命令
* 返回参数：无
* 说    明：1读取信息，2重启，3设置时间
****************************************************************************/
void set_ipc_device_cmd(uint8_t cmd)
{
	sg_ipc_flag.cmd = cmd;
}
/****************************************************************************
* 名    称: set_ipc_read_status
* 功    能：返回读取的状态：
* 入口参数：
* 返回参数：sg_ipc_flag.status
* 说    明：0正在读取，1读取成功，2失败
****************************************************************************/
void set_ipc_read_status(uint8_t dat)
{
	sg_ipc_flag.status = dat;
}
/****************************************************************************
* 名    称: get_ipc_read_status
* 功    能：返回读取的状态：
* 入口参数：
* 返回参数：sg_ipc_flag.status
* 说    明：0正在读取，1读取成功，2失败
****************************************************************************/
uint8_t get_ipc_read_status(void)
{
	return sg_ipc_flag.status;
}

/****************************************************************************
* 名    称: onvif_get_ipc_ip_str
* 功    能：获取IPC IP地址：
* 入口参数：
* 返回参数： 
* 说    明： 
****************************************************************************/
void onvif_get_ipc_ip_str(char *buff)
{
	uint8_t ip[4]     = {0};
	uint8_t num = app_get_camera_num_function();
	if(app_get_camera_function(ip,num) < 0)
	{}
	else
	{
		sprintf(buff,"%d.%d.%d.%d", ip[0],ip[1],ip[2],ip[3]);
	}
}
/****************************************************************************
* 名    称: onvif_get_ipc_port_str
* 功    能：获取IPC IP端口：
* 入口参数：
* 返回参数： 
* 说    明： 
****************************************************************************/
int onvif_get_ipc_port_str(void)
{
	uint8_t num = app_get_camera_num_function();
	return app_get_camera_port_function(num);
}

/****************************************************************************
* 名    称: onvif_get_ipc_net_status
* 功    能：获取IPC 网络状态：
* 入口参数：
* 返回参数： 
* 说    明： 
****************************************************************************/
uint8_t onvif_get_ipc_net_status(void)
{
	uint8_t num = app_get_camera_num_function();
	return com_report_get_camera_status(num);
}

/***************************
函数名:Hex2Ascii
功能描述:把16进制转Ascii字符
参数：16进制
返回：Ascii字符
***************************/
unsigned char Ascii2Hex( unsigned char bAscii )		
{
	unsigned char bHex = 0;
	if( ( bAscii >= '0' ) && ( bAscii <= '9' ) )
		bHex =  bAscii - '0';
	else if( ( bAscii >= 'A' ) && ( bAscii <= 'F' ) )
		bHex = bAscii - '7';
	else if( ( bAscii >= 'a' ) && ( bAscii <= 'f' ) )
		bHex = bAscii - 0x57;
	else
		bHex = 0xff;
	return bHex;
}

int tcp_http_test(int sockfd) // 百度网页测试
{
	int len=0;
	char *send_data = "GET / HTTP/1.1\r\n\r\n";
	char rcvData[512];

	send(sockfd, send_data, 18, 0); 
	len = recv(sockfd, rcvData, TCP_RX_BUFSIZE, 0);	/* 接收并打印响应的数据，使用加密数据传输 */
	if(len > 0)
	{
		if(ONVIF_DEBUG) printf("%s \r\n", rcvData);
	}
	return len;
}
