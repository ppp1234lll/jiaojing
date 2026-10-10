#ifndef __TCP_CLIENT_H
#define __TCP_CLIENT_H

#include "sys.h"
#include "includes.h" 

#define TCP_CLIENT1_RX_BUFSIZE	 	1024		// 定义tcp server最大接收数据长度
#define TCP_CLIENT1_DATA					0x8000	// 定义有数据发送

#define TCP_CLIENT2_RX_BUFSIZE	 	512			// 定义tcp server最大接收数据长度
#define TCP_CLIENT2_DATA					0x8000	// 定义有数据发送


#define TCP_CLIENT3_RX_BUFSIZE	 	512			// 定义tcp server最大接收数据长度
#define TCP_CLIENT3_DATA					0x8000	// 定义有数据发送

#define TCP_CLIENT4_RX_BUFSIZE	 	512			// 定义tcp server最大接收数据长度
#define TCP_CLIENT4_DATA					0x8000	// 定义有数据发送
#define CLIENTMAX   4 //最大客户端连接数量

//客户端任务结构体
typedef struct  
{
	struct netconn *conn;//客户端(连接结构体)
	OS_STK    *clientSTK;//客户端(任务堆栈)
	uint8_t   num;			 //客户端(编号)
}__attribute__((aligned(8)))tcp_client;

//客户端地址结构体
typedef struct  
{
	uint8_t   num;			 //客户端(编号)
	uint8_t   state[CLIENTMAX];//客户端连接状态
}client_ad;


int8_t tcp_client_init(void *arg);  //tcp客户端初始化(创建tcp客户端线程)

void tcp_client_start_function(void); // tcp客户端启动函数
void tcp_client_stop_function(void);  // tcp客户端停止函数

void tcp_cilent_send_buff(uint8_t *buff, uint16_t len,uint8_t websocket);

void tcp_cilent1_send_buff(uint8_t *buff, uint16_t len);
void tcp_client2_send_buff(uint8_t *buff, uint16_t len);
void tcp_client3_send_buff(uint8_t *buff, uint16_t len);
void tcp_client4_send_buff(uint8_t *buff, uint16_t len);
void tcp_backup_send_buff(uint8_t *buff, uint16_t len); // 备用服务器发送(仅HTTP场景)
uint8_t tcp_cilent1_get_link_status(void);
uint8_t tcp_client2_get_link_status(void);
uint8_t tcp_client3_get_link_status(void);
uint8_t tcp_backup_get_link_status(void);

uint8_t tcp_cilent1_get_recv_status(void);
uint8_t tcp_client2_get_recv_status(void);
void tcp_client_stop_function(void);
#endif

