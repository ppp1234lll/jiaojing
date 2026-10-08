#include "bootload.h"
#include "save.h"
#include "includes.h"
#include "update.h"
#include "iap.h"
#include "crc.h"
#include "bsp.h"
#include "malloc.h"
#include "tcp_client.h"
#include "iwdg.h"
#include "gsm.h"
#include "save.h"
#include "w25qxx.h"
#include "lfs_port.h"
#include <queue.h>

const uint8_t DATA_HEAD[5] = {'s','t','a','r','t'};//帧头
const uint8_t DATA_END[3] = {'e','n','d'};         //所有数据结束
static uint8_t ackData[10] = {0x7F,0xF7,0xDA,0,0,0,0,0x57,0x75,0x00};//响应获取数据包

extern queue_s	 sg_queue_updata;
uint8_t gsm_recv_init = 0; // 无线接收标志

uint8_t update_error_recv = 0;

//定义且初始化接收结构体
MYMODEM my_modem = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	ApplicationAddress,
	0
};

/*------------------------------------------------------------------------------
函数名称：my_modem_receive_task
函数功能：数据接收任务
入口参数: uint8_t data：接收数据 MYMODEM *modem：数据接收结构体
出口参数：
备注：
-----------------------------------------------------------------------------*/
void my_modem_receive_task(uint8_t data, MYMODEM *modem)
{
	if( update_get_mode_function() == UPDATE_MODE_LWIP)
	{
		my_modem_timer_clear;   // 计数器清空
		if(modem->receive_state == 0) // 接收未完成
		{
			if(modem->receive_count < LWIP_CHUNK_SIZE + 11)  // 数据不大于一包数据大小
			{
				my_modem_timer_clear;   // 计数器清空
				if(modem->receive_count == 0){
					my_modem_timer_start; // 当开始接收数据时，打开定时器
				}
				modem->receive_buf[modem->receive_count++] = data;   // 保存数据
			}
			else  // 超过一包数据大小
			{
				modem->receive_state = 1; // 数据接收完成
			}
		}	
	}
	else
	{
		if(modem->receive_state == 0) // 接收未完成
		{
			modem->receive_buf[modem->receive_count++] = data;   //保存数据
		}
		if(modem->receive_count >= GSM_CHUNK_SIZE + 11)  //数据不大于一包数据大小
		{
			modem->receive_state = 1; //数据接收完成
		}
	}
}


/*------------------------------------------------------------------------------
函数名称：my_modem_receive_timeout
函数功能：数据接收超时处理
入口参数: MYMODEM *modem：数据接收结构体
出口参数：
备注：
-----------------------------------------------------------------------------*/
void my_modem_receive_timeout(MYMODEM *modem)
{
	modem->receive_state = 1;  //超时接收完成标志强制置一
}

/*------------------------------------------------------------------------------
函数名称：my_modem_send_ack
函数功能：数据回应
入口参数: uint8_t state：回应状态
出口参数：
备注：
-----------------------------------------------------------------------------*/
uint8_t my_modem_send_ack(MYMODEM *modem, uint8_t state)
{
	static uint8_t  flag = 0;
	static uint16_t cktime=500;			// 检测5s反馈	
	uint8_t  		sta=1;
	uint16_t 		sum = 0;

	switch(flag) {
		case 1:
			cktime--;
			if(cktime == 0) {
				flag = 0;
				sta = 1;
				break;
			}
			if(my_modem.receive_state) {
				flag = 0;
				sta = 0;
				cktime = 0;
				break;
			}
			sta = 2;
			break;
		default:
			ackData[3] = modem->packet_index & 0xff;  		 //数据包低8位
			ackData[4] = (modem->packet_index >> 8) & 0xff;  //数据包高8位

			ackData[5] = state;

			for(uint8_t i = 0; i < 4; i++) sum += ackData[2+i];  //计算校验和
			ackData[6] = sum & 0xff;

			modem->receive_state = 0; //接收下一包数据
			modem->receive_count = 0; //已接收数据量清零

			/* 发送函数 */
			update_tcp_send_function(ackData, 9);
			/* 数据标记 */
			flag = 1;
			cktime = 500;
			break;
	}
	
	return sta;
}

/*------------------------------------------------------------------------------
函数名称：my_modem_crc
函数功能：获取crc校验
入口参数: MYMODEM *modem：数据接收结构体
          uint16_t start：校验起始地址 uint16_t end：校验结束地址
出口参数：
备注：
-----------------------------------------------------------------------------*/
uint16_t my_modem_crc(MYMODEM *modem,uint16_t start,uint16_t end)
{
	int i, j, flag;
	uint16_t crc_val = 0xFFFF;

	for (i = start; i < end; i++)
	{
		crc_val ^= *(modem->receive_buf + i);
		for(j = 0; j < 8; j++)
		{
			flag = crc_val&0x01;
			crc_val >>= 1;
			if(1 == flag)crc_val ^= 0xA001;
		}
	}
	return crc_val;
}

//uint32_t iap_buf[512+10] = {0};  //开辟(512+10)*4>2k的数组内存

save_param_t g_saveparam_t = {0};
static uint8_t sg_file_init_t;
/************************************************************
*
* Function name	: my_modem_recevie_file_init
* Description	: 接收初始化
* Parameter		: 
* Return		: 
*	
************************************************************/
void my_modem_recevie_file_init(uint16_t size)
{
	Init_Queue(&sg_queue_updata);
	g_saveparam_t.bin_chunk = 0;
	g_saveparam_t.bin_size = size;
	gsm_recv_init = 0;
}

/************************************************************
*
* Function name	: my_modem_receive_file_end
* Description	: 接收结束
* Parameter		: 
* Return		: 
*	
************************************************************/
void my_modem_receive_file_end(uint8_t status) 
{
	/* 检测更新状态 */
	if(status == 1) 
	{
		if(my_modem.packet_index == g_saveparam_t.bin_chunk)
		{
			g_saveparam_t.iap_update_flag = 1;
			save_stroage_update_file_infor_function(g_saveparam_t);
		}
		else
			g_saveparam_t.iap_update_flag = 0;
	}
	sg_file_init_t = 0;
}

/************************************************************
*
* Function name	: my_modem_detection_status
* Description	: 更新状态监测
* Parameter		: 
* Return		: 
*	
************************************************************/
void my_modem_detection_status(void)
{
	if(sg_file_init_t == 1) {
		sg_file_init_t = 0;
	}
}

/************************************************************
*
* Function name	: my_modem_detcet_update_status_function
* Description	: 检测更新状态
* Parameter		: 
* Return		: 
*	
************************************************************/
void my_modem_detcet_update_status_function(void)
{
	/* 检测是否更新完成 */
	if (g_saveparam_t.iap_update_flag == 2) 
	{
		/* 更新完成 */
		g_saveparam_t.iap_update_flag = 0;
		save_stroage_update_file_infor_function(g_saveparam_t);
	}
}

/************************************************************
*
* Function name	: my_modem_init
* Description	: 初始化
* Parameter		: 
* Return		: 
*	
************************************************************/
void my_modem_init(void)
{
	memset(&g_saveparam_t,0,sizeof(g_saveparam_t));
	save_read_update_file_infor_function(&g_saveparam_t);
	my_modem_detcet_update_status_function();
}

//uint8_t sg_appbuf[1024] = {0}; 
/*------------------------------------------------------------------------------
函数名称：my_modem_receive_one_pack_deal
函数功能：处理包数据
入口参数: MYMODEM *modem：数据接收结构体
出口参数：
备注：
-----------------------------------------------------------------------------*/
void my_modem_receive_one_pack_deal(MYMODEM *modem)
{
	OS_CPU_SR 	cpu_sr;
	uint16_t	crc16	 = 0;
	uint32_t 	write_addr =  UPDATA_SPIFLASH_ADDR;	
	
	if(update_get_mode_function() == UPDATE_MODE_LWIP)
		write_addr = UPDATA_SPIFLASH_ADDR + (g_saveparam_t.bin_chunk) * LWIP_CHUNK_SIZE;
	else if(update_get_mode_function() == UPDATE_MODE_GPRS)
		write_addr = UPDATA_SPIFLASH_ADDR + (g_saveparam_t.bin_chunk) * GSM_CHUNK_SIZE;

	OS_ENTER_CRITICAL();// 关中断
	if( update_get_mode_function() == UPDATE_MODE_LWIP )
	{
		W25QXX_Write(&modem->receive_buf[11],write_addr,modem->receive_pack_len);
		OS_EXIT_CRITICAL();// 开中断
		crc16 = usMBCRC16((u8*)&modem->receive_buf[11], modem->receive_pack_len);
		g_saveparam_t.check_list_crc16[g_saveparam_t.bin_chunk] = crc16;
	}
	else
	{
		W25QXX_Write(&modem->receive_buf[0],write_addr,modem->receive_pack_len);
		OS_EXIT_CRITICAL();// 开中断
		crc16 = usMBCRC16((u8*)&modem->receive_buf[0], modem->receive_pack_len);
		g_saveparam_t.check_list_crc16[g_saveparam_t.bin_chunk] = crc16;
	}		
	/* 对数据包进行累加 */
	g_saveparam_t.bin_chunk++; 										// 来一个数据包加一
	g_saveparam_t.bin_last_chunk_size = modem->receive_pack_len;	// 记录本地数据包的长度
}

/*------------------------------------------------------------------------------
函数名称：my_modem_receive_all_pack_deal
函数功能：接收完所有数据包处理
入口参数: MYMODEM *modem：数据接收结构体
出口参数：
备注：
-----------------------------------------------------------------------------*/
uint8_t my_modem_receive_all_pack_deal(MYMODEM *modem)
{
	IWDG_Feed();
	if( update_get_mode_function() == UPDATE_MODE_LWIP )
	{
		my_modem_send_ack(modem, UPDATE_SUCCESS);  //反馈更新成功
		IWDG_Feed();
	}
	my_modem_receive_file_end(1);
//	OSTimeDlyHMSM(0,0,0,100);
	lfs_unmount(&g_lfs_t);
	IWDG_Feed();
	
	/* 更新完成,重启设备 */
	System_SoftReset();

	return UPDATE_SUCCESS;  //返回接收完所有数据
}

/*------------------------------------------------------------------------------
函数名称：my_modem_analaze_data
函数功能：解析数据包
入口参数: MYMODEM *modem：数据接收结构体
出口参数：
备注：中间数据包 0~4为帧头 5~6为数据包校验值 7~8为包数标签 9~10为数据包长度
      最后一包数据 0~4为帧头 5~7为帧尾 8~9为校验值
-----------------------------------------------------------------------------*/
uint8_t my_modem_analaze_data(MYMODEM *modem)
{
	uint16_t count_crc;
	uint8_t error_state = 0;

	if( update_get_mode_function() == UPDATE_MODE_LWIP )
	{
		modem->receive_pack_crc = (modem->receive_buf[6] << 8) | modem->receive_buf[5]; //获取数据包校验值
		modem->packet_index 		= (modem->receive_buf[8] << 8) | modem->receive_buf[7]; //数据包数
		modem->receive_pack_len = (modem->receive_buf[10] << 8)| modem->receive_buf[9]; //数据包长度
		count_crc = my_modem_crc(modem, 11, modem->receive_pack_len+11);  //计算数据包crc校验值
		if(modem->receive_pack_crc == count_crc)//&& modem->packet_index_last == modem->packet_index-1)  //校验值正确
		{
			error_state = SUCCESS_ONE_PACKET;  //正确接收到一包数据
			my_modem_receive_one_pack_deal(modem);  //处理一包数据
		}
		else //校验错误
		{
			modem->packet_index = modem->packet_index_last;
			error_state = ERROR_CHECK; //校验错误
		}	
	}
	else if( update_get_mode_function() == UPDATE_MODE_GPRS )
	{
		count_crc = my_modem_crc(modem, 0, modem->receive_pack_len);  //计算数据包crc校验值
		if(modem->receive_pack_crc == count_crc )//&& modem->packet_index_last == modem->packet_index-1)  //校验值正确
		{
			error_state = SUCCESS_ONE_PACKET;  //正确接收到一包数据
			my_modem_receive_one_pack_deal(modem);  //处理一包数据
		}
		else //校验错误
		{
			modem->packet_index = modem->packet_index_last;
			error_state = ERROR_CHECK; //校验错误
			update_error_recv++;
		}
	}
	return error_state;
}
uint16_t disconnect_cnt=0;
/*------------------------------------------------------------------------------
函数名称：my_modem_recieve_gprs_deal
函数功能：无线数据接收处理
入口参数: MYMODEM *modem：数据接收结构体
出口参数：
备注：
-----------------------------------------------------------------------------*/
void my_modem_recieve_gprs_deal(MYMODEM *modem)
{
	uint8_t state = 1;
	uint8_t error_state = 0;
	static  uint8_t no_ack_cnt = 0;  //无数据接收计数
	int8_t  queue_status = 0;

	queue_status = update_queue_find_msg(modem);
	if( queue_status >= 0)
	{
		disconnect_cnt = 0;
		if (modem->buf_last_flag)
		{
			error_state = my_modem_analaze_data(modem);  //解析中间数据包
			error_state = my_modem_receive_all_pack_deal(modem); //接收所有数据完成
		} 
		else 
		{
			error_state = my_modem_analaze_data(modem);  //解析中间数据包
		} 		
	}	
	else if(queue_status == -1)
	{
		disconnect_cnt = 0;
		error_state = ERROR_FRAME;  //帧头错误
	}
	else if(queue_status == -2)
	{
		no_ack_cnt++;
		error_state = ERROR_NO_RECEIVE;  //无效数据		
	}
	switch(error_state)  //判断接收状态标志
	{
		case SUCCESS_ONE_PACKET:  //数据接收成功
			modem->error_count = 0; //出错计数清零
			modem->packet_index++; 
			modem->receive_state = 0; //接收下一包数据
			modem->receive_count = 0; //已接收数据量清零
			modem->packet_index_last = modem->packet_index; //获取新一包数据
		break;
		case ERROR_ALL_CHECK:  //校验错误
			OSTimeDlyHMSM(0,0,1,0);
			OSTimeDlyHMSM(0,0,1,0);
			OSTimeDlyHMSM(0,0,1,0);
			OSTimeDlyHMSM(0,0,1,0);
			OSTimeDlyHMSM(0,0,1,0);
		break;
		default: // 缺省值
			if((modem->error_count++ >= 5))  //连续5次请求都出错了
			{
				modem->error_count = 5;  //出错次数保持在5次，以免溢出
				modem->receive_state = 0; //接收下一包数据
				modem->receive_count = 0; //已接收数据量清零
				modem->packet_index = 0; //重新获取数据包
				modem->packet_index_last = 0;
				modem->buf_last_flag = 0;
			}
		break;
	}

	/* 5 分钟内未获取到数据，本次更新错误 */
	if (disconnect_cnt++ >= 18000)  // 5min = 300000 ms/10ms = 30000
	{
		disconnect_cnt=0;
		my_modem_receive_file_end(0);
		update_error_occurred_in_function();
	}
	modem->error_count = 0;
	modem->receive_state = 0;
}

/*------------------------------------------------------------------------------
函数名称： my_modem_recieve_lwip_deal
函数功能：有线数据接收处理
入口参数: MYMODEM *modem：数据接收结构体
出口参数：
备注：
-----------------------------------------------------------------------------*/
void my_modem_recieve_lwip_deal(MYMODEM *modem)
{
	uint8_t ret = 0;
	uint8_t state = 1;
	uint8_t error_state = 0;
	static uint8_t no_ack_cnt = 0;  //无数据接收计数

	if (sg_file_init_t == 0) {
		sg_file_init_t = 1;
		my_modem_recevie_file_init(LWIP_CHUNK_SIZE);
	}
	
	ret = my_modem_send_ack(modem, error_state);
	if( ret == 0)  //接收完1包数据
	{  
		if(modem->receive_count > 5)     //接收到一包数据
		{
			no_ack_cnt = 0;  //无数据接收计数清零

			for(uint8_t i = 0; i < 5; i++) //判断帧头是否正确
			{
				if(modem->receive_buf[i] != DATA_HEAD[i]){state = 0; break;}
			}
			if(state)  //帧头正确
			{
				if (modem->receive_buf[5] == DATA_END[0] && \
						modem->receive_buf[6] == DATA_END[1] && \
						modem->receive_buf[7] == DATA_END[2] )  //最后一包数据，接收完所有的数据
				{
					error_state = my_modem_receive_all_pack_deal(modem); //接收所有数据完成
				} else {
					error_state = my_modem_analaze_data(modem);  //解析中间数据包
				} 
					
			}
			else error_state = ERROR_FRAME;  //帧头错误
		}
		else //很长一段时间没有接收到有效数据
		{
			no_ack_cnt++;
			error_state = ERROR_NO_RECEIVE;  //无效数据
		}

		switch(error_state)  //判断接收状态标志
		{
			case SUCCESS_ONE_PACKET:  //数据接收成功
				modem->error_count = 0; //出错计数清零
				modem->packet_index += 1; //获取新一包数据
				modem->packet_index_last = modem->packet_index;
			break;
			case ERROR_ALL_CHECK:  //校验错误
				OSTimeDlyHMSM(0,0,1,0);
				OSTimeDlyHMSM(0,0,1,0);
				OSTimeDlyHMSM(0,0,1,0);
				OSTimeDlyHMSM(0,0,1,0);
				OSTimeDlyHMSM(0,0,1,0);
			break;
			default: //缺省值
				if((modem->error_count++ >= 5))  //连续5次请求都出错了
				{
					modem->error_count = 5;  //出错次数保持在5次，以免溢出
					modem->packet_index = 0; //重新获取数据包
					modem->packet_index_last = 0;
				}
			break;
		}
		if(no_ack_cnt >= 15)    //超过15次无数据，即15*200ms，定时器最小时基为200ms
		{
			my_modem_timer_stop;  //关闭定时器，停止请求数据
			my_modem_timer_clear; //清空定时器
			/* 更新错误 */
			my_modem_receive_file_end(0);
			update_error_occurred_in_function();
		}
		disconnect_cnt=0;
	} else if (ret == 1) {
		/* 连续12次都未获取到数据，本次更新错误 */
		if (disconnect_cnt++>=12)
		{
			disconnect_cnt=0;
			my_modem_receive_file_end(0);
			update_error_occurred_in_function();
		}
		modem->error_count = 0;
		modem->receive_state = 0;
	}
}


void my_modem_timer_task(void)
{
	my_modem_receive_timeout(&my_modem);	//接收超时处理
	my_modem_timer_stop;  					//关闭定时器，停止请求数据
}


/************************************************************
*
* Function name	: update_queue_find_msg
* Description	: 获取缓存区中的数据
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t update_queue_find_msg(MYMODEM *modem)
{
	static uint8_t	msg_head[5]  = {0}; // 包头
	static uint8_t	msg_end[3]   = {0}; // 包尾
	static uint16_t crc_data = 0;
	static uint16_t data_num = 0;
	static uint16_t data_length = 0;
	uint8_t  msg_data[2]  = {0};
		
	if(Get_Queue_Count(&sg_queue_updata) >= 11) // 缓存中的数据大于 11
	{
		if(gsm_recv_init == 0)
		{
			for(uint8_t i=0;i<5;i++) // 取出包头
			 msg_head[i] = Dequeue_One_Byte(&sg_queue_updata); 
			
			for(uint8_t i=0;i<2;i++) // 取出CRC16 校验值
			 msg_data[i] = Dequeue_One_Byte(&sg_queue_updata); 		
			crc_data = msg_data[0]<<8 | msg_data[1];
			
			for(uint8_t i=0;i<2;i++) // 取出当前包标签
			 msg_data[i] = Dequeue_One_Byte(&sg_queue_updata); 		
			data_num = msg_data[0]<<8 | msg_data[1];

			for(uint8_t i=0;i<2;i++) // 取出数据包长度
			 msg_data[i] = Dequeue_One_Byte(&sg_queue_updata); 		
			data_length = msg_data[0]<<8 | msg_data[1];
		}			
	}
	else
		return -2;	
	
	// 判断
	for(uint8_t i = 0; i < 5; i++) //判断帧头是否正确
	{
		if(msg_head[i] != DATA_HEAD[i]){gsm_recv_init = 0; return -1;}
	}
	modem->receive_pack_crc = crc_data;     // 获取数据包校验值
	modem->packet_index     = data_num;     // 数据包数
	modem->receive_pack_len = data_length;  // 数据包长度			
	gsm_recv_init = 1;

	if((Get_Queue_Count(&sg_queue_updata) >= data_length+3)&&(gsm_recv_init == 1)) // 缓存中的数据大于 11
	{	
		gsm_recv_init = 0;
		Dequeue_Bytes_To_Buffer(&sg_queue_updata,modem->receive_buf,data_length);
		
		for(uint8_t i=0;i<3;i++) // 取出包尾
		 msg_end[i] = sg_queue_updata.buf[sg_queue_updata.front+i]; 		
		
		if(msg_end[0]==DATA_END[0] &&msg_end[1]==DATA_END[1]&&msg_end[2]==DATA_END[2])  //最后一包数据
		{
			modem->buf_last_flag = 1;
		}
		else
			modem->buf_last_flag = 0;	
	}
	return 0; 
}




