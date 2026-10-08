#include "iap.h"
#include "w25qxx.h"
#include "stmflash.h"

/************************************************************
*
* Function name	: save_run_param
* Description	: 存储程序运行信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void save_write_run_param(run_result_t param)
{
	W25QXX_Write((uint8_t *)&param,APP_RUN_PARAM_ADDR,sizeof(run_result_t));
}

/************************************************************
*
* Function name	: save_read_run_param
* Description	: 读取程序运行信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void save_read_run_param(run_result_t *param)
{
	W25QXX_Read((uint8_t*)param,APP_RUN_PARAM_ADDR,sizeof(run_result_t));
}
