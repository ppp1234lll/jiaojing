#include "tcp_client.h"
#include "lwip/opt.h"
#include "lwip_comm.h"
#include "lwip/lwip_sys.h"
#include "lwip/api.h"
#include "lwip/tcp.h"
#include "includes.h"
#include "lwip/tcp_impl.h"
#include "malloc.h"
#include "http_com.h"
#include "eth.h"

/*
 * 单 HTTP 服务器改造:
 *   设备只保留 1 个 TCP 客户端连接槽(client1), 用于 WebSocket 长连接 / HTTP 短连接;
 *   client2 / client3 的多连接副本(及其对应 http_com2/3) 已移除, 避免重复处理逻辑
 *   与全局发送缓冲(http_send_buff)的并发竞态。
 *   新连接到来时若槽位已被占用, 直接关闭该新连接, 服务器继续监听。
 */

//CLIENT1客户端任务
#define TCP_CLIENT1_PRIO			 6
//任务堆栈大小
#define TCP_CLIENT1_STK_SIZE	 256
//任务堆栈
__align(8) static OS_STK TCP_CLIENT1_TASK_STK[TCP_CLIENT1_STK_SIZE];


struct netconn *tcp_cilent1_conn = NULL;	// cilent1网络连接结构体
struct netbuf *sg_cilent1_recvbuf = NULL;
u8 *tcp_cilent1_sendbuf;	
uint16_t tcp_cilent1_flag;								//cilent1数据发送标志位
uint8_t tcp_cilent1_recvflag;						//cilent1数据接收

/* 以下 client2/client3 变量保留(供头文件声明的接口使用), 但不再创建对应连接任务 */

struct netconn *tcp_client2_conn = NULL;	// client2网络连接结构体
struct netbuf *sg_client2_recvbuf = NULL;
u8 *tcp_client2_sendbuf;	
uint16_t tcp_client2_flag;					    //client2数据发送标志位
uint8_t tcp_client2_recvflag;						//client2数据接收


struct netconn *tcp_client3_conn = NULL;	// client3网络连接结构体
struct netbuf *sg_client3_recvbuf = NULL;
u8 *tcp_client3_sendbuf;	
uint16_t tcp_client3_flag;								//client3数据发送标志位
uint8_t tcp_client3_recvflag;						//client3数据接收

// cilent1任务函数
static void tcp_cilent1_thread(void *arg)
{
	err_t err,recv_err;
	struct pbuf *q;
	u32 data_len = 0;
	OS_CPU_SR cpu_sr;
	u16 			 port; // client 端口号
	ip_addr_t  ip;   // client IP地址
	u32 link_count = 0;
	
	LWIP_UNUSED_ARG(arg);
	if(TCP_CLIENT_DEBUG) printf("cilent1...1\n");
	netconn_getaddr(tcp_cilent1_conn,&ip,&port,0);   //获取远端IP地址和端口号	
	while (1) 
	{
		link_count++;
		if(link_count > 1000)
		{
			if(lwipdev.client_websocket_id != 1)
			{
				link_count = 0;
				goto CLIENT1_ERROR;
			}
		}
		if((tcp_cilent1_flag & TCP_CLIENT1_DATA) == TCP_CLIENT1_DATA) //有数据要发送
		{
			err = netconn_write(tcp_cilent1_conn,tcp_cilent1_sendbuf,(tcp_cilent1_flag & 0x3fff),NETCONN_COPY); 
			if(TCP_CLIENT_DEBUG) printf("cilent1_send end...\n");
			if(err != ERR_OK)
			{
				if(TCP_CLIENT_DEBUG) printf("cilent1_send error\r\n");
			}
			tcp_cilent1_flag &= ~TCP_CLIENT1_DATA;
		}
		
		if((recv_err = netconn_recv(tcp_cilent1_conn,&sg_cilent1_recvbuf)) == ERR_OK)  	//接收到数据
		{
				OS_ENTER_CRITICAL(); //关中断
				for(q=sg_cilent1_recvbuf->p;q!=NULL;q=q->next)  //遍历完整个pbuf链表
				{
					if(q->len > 0) 
						http_com_stroage_data(q->payload,q->len);
					data_len += q->len;  	
				}
				OS_EXIT_CRITICAL();  // 开中断
				if(TCP_CLIENT_DEBUG)	printf("cilent1_recv:%d...\n",data_len);  //接收到的数据
				data_len=0;  				 // 复制完成后data_len要清零。	
				netbuf_delete(sg_cilent1_recvbuf);
				sg_cilent1_recvbuf = NULL;			
		}
		else if(recv_err == ERR_CLSD||recv_err==ERR_RST)      //接收到关闭或复位数据
		{
			CLIENT1_ERROR:
			netconn_close(tcp_cilent1_conn);                            //关闭连接
			netconn_delete(tcp_cilent1_conn);                           //删除连接
			tcp_cilent1_conn = NULL;
			if(lwipdev.client_websocket_id == 1)
			{
				lwipdev.client_websocket_id = 0;
				eth_set_network_reset();
			}
			lwipdev.tcp_client1 = 0;                          //第 client->num 个设备状态置0 表示客户端未连接
			if(TCP_CLIENT_DEBUG) printf("cilent1...2\n");
			OS_ENTER_CRITICAL();		// 关中断
			OSTaskDel(TCP_CLIENT1_PRIO);	// 删除TCP任务
			OS_EXIT_CRITICAL();			// 开中断
		}
		OSTimeDly(1);				/* 1 tick = 5ms @200Hz, 按节拍显式延时, 避免 ms->tick 换算陷阱 */
	}
}

//创建TCP客户端线程(单连接槽)
//返回值:OS_ERR_NONE 创建成功; 其它 失败(或槽位已占用)
int8_t tcp_client_init(void *arg)
{
	INT8U err      = OS_ERR_NONE;
	INT8U name_err = OS_ERR_NONE;
	OS_CPU_SR cpu_sr;

	/* 单服务器: 仅 1 个连接槽, 已占用则拒绝新连接(服务器保持监听) */
	if(lwipdev.tcp_client1 != 0)
	{
		if(TCP_CLIENT_DEBUG) printf("tcp_client busy, reject new connection\r\n");
		netconn_close((struct netconn *)arg);
		netconn_delete((struct netconn *)arg);
		return -1;
	}

	tcp_cilent1_conn = (struct netconn *)arg;
	OS_ENTER_CRITICAL();	//关中断
	err = OSTaskCreateExt(	 tcp_cilent1_thread, 																//建立扩展任务(任务代码指针) 
										(void *)0,																				//传递参数指针 
										(OS_STK*)&TCP_CLIENT1_TASK_STK[TCP_CLIENT1_STK_SIZE-1], //分配任务堆栈栈顶指针 
										(INT8U)TCP_CLIENT1_PRIO, 														//分配任务优先级 
										(INT16U)TCP_CLIENT1_PRIO,														//(未来的)优先级标识(与优先级相同) 
										(OS_STK *)&TCP_CLIENT1_TASK_STK[0], 									//分配任务堆栈栈底指针 
										(INT32U)TCP_CLIENT1_STK_SIZE, 												//指定堆栈的容量(检验用) 
										(void *)0,																				//指向用户附加的数据域的指针 
										(INT16U)OS_TASK_OPT_STK_CHK|OS_TASK_OPT_STK_CLR);	//建立任务设定选项 
	OSTaskNameSet(TCP_CLIENT1_PRIO, (INT8U *)(void *)"tcp_cilent1", &name_err);
  OS_EXIT_CRITICAL();		//开中断

	if(err == OS_ERR_NONE)
	{
		lwipdev.tcp_client1 = 1; // 连接状态置1(已连接)	
		return OS_ERR_NONE;
	}

	/* 任务创建失败: 释放连接, 避免连接无人处理而悬空 */
	if(TCP_CLIENT_DEBUG) printf("tcp_cilent1 task create err:%d\r\n",err);
	netconn_close(tcp_cilent1_conn);
	netconn_delete(tcp_cilent1_conn);
	tcp_cilent1_conn = NULL;
	return err;
}

/************************************************************
*
* Function name	: tcp_cilent_send_buff
* Description	: 发送数据(单服务器: 统一发往唯一的 client1 连接)
* Parameter		: 
* Return		: 
*	
************************************************************/
void tcp_cilent_send_buff(uint8_t *buff, uint16_t len,uint8_t websocket)
{
	LWIP_UNUSED_ARG(websocket);
	tcp_cilent1_send_buff(buff,len);
}

/************************************************************
*
* Function name	: tcp_cilent1_send_buff
* Description	: cilent1 发送数据
* Parameter		: 
* Return		: 
*	
************************************************************/
void tcp_cilent1_send_buff(uint8_t *buff, uint16_t len)
{
	if(lwipdev.tcp_client1)
	{
		tcp_cilent1_sendbuf = buff;
		tcp_cilent1_flag    = len + TCP_CLIENT1_DATA;
	}
}

/************************************************************
*
* Function name	: tcp_client2_send_buff
* Description	: client2 发送数据(单服务器下不再使用)
* Parameter		: 
* Return		: 
*	
************************************************************/
void tcp_client2_send_buff(uint8_t *buff, uint16_t len)
{
	if(lwipdev.tcp_client2)
	{
		tcp_client2_sendbuf = buff;
		tcp_client2_flag    = len + TCP_CLIENT2_DATA;
	}
}

/************************************************************
*
* Function name	: tcp_client3_send_buff
* Description	: client3 发送数据(单服务器下不再使用)
* Parameter		: 
* Return		: 
*	
************************************************************/
void tcp_client3_send_buff(uint8_t *buff, uint16_t len)
{
	if(lwipdev.tcp_client3)
	{
		tcp_client3_sendbuf = buff;
		tcp_client3_flag    = len + TCP_CLIENT3_DATA;
	}
}

/************************************************************
*
* Function name	: tcp_cilent1_get_link_status
* Description	: 获取TCP连接状态
* Parameter		: 
* Return		: 
*	只有在TCP连接时才可以发送数据
************************************************************/
uint8_t tcp_cilent1_get_link_status(void)
{
	return lwipdev.tcp_client1;
}

/************************************************************
*
* Function name	: tcp_client2_get_link_status
* Description	: 获取TCP连接状态
* Parameter		: 
* Return		: 
*	只有在TCP连接时才可以发送数据
************************************************************/
uint8_t tcp_client2_get_link_status(void)
{
	return lwipdev.tcp_client2;
}

/************************************************************
*
* Function name	: tcp_client3_get_link_status
* Description	: 获取TCP连接状态
* Parameter		: 
* Return		: 
*	只有在TCP连接时才可以发送数据
************************************************************/
uint8_t tcp_client3_get_link_status(void)
{
	return lwipdev.tcp_client3;
}

/************************************************************
*
* Function name	: tcp_cilent1_get_recv_status
* Description	: 获取接收状态
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t tcp_cilent1_get_recv_status(void)
{
	return tcp_cilent1_recvflag;
}

uint8_t tcp_client2_get_recv_status(void)
{
	return tcp_client2_recvflag;
}

/************************************************************
*
* Function name	: tcp_client_stop_function
* Description	: 客户端停止函数(单服务器: 只处理 client1, 并对空指针做保护)
* Parameter		: 
* Return		: 
*	
************************************************************/
void tcp_client_stop_function(void)
{	
	OS_CPU_SR cpu_sr;

	if(tcp_cilent1_conn != NULL)
	{
		netconn_close(tcp_cilent1_conn);                            //关闭连接
		netconn_delete(tcp_cilent1_conn);                           //删除连接
		tcp_cilent1_conn = NULL;
	}
	lwipdev.tcp_client1 = 0;   //第 client->num 个设备状态置0 表示客户端未连接
	if(lwipdev.client_websocket_id == 1)
		lwipdev.client_websocket_id = 0;

	OS_ENTER_CRITICAL();		// 关中断
	OSTaskDel(TCP_CLIENT1_PRIO);	// 删除TCP任务
	OS_EXIT_CRITICAL();			// 开中断
}
