/********************************************************************************
*
* @File name  ：udp.c
* @Description：udp操作
* @Author     ：编号9527
* Version Date       Modification Description
* 1.0     2019-08-16 1.udp初始化并接收广播包
*
********************************************************************************/
#include "lwip_comm.h" 
#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "lwip/udp.h"
#include "lwip/igmp.h"

#include "udp_broadcast.h"
#include "start.h"
#include "appconfig.h"
#include "app.h"
#include "malloc.h"

#define UDP_REPORT_START ("<?xml version=\"1.0\" encoding=\"UTF-8\" ?><ProbeMatch><Types>inquiry</Types>")
#define UDP_REPORT_END   ("</ProbeMatch>\r\n")

//const char test_udp_infor[] = {
//"<?xml version=\"1.0\" encoding=\"UTF-8\" ?><ProbeMatch><Types>inquiry</Types><Uuid>00000000-0DE1-48DB-97E3-40B106467932</Uuid><DeviceDescription>CS-C6TC-32WFR</DeviceDescription><DeviceSN>CS-C6TC-32WFR0120170327CCCH738231995</DeviceSN><CommandPort>8000</CommandPort><MAC>54-c4-15-9a-38-74</MAC><IPv4Address>172.20.20.100</IPv4Address><IPv4SubnetMask>255.255.255.0</IPv4SubnetMask><IPv4Gateway>172.20.20.254</IPv4Gateway><SoftwareVersion>V5.2.3build 180804</SoftwareVersion></ProbeMatch>\r\n"
//};

/************************************************************
*
* Function name	: udp_create_report_infor_function
* Description	: 生成查询回传包
* Parameter		: 
* Return		: 
*	
************************************************************/
uint16_t udp_create_report_infor_function(char *buff)
{
	struct local_ip_t *local_t = app_get_local_network_function();
	uint16_t len = 0;
	uint8_t cpu_id[25] = {0};
	char  buff_uuid[]  = {"<Uuid>00000000-0DE1-48DB-97E3-40B106467932</Uuid>"};
	char  buff_sn[]	  = {"<DeviceSN>24003C3437510A34373635</DeviceSN>"};
	char  buff_device[] = {"<DeviceDescription>FN1110-L</DeviceDescription>"};
	char  buff_soft[]   = {"<SoftwareVersion>FN1110-L-1.0.0.200604</SoftwareVersion>"};
	
	char  buff_mac[]	= {"<MAC>54-c4-15-9a-38-72</MAC>"};
	char  buff_ip[]		= {"<IPv4Address>172.200.200.100</IPv4Address>"};
	char  buff_mask[]	= {"<IPv4SubnetMask>255.255.255.000</IPv4SubnetMask>"};
	char  buff_gateway[] = {"<IPv4Gateway>172.200.200.254</IPv4Gateway>"};
	
	start_get_device_id_str(cpu_id);
	/* 设备序列号 */
	memset(buff_sn,0,sizeof(buff_sn));
	sprintf(buff_sn,"<DeviceSN>%s</DeviceSN>",cpu_id);
	/* 设备序列号-UUID */
	memset(buff_uuid,0,sizeof(buff_uuid));
	sprintf(buff_uuid,"<Uuid>00000000-%c%c-%c%c-%c%c-%s</Uuid>",cpu_id[0],cpu_id[1],cpu_id[2],\
																cpu_id[3],cpu_id[4],cpu_id[5],&cpu_id[6]);
	/* 硬件版本号 */
	memset(buff_device,0,sizeof(buff_device));
	sprintf(buff_device,"<DeviceDescription>%s</DeviceDescription>",HARD_NO_STR);
	/* 软件版本号 */
	memset(buff_soft,0,sizeof(buff_soft));
	sprintf(buff_soft,"<SoftwareVersion>%s</SoftwareVersion>",SOFT_NO_STR);
	
	/* MAC地址 */
	memset(buff_mac,0,sizeof(buff_mac));
	sprintf(buff_mac,"<MAC>%02x-%02x-%02x-%02x-%02x-%02x</MAC>",local_t->mac[0],\
																local_t->mac[1],\
																local_t->mac[2],\
																local_t->mac[3],\
																local_t->mac[4],\
																local_t->mac[5]);
																
	/* 本地IP */
	memset(buff_ip,0,sizeof(buff_ip));
	sprintf(buff_ip,"<IPv4Address>%d:%d:%d:%d</IPv4Address>", local_t->ip[0],\
																local_t->ip[1],\
																local_t->ip[2],\
																local_t->ip[3]);
	/* 掩码 */
	memset(buff_mask,0,sizeof(buff_mask));
	sprintf(buff_mask,"<IPv4SubnetMask>%d:%d:%d:%d</IPv4SubnetMask>", local_t->netmask[0],\
																	  local_t->netmask[1],\
																	  local_t->netmask[2],\
																	  local_t->netmask[3]);
	/* 网关 */
	memset(buff_gateway,0,sizeof(buff_gateway));
	sprintf(buff_gateway,"<IPv4Gateway>%d:%d:%d:%d</IPv4Gateway>", local_t->gateway[0],\
																   local_t->gateway[1],\
																   local_t->gateway[2],\
																   local_t->gateway[3]);
																
	sprintf(buff,"%s%s%s%s%s%s%s%s%s%s",UDP_REPORT_START,\
							  buff_uuid,buff_sn,buff_device,buff_soft,\
							  buff_mac,buff_ip,buff_mask,buff_gateway,\
							  UDP_REPORT_END);
	len = strlen(buff);
	return len;
}

/********************************************************************************
*
* Function name   ：
* Description     ：
* Parameter       ：
* Return          ：
*
********************************************************************************/
void udp_demo_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
					   ip_addr_t *addr, u16_t port)
{
	struct pbuf *q = NULL;
	struct ip_addr my_ipaddr;
	//uint8_t *temp = (uint8_t *)addr;
	char  *buff = NULL;
	uint16_t len  = 0;
	
	buff = mymalloc(SRAMIN,600);
	memset(buff,0,600);
	
//	IP4_ADDR(&my_ipaddr, temp[0], temp[1], temp[2], temp[3]); 	// 保存源IP
	IP4_ADDR(&my_ipaddr,239,255,255,250);
	if (strstr(p->payload, "<Types>inquiry</Types>"))
	{
		len = udp_create_report_infor_function(buff);
		
//		len = strlen(test_udp_infor);
		q = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
		memset(q->payload, 0, len);
		memcpy(q->payload,buff,len);
		
		udp_sendto(pcb, q, &my_ipaddr, port); 				// 将报文返回给原主机
	}
	myfree(SRAMIN,buff);
	pbuf_free(q);
	pbuf_free(p);
}

/********************************************************************************
*
* Function name   ：
* Description     ：
* Parameter       ：
* Return          ：
*
********************************************************************************/
int32_t udp_broadcast_init(void)
{
	#if 0
	struct ip_addr ipaddr;
	struct udp_pcb *pcb;

	pcb	= udp_new();
	if(pcb == NULL)	 // 申请失败
	{
		return 1;
	}
	else
	{
		IP4_ADDR(&ipaddr,239,0,1,2);
		if(udp_bind(pcb, &ipaddr, 37020) == ERR_OK ) // 为本地IP绑定端口，IP_ADDR_ANY为0，其实说明使用本地IP地址，推荐优先使用。因为DHCP情况下，我们是无法事先知道IP的。
		{
			udp_recv(pcb, udp_demo_callback, NULL);     // 注册报文处理回调				
		}
		else
		{
			return 1;
		}
	}
	return 0;
	#endif
	err_t  err;
	struct ip_addr ipaddr;
	struct udp_pcb *pcb;
		

	
	IP4_ADDR(&ipaddr,239,255,255,250);
	err = igmp_joingroup(IP_ADDR_ANY,&ipaddr);
	if(err != ERR_OK) {
		return 1;
	}
	
	pcb	= udp_new();
	if(pcb == NULL)	 // 申请失败
	{
		return 1;
	}
	else
	{
		if(udp_bind(pcb, IP_ADDR_ANY, 37020) == ERR_OK ) // 为本地IP绑定端口，IP_ADDR_ANY为0，其实说明使用本地IP地址，推荐优先使用。因为DHCP情况下，我们是无法事先知道IP的。
		{
			udp_recv(pcb, udp_demo_callback, NULL);     // 注册报文处理回调				
		}
		else
		{
			return 1;
		}
	}
	
	return 0;
}



