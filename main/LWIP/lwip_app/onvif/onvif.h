#ifndef __ONVIF_H
#define __ONVIF_H
#include "sys.h"
#include "includes.h"
#include "lwip/api.h"
#include "lwip/lwip_sys.h"
#include "lwip/netif.h"
#include "lwip/ip.h"
#include "lwip/tcp.h"
#include "lwip/init.h"
#include "netif/etharp.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/igmp.h"
#include "lwip/sockets.h"

#define MULTICAST_ADDR    "239.255.255.250"  // IPC组播IP地址
#define MULTICAST_PROT    3702            	 // IPC组播端口号
#define HIKVISION_MULTICAST_PROT    37020  	 // IPC组播端口号

#define DAHUA_MULTICAST_ADDR    "239.255.255.251"  // IPC组播IP地址
#define DAHUA_MULTICAST_PROT    37810  	 // IPC组播端口号
#define TEST_LOCAL_IP      "192.168.2.32"   // PC端测试IP
#define TEST_LOCAL_PROT    65000            	// PC端测试端口号

#define ONVIF_TX_BUFSIZE		1500	//定义最大发送数据长度
#define ONVIF_RX_BUFSIZE    3500

#define IPC_NUM_MAX         10      // IPC搜索个数 

typedef enum 
{
    HIKVISION_P = 0,
    DAHUA_P,
    ONVIF_P ,
    IPC_PROTOCOL_END
}ipc_protocol_t;   // 通信协议

#define ONVIF_START     ((uint8_t)0x01)  // 开始搜索
#define ONVIF_INIT      ((uint8_t)0x02)  // 初始化

#define ONVIF_END      	((uint8_t)0x80)  // 搜索结束


typedef struct
{
	uint8_t status;
	uint8_t search_flag;// 查询标志位(第0位搜索：0禁止，1允许)
	uint8_t ipc_num;    // 数量
	uint8_t ip[IPC_NUM_MAX][4];
	uint16_t onvif_times;
	ipc_protocol_t ipc_protocol_status;
}IPC_Info_t;      // IPC摄像头参数

extern IPC_Info_t ipcInfo;  // IPC摄像头参数

INT8U onvif_init(void);
void onvif_udp_start(void);
int ONVIF_IPC_Send_API(int sockfd,char* data,int len,char* ip,int port);

int ONVIF_IPC_Search_API(int sockfd,char* ip,int port);
int HIKVISION_IPC_Search_API(int sockfd,char* ip,int port);
int DAHUA_IPC_Search_API(int sockfd,char* ip,int port);
int ONVIF_IPC_Recv_API(int sockfd);

int ONVIF_Search_API(int sockfd);
void ONVIF_IPC_NET_Detection_API(void);
void onvif_search_timer_function(void);
void onvif_udp_stop(void);
void onvif_save_ipc_info(char *ip,uint8_t id);  // 保存摄像机信息

void onvif_set_search_flag(uint8_t flag);
void onvif_get_ipc_appoint_ip(char *buf,uint8_t id);
uint8_t onvif_get_ipc_num(void);
uint8_t onvif_get_ipc_status(void) ;
void onvif_set_ipc_status(uint8_t data);
int8_t onvif_match_camera_ip(uint8_t *ip);
int8_t onvif_match_ip(uint8_t *ip1,uint8_t *ip2);



#endif

