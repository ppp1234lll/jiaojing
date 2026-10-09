#include "includes.h"
#include "update.h"
#include "gsm.h"
#include "GPRS.h"
#include "bootload.h"
#include "iwdg.h"
#include "http_update.h"
#include <queue.h>

queue_s	 sg_queue_updata =	{0};	// 更新
uint32_t update_count_time = 0;
/************************************************************
*
* Function name	: update_mobile_task_function
* Description	: 更新函数 - Mobile network
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t update_mobile_task_function(void)
{
	update_param_t *updateparam = NULL;
	int8_t  ret = 0;
	ip_addr_t server_ipaddr;
	uint16_t  server_port;
	////

	/* 断开当前4G连接, 准备升级 */
	gprs_disconnect();
	led_control_function(LD_GPRS, LD_OFF);

	/* 每次升级前取最新升级服务器地址/端口 */
	updateparam = update_get_infor_data_function();
	server_port = updateparam->port;
	IP4_ADDR(&server_ipaddr, updateparam->ip[0], updateparam->ip[1], updateparam->ip[2], updateparam->ip[3]);

	sg_http_update_param.section_len = (UPDATE_CHUNK_SIZE - 2);
	sg_http_update_param.http_response_recv_size = 0;

	/* 1: 获取 info.txt, 比较版本号 */
	ret = http_update_get_info_txt_by_gprs(&server_ipaddr, server_port);
	if( (ret < 0) || (ret == 2) )
	{
		if(ret < 0){ printf("\n获得info.txt信息,失败! ret: %d\n", ret); }
		else{ printf("\n版本是最新版本,无需更新!\n"); }
		goto UPDATE_END;
	}

	/* 2: 获取升级文件大小 */
	ret = http_update_get_crc_bin_file_size_by_gprs();
	if(ret < 0)
	{
		printf("\n获得crc_bin文件大小,失败! ret: %d\n", ret);
		goto UPDATE_END;
	}

	/* 3: 分块(Range)下载升级文件 */
	ret = http_update_get_crc_bin_file_data_by_gprs();
	if(ret < 0)
	{
		printf("\n获得crc_bin文件内容,失败! ret: %d\n", ret);
		goto UPDATE_END;
	}

	/* 升级完成, 写升级参数并重启设备 */
	printf("\n升级完成,重启设备...\n");
	http_update_success_reboot();

	ret = 0;

UPDATE_END:
	updateparam->error = 0;
	if(ret < 0)
	{
		printf("\n升级失败, ret: %d\n", ret);
		updateparam->success = 0;
		http_update_failed();     // 写Flash记录失败状态
	}
	else{ updateparam->success = 1; }
	updateparam->end  = 1;
	updateparam->mode = UPDATE_MODE_NULL;  // 升级结束, 退出升级模式

	led_control_function(LD_GPRS, LD_OFF);

	/* 升级结束, 触发4G业务链路重新连接 */
	gsm_set_network_reset_function();

	if(ret < 0){ return(-1); }
	return(0);
}

/************************************************************
*
* Function name	: update_gsm_recevie_data_function
* Description	: 更新数据接收函数 - 无线网络
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t update_gsm_recevie_data_function(uint8_t *buff, uint16_t len)
{
	uint8_t *p1    = NULL;
	int temp1,temp2;
	
	if(update_detection_status_function() == UPDATE_MODE_GPRS && (gsm_get_network_connect_status_function() == 1 ))
	{
		p1 = (uint8_t*)strstr((char*)buff,"+MIPURC: \"rtcp\"");
		if(p1!=0 ) 
		{
			p1 = (uint8_t*)strstr((char*)p1,"\"rtcp\"");
			sscanf((char*)p1,"\"rtcp\",%d,%d",&temp1,&temp2);
			p1 = (uint8_t*)strstr((char*)p1,",");
			p1 = (uint8_t*)strstr((char*)p1+1,",");
			p1 = (uint8_t*)strstr((char*)p1+1,",");
			if(p1 !=NULL)
				Enqueue_Bytes_From_Buffer(p1+1,&sg_queue_updata,temp2); // 存入队列
			else
				return -1;
			
			return temp2;
		}
	}
	return -1;
}



