#include "main.h"

#define PWR_TST_READ   	GPIO_ReadInputDataBit(GPIOD,GPIO_Pin_10)    // 12V检测

uint8_t appbuf[CHUNK_SIZE] = {0};

save_param_t save_param = {
	.iap_update_flag = 0,
	.bin_size = 0,
	.bin_chunk = 0,		//块,bin有多少个512
	.bin_last_chunk_size = 0,//最后一块多少字节
	.check_list_crc16 = {0},//校验表
};

run_result_t sg_run_param = {0};

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
	system_setup_function(); // 初始化系统
	led_gpio_init_function();
	usart_debug_init_function(115200);
	W25QXX_Init();
//	printf("OTA\n");
	IWDG_Init(4,1000);       // 初始化看门狗
	update_check_function();
}

/************************************************************
*
* Function name	: system_setup_function
* Description	: 系统启动初始化
* Parameter		: 
* Return		: 
*	
************************************************************/
void system_setup_function(void)
{
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置系统中断优先级分组2
	delay_init(168);  //初始化延时函数

}

/************************************************************
*
* Function name	: update_check_function
* Description	: 更新检测
* Parameter		: 
* Return		: 
*	
************************************************************/
void update_check_function(void)
{
	uint8_t   count = 30;
	uint32_t  app_run_addr = APP1_FLASH_STORE;
  /* 检测是否正常上电 */
	while(count--)
	{
		delay_ms(100);
		IWDG_Feed();
		if(PWR_TST_READ == 0) break;
	}
	save_read_update_file_infor_function(&save_param);
	save_read_run_param(&sg_run_param);
	
	if(DeviceRstReason()<0)  // 复位检测，如果多次硬件看门狗复位，进入出厂程序     
		app_run_addr = FSCTORY_APP_ADDR;  
	
/*强制升级检测、开始变砖检测修复 */
	if( sg_run_param.JumpResult != 1)      /*跳转失败，则运行应急程序*/
	{			
//		printf("执行应急程序\n") ;
		app_run_addr = FSCTORY_APP_ADDR;     // 跳转到应急程序运行 
    goto JUMP_APP;		
	}
	else                                   /*跳转APP未出现失败，设置标志位jumpResult为暂未跳转成功状态*/
	{
		sg_run_param.JumpResult = 0 ;         // 复位JumpResult标志位
		save_write_run_param(sg_run_param);   // 保存JumpResult
	}	
/*结束变砖检测修复、强制升级检测*/

/* 跳转地址判断 */
	if( sg_run_param.jump_addr == RUN_APP_ADDR)  
	{			
//		printf("执行正常程序\n") ;
		app_run_addr = RUN_APP_ADDR;        // 跳转到运行             
	}
	else if( sg_run_param.jump_addr == FSCTORY_APP_ADDR)  
	{
//		printf("执行应急程序\n") ;
		app_run_addr = FSCTORY_APP_ADDR;     // 跳转到应急程序运行   
	}	
/* 跳转地址判断 */
	
/*开始检测是否需要升级APP(即判断IapFlag标志位)*/
	if(save_param.iap_update_flag == 1)	   /*IapFlag合法*/ //IapFlag标志位合法性校验
	{
//		printf("执行升级程序\n") ;
		if(updating_function() < 0 )   //跳转到升级程序              
			app_run_addr = RUN_APP_ADDR; 
		else
		  app_run_addr = FSCTORY_APP_ADDR;     // 跳转到应急程序运行  	
	}
	else                                   /*不需要升级*/
	{
//		printf("执行跳转程序\n") ;
		app_run_addr = RUN_APP_ADDR;        // 跳转到RunApp，执行原先的程序		
	}			
/*检测是否需要升级APP结束*/

	JUMP_APP:
	iap_load_app(app_run_addr) ;                       //执行APP，不会再往下执行了
//	printf("设备故障:") ;				
}
/************************************************************
*
* Function name	: update_function
* Description	: 更新检测
* Parameter		: 
* Return		: 
*	
************************************************************/
int8_t updating_function(void)
{
	uint16_t i = 0;
	uint16_t crc16 = 0;
	uint32_t read_addr = UPDATA_SPIFLASH_ADDR;
	
	while(1)
	{
		for(i = 0; i < save_param.bin_chunk; i++)//校验
		{
			IWDG_Feed();
			if(i < save_param.bin_chunk - 1)
			{
				read_addr = UPDATA_SPIFLASH_ADDR + i * save_param.bin_size;
				W25QXX_Read(appbuf,read_addr,save_param.bin_size);

				crc16 = my_crc16((u8*)appbuf, 0,save_param.bin_size);
				if(crc16 != save_param.check_list_crc16[i])//校验错误
				{
					return -1;
				}
			}
			else
			{
				read_addr = UPDATA_SPIFLASH_ADDR + i * save_param.bin_size;
				W25QXX_Read(appbuf,read_addr,save_param.bin_last_chunk_size);
				crc16 = my_crc16((u8*)appbuf, 0,save_param.bin_last_chunk_size);
				if(crc16 != save_param.check_list_crc16[i])//校验错误
				{
					return -1;
				}
			}
		}
		IWDG_Feed();
		if(i >= save_param.bin_chunk)//校验通过
		{
			for(i = 0; i < save_param.bin_chunk; i++)
			{
				led_show_control(i);
				IWDG_Feed();
				if(i < save_param.bin_chunk - 1)
				{
					read_addr = UPDATA_SPIFLASH_ADDR + i * save_param.bin_size;
					W25QXX_Read(appbuf,read_addr,save_param.bin_size);	
					iap_write_appbin(RUN_APP_ADDR + i * save_param.bin_size, appbuf, save_param.bin_size);
				}
				else
				{
					read_addr = UPDATA_SPIFLASH_ADDR + i * save_param.bin_size;
					W25QXX_Read(appbuf,read_addr,save_param.bin_last_chunk_size);
					iap_write_appbin(RUN_APP_ADDR + i * save_param.bin_size, appbuf, save_param.bin_last_chunk_size);
				}
			}
			IWDG_Feed();
			if(i >= save_param.bin_chunk)
			{
				//更新标志位,保存
				save_param.iap_update_flag = 2;
				save_stroage_update_file_infor_function(save_param);
				delay_ms(100);	
				IWDG_Feed();
				return 0;
			}
		}
	}
}

/************************************************************
*
* Function name	: DeviceRstReason
* Description	: 判断硬件重启原因
* Parameter		: 
* Return		: 
*	
************************************************************/
int DeviceRstReason(void)
{
	if( SET == RCC_GetFlagStatus( RCC_FLAG_PORRST) )
	{
		printf("POR/PDR重上电启动\n");
	}
	if( SET == RCC_GetFlagStatus( RCC_FLAG_BORRST) )
	{
		printf("POR/PDR/BOR重上电启动\n");
	}
	if( SET == RCC_GetFlagStatus(RCC_FLAG_SFTRST) )
	{
		printf("软复位启动\n");
	}
	if( SET == RCC_GetFlagStatus(RCC_FLAG_IWDGRST) )
	{
		sg_run_param.reset_num++;
		printf("独立看门狗启动\n");
	}
	if( SET == RCC_GetFlagStatus(RCC_FLAG_WWDGRST) )
	{
		printf("窗口看门狗启动\n");
	}
	if( (RESET == RCC_GetFlagStatus(RCC_FLAG_SFTRST))  &&
		  (RESET == RCC_GetFlagStatus(RCC_FLAG_IWDGRST)) &&
	    (RESET == RCC_GetFlagStatus(RCC_FLAG_WWDGRST)) &&
		  (SET   == RCC_GetFlagStatus(RCC_FLAG_PINRST))
   	)
	{
		sg_run_param.reset_num++;
		printf("硬复位启动\n");
	}
	RCC_ClearFlag() ;
	
	if( sg_run_param.reset_num >= 10)
	{
		sg_run_param.reset_num = 0;
		save_write_run_param(sg_run_param);
		return -1;				
	}
	else
	{
		save_write_run_param(sg_run_param);
		return 0;		
	}
}

/************************************************************
*
* Function name	: led_gpio_init_function
* Description	: 初始化指示灯控制io:默认不开启
* Parameter		: 
* Return		: 
*	
************************************************************/
void led_gpio_init_function(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB|RCC_AHB1Periph_GPIOD|RCC_AHB1Periph_GPIOE,ENABLE); //使能GPIOB|GPIOE的时钟

	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_8|GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_OUT;			// 输出
	GPIO_InitStructure.GPIO_OType=GPIO_OType_PP;  		// 推挽输出
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_NOPULL;  	// 上拉
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_100MHz; 	// 高速GPIO
	GPIO_Init(GPIOB,&GPIO_InitStructure);

	GPIO_SetBits(GPIOB,GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_8|GPIO_Pin_9);

	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_OUT;			// 输出
	GPIO_InitStructure.GPIO_OType=GPIO_OType_PP;  		// 推挽输出
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_NOPULL;  	// 上拉
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_100MHz; 	// 高速GPIO
	GPIO_Init(GPIOE,&GPIO_InitStructure);
	GPIO_SetBits(GPIOE,GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6);

	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_10;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IN;			// 输入
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_UP;  	    // 上拉
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_100MHz; 	// 高速GPIO
	GPIO_Init(GPIOD,&GPIO_InitStructure);
}

/************************************************************
*
* Function name	: led_show_control
* Description	: LED灯光控制
* Parameter		: 
* Return		: 
*	
************************************************************/
static void led_show_control(uint8_t mode)
{
	static uint8_t flag = 0;
	uint8_t num = mode % 40;
	if(num<20 && flag == 0) {
		flag = 1;
		GPIO_ResetBits(GPIOB,GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_8|GPIO_Pin_9);
		GPIO_ResetBits(GPIOE,GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6);
	} else if(num >=20 && flag == 1){
		flag = 0;
		GPIO_SetBits(GPIOB,GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_8|GPIO_Pin_9);
		GPIO_SetBits(GPIOE,GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6);
	}
}
/************************************************************
*
* Function name	: save_stroage_update_file_infor_function
* Description	: 存储更新信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void save_stroage_update_file_infor_function(save_param_t param)
{
  #if (UPDATE_INFOR_SAVEMODE == 0)
  	STMFLASH_Write(UPDATA_PARAM_ADDR, (u32 *)&param, sizeof(save_param_t) / 4);
	#else
		W25QXX_Write((uint8_t *)&param,UPDATA_PARAM_ADDR,sizeof(save_param_t));
	#endif
}

/************************************************************
*
* Function name	: save_read_update_file_infor_function
* Description	: 读取更新信息
* Parameter		: 
* Return		: 
*	
************************************************************/
void save_read_update_file_infor_function(save_param_t *param)
{
  #if (UPDATE_INFOR_SAVEMODE == 0)
  	STMFLASH_Read(UPDATA_PARAM_ADDR, (u32 *)param, sizeof(save_param_t) / 4);
	#else
		W25QXX_Read((uint8_t*)param,UPDATA_PARAM_ADDR,sizeof(save_param_t));
	#endif
}
 
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

