#include "includes.h"
#include "update.h"
#include "gsm.h"
#include "GPRS.h"
#include "bootload.h"
#include "iwdg.h"
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
	update_param_t *sg_updateparam_t = update_get_infor_data_function();
	
	my_modem_recevie_file_init(GSM_CHUNK_SIZE);
	update_count_time = 0;	
	while(1) 
	{
		if(sg_updateparam_t->mode != UPDATE_MODE_NULL && (gsm_get_network_connect_status_function() == 1 ))
		{
			my_modem_recieve_gprs_deal(&my_modem);
		}
		
		/* 检测运行标志位 */
		if( sg_updateparam_t->error == 1 ||sg_updateparam_t->success == 1 ||sg_updateparam_t->end == 1) 
		{
			goto UP_ERROR;
		}
					
		update_count_time ++;
		if(update_count_time >= 120000)  // 20min后还在升级阶段,系统重启
		{
			update_count_time = 0;
			System_SoftReset();
		}
		OSTimeDlyHMSM(0,0,0,10);  			
		
		IWDG_Feed();
	}
	
UP_ERROR: // 更新失败
	/* 发生更新错误 */
	sg_updateparam_t->error	 = 0;
	sg_updateparam_t->success = 0;
	sg_updateparam_t->end = 0;
	sg_updateparam_t->mode	 = UPDATE_MODE_NULL;
	gsm_reset_task_function();  	
	my_modem_detection_status();

	return -1;
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



