#include "https.h"
#include "includes.h"
#include "http_com.h"
#include "lwip_comm.h"
#include "netif/etharp.h"
#include "ethernetif.h" 
#include "app.h"
#include "det.h"
#include "tcp_server.h"

/************************************************************
*
* Function name	: http_task_function
* Description	: httpº¯Êý
* Parameter		: 
* Return		: 
*	
************************************************************/
void http_task_function(void)
{
	while(1)
	{
		com_deal_http_info_function();
		app_http_com_send_function();
	
		OSTimeDlyHMSM(0,0,0,10);  // ÑÓÊ±20ms
	}
}






















