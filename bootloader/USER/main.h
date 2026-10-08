#ifndef __MAIN_H__
#define __MAIN_H__

#include "sys.h"
#include "delay.h"
#include "stdio.h"
#include "stmflash.h" 
#include "lfs.h"
#include "w25qxx.h"
#include "crc.h"
#include "iap.h"
#include "string.h"
#include "IWDG.h"
#include "malloc.h"
#include "usart_debug.h"


//W25Q32升级文件存储地址
#define UPDATE_INFOR_SAVEMODE 	1	                  // 0-存储在内部flash 1-存储在外部flash(升级参数改存W25Q128,与main保持一致)

#if (UPDATE_INFOR_SAVEMODE == 0)
	#define UPDATA_SPIFLASH_ADDR    3*1024*1024	        // 升级BIN文件
	#define UPDATA_PARAM_ADDR       0x08008000          // 升级参数
	#define APP_RUN_PARAM_ADDR      3*1024*1024-4*1024	// 程序运行参数
#else
	#define UPDATA_SPIFLASH_ADDR    3*1024*1024	        // 升级BIN文件
	#define UPDATA_PARAM_ADDR       3*1024*1024-16*1024 // 升级参数(外部W25Q128, 0x2FC000)
	#define APP_RUN_PARAM_ADDR      3*1024*1024-4*1024	// 程序运行参数
#endif

#define CHUNK_SIZE     1024	 // 升级包大小

typedef struct
{
	int8_t   iap_update_flag;      // 更新标志位  0  1
	uint16_t bin_size;             // 每包数据大小 
	uint16_t bin_chunk;		         // 块,bin有多少个512
	uint16_t bin_last_chunk_size;  // 最后一块多少字节
	uint16_t check_list_crc16[500];// 校验表	
}save_param_t;

typedef struct
{
	int8_t   JumpResult;
	int8_t   reset_num;    // 重启次数
	uint32_t jump_addr;   // 跳转地址
}run_result_t;

void system_setup_function(void);
void update_check_function(void);
int8_t updating_function(void);
void led_gpio_init_function(void);
void led_show_control(uint8_t mode);
void save_stroage_update_file_infor_function(save_param_t param);
void save_read_update_file_infor_function(save_param_t *param);
int DeviceRstReason(void);

void save_write_run_param(run_result_t param);
void save_read_run_param(run_result_t *param);

#endif
