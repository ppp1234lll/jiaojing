#include "lwip_comm.h" 
#include "netif/etharp.h"
#include "lwip/dhcp.h"
#include "ethernetif.h" 
#include "lwip/timers.h"
#include "lwip/tcp_impl.h"
#include "lwip/ip_frag.h"
#include "lwip/tcpip.h" 
#include "malloc.h"
#include "delay.h"
#include <stdio.h>
#include "ucos_ii.h" 
#include "lwip_ping.h"   
#include "httpd.h"
#include "app.h"
#include "dns.h"
#include "gsm.h"
#include "led.h"
#include "lan8720.h"

__lwip_dev lwipdev;							// lwip控制结构体 
struct netif lwip_netif;					// 定义一个全局的网络接口

extern uint32_t memp_get_memorysize(void);	// 在memp.c里面定义
extern uint8_t  *memp_memory;				// 在memp.c里面定义.
extern uint8_t  *ram_heap;					// 在mem.c里面定义.


/////////////////////////////////////////////////////////////////////////////////
//lwip两个任务定义(内核任务和DHCP任务)

//lwip内核任务堆栈(优先级和堆栈大小在lwipopts.h定义了) 
OS_STK * TCPIP_THREAD_TASK_STK;	 


//用于以太网中断调用
void lwip_pkt_handle(void)
{
	ethernetif_input(&lwip_netif);
}

//lwip内核部分,内存申请
//返回值:0,成功;
//    其他,失败
u8 lwip_comm_mem_malloc(void)
{
	u32 mempsize;
	u32 ramheapsize;
	
	/* 先释放内存 */
	lwip_comm_mem_free();
	/* 申请内存 */
	mempsize=memp_get_memorysize();															// 得到memp_memory数组大小
	memp_memory=mymalloc(SRAMIN,mempsize);													// 为memp_memory申请内存
	ramheapsize=LWIP_MEM_ALIGN_SIZE(MEM_SIZE)+2*LWIP_MEM_ALIGN_SIZE(4*3)+MEM_ALIGNMENT;  	// 得到ram heap大小
	ram_heap=mymalloc(SRAMIN,ramheapsize);												 	// 为ram_heap申请内存 
	TCPIP_THREAD_TASK_STK=mymalloc(SRAMIN,TCPIP_THREAD_STACKSIZE*4);						// 给内核任务申请堆栈 
//	LWIP_DHCP_TASK_STK=mymalloc(SRAMIN,LWIP_DHCP_STK_SIZE*4);								// 给dhcp任务堆栈申请内存空间
	if(!memp_memory||!ram_heap||!TCPIP_THREAD_TASK_STK)										// 有申请失败的
	{
		lwip_comm_mem_free();
		return 1;
	}
	return 0;	
}

//lwip内核部分,内存释放
void lwip_comm_mem_free(void)
{ 	
	myfree(SRAMIN,memp_memory);
	myfree(SRAMIN,ram_heap);
	myfree(SRAMIN,TCPIP_THREAD_TASK_STK);
//	myfree(SRAMIN,LWIP_DHCP_TASK_STK);
	
	memp_memory = NULL;
	ram_heap    = NULL;
	TCPIP_THREAD_TASK_STK = NULL;
//	LWIP_DHCP_TASK_STK    = NULL;
	
}
//lwip 默认IP设置
//lwipx:lwip控制结构体指针
void lwip_comm_default_ip_set(__lwip_dev *lwipx)
{
	struct local_ip_t *param;
	
	param = app_get_local_network_function();
	
	/* 获取本地IP */
	lwipx->ip[0] = param->ip[0];	
	lwipx->ip[1] = param->ip[1];
	lwipx->ip[2] = param->ip[2];
	lwipx->ip[3] = param->ip[3];
	
	/* 获取本地子网掩码 */
	lwipx->netmask[0] = param->netmask[0];	
	lwipx->netmask[1] = param->netmask[1];
	lwipx->netmask[2] = param->netmask[2];
	lwipx->netmask[3] = param->netmask[3];
	
	/* 获取默认网关 */
	lwipx->gateway[0] = param->gateway[0];	
	lwipx->gateway[1] = param->gateway[1];
	lwipx->gateway[2] = param->gateway[2];
	lwipx->gateway[3] = param->gateway[3];	
	
	/* 获取MAC */
	lwipx->mac[0] = param->mac[0]; 	// 高三字节(IEEE称之为组织唯一ID,OUI)地址固定为:2.0.0
	lwipx->mac[1] = param->mac[1];
	lwipx->mac[2] = param->mac[2];
	lwipx->mac[3] = param->mac[3]; 	// 低三字节用STM32的唯一ID
	lwipx->mac[4] = param->mac[4];
	lwipx->mac[5] = param->mac[5]; 
	
	lwipx->dns[0] = param->dns[0];
	lwipx->dns[1] = param->dns[1];
	lwipx->dns[2] = param->dns[2];
	lwipx->dns[3] = param->dns[3];
	

} 

/************************************************************
*
* Function name	: lwip_get_remote_ip
* Description	: 获取服务器IP
* Parameter		: 
* Return		: 
*	
************************************************************/
void lwip_get_remote_ip(uint8_t *ip)
{
	ip[0] = lwipdev.remoteip[0];
	ip[1] = lwipdev.remoteip[1];
	ip[2] = lwipdev.remoteip[2];
	ip[3] = lwipdev.remoteip[3];
}

/************************************************************
*
* Function name	: lwip_updata_remote_network_infor
* Description	: 更新远端参数
* Parameter		: 
* Return		: 
*	
************************************************************/
void lwip_updata_remote_network_infor(__lwip_dev *lwipx)
{

}

//LWIP初始化(LWIP启动的时候使用)
//返回值:0,成功
//      1,内存错误
//      2,LAN8720初始化失败
//      3,网卡添加失败.
u8 lwip_comm_init(void)
{
//	OS_CPU_SR 	   cpu_sr;
	struct netif   *Netif_Init_Flag;	// 调用netif_add()函数时的返回值,用于判断网络初始化是否成功
	struct ip_addr ipaddr;  			// ip地址
	struct ip_addr netmask; 			// 子网掩码
	struct ip_addr gw;      			// 默认网关 
	
	if(ETH_Mem_Malloc())
	{
		return 1;						// 内存申请失败
	}
	if(lwip_comm_mem_malloc())
	{
		return 1;						// 内存申请失败
	}
	if(LAN8720_Init())
	{
		return 2;						// 初始化LAN8720失败
	}	
	/* 初始化网络 */
	tcpip_init(NULL,NULL);
	lwip_comm_default_ip_set(&lwipdev);	//设置默认IP等信息
	Netif_Init_Flag = netif_add(&lwip_netif,&ipaddr,&netmask,&gw,NULL,&ethernetif_init,&tcpip_input);//向网卡列表中添加一个网口
	if(Netif_Init_Flag==NULL)
	{
		lwipdev.init = 0;
		return 3;
	}
	else
	{
		netif_set_default(&lwip_netif); // 设置netif为默认网口
		lwipdev.init = 1;
	}
	
	/* 启动网络功能 */
	igmp_init();
	httpd_init();
	icmp_pcb_init();
	dns_init();
	
	return 0;//操作OK.
}   

/************************************************************
*
* Function name	: lwip_start_function
* Description	: 网络启动函数
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t lwip_start_function(void)
{
	if(lwipdev.init == 0) {
		lwip_comm_init();
	}
	
	if(lwipdev.init == 1) {
		ETH_MACDMA_Config(); 					// 重新配置MAC和DMA
		lwip_comm_default_ip_set(&lwipdev);		// 设置默认IP等信息
		
		IP4_ADDR(&lwip_netif.ip_addr,lwipdev.ip[0],lwipdev.ip[1],lwipdev.ip[2],lwipdev.ip[3]);
		IP4_ADDR(&lwip_netif.netmask,lwipdev.netmask[0],lwipdev.netmask[1] ,lwipdev.netmask[2],lwipdev.netmask[3]);
		IP4_ADDR(&lwip_netif.gw,lwipdev.gateway[0],lwipdev.gateway[1],lwipdev.gateway[2],lwipdev.gateway[3]);
		
		netif_set_up(&lwip_netif);				// 打开netif网口
		
		led_control_function(LD_LAN,LD_ON);
		lwipdev.netif_state = 1;
	}
	
	return 0;
}

/************************************************************
*
* Function name	: lwip_stop_function
* Description	: 网络停止函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void lwip_stop_function(void)
{
	if(lwipdev.init == 1) 
	{
		if(lwipdev.netif_state == 1) 
		{
			lwipdev.netif_state = 0;
			netif_set_down(&lwip_netif); // 关闭网卡
		}
	}
}


/************************************************************
*
* Function name	: lwip_get_mac_addr
* Description	: 获取网卡MAC地址
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t *lwip_get_mac_addr(void)
{
	static uint8_t temp[20] = {0};
	
	sprintf((char*)temp,"%02x-%02x-%02x-%02x-%02x-%02x",lwipdev.mac[0],
														lwipdev.mac[1],
														lwipdev.mac[2],
														lwipdev.mac[3],
														lwipdev.mac[4],
														lwipdev.mac[5]);
	
	return temp;
}










