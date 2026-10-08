#include "start.h"
#include "includes.h"
#include "delay.h"
#include "cmox_crypto.h"
#include "bsp.h"
static void system_setup(void);
/************************************************************
*
* Function name	: main
* Description	: 主函数
* Parameter		: 
* Return		: 
*	
************************************************************/
int main(void)
{
	system_setup();
	start_system_init_function(); // 初始化系统
	OSInit(); 										// UCOS初始化
	start_creat_task_function();  // 初始化任务
	OSStart(); 					  // 开启UCOS
}

/************************************************************
*
* Function name	: 
* Description	: 
* Parameter		: 
* Return		: 
*	
************************************************************/
static void system_setup(void)
{
	SystemInit();
	
	delay_init(168);		 					// 初始化延时函数
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);	// 中断分组配置
	

	/* Initialize cryptographic library */
  if (cmox_initialize(NULL) != CMOX_INIT_SUCCESS)
  {
    printf("error\n");
  }
}

