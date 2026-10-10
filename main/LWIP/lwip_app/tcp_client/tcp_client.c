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
 * 有线 HTTP 服务器改造:
 *   设备保留 1 个常规连接槽(client1) 用于 WebSocket 长连接 / HTTP 短连接;
 *   client2 / client3 的多连接副本(及其对应 http_com2/3) 已移除, 避免重复处理逻辑
 *   与全局发送缓冲(http_send_buff)的并发竞态。
 *   新增"备用服务器槽"(仅用于 HTTP 通信): 当上一个 HTTP 连接没有及时断开、槽位仍被占用,
 *   而平台又发起新连接时, 立即强制关闭旧的 HTTP 连接, 并由备用槽承接新连接, 保证平台可继续通信。
 *   WebSocket 属正常长连接, 不启用备用槽, 新连接仍按原逻辑拒绝。
 *   任意时刻实际活动的 HTTP 连接只有 1 个, 因此全局收发缓冲无需分路。
 */

//CLIENT1客户端任务
#define TCP_CLIENT1_PRIO			 6
//任务堆栈大小
#define TCP_CLIENT1_STK_SIZE	 256
//任务堆栈
__align(8) static OS_STK TCP_CLIENT1_TASK_STK[TCP_CLIENT1_STK_SIZE];

//备用服务器任务(仅用于HTTP通信)
#define TCP_BACKUP_PRIO			 7
#define TCP_BACKUP_STK_SIZE	 256
__align(8) static OS_STK TCP_BACKUP_TASK_STK[TCP_BACKUP_STK_SIZE];


struct netconn *tcp_cilent1_conn = NULL;	// cilent1网络连接结构体
struct netbuf *sg_cilent1_recvbuf = NULL;
u8 *tcp_cilent1_sendbuf;	
uint16_t tcp_cilent1_flag;								//cilent1数据发送标志位
uint8_t tcp_cilent1_recvflag;						//cilent1数据接收

/*
 * 备用服务器(仅用于 HTTP 通信):
 *   平台的上一个 HTTP 连接没有及时断开(槽位被占), 平台又发起新连接时,
 *   原逻辑是直接拒绝新连接(平台连不上); 现在改为: 立即强制关闭旧的 HTTP 连接,
 *   并把新连接交给"备用槽"承接, 使平台能继续通信。
 *   WebSocket 属正常长连接, 不启用备用槽(保持原有拒绝行为)。
 */
struct netconn *tcp_backup_conn = NULL;		// 备用服务器网络连接结构体
struct netbuf *sg_backup_recvbuf = NULL;
u8 *tcp_backup_sendbuf;
uint16_t tcp_backup_flag;								// 备用服务器数据发送标志位

/*
 * 协作式关闭请求标志(方案1):
 *   由服务器线程置位, 对应的连接任务在自己的循环里检测到后"自行关闭连接并删除自己"。
 *   避免服务器线程直接 netconn_delete + OSTaskDel 一个正阻塞在 netconn_recv 中的任务
 *   ——那会先释放该任务仍挂在等待链上的 mailbox/OS_EVENT, 随后 OSTaskDel 再从已(可能被)
 *   复用的对象上摘链, 长期反复(数千次)会破坏 uCOS 事件对象/LwIP 邮箱, 导致服务器线程
 *   不再 accept(accept 邮箱填满后 LwIP 对新连接回 RST)。
 */
static volatile uint8_t tcp_client1_close_req = 0;
static volatile uint8_t tcp_backup_close_req  = 0;

/************************************************************
* Function name	: tcp_client1_force_close
* Description	: 关闭主槽(client1)旧连接(协作式)
* Parameter		:
* Return		:
*	用于"主槽被未及时断开的HTTP连接占用"时, 释放旧连接与残留接收状态:
*	仅置请求标志, 由 client1 任务自身完成 netconn_close/delete + OSTaskDel;
*	本函数等待其释放。client1 任务优先级(6)高于调用者(服务器线程10), 通常很快完成。
************************************************************/
static void tcp_client1_force_close(void)
{
	OS_CPU_SR cpu_sr;
	uint32_t wait = 0;

	tcp_client1_close_req = 1;
	/* 等待 client1 任务自行关闭连接并退出(最多约 500ms 兜底) */
	while((lwipdev.tcp_client1 != 0) && (wait < 100))
	{
		OSTimeDly(1);
		wait++;
	}

	if(lwipdev.tcp_client1 != 0)
	{
		/* 兜底: 任务未按预期退出。先删任务(使其脱离各等待链), 再释放连接对象。 */
		OS_ENTER_CRITICAL();		// 关中断
		OSTaskDel(TCP_CLIENT1_PRIO);
		OS_EXIT_CRITICAL();			// 开中断
		if(tcp_cilent1_conn != NULL)
		{
			netconn_close(tcp_cilent1_conn);                            //关闭连接
			netconn_delete(tcp_cilent1_conn);                           //删除连接
			tcp_cilent1_conn = NULL;
		}
		lwipdev.tcp_client1 = 0;
	}
	tcp_client1_close_req = 0;

	if(lwipdev.client_websocket_id == 1)
		lwipdev.client_websocket_id = 0;
	http_com_reset_recv_function(); // 清理旧连接可能遗留的半包/解析状态
}

/************************************************************
* Function name	: tcp_backup_force_close
* Description	: 关闭备用槽旧连接(协作式)
* Parameter		:
* Return		:
************************************************************/
static void tcp_backup_force_close(void)
{
	OS_CPU_SR cpu_sr;
	uint32_t wait = 0;

	tcp_backup_close_req = 1;
	/* 等待备用任务自行关闭连接并退出(最多约 500ms 兜底) */
	while((lwipdev.tcp_client_backup != 0) && (wait < 100))
	{
		OSTimeDly(1);
		wait++;
	}

	if(lwipdev.tcp_client_backup != 0)
	{
		OS_ENTER_CRITICAL();		// 关中断
		OSTaskDel(TCP_BACKUP_PRIO);
		OS_EXIT_CRITICAL();			// 开中断
		if(tcp_backup_conn != NULL)
		{
			netconn_close(tcp_backup_conn);                             //关闭连接
			netconn_delete(tcp_backup_conn);                            //删除连接
			tcp_backup_conn = NULL;
		}
		lwipdev.tcp_client_backup = 0;
	}
	tcp_backup_close_req = 0;

	if(lwipdev.client_websocket_id == 1)
	{
		lwipdev.client_websocket_id = 0;
		eth_set_network_reset();
	}
	http_com_reset_recv_function(); // 清理旧连接可能遗留的半包/解析状态
}

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
		if(tcp_client1_close_req)		// 服务器请求协作式关闭: 由本任务自行收尾
			goto CLIENT1_ERROR;
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
			if(tcp_cilent1_conn != NULL)  // 可能已被"强制关闭旧HTTP连接"释放, 避免二次释放
			{
				netconn_close(tcp_cilent1_conn);                            //关闭连接
				netconn_delete(tcp_cilent1_conn);                           //删除连接
				tcp_cilent1_conn = NULL;
			}
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

// 备用服务器任务函数(仅承接HTTP连接)
static void tcp_backup_thread(void *arg)
{
	err_t err,recv_err;
	struct pbuf *q;
	u32 data_len = 0;
	OS_CPU_SR cpu_sr;
	u16 port;					// 远端端口号
	ip_addr_t ip;			// 远端IP地址
	u32 link_count = 0;

	LWIP_UNUSED_ARG(arg);
	if(TCP_CLIENT_DEBUG) printf("backup...1\n");
	netconn_getaddr(tcp_backup_conn,&ip,&port,0);   //获取远端IP地址和端口号
	while (1)
	{
		if(tcp_backup_close_req)		// 服务器请求协作式关闭: 由本任务自行收尾
			goto BACKUP_ERROR;
		link_count++;
		if(link_count > 1000)
		{
			if(lwipdev.client_websocket_id != 1)
			{
				link_count = 0;
				goto BACKUP_ERROR;
			}
		}
		if((tcp_backup_flag & TCP_CLIENT1_DATA) == TCP_CLIENT1_DATA) //有数据要发送
		{
			err = netconn_write(tcp_backup_conn,tcp_backup_sendbuf,(tcp_backup_flag & 0x3fff),NETCONN_COPY);
			if(err != ERR_OK)
			{
				if(TCP_CLIENT_DEBUG) printf("backup_send error\r\n");
			}
			tcp_backup_flag &= ~TCP_CLIENT1_DATA;
		}

		if((recv_err = netconn_recv(tcp_backup_conn,&sg_backup_recvbuf)) == ERR_OK)  	//接收到数据
		{
			OS_ENTER_CRITICAL(); //关中断
			for(q=sg_backup_recvbuf->p;q!=NULL;q=q->next)  //遍历完整个pbuf链表
			{
				if(q->len > 0)
					http_com_stroage_data(q->payload,q->len);
				data_len += q->len;
			}
			OS_EXIT_CRITICAL();  // 开中断
			if(TCP_CLIENT_DEBUG) printf("backup_recv:%d...\n",data_len);
			data_len = 0;
			netbuf_delete(sg_backup_recvbuf);
			sg_backup_recvbuf = NULL;
		}
		else if(recv_err == ERR_CLSD||recv_err==ERR_RST)      //接收到关闭或复位数据
		{
			BACKUP_ERROR:
			if(tcp_backup_conn != NULL)  // 可能已被"强制关闭"释放, 避免二次释放
			{
				netconn_close(tcp_backup_conn);                             //关闭连接
				netconn_delete(tcp_backup_conn);                            //删除连接
				tcp_backup_conn = NULL;
			}
			if(lwipdev.client_websocket_id == 1)
			{
				lwipdev.client_websocket_id = 0;
				eth_set_network_reset();
			}
			lwipdev.tcp_client_backup = 0;
			if(TCP_CLIENT_DEBUG) printf("backup...2\n");
			OS_ENTER_CRITICAL();		// 关中断
			OSTaskDel(TCP_BACKUP_PRIO);	// 删除备用任务
			OS_EXIT_CRITICAL();			// 开中断
		}
		OSTimeDly(1);				/* 1 tick = 5ms @200Hz, 按节拍显式延时, 避免 ms->tick 换算陷阱 */
	}
}

//创建主连接槽任务(client1)
static int8_t tcp_primary_client_init(void *arg)
{
	INT8U err      = OS_ERR_NONE;
	INT8U name_err = OS_ERR_NONE;
	OS_CPU_SR cpu_sr;

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

//创建备用连接槽任务(仅HTTP场景使用)
static int8_t tcp_backup_client_init(void *arg)
{
	INT8U err      = OS_ERR_NONE;
	INT8U name_err = OS_ERR_NONE;
	OS_CPU_SR cpu_sr;

	tcp_backup_conn = (struct netconn *)arg;
	OS_ENTER_CRITICAL();	//关中断
	err = OSTaskCreateExt(	 tcp_backup_thread, 																//建立扩展任务(任务代码指针) 
										(void *)0,																				//传递参数指针 
										(OS_STK*)&TCP_BACKUP_TASK_STK[TCP_BACKUP_STK_SIZE-1], 	//分配任务堆栈栈顶指针 
										(INT8U)TCP_BACKUP_PRIO, 														//分配任务优先级 
										(INT16U)TCP_BACKUP_PRIO,														//(未来的)优先级标识(与优先级相同) 
										(OS_STK *)&TCP_BACKUP_TASK_STK[0], 										//分配任务堆栈栈底指针 
										(INT32U)TCP_BACKUP_STK_SIZE, 												//指定堆栈的容量(检验用) 
										(void *)0,																				//指向用户附加的数据域的指针 
										(INT16U)OS_TASK_OPT_STK_CHK|OS_TASK_OPT_STK_CLR);	//建立任务设定选项 
	OSTaskNameSet(TCP_BACKUP_PRIO, (INT8U *)(void *)"tcp_backup", &name_err);
  OS_EXIT_CRITICAL();		//开中断

	if(err == OS_ERR_NONE)
	{
		lwipdev.tcp_client_backup = 1; // 备用连接状态置1
		return OS_ERR_NONE;
	}

	if(TCP_CLIENT_DEBUG) printf("tcp_backup task create err:%d\r\n",err);
	netconn_close(tcp_backup_conn);
	netconn_delete(tcp_backup_conn);
	tcp_backup_conn = NULL;
	return err;
}

//创建TCP客户端线程
//返回值:OS_ERR_NONE 创建成功; 其它 失败(或连接被拒绝)
int8_t tcp_client_init(void *arg)
{
	int8_t err = OS_ERR_NONE;

	/* 无活动连接: 走常规主槽 */
	if((lwipdev.tcp_client1 == 0) && (lwipdev.tcp_client_backup == 0))
		return tcp_primary_client_init(arg);

	/* 已有活动连接为 WebSocket: 属正常长连接, 不启用备用服务器, 保持原拒绝行为 */
	if(lwipdev.client_websocket_id == 1)
	{
		if(TCP_CLIENT_DEBUG) printf("websocket active, reject new connection\r\n");
		netconn_close((struct netconn *)arg);
		netconn_delete((struct netconn *)arg);
		return -1;
	}

	/* 已有活动连接为"未及时断开的HTTP连接": 立即强制关闭旧连接, 让备用服务器承接新连接 */
	if(lwipdev.tcp_client1 != 0)  // 旧连接在主槽
	{
		if(TCP_CLIENT_DEBUG) printf("http stale on client1, switch to backup\r\n");
		tcp_client1_force_close();                 // 立即强制关闭旧HTTP连接
		err = tcp_backup_client_init(arg);         // 新连接由备用槽承接
	}
	else                          // 旧连接在备用槽
	{
		if(TCP_CLIENT_DEBUG) printf("http stale on backup, switch to client1\r\n");
		tcp_backup_force_close();                  // 立即强制关闭旧HTTP连接
		err = tcp_primary_client_init(arg);        // 新连接回到主槽
	}

	if(err != OS_ERR_NONE)
	{
		/* 槽位任务创建失败: 释放新连接, 避免无人处理而悬空 */
		if(TCP_CLIENT_DEBUG) printf("http slot create err:%d\r\n",err);
		netconn_close((struct netconn *)arg);
		netconn_delete((struct netconn *)arg);
		return -1;
	}
	return OS_ERR_NONE;
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
	/* 实际活动的HTTP连接只会有一个: 优先主槽, 主槽空闲时发往备用槽 */
	if(lwipdev.tcp_client1)
		tcp_cilent1_send_buff(buff,len);
	else if(lwipdev.tcp_client_backup)
		tcp_backup_send_buff(buff,len);
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
* Function name	: tcp_backup_send_buff
* Description	: 备用服务器发送数据(仅HTTP场景)
* Parameter		: 
* Return		: 
*	
************************************************************/
void tcp_backup_send_buff(uint8_t *buff, uint16_t len)
{
	if(lwipdev.tcp_client_backup)
	{
		tcp_backup_sendbuf = buff;
		tcp_backup_flag    = len + TCP_CLIENT1_DATA;
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
* Function name	: tcp_backup_get_link_status
* Description	: 获取备用服务器连接状态
* Parameter		: 
* Return		: 
*	只有在TCP连接时才可以发送数据
************************************************************/
uint8_t tcp_backup_get_link_status(void)
{
	return lwipdev.tcp_client_backup;
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

	tcp_client1_close_req = 0;
	tcp_backup_close_req  = 0;

	/* 先删任务(使其脱离 mbox/事件等待链), 再释放连接对象, 避免"先释放 mailbox 后摘链" */
	OS_ENTER_CRITICAL();		// 关中断
	OSTaskDel(TCP_CLIENT1_PRIO);	// 删除TCP任务
	OS_EXIT_CRITICAL();			// 开中断

	if(tcp_cilent1_conn != NULL)
	{
		netconn_close(tcp_cilent1_conn);                            //关闭连接
		netconn_delete(tcp_cilent1_conn);                           //删除连接
		tcp_cilent1_conn = NULL;
	}
	lwipdev.tcp_client1 = 0;   //第 client->num 个设备状态置0 表示客户端未连接
	if(lwipdev.client_websocket_id == 1)
		lwipdev.client_websocket_id = 0;

	/* 备用服务器连接同步关闭(仅HTTP场景) */
	OS_ENTER_CRITICAL();		// 关中断
	OSTaskDel(TCP_BACKUP_PRIO);	// 删除备用任务
	OS_EXIT_CRITICAL();			// 开中断

	if(tcp_backup_conn != NULL)
	{
		netconn_close(tcp_backup_conn);                             //关闭连接
		netconn_delete(tcp_backup_conn);                            //删除连接
		tcp_backup_conn = NULL;
	}
	lwipdev.tcp_client_backup = 0;
}
