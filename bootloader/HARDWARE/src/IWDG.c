#include "iwdg.h"

#define SOFT_IWDG_ENABLE  0u  // 软件看门狗
#define HARD_IWDG_ENABLE  0u  // 硬件看门狗


void IWDG_Init(u8 prer,u16 rlr)
{
	#if SOFT_IWDG_ENABLE > 0U
	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable); //使能对IWDG->PR IWDG->RLR的写
	IWDG_SetPrescaler(prer); //设置IWDG分频系数
	IWDG_SetReload(rlr);   //设置IWDG装载值
	IWDG_ReloadCounter(); //reload
	IWDG_Enable();       //使能看门狗
	#endif
	
	#if HARD_IWDG_ENABLE > 0U
	
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE); //使能GPIOB|GPIOE的时钟

	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;			// 输出
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;  		// 推挽输出
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;  	// 上拉
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz; 	// 高速GPIO
	GPIO_Init(GPIOA,&GPIO_InitStructure);
	GPIO_SetBits(GPIOA,GPIO_Pin_0);	
	
	#endif
}

//喂独立看门狗
void IWDG_Feed(void)
{
	#if SOFT_IWDG_ENABLE > 0U
	IWDG_ReloadCounter();//reload
	#endif
	
	#if HARD_IWDG_ENABLE > 0U	
	if(GPIO_ReadOutputDataBit(GPIOA, GPIO_Pin_0) == 0)
	{
		GPIO_SetBits(GPIOA, GPIO_Pin_0);
	}
  else
	{
		GPIO_ResetBits(GPIOA, GPIO_Pin_0);
	}	
	#endif
}
