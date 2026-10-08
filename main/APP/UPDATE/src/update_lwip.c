#include "update.h"
#include "iap.h"
#include "bootload.h"
#include "lwip/opt.h"
#include "lwip_comm.h"
#include "lwip/lwip_sys.h"
#include "lwip/api.h"
#include "lwip/tcp.h"
#include "lwip/tcp_impl.h"
#include "gsm.h"
#include "eth.h"
#include "malloc.h"
#include "iwdg.h"

static update_param_t *sg_updateparam_t;
struct netconn *tcp_update = NULL;
struct netbuf 	 *recvbuf = NULL;

/************************************************************
*
* Function name	: update_lwip_network_connect_function
* Description	: tcp连接函数
* Parameter		: 
* Return		: 
*	
************************************************************/
static int8_t update_lwip_network_connect_function(ip_addr_t ip, uint16_t port)
{
	uint8_t index = 0;
	err_t	err;
	
	for(index=0; index<5; index++) {
		tcp_update = netconn_new(NETCONN_TCP);
		if( tcp_update == NULL ) {
			continue;
		}
		err = netconn_connect(tcp_update,&ip,port);
		if(err != ERR_OK) {
			netconn_delete(tcp_update);
			tcp_update = NULL;
			continue;
		} else {
			sg_updateparam_t->tcp_t.connect = 1;
			tcp_update->recv_timeout = 10;
			sg_updateparam_t->tcp_t.state = 2;
			return 0;
		}
	}
	/* tcp连接失败 */
	eth_set_network_reset();
	
	return -1;
}

/************************************************************
*
* Function name	: update_lwip_rec_data_function
* Description	: 接收数据读取函数
* Parameter		: 
* Return		: 
*	
************************************************************/
static int8_t update_lwip_rec_data_function(void)
{
	uint8_t   *data;
	struct pbuf *q;
	uint16_t  index = 0;
	uint16_t  data_len = 0;
	OS_CPU_SR cpu_sr;
	err_t	  recv_err;

	if((recv_err = netconn_recv(tcp_update,&recvbuf)) == ERR_OK) {
		OS_ENTER_CRITICAL(); //关中断
		for(q=recvbuf->p; q!=NULL; q=q->next)  //遍历完整个pbuf链表
		{
			//判断要拷贝到TCP_CLIENT_RX_BUFSIZE中的数据是否大于TCP_CLIENT_RX_BUFSIZE的剩余空间，如果大于
			//的话就只拷贝TCP_CLIENT_RX_BUFSIZE中剩余长度的数据，否则的话就拷贝所有的数据
			if(q->len > (LWIP_CHUNK_SIZE+11 - data_len)) {
				data = q->payload;
				for(index=0; index<(LWIP_CHUNK_SIZE+11 - data_len); index++) {
					my_modem_receive_task(data[index],&my_modem);
				}
			}else {
				data = q->payload;
				for(index=0; index<q->len; index++) {
					my_modem_receive_task(data[index],&my_modem);
				}
			}
			
			data_len += q->len;  	
			if(data_len > (LWIP_CHUNK_SIZE+11)) {
				break; // 超出TCP客户端接收数组,跳出	
			}
		}
		OS_EXIT_CRITICAL();  //开中断	
		netbuf_delete(recvbuf);
		recvbuf = NULL;
	} else if(recv_err == ERR_CLSD) {
		netconn_close(tcp_update);
		netconn_delete(tcp_update);
		tcp_update = NULL;
		sg_updateparam_t->tcp_t.connect = 0;
		sg_updateparam_t->tcp_t.state   = 1;	// 重新连接
		
		return -1;
	}
	
	return 0;
}

/************************************************************
*
* Function name	: update_lwip_end_function
* Description	: 更新结束函数
* Parameter		: 
* Return		: 
*	
************************************************************/
static void update_lwip_end_function(void)
{
	/* 结束有线网络更新 */
	sg_updateparam_t->mode = UPDATE_MODE_NULL; 	// 更新失败
	/* 释放TCP */
	netconn_close(tcp_update);
	netconn_delete(tcp_update);
	tcp_update = NULL;
	sg_updateparam_t->end = 1;
}

/************************************************************
*
* Function name	: update_lwip_task_function
* Description	: 更新-有线网络-程序
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t update_lwip_task_function(void)
{
	ip_addr_t server_ipaddr;
	uint16_t  server_port;
	int8_t    ret = 0;
	
	/* 初始化参数 */
	sg_updateparam_t = update_get_infor_data_function();
	server_port = sg_updateparam_t->port;
	IP4_ADDR( &server_ipaddr,sg_updateparam_t->ip[0],sg_updateparam_t->ip[1],\
				sg_updateparam_t->ip[2],sg_updateparam_t->ip[3]);
	
	/* 连接更新服务器 */
	ret = update_lwip_network_connect_function(server_ipaddr,server_port);
	if(ret != 0) {
		goto UP_ERROR;
	}	
	/* 开始获取数据 */
	while(1) {
		ret = update_lwip_rec_data_function();
		if(ret != 0) {
			goto UP_ERROR;
		}
		
		/* 处理数据 */
		if((update_get_mode_function() == UPDATE_MODE_LWIP) &&(sg_updateparam_t->tcp_t.connect == 1))
		{
			my_modem_recieve_lwip_deal(&my_modem);
		}
			 
		/* 检测运行标志位 */
		if( sg_updateparam_t->error == 1 ||sg_updateparam_t->success == 1 || sg_updateparam_t->end == 1) {
			goto UP_ERROR;
		}
		OSTimeDlyHMSM(0,0,0,10);  			
		IWDG_Feed();
	}

UP_ERROR:
	sg_updateparam_t->error	  = 0;
	sg_updateparam_t->success = 0;
	sg_updateparam_t->end     = 0;
	sg_updateparam_t->mode	  = UPDATE_MODE_NULL;
	
	my_modem_detection_status();
	update_lwip_end_function();
	
	return -1;
}

/************************************************************
*
* Function name	: update_tcp_send_function
* Description	: tcp发送函数
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t update_tcp_send_function(uint8_t *buff, uint16_t len) 
{
	err_t err;
	
	err = netconn_write( tcp_update ,\
						 buff,\
						 len,\
						 NETCONN_COPY); 
	return err;
	
}

