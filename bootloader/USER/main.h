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

/* 升级状态 (取值须与 main/APP/TASK/inc/http_update.h 一致) */
typedef enum
{
	UPDATE_NONE    = 0,
	UPDATE_SUCCESS = 1,
	UPDATE_FAILED  = 2,
} update_status_t;

/* 无线HTTP升级参数 (布局须与 main/APP/TASK/inc/http_update.h 的 struct BOOT_UPDATE_PARAM 完全一致)
   存储于 UPDATA_PARAM_ADDR(0x2FC000), 与有线 save_param_t 复用同一地址;
   两者通过鉴别字段区分: 无线时 is_update 恰好==1, 有线时该32位值含 bin_size, 不等于1。 */
struct BOOT_UPDATE_PARAM
{
	unsigned int is_update;     // true:需要升级, false:无需升级
	unsigned int section_size;  // 每包的实际数据大小(字节)
	unsigned int section_count; // 总包数
	unsigned int update_status; // 升级状态
};

void system_setup_function(void);
void update_check_function(void);
int8_t updating_function(void);
void led_gpio_init_function(void);
void led_show_control(uint8_t mode);
void save_stroage_update_file_infor_function(save_param_t param);
void save_read_update_file_infor_function(save_param_t *param);
int DeviceRstReason(void);

/* 无线HTTP升级: boot 参数读写与搬运 */
void update_read_boot_param(struct BOOT_UPDATE_PARAM *param);
void update_write_boot_param(struct BOOT_UPDATE_PARAM *param);
int8_t update_app_from_boot_param(void);

void save_write_run_param(run_result_t param);
void save_read_run_param(run_result_t *param);

#endif
