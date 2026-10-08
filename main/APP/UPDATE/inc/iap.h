#ifndef _IAP_H_
#define _IAP_H_
#include "sys.h"
#include "app.h"
/* 参数 */
#define UPDATA_SPIFLASH_ADDR   3*1024*1024	//W25Q128升级文件存储地址
#define UPDATE_STATUS_ADDR     3*1024*1024-4096	// 升级状态


typedef struct
{
	int8_t   iap_update_flag;      // 更新标志位  0  1
	uint16_t bin_size;             // 每包数据大小 
	uint16_t bin_chunk;		         // 块,bin有多少个512
	uint16_t bin_last_chunk_size;  // 最后一块多少字节
	uint16_t check_list_crc16[500];// 校验表	
}save_param_t;

/* 函数声明 */

void save_write_run_param(run_result_t param);
void save_read_run_param(run_result_t *param);

#endif
