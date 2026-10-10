#include "start.h"
#include "includes.h"
#include "appconfig.h"
#include "app.h"
#include "delay.h"
#include "timer.h"
#include "key.h"
#include "adc.h"
#include "w25qxx.h"
#include "led.h"
#include "relay.h"
#include "eth.h"
#include "malloc.h"
#include "det.h"
#include "gsm.h"
#include "print.h"
#include "hal_lis3dh.h"
#include "aht20.h"
#include "LAN8720.h"
#include "lwip_comm.h"
#include "httpd.h"
#include "lwip_ping.h"
#include "com.h"
#include "rtc.h"
#include "update.h"
#include "save.h"
#include "iwdg.h"
#include "BL0910.h"
#include "rs485.h"
#include "usart_debug.h"
#include "lfs_port.h"
#include "adc.h"
#include "fan.h"
#include "rng.h"
#include "http_com.h"

ChipID_t g_chipid_t;
code_id_t code_id_crc;

/* 初始化PVD */
void PVD_Init(void)
{
	EXTI_InitTypeDef EXTI_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE); //使能PVD电压检测模块的时钟
	PWR_PVDLevelConfig(PWR_PVDLevel_3); // 设定监控阀值
	PWR_PVDCmd(ENABLE); // 使能PVD

	EXTI_InitStructure.EXTI_Line = EXTI_Line16; // PVD连接到中断线16上
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt; //使用中断模式
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;//电压低于阀值时产生中断
	EXTI_InitStructure.EXTI_LineCmd = ENABLE; // 使能中断线
	EXTI_Init(&EXTI_InitStructure); // 初始
	
	NVIC_InitStructure.NVIC_IRQChannel = PVD_IRQn; //定时器3中断
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0; //抢占优先级1
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0; //子优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

/* PVD中断处理 */
void PVD_IRQHandler(void)
{
	EXTI_ClearITPendingBit(EXTI_Line16);
	if(PWR_GetFlagStatus( PWR_FLAG_PVDO ))    /* 1为VDD小于PVD阈值,掉电情况 */
	{
		while(1)
		{
			printf("reset:0x%8x\r\n",RCC->CSR);
			for(uint32_t i=0;i<0x50000;i++);
			IWDG_Feed();
			pwr_tst_detection();
			if(det_get_220v_in_function() == 1)  // 220V上电
				System_SoftReset();			
		}
	}
}

void BOR_Init(void)
{
	FLASH_OB_Unlock();
	FLASH_OB_BORConfig(OB_BOR_LEVEL2);
	FLASH_OB_Launch();
}


/************************************************************
*
* Function name	: start_system_init_function
* Description	: 系统初始化函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void start_system_init_function(void)
{
//	PVD_Init();
//	BOR_Init();
	IWDG_Init(4,1000);				// 初始化看门狗 2s
	
	start_get_device_id_function();					// 获取本机ID
	mymem_init(SRAMIN);											// 内存初始化
	
	led_gpio_init_function();						// LED初始化（已测试）
	relay_gpio_init_function();				  // 继电器初始化（已测试）	
	usart_debug_init_function(115200);  // 调试接口——串口2（已测试）
	key_init_function();	 						  // 按键初始化(已测试)
	TIM3_Int_Init(1000-1,84-1);					// 定时器3初始化(已测试)
	TIM2_Int_Init(10000-1,84-1);				// 定时器2初始化(已测试)
	TIM6_Int_Init(10000-1,8400-1);			// 定时器6初始化 1Hz(已测试)
	My_RTC_Init();											// RTC初始化(已测试)
//	RTC_Set_AlarmA(RTC_Weekday_Tuesday,1,00,00);
	
	printf("13213\r\n");
	
	RNG_Init();                         // 硬件随机数初始化
	
	printf("88888\r\n");
	
	
	IWDG_Feed();
	
	bl0910_init_function();						// 电能检测初始化(已测试)	
	fan_gpio_init_function();           // 风扇初始化(已测试)

 // fan_test();

	hal_lis3dh_init(true);	 						// 陀螺仪初始化 IIC (已测试)	
	aht20_init_function();							// 温湿度初始化(已测试)
	printf("ceshi\r\n");
	printf("RCC->CSR2..:0x%08x..\r\n",RCC->CSR);	
//	RCC_ClearFlag();
		
	while((RCC->CSR & 0x0A000000) != 0)
	{ 
		pwr_tst_detection();
		if(det_get_220v_in_function() == 1)  // 220V上电
		{
			RCC_ClearFlag();
			System_SoftReset();
		}
		else
		{
			delay_ms(1000);
			IWDG_Feed();
		}
	}
	RCC_ClearFlag();                	// 清楚标志位
	IWDG_Feed();
	W25QXX_Init();			 							// 初始化spiflash
	save_init_function();	
	com_recevie_function_init();			// 初始化接收缓冲区
	http_com_buff_init();
	app_get_storage_param_function();	// 获取本地存储的数据
	update_status_detection();				// 检测上次升级结果并上报平台
	my_app_run_param_init();
	IWDG_Feed();
}

/************************************************************
*
* Function name	: start_get_device_id_function
* Description	: 获取本机ID
* Parameter		: 
* Return		: 
*	STM2F1_UUID_ADDR  0X1FFFF7E8   // 任意的一个数
	STM2F3_UUID_ADDR  0X1FFFF7AC   // 任意的一个数
	STM2F4_UUID_ADDR  0X1FFF7A10   // 任意的一个数
	STM2F7_UUID_ADDR  0X1FF0F420   // 任意的一个数
************************************************************/
void start_get_device_id_function(void)
{
	volatile uint32_t addr;
	addr  = 0x1FFF822E;
	addr -= 0x800;
	addr -= 0x1e;	
	
	g_chipid_t.id[0] = *(__I uint32_t *)(addr + 0x00);
	g_chipid_t.id[1] = *(__I uint32_t *)(addr + 0x04);
	g_chipid_t.id[2] = *(__I uint32_t *)(addr + 0x08);
}

/************************************************************
*
* Function name	: start_get_device_id_str
* Description	: 获取本机ID
* Parameter		: 
* Return		: 
*	
************************************************************/
void start_get_device_id_str(uint8_t *str)
{
	sprintf((char*)str,"%04X%04X%04X",g_chipid_t.id[0],g_chipid_t.id[1],g_chipid_t.id[2]);
}

/************************************************************
*
* Function name	: start_pack_device_uuid_str
* Description	: 打包成UUID
* Parameter		: 
* Return		: 
*	"079f23cd-0988-459f-96f5-fa1c507dd07c"
	 00000000 0000 0000 0000 0000
************************************************************/
void start_pack_device_uuid_str(char *data)
{
	sprintf(data,"%08x-%04x-%04x-%04x-%04x%08x",
					0x507dd07c,(uint16_t)(g_chipid_t.id[0]>>4),(uint16_t)g_chipid_t.id[0],
					(uint16_t)(g_chipid_t.id[1]>>4),(uint16_t)g_chipid_t.id[1],g_chipid_t.id[2]);
}



void start_get_device_id(uint32_t *id)
{
	id[0] = g_chipid_t.id[0];
	id[1] = g_chipid_t.id[1];
	id[2] = g_chipid_t.id[2];
}

/* 任务优先级 */
#define APP_TASK_PRIO		11
/* 任务堆栈大小 */
#define APP_STK_SIZE		512
/* 任务堆栈 */
__align(8) static OS_STK START_TASK_STK[APP_STK_SIZE];
/* 任务函数 */
void app_task(void *argument);

/* 任务优先级 */
#define ETH_TASK_PRIO		13
/* 任务堆栈大小 */
#define ETH_STK_SIZE		256
/* 任务堆栈 */
__align(8) static OS_STK ETH_TASK_STK[ETH_STK_SIZE];
/* 任务函数 */
void eth_task(void *argument);

/* 任务优先级 */
#define DET_TASK_PRIO		12
/* 任务堆栈大小 */
#define DET_STK_SIZE		512
/* 任务堆栈 */
__align(8) static OS_STK DET_TASK_STK[DET_STK_SIZE];
/* 任务函数 */
void det_task(void *argument);

//任务优先级
#define STORAGESTACK_PRIO		30
//任务堆栈大小	
#define STORAGESTACK_STK_SIZE 		128
//任务堆栈	
OS_STK STORAGESTACK_STK[STORAGESTACK_STK_SIZE];
//任务函数
void storagestack_task(void *p_arg);


/* 任务优先级 */
#define GSM_TASK_PRIO		19
/* 任务堆栈大小 */
#define GSM_STK_SIZE		512
/* 任务堆栈 */
OS_STK GSM_TASK_STK[GSM_STK_SIZE];
/* 任务函数 */
void gsm_task(void *argument);


#define PRINT_TASK_PRIO		29   /* 任务优先级 */
#define PRINT_STK_SIZE		300 /* 任务堆栈大小 */
OS_STK 	PRINT_TASK_STK[PRINT_STK_SIZE]; /* 任务堆栈 */
void print_task(void *argument); /* 任务函数 */
/************************************************************
*
* Function name	: start_creat_task_function
* Description	: 创建任务
* Parameter		: 
* Return		: 
*	
************************************************************/
void start_creat_task_function(void)
{
	OS_CPU_SR cpu_sr;
	INT8U  err;			
	OS_ENTER_CRITICAL();  																	// 关中断
	
	/* 创建任务 */
	OSTaskCreateExt(	 app_task, 																					//建立扩展任务(任务代码指针) 
										(void *)0,																					//传递参数指针 
										(OS_STK*)&START_TASK_STK[APP_STK_SIZE-1], 					//分配任务堆栈栈顶指针 
										(INT8U)APP_TASK_PRIO, 															//分配任务优先级 
										(INT16U)APP_TASK_PRIO,															//(未来的)优先级标识(与优先级相同) 
										(OS_STK *)&START_TASK_STK[0], 											//分配任务堆栈栈底指针 
										(INT32U)APP_STK_SIZE, 															//指定堆栈的容量(检验用) 
										(void *)0,																					//指向用户附加的数据域的指针 
										(INT16U)OS_TASK_OPT_STK_CHK|OS_TASK_OPT_STK_CLR);		//建立任务设定选项 
	OSTaskNameSet(APP_TASK_PRIO, (INT8U *)(void *)"app", &err);
	OSTaskCreateExt(	 eth_task, 																					//建立扩展任务(任务代码指针) 
										(void *)0,																					//传递参数指针 
										(OS_STK*)&ETH_TASK_STK[ETH_STK_SIZE-1], 					//分配任务堆栈栈顶指针 
										(INT8U)ETH_TASK_PRIO, 															//分配任务优先级 
										(INT16U)ETH_TASK_PRIO,															//(未来的)优先级标识(与优先级相同) 
										(OS_STK *)&ETH_TASK_STK[0], 											//分配任务堆栈栈底指针 
										(INT32U)ETH_STK_SIZE, 															//指定堆栈的容量(检验用) 
										(void *)0,																					//指向用户附加的数据域的指针 
										(INT16U)OS_TASK_OPT_STK_CHK|OS_TASK_OPT_STK_CLR);		//建立任务设定选项 
	OSTaskNameSet(ETH_TASK_PRIO, (INT8U *)(void *)"eth", &err);
										
	OSTaskCreateExt(	 det_task, 																					//建立扩展任务(任务代码指针) 
										(void *)0,																					//传递参数指针 
										(OS_STK*)&DET_TASK_STK[DET_STK_SIZE-1], 					//分配任务堆栈栈顶指针 
										(INT8U)DET_TASK_PRIO, 															//分配任务优先级 
										(INT16U)DET_TASK_PRIO,															//(未来的)优先级标识(与优先级相同) 
										(OS_STK *)&DET_TASK_STK[0], 											//分配任务堆栈栈底指针 
										(INT32U)DET_STK_SIZE, 															//指定堆栈的容量(检验用) 
										(void *)0,																					//指向用户附加的数据域的指针 
										(INT16U)OS_TASK_OPT_STK_CHK|OS_TASK_OPT_STK_CLR);		//建立任务设定选项 
	OSTaskNameSet(DET_TASK_PRIO, (INT8U *)(void *)"det", &err);

	OSTaskCreateExt(	 gsm_task, 																					//建立扩展任务(任务代码指针) 
										(void *)0,																					//传递参数指针 
										(OS_STK*)&GSM_TASK_STK[GSM_STK_SIZE-1], 					//分配任务堆栈栈顶指针 
										(INT8U)GSM_TASK_PRIO, 															//分配任务优先级 
										(INT16U)GSM_TASK_PRIO,															//(未来的)优先级标识(与优先级相同) 
										(OS_STK *)&GSM_TASK_STK[0], 											//分配任务堆栈栈底指针 
										(INT32U)GSM_STK_SIZE, 															//指定堆栈的容量(检验用) 
										(void *)0,																					//指向用户附加的数据域的指针 
										(INT16U)OS_TASK_OPT_STK_CHK|OS_TASK_OPT_STK_CLR);		//建立任务设定选项 
	OSTaskNameSet(GSM_TASK_PRIO, (INT8U *)(void *)"gsm", &err);

	OSTaskCreateExt(	 print_task,                                        //建立扩展任务(任务代码指针) 
										(void *)0,																					//传递参数指针 
										(OS_STK*)&PRINT_TASK_STK[PRINT_STK_SIZE-1], 					//分配任务堆栈栈顶指针 
										(INT8U)PRINT_TASK_PRIO, 															//分配任务优先级 
										(INT16U)PRINT_TASK_PRIO,															//(未来的)优先级标识(与优先级相同) 
										(OS_STK *)&PRINT_TASK_STK[0], 											//分配任务堆栈栈底指针 
										(INT32U)PRINT_STK_SIZE, 															//指定堆栈的容量(检验用) 
										(void *)0,																					//指向用户附加的数据域的指针 
										(INT16U)OS_TASK_OPT_STK_CHK|OS_TASK_OPT_STK_CLR);		//建立任务设定选项 
	OSTaskNameSet(PRINT_TASK_PRIO, (INT8U *)(void *)"print", &err);
										
//	OSTaskCreateExt(	 storagestack_task, 															 //建立扩展任务(任务代码指针) 
//										(void *)0,																					//传递参数指针 
//										(OS_STK*)&STORAGESTACK_STK[STORAGESTACK_STK_SIZE-1], //分配任务堆栈栈顶指针 
//										(INT8U)STORAGESTACK_PRIO, 															//分配任务优先级 
//										(INT16U)STORAGESTACK_PRIO,															//(未来的)优先级标识(与优先级相同) 
//										(OS_STK *)&STORAGESTACK_STK[0], 											//分配任务堆栈栈底指针 
//										(INT32U)STORAGESTACK_STK_SIZE, 												 //指定堆栈的容量(检验用) 
//										(void *)0,																					//指向用户附加的数据域的指针 
//										(INT16U)OS_TASK_OPT_STK_CHK|OS_TASK_OPT_STK_CLR);		//建立任务设定选项 
//	OSTaskNameSet(STORAGESTACK_PRIO, (INT8U *)(void *)"storagestack", &err);
	
	OS_EXIT_CRITICAL();  		 															// 开中断
}

/************************************************************
*
* Function name	: app_task_function
* Description	: 主要任务函数
* Parameter		: 
* Return		: 
*	
************************************************************/
void app_task(void *argument)
{
	OSStatInit();	  	// 初始化统计任务
	app_task_function();
}

/************************************************************
*
* Function name	: eth_task
* Description	: 网口检测任务:一直对网口进行轮询
* Parameter		: 
* Return		: 
*	
************************************************************/
void eth_task(void *argument)
{
	eth_network_line_status_detection_function();
}

/************************************************************
*
* Function name	: det_task
* Description	: 检测函数：包括温湿度、adc等
* Parameter		: 
* Return		: 
*	
************************************************************/
void det_task(void *argument)
{
	det_task_function();
}

/************************************************************
*
* Function name	: gsm_task
* Description	: 2G模块任务：打电话
* Parameter		: 
* Return		: 
*	
************************************************************/
void gsm_task(void *argument)
{
	gsm_task_function();
}

/************************************************************
*
* Function name	: gsm_task
* Description	: 2G模块任务：打电话
* Parameter		: 
* Return		: 
*	
************************************************************/
void print_task(void *argument)
{
	print_task_function();
}	
/************************************************************
*
* Function name	: gsm_task
* Description	: 2G模块任务：打电话
* Parameter		: 
* Return		: 
*	
************************************************************/
void storagestack_task(void *p_arg)
{
	OS_TCB *ptcb;       //定义一个任务控制块，结构体指针
	OS_STK_DATA stkDat;   //定义堆栈结构体变量 
	while(1)
	{
		ptcb = &OSTCBTbl[0];//将指针指向任务表的第一个任务
		printf("************************************ App Task Debug Info ***********************************\r\n");
		printf("  Prio    Used     Free    Per     TaskName\r\n");
		while (ptcb != NULL)//轮询每一个任务
		{
				OSTaskStkChk(ptcb->OSTCBPrio, &stkDat);//Check task stack
				printf("   %2d    %5d    %5d    %02d%%     %s\r\n", ptcb->OSTCBPrio, stkDat.OSUsed, stkDat.OSFree, (stkDat.OSUsed * 100)/(stkDat.OSUsed + stkDat.OSFree), ptcb->OSTCBTaskName);        
				ptcb = ptcb->OSTCBPrev;//Previous TCB list
		}
		printf("\r\n");
		OSTimeDlyHMSM(0,0,10,0); //延时3s
	}
}

