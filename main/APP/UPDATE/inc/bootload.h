#ifndef _BOOTLOAD_H_
#define _BOOTLOAD_H_

#include "sys.h"
#include <stdint.h>
#include "timer.h"
#include "iap.h"
#include "delay.h"
#include "stmflash.h"

#define LWIP_CHUNK_SIZE     1024   //一次性接收的最大数据 2K 
#define ApplicationAddress					0x08008000  //应用程序写入起始地址，保留0X08000000~0X0801FFFF的空间为IAP使用

/*
*定义接收反馈枚举变量
*/
enum
{
	ERROR_NO_RECEIVE = 1,
	ERROR_FRAME = 2,
	ERROR_CHECK = 3,
	ERROR_ALL_CHECK = 4,
	SUCCESS_ONE_PACKET = 5,
	SUCCESS_ALL_DATA = 6,

	START_UPDATE = 7,
	UPDATE_OK = 8,
	DATA_ERROR = 9,
	UPDATE_SUCCESS = 10,
};

/*
*定义接收结构体变量
*/
typedef struct
{
  uint8_t 	buf_last_flag;  // 最后一包数据
  uint8_t 	receive_state;  //接收状态
  uint16_t 	receive_count;  //接收计数
  uint16_t 	packet_index,packet_index_last;   //已经接收到的包数标签
  uint8_t 	error_count;     //连续获取无反应计数
  
  uint16_t 	receive_pack_crc; //接收到的crc校验值
  uint32_t 	receive_pack_len; //接收到的数据包长度
  uint32_t 	fwrite_addr;      //写入首地址
  
  uint8_t receive_buf[LWIP_CHUNK_SIZE+15]; //接收缓存
}MYMODEM;

#define my_modem_timer_clear TIM_SetCounter(TIM2,0)
#define my_modem_timer_start TIM_Cmd(TIM2,ENABLE)
#define my_modem_timer_stop  TIM_Cmd(TIM2,DISABLE)


extern MYMODEM my_modem;

extern void my_modem_receive_task(uint8_t data, MYMODEM *modem);
extern void my_modem_receive_timeout(MYMODEM *modem);
extern void my_modem_recieve_lwip_deal(MYMODEM *modem);
extern void my_modem_timer_task(void);

void my_modem_recevie_file_init(uint16_t size);
void my_modem_receive_file_end(uint8_t status);
void my_modem_detcet_update_status_function(void);
void my_modem_init(void);
void my_modem_detection_status(void);


#endif
