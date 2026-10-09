#include "update.h"
#include "iap.h"
#include "bootload.h"
#include "includes.h"
#include "lwip/tcp_impl.h"
#include "gsm.h"
#include "eth.h"
#include "malloc.h"
#include "lwip/opt.h"
#include "lwip_comm.h"
#include "lwip/lwip_sys.h"
#include "lwip/api.h"
#include "lwip/tcp.h"
#include "save.h"
#include "iwdg.h"
#include "app.h"
#include "com.h"
#include "http_update.h"
#include <queue.h>

update_param_t sg_updateparam_t = {
	.ip={114,115,219,15},
	.port = 54323,
};

/************************************************************
*
* Function name	: update_get_infor_data_function
* Description	: 获取更新数据信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void *update_get_infor_data_function(void)
{
	return &sg_updateparam_t;
}

/************************************************************
*
* Function name	: update_get_mode_function
* Description	: 获取更新状态
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t update_get_mode_function(void)
{
	return sg_updateparam_t.mode;
}

/************************************************************
*
* Function name	: update_update_error
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
void update_error_occurred_in_function(void)
{
	sg_updateparam_t.error = 1;
}

/************************************************************
*
* Function name	: update_addr_ip
* Description	: 获取更新地址端口ip
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t *update_addr_ip(void)
{
	static uint8_t ip[20] = {0};
	
	sprintf((char*)ip,"%d.%d.%d.%d",sg_updateparam_t.ip[0],sg_updateparam_t.ip[1],sg_updateparam_t.ip[2],sg_updateparam_t.ip[3]);
	
	return ip;
}

/************************************************************
*
* Function name	: update_addr_port
* Description	: 获取更新地址端口
* Parameter		: 
* Return		: 
*	
************************************************************/
uint32_t update_addr_port(void)
{
	return sg_updateparam_t.port;
}

/************************************************************
*
* Function name	: update_set_update_addr
* Description	: 设置更新地址
* Parameter		: 
* Return		: 
*	
************************************************************/
void update_set_update_addr(uint8_t *ip,uint32_t port)
{
	sg_updateparam_t.ip[0] = ip[0];
	sg_updateparam_t.ip[1] = ip[1];
	sg_updateparam_t.ip[2] = ip[2];
	sg_updateparam_t.ip[3] = ip[3];
	
	sg_updateparam_t.port = port;
	app_set_save_infor_function(SAVE_UPDATE);
}

/************************************************************
*
* Function name	: update_save_addr
* Description	: 存储更新地址
* Parameter		: 
* Return		: 
*	
************************************************************/
void update_save_addr(void)
{
	save_stroage_update_addr(sg_updateparam_t.ip,sg_updateparam_t.port);
}

/************************************************************
*
* Function name	: update_read_addr
* Description	: 读取更新地址
* Parameter		: 
* Return		: 
*	
************************************************************/
void update_read_addr(void)
{
	save_read_update_addr(sg_updateparam_t.ip,&sg_updateparam_t.port);
}

/************************************************************
*
* Function name	: update_set_update_mode
* Description	: 设置更新方式
* Parameter		: 
* Return		: 
*	
************************************************************/
void update_set_update_mode(uint8_t mode)
{
	sg_updateparam_t.mode = mode;
}


/************************************************************
*
* Function name	: update_detection_status_function
* Description	: 监测更新方式
* Parameter		: 
* Return		: 
*	
************************************************************/
uint8_t update_detection_status_function(void)
{
	return sg_updateparam_t.mode;
}

/************************************************************
*
* Function name	: update_end_function
* Description	: 更新打断函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void update_end_function(void)
{
	sg_updateparam_t.end = 1;
}

/************************************************************
*
* Function name	: update_status_detection
* Description	: 开机检测上次升级结果并上报平台(无线HTTP升级)
* Parameter		: 
* Return		: 
*	
************************************************************/
void update_status_detection(void)
{
	struct BOOT_UPDATE_PARAM boot_update_param = {0};
	////

	W25QXX_Read((uint8_t*)(&boot_update_param), UPDATA_PARAM_ADDR, sizeof(struct BOOT_UPDATE_PARAM));

	switch(boot_update_param.update_status)
	{
		case BOOT_UPDATE_SUCCESS:
			/* 上报: 升级成功 */
			app_set_reply_parameters_function(CONFIGURE_UPDATE_SYSTEM, 0x01);
			http_update_clear_param();
			break;

		case BOOT_UPDATE_FAILED:
			/* 上报: 升级失败 */
			app_set_reply_parameters_function(CONFIGURE_UPDATE_SYSTEM, 0x00);
			http_update_clear_param();
			break;

		default:
			break;
	}
}

