#include "tcp_server.h"
#include "lwip/opt.h"
#include "lwip_comm.h"
#include "app.h"
#include "http_com.h"
#include "lwip/lwip_sys.h"
#include "lwip/api.h"
#include "lwip/opt.h"
#include "lwip/tcp.h"
#include "lwip/ip.h"
#include "led.h"
#include "tcp_client.h"

struct netconn *tcp_server_conn, *tcp_server_newconn;

volatile uint8_t g_dbg_accept_err_reset = 0;   /* 诊断: accept 持续异常失败触发的网络重启次数 */
extern void eth_set_network_reset(void);       /* 定义在 APP/TASK/src/eth.c */

//TCP客户端任务
#define TCPSERVER_PRIO			10
//任务堆栈大小
#define TCPSERVER_STK_SIZE	256
//任务堆栈
__align(8) static OS_STK TCPSERVER_TASK_STK[TCPSERVER_STK_SIZE];

//tcp服务器任务
static void tcp_server_thread(void *arg)
{
	err_t err;
	struct local_ip_t  *local = app_get_local_network_function();
	INT8U osres;
	static uint8_t accept_err_run = 0;   // netconn_accept 连续异常失败计数(自愈用)
	LWIP_UNUSED_ARG(arg);

	if(TCP_SERVER_DEBUG) printf("create tcp_server_thread \r\n");
	
	while (1) 
	{
		if(lwipdev.tcp_server_status == LWIP_TCP_INIT_CONNECT) // TCP创建
		{	
			tcp_server_conn = netconn_new(NETCONN_TCP);//创建一个TCP服务器连接
			if(tcp_server_conn == NULL)
			{
				if(TCP_SERVER_DEBUG) printf("tcp_server_conn error \r\n");
				lwipdev.tcp_server_reset = 1;
			}
			else
			{
				err = netconn_bind(tcp_server_conn,IP_ADDR_ANY,local->port);//绑定端口号TCP_SERVER_PORT
				if(err == ERR_OK)
				{
					err = netconn_listen(tcp_server_conn);	//进入监听模式
					if(err == ERR_OK)
					{
						if(TCP_SERVER_DEBUG) printf("TCP Server ready to accept");
						tcp_server_conn->recv_timeout = 10;		//禁止阻塞线程 等待20ms
						lwipdev.tcp_server_status = LWIP_TCP_SERVER_LINK;	// 服务器等待连接				
					}
					else
						if(TCP_SERVER_DEBUG) printf("TCP Server listen failed");
				}
				else
					if(TCP_SERVER_DEBUG) printf("netconn bind failed");			
			}
		}
		else if(lwipdev.tcp_server_status == LWIP_TCP_SERVER_LINK) // TCP创建成功
		{
			err = netconn_accept(tcp_server_conn,&tcp_server_newconn);  //接收连接请求
			if (err == ERR_OK)    //处理新连接的数据
			{
				accept_err_run = 0;
				if(TCP_SERVER_DEBUG) printf("netconn_accept\n");
				tcp_server_newconn->recv_timeout = 10;
				/* 关键: 给发送也设超时。对端(平台)不读取时 netconn_write 会长期阻塞在
				   发送缓冲/接收窗口上; 若此时上层因"槽位被占用"要关闭该连接, 任务就会
				   长时间卡在 netconn_write 里, 迫使上层强杀任务, 而该任务已入队的 API
				   消息仍会被 tcpip 线程继续处理 → 作用于已释放/复用的 netconn(重复释放,
				   耗尽 uCOS 队列/事件池)。设 20ms 发送超时后, 写会及时返回, 任务总能
				   在协作等待窗口内自行收尾退出。*/
				tcp_server_newconn->send_timeout = 20;
				// 创建客户端任务(主槽/备用槽; 主槽被未及时断开的HTTP连接占用时, 由备用槽承接并在
				// tcp_client_init 内立即强制关闭旧HTTP连接; WebSocket占用或创建失败时新连接已关闭)
				osres = tcp_client_init((void *)tcp_server_newconn);
				if(TCP_SERVER_DEBUG) printf("tcp_server:%d\n",osres);
				if(osres != OS_ERR_NONE)
					tcp_server_newconn = NULL;		// 新连接已释放, 避免悬空

				led_control_function(LD_LAN,LD_FLICKER);
				lwipdev.tcp_server_link = 1; // 标志位置1
			}
			else if(err != ERR_TIMEOUT)   // ERR_TIMEOUT=空闲轮询属正常; 其余(内存耗尽等)为异常
			{
				/* 兜底自愈: accept 持续异常失败(典型为 TCP_PCB/内存池耗尽——极端场景下
				   pcb 因优雅关闭无法完成而滞留) 时, 网络层已无法接受任何新连接且不会自行
				   恢复; 触发一次网络重启(lwip 重建 memp 池与 pcb), 避免设备永久失去服务。 */
				if(++accept_err_run >= 100u)     // 连续100次异常失败
				{
					accept_err_run = 0;
					g_dbg_accept_err_reset++;
					eth_set_network_reset();     // 网络重启(lwip 重建内存池)
				}
			}
			else
			{
				accept_err_run = 0;              // ERR_TIMEOUT: 正常空闲轮询, 清计数
			}
		}
		/* 停止tcp */
		if(lwipdev.tcp_server_reset == 1)					  // 重启tcp连接
		{
			lwipdev.tcp_server_reset = 0;
			tcp_server_stop_function();
		}	
		IWDG_Feed();
		/* 按节拍显式延时(2 tick = 10ms @OS_TICKS_PER_SEC=200), 避免毫秒换算陷阱:
		   OSTimeDlyHMSM(0,0,0,ms) 在节拍 200Hz 下 ms<=2 会算出 0 tick, 而 OSTimeDly(0) 不延时;
		   本任务优先级(10)高于 app 任务(11), 忙等会饿死 app 主任务, 导致参数保存永不执行。 */
		OSTimeDly(2);
	}
}


//创建TCP服务器线程
//返回值:0 TCP服务器创建成功
//		其他 TCP服务器创建失败
INT8U tcp_server_init(void)
{
	INT8U res, err;
	OS_CPU_SR cpu_sr;
	
	OS_ENTER_CRITICAL();	//关中断
	OSTaskCreateExt(	 tcp_server_thread, 																					//建立扩展任务(任务代码指针) 
										(void *)0,																					//传递参数指针 
										(OS_STK*)&TCPSERVER_TASK_STK[TCPSERVER_STK_SIZE-1], 					//分配任务堆栈栈顶指针 
										(INT8U)TCPSERVER_PRIO, 															//分配任务优先级 
										(INT16U)TCPSERVER_PRIO,															//(未来的)优先级标识(与优先级相同) 
										(OS_STK *)&TCPSERVER_TASK_STK[0], 											//分配任务堆栈栈底指针 
										(INT32U)TCPSERVER_STK_SIZE, 															//指定堆栈的容量(检验用) 
										(void *)0,																					//指向用户附加的数据域的指针 
										(INT16U)OS_TASK_OPT_STK_CHK|OS_TASK_OPT_STK_CLR);		//建立任务设定选项 
	OSTaskNameSet(TCPSERVER_PRIO, (INT8U *)(void *)"tcp_server", &err);
//	res = OSTaskCreate(tcp_server_thread,(void*)0,(OS_STK*)&TCPSERVER_TASK_STK[TCPSERVER_STK_SIZE-1],TCPSERVER_PRIO); //创建TCP服务器线程
	OS_EXIT_CRITICAL();		//开中断
	
	return res;
}

/************************************************************
*
* Function name	: tcp_server_start_function
* Description	: tcp服务器启动函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void tcp_server_start_function(void)
{
	/* 创建TCP服务器 */
	lwipdev.tcp_server_status = 0;
	tcp_server_init();
}

/************************************************************
*
* Function name	: tcp_server_stop_function
* Description	: tcp服务器停止函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void tcp_server_stop_function(void)
{
	lwipdev.tcp_server_reset = 0;
	
	OS_CPU_SR cpu_sr;
	led_control_function(LD_LAN,LD_ON);
	/* 关闭并释放客户端连接(单槽): tcp_server_newconn 与 tcp_cilent1_conn 是同一对象,
	   已由 tcp_client_stop_function() 释放, 此处只清空, 避免同一 netconn 二次释放 */
	tcp_client_stop_function();
	tcp_server_newconn = NULL;

	/* 关闭tcp服务器(仅监听对象): 只 delete 即可(内部会排空 acceptmbox 并释放监听 pcb),
	   不再先 close —— netconn_close 对 LISTEN 状态无实际作用, 徒增一次 API 往返 */
	if(lwipdev.tcp_server_status != LWIP_TCP_NO_CONNECT)
	{
		if(tcp_server_conn!= NULL)
		{
			netconn_delete(tcp_server_conn);
		}
	}
	tcp_server_conn = NULL;
	lwipdev.tcp_server_link = 0;
	lwipdev.tcp_server_status = LWIP_TCP_NO_CONNECT;
	
	OS_ENTER_CRITICAL();		// 关中断
	OSTaskDel(TCPSERVER_PRIO);	// 删除TCP任务
	OS_EXIT_CRITICAL();			// 开中断
}


/************************************************************
*
* Function name	: tcp_server_get_link_status
* Description	: 获取TCP连接状态
* Parameter		: 
* Return		: 
*	只有在TCP连接时才可以发送数据
************************************************************/
uint8_t tcp_server_get_link_status(void)
{
	return lwipdev.tcp_server_link;
}



