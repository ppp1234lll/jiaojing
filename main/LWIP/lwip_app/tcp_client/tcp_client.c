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

//CLIENT1客户端任务
#define TCP_CLIENT1_PRIO			 6
//任务堆栈大小
#define TCP_CLIENT1_STK_SIZE	 256
//任务堆栈
__align(8) static OS_STK TCP_CLIENT1_TASK_STK[TCP_CLIENT1_STK_SIZE];

//CLIENT2客户端任务
#define TCP_CLIENT2_PRIO			 7
//任务堆栈大小
#define TCP_CLIENT2_STK_SIZE	 256
//任务堆栈
__align(8) static OS_STK TCP_CLIENT2_TASK_STK[TCP_CLIENT2_STK_SIZE];

//CLIENT3客户端任务
#define TCP_CLIENT3_PRIO			 8
//任务堆栈大小
#define TCP_CLIENT3_STK_SIZE	 256
//任务堆栈
__align(8) static OS_STK TCP_CLIENT3_TASK_STK[TCP_CLIENT3_STK_SIZE];


struct netconn *tcp_cilent1_conn = NULL;	// cilent1网络连接结构体
struct netbuf *sg_cilent1_recvbuf = NULL;
u8 *tcp_cilent1_sendbuf;	
uint16_t tcp_cilent1_flag;								//cilent1数据发送标志位
uint8_t tcp_cilent1_recvflag;						//cilent1数据接收


struct netconn *tcp_client2_conn = NULL;	// client2网络连接结构体
struct netbuf *sg_client2_recvbuf = NULL;
u8 *tcp_client2_sendbuf;	
uint16_t tcp_client2_flag;					    //client2数据发送标志位
uint8_t tcp_client2_recvflag;						//client2数据接收


struct netconn *tcp_client3_conn = NULL;	// client2网络连接结构体
struct netbuf *sg_client3_recvbuf = NULL;
u8 *tcp_client3_sendbuf;	
uint16_t tcp_client3_flag;								//client2数据发送标志位
uint8_t tcp_client3_recvflag;						//client2数据接收

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
		OSTimeDlyHMSM(0,0,0,5);
	}
}

// client2客户端任务函数
static void tcp_client2_thread(void *arg)
{
	err_t err,recv_err;
	struct pbuf *q;
	u32 data_len = 0;
	OS_CPU_SR cpu_sr;
	u16 			 port; // client 端口号
	ip_addr_t  ip;   // client IP地址
	u32 link_count2 = 0;
	static uint16_t client2_count =0;
	
	LWIP_UNUSED_ARG(arg);
	netconn_getaddr(tcp_client2_conn,&ip,&port,0);        //获取远端IP地址和端口号	
	if(TCP_CLIENT_DEBUG) if(TCP_CLIENT_DEBUG) printf("cilent2...1\n");
	
	while (1) 
	{
		link_count2++;
		if(link_count2 > 1000)
		{
			if(lwipdev.client_websocket_id != 2)
			{
				link_count2 = 0;
				goto CLIENT2_ERROR;
			}
		}
		if((tcp_client2_flag & TCP_CLIENT2_DATA) == TCP_CLIENT2_DATA) //有数据要发送
		{
			if(TCP_CLIENT_DEBUG)	printf("cilent2 send end...\n");
			client2_count++;
			err = netconn_write(tcp_client2_conn,tcp_client2_sendbuf,(tcp_client2_flag & 0x3fff),NETCONN_COPY); 
			if(err != ERR_OK)
			{
				if(TCP_CLIENT_DEBUG) printf("cilent2 error\r\n");
			}
			tcp_client2_flag &= ~TCP_CLIENT2_DATA;
		}
		
		if((recv_err = netconn_recv(tcp_client2_conn,&sg_client2_recvbuf)) == ERR_OK)  	//接收到数据
		{
				OS_ENTER_CRITICAL(); //关中断
				for(q=sg_client2_recvbuf->p;q!=NULL;q=q->next)  //遍历完整个pbuf链表
				{
					if(q->len > 0) 
						http_com_stroage_data2(q->payload,q->len);
					data_len += q->len;  	
				}
				tcp_client2_recvflag = 1; // 有数据接收
				OS_EXIT_CRITICAL();  // 开中断
				if(TCP_CLIENT_DEBUG) printf("cilent2 recv:%d...\n",data_len);  //接收到的数据
				
				data_len=0;  				 // 复制完成后data_len要清零。	
				netbuf_delete(sg_client2_recvbuf);
				sg_client2_recvbuf = NULL;			
		}
		else if(recv_err == ERR_CLSD||recv_err==ERR_RST)    //接收到关闭或复位数据
		{
			CLIENT2_ERROR:
			netconn_close(tcp_client2_conn);                  //关闭连接
			netconn_delete(tcp_client2_conn);                 //删除连接
			lwipdev.tcp_client2 = 0;                          //第 client->num 个设备状态置0 表示客户端未连接
			if(lwipdev.client_websocket_id == 2)
			{
				lwipdev.client_websocket_id = 0;
				eth_set_network_reset();
			}
			if(TCP_CLIENT_DEBUG) printf("cilent2...2\n");
			OS_ENTER_CRITICAL();		// 关中断
			OSTaskDel(TCP_CLIENT2_PRIO);	// 删除TCP任务
			OS_EXIT_CRITICAL();			// 开中断
		}
		OSTimeDlyHMSM(0,0,0,5);
	}
}

// client3客户端任务函数
static void tcp_client3_thread(void *arg)
{
	err_t err,recv_err;
	struct pbuf *q;
	u32 data_len = 0;
	OS_CPU_SR cpu_sr;
	u16 			 port; // client 端口号
	ip_addr_t  ip;   // client IP地址
	u32 link_count3 = 0;
	static u16 client3_count = 0;
	
	LWIP_UNUSED_ARG(arg);
	netconn_getaddr(tcp_client3_conn,&ip,&port,0);        //获取远端IP地址和端口号	
	if(TCP_CLIENT_DEBUG) if(TCP_CLIENT_DEBUG) printf("cilent3...1\n");
	
	while (1) 
	{
		link_count3++;
		if(link_count3 > 1000)
		{
			if(lwipdev.client_websocket_id != 3)
			{
				link_count3 = 0;
				goto CLIENT3_ERROR;
			}
		}
		if((tcp_client3_flag & TCP_CLIENT3_DATA) == TCP_CLIENT3_DATA) //有数据要发送
		{
			if(TCP_CLIENT_DEBUG)	printf("cilent3 send end...\n");
			client3_count++;
			err = netconn_write(tcp_client3_conn,tcp_client3_sendbuf,(tcp_client3_flag & 0x3fff),NETCONN_COPY); 
			if(err != ERR_OK)
			{
				if(TCP_CLIENT_DEBUG) printf("cilent3 error\r\n");
			}
			tcp_client3_flag &= ~TCP_CLIENT3_DATA;
		}
		
		if((recv_err = netconn_recv(tcp_client3_conn,&sg_client3_recvbuf)) == ERR_OK)  	//接收到数据
		{
				OS_ENTER_CRITICAL(); //关中断
				for(q=sg_client3_recvbuf->p;q!=NULL;q=q->next)  //遍历完整个pbuf链表
				{
					if(q->len > 0) 
					{
						http_com_stroage_data3(q->payload,q->len);
					}		
					data_len += q->len;  	
				}
				tcp_client3_recvflag = 1; // 有数据接收
				OS_EXIT_CRITICAL();  // 开中断
				if(TCP_CLIENT_DEBUG) printf("cilent3 recv:%d...\n",data_len);  //接收到的数据
				data_len=0;  				 // 复制完成后data_len要清零。	
				netbuf_delete(sg_client3_recvbuf);
				sg_client3_recvbuf = NULL;			
		}
		else if(recv_err == ERR_CLSD||recv_err==ERR_RST)      //接收到关闭或复位数据
		{
			CLIENT3_ERROR:
			netconn_close(tcp_client3_conn);                            //关闭连接
			netconn_delete(tcp_client3_conn);                           //删除连接
			lwipdev.tcp_client3 = 0;                          //第 client->num 个设备状态置0 表示客户端未连接
			if(lwipdev.client_websocket_id == 3)
			{
				lwipdev.client_websocket_id = 0;
				eth_set_network_reset();
			}
			if(TCP_CLIENT_DEBUG) printf("cilent3...2\n");
			OS_ENTER_CRITICAL();		// 关中断
			OSTaskDel(TCP_CLIENT3_PRIO);	// 删除TCP任务
			OS_EXIT_CRITICAL();			// 开中断
		}
		OSTimeDlyHMSM(0,0,0,5);
	}
}

//创建TCP客户端线程
//返回值:0 TCP客户端创建成功
//		其他 TCP客户端创建失败
int8_t tcp_client_init(void *arg)
{
	INT8U err;
	OS_CPU_SR cpu_sr;

	if(lwipdev.tcp_client1 == 0)
	{
		tcp_cilent1_conn = (struct netconn *)arg;
		OS_ENTER_CRITICAL();	//关中断
		OSTaskCreateExt(	 tcp_cilent1_thread, 																//建立扩展任务(任务代码指针) 
											(void *)0,																				//传递参数指针 
											(OS_STK*)&TCP_CLIENT1_TASK_STK[TCP_CLIENT1_STK_SIZE-1], //分配任务堆栈栈顶指针 
											(INT8U)TCP_CLIENT1_PRIO, 														//分配任务优先级 
											(INT16U)TCP_CLIENT1_PRIO,														//(未来的)优先级标识(与优先级相同) 
											(OS_STK *)&TCP_CLIENT1_TASK_STK[0], 									//分配任务堆栈栈底指针 
											(INT32U)TCP_CLIENT1_STK_SIZE, 												//指定堆栈的容量(检验用) 
											(void *)0,																				//指向用户附加的数据域的指针 
											(INT16U)OS_TASK_OPT_STK_CHK|OS_TASK_OPT_STK_CLR);	//建立任务设定选项 
		OSTaskNameSet(TCP_CLIENT1_PRIO, (INT8U *)(void *)"tcp_cilent1", &err);
		
  	OS_EXIT_CRITICAL();		//开中断
		if(err == OS_ERR_NONE)
		{
			lwipdev.tcp_client1 = 1; // 连接状态置1(已连接)	
			return OS_ERR_NONE;
		}			
		else
			return err;
	}
	
	if(lwipdev.tcp_client2 == 0)
	{
		tcp_client2_conn = (struct netconn *)arg;
		OS_ENTER_CRITICAL();	//关中断
		OSTaskCreateExt(	 tcp_client2_thread, 																				 //建立扩展任务(任务代码指针) 
											(void *)0,																					       //传递参数指针 
											(OS_STK*)&TCP_CLIENT2_TASK_STK[TCP_CLIENT2_STK_SIZE-1],//分配任务堆栈栈顶指针 
											(INT8U)TCP_CLIENT2_PRIO, 													//分配任务优先级 
											(INT16U)TCP_CLIENT2_PRIO,													//(未来的)优先级标识(与优先级相同) 
											(OS_STK *)&TCP_CLIENT2_TASK_STK[0], 							//分配任务堆栈栈底指针 
											(INT32U)TCP_CLIENT2_STK_SIZE, 										//指定堆栈的容量(检验用) 
											(void *)0,																					//指向用户附加的数据域的指针 
											(INT16U)OS_TASK_OPT_STK_CHK|OS_TASK_OPT_STK_CLR);		//建立任务设定选项 
		OSTaskNameSet(TCP_CLIENT2_PRIO, (INT8U *)(void *)"tcp_client2", &err);
		
	//	res = OSTaskCreate(tcp_client2_thread,(void*)0,(OS_STK*)&TCP_CLIENT2_TASK_STK[TCP_CLIENT2_STK_SIZE-1],TCP_CLIENT2_PRIO); //创建TCP服务器线程
		OS_EXIT_CRITICAL();		//开中断
		if(TCP_CLIENT_DEBUG) printf("tcp_client2:%d\n",err);	
		if(err == OS_ERR_NONE)
		{
			lwipdev.tcp_client2 = 1; // 连接状态置1(已连接)	
			return OS_ERR_NONE;
		}			
		else
			return err;
	}	
	
	if(lwipdev.tcp_client3 == 0)
	{
		tcp_client3_conn = (struct netconn *)arg;
		OS_ENTER_CRITICAL();	//关中断
		OSTaskCreateExt(	 tcp_client3_thread, 																				 //建立扩展任务(任务代码指针) 
											(void *)0,																					       //传递参数指针 
											(OS_STK*)&TCP_CLIENT3_TASK_STK[TCP_CLIENT3_STK_SIZE-1],//分配任务堆栈栈顶指针 
											(INT8U)TCP_CLIENT3_PRIO, 													//分配任务优先级 
											(INT16U)TCP_CLIENT3_PRIO,													//(未来的)优先级标识(与优先级相同) 
											(OS_STK *)&TCP_CLIENT3_TASK_STK[0], 							//分配任务堆栈栈底指针 
											(INT32U)TCP_CLIENT3_STK_SIZE, 										//指定堆栈的容量(检验用) 
											(void *)0,																					//指向用户附加的数据域的指针 
											(INT16U)OS_TASK_OPT_STK_CHK|OS_TASK_OPT_STK_CLR);		//建立任务设定选项 
		OSTaskNameSet(TCP_CLIENT3_PRIO, (INT8U *)(void *)"tcp_client3", &err);
		
		OS_EXIT_CRITICAL();		//开中断
		if(TCP_CLIENT_DEBUG) printf("tcp_client3:%d\n",err);	
		if(err == OS_ERR_NONE)
		{
			lwipdev.tcp_client3 = 1; // 连接状态置1(已连接)	
			return OS_ERR_NONE;
		}			
		else
			return err;
	}
	return err;
}
/************************************************************
*
* Function name	: tcp_cilent_send_buff
* Description	: cilent 发送数据
* Parameter		: 
* Return		: 
*	
************************************************************/
void tcp_cilent_send_buff(uint8_t *buff, uint16_t len,uint8_t websocket)
{
	switch(lwipdev.client_websocket_id)
	{
		case 1:
			if(websocket)
				tcp_cilent1_send_buff(buff,len);
			else
			{
				if(lwipdev.client2_id == 1) 
				{
					tcp_client2_send_buff(buff,len);
					lwipdev.client2_id = 0;
				}
				if(lwipdev.client3_id == 1) 
				{
					tcp_client3_send_buff(buff,len);
					lwipdev.client3_id = 0;
				}
			}	
			break;
			
		case 2:
			if(websocket)
				tcp_client2_send_buff(buff,len);
			else
			{
				if(lwipdev.client1_id == 1) 
				{
					tcp_cilent1_send_buff(buff,len);
					lwipdev.client1_id = 0;
				}
				if(lwipdev.client3_id == 1) 
				{
					tcp_client3_send_buff(buff,len);
					lwipdev.client3_id = 0;
				}
			}	
			break;
						
		case 3:
			if(websocket)
				tcp_client3_send_buff(buff,len);
			else
			{
				if(lwipdev.client1_id == 1) 
				{
					tcp_cilent1_send_buff(buff,len);
					lwipdev.client1_id = 0;
				}
				if(lwipdev.client2_id == 1) 
				{
					tcp_client2_send_buff(buff,len);
					lwipdev.client2_id = 0;
				}
			}	
			break;			
			
		default:
			if(!websocket)
			{
				if(lwipdev.client1_id == 1) 
				{
					tcp_cilent1_send_buff(buff,len);
					lwipdev.client1_id = 0;
				}
				if(lwipdev.client2_id == 1) 
				{
					tcp_client2_send_buff(buff,len);
					lwipdev.client2_id = 0;
				}
				if(lwipdev.client3_id == 1) 
				{
					tcp_client3_send_buff(buff,len);
					lwipdev.client3_id = 0;
				}
			}
		break;
	}
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
* Description	: client2 发送数据
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
* Description	: client3 发送数据
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
* Function name	: tcp_server_get_link_status
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
* Function name	: tcp_server_get_link_status
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
* Function name	: tcp_server_get_link_status
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
* Function name	: tcp_server_get_link_status
* Description	: 获取TCP连接状态
* Parameter		: 
* Return		: 
*	只有在TCP连接时才可以发送数据
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
* Description	: 客户端停止函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void tcp_client_stop_function(void)
{	
	OS_CPU_SR cpu_sr;
	netconn_close(tcp_cilent1_conn);                            //关闭连接
	netconn_delete(tcp_cilent1_conn); 	//删除连接
	lwipdev.tcp_client1 = 0;   //第 client->num 个设备状态置0 表示客户端未连接
	if(lwipdev.client_websocket_id == 1)
		lwipdev.client_websocket_id = 0;
	                       
	netconn_close(tcp_client2_conn);                            //关闭连接
	netconn_delete(tcp_client2_conn);                           //删除连接
	lwipdev.tcp_client2 = 0;                          //第 client->num 个设备状态置0 表示客户端未连接
	if(lwipdev.client_websocket_id == 2)
		lwipdev.client_websocket_id = 0;
	
	netconn_close(tcp_client3_conn);                            //关闭连接
	netconn_delete(tcp_client3_conn);                           //删除连接
	lwipdev.tcp_client3 = 0;                          //第 client->num 个设备状态置0 表示客户端未连接
	if(lwipdev.client_websocket_id == 3)
		lwipdev.client_websocket_id = 0;
	
	OS_ENTER_CRITICAL();		// 关中断
	OSTaskDel(TCP_CLIENT1_PRIO);	// 删除TCP任务
	OSTaskDel(TCP_CLIENT2_PRIO);	// 删除TCP任务
	OSTaskDel(TCP_CLIENT3_PRIO);	// 删除TCP任务
	OS_EXIT_CRITICAL();			// 开中断
}


