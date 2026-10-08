#include "rng.h"
#include "delay.h"
#include "bsp.h"


//初始化RNG
//返回值:0,成功;1,失败
u8 RNG_Init(void)
{
	u16 retry=0; 
	RCC->AHB2ENR=1<<6;	//开启RNG时钟,来自PLL48CLK
	RNG->CR|=1<<2;		//使能RNG
	while((RNG->SR&0X01)==0&&retry<10000)	//等待随机数就绪
	{
		retry++;
		delay_us(100);
	}
	if(retry>=10000)return 1;//随机数产生器工作不正常
	return 0;
}
//得到随机数
//返回值:获取到的随机数
u32 RNG_Get_RandomNum(void)
{	 
	while((RNG->SR&0X01)==0);	//等待随机数就绪  
	return RNG->DR;	
}
//得到某个范围内的随机数
//min,max,最小,最大值.
//返回值:得到的随机数(rval),满足:min<=rval<=max
int RNG_Get_RandomRange(int min,int max)
{ 
   return RNG_Get_RandomNum()%(max-min+1)+min;
}

#ifdef MBEDTLS_ENTROPY_HARDWARE_ALT

#include "mbedtls/entropy_poll.h"

extern RNG_HandleTypeDef hrng;

int mbedtls_hardware_poll( void *Data, unsigned char *Output, size_t Len, size_t *oLen )
{
  uint32_t index;
  uint32_t randomValue;
		
  for (index = 0; index < Len/4; index++)
  {
		RNG_Get_RandomRange(&hrng, &randomValue);
		*oLen += 4;
		memset(&(Output[index * 4]), (int)randomValue, 4);
  }
  
  return 0;
}


#endif /*MBEDTLS_ENTROPY_HARDWARE_ALT*/


void rng_test(void)
{
	while(1)
	{
		printf("%u", RNG_Get_RandomNum());
		printf("\r\n\r\n");
		delay_ms(200);
	
		printf("%u", RNG_Get_RandomRange(0, 9));
		printf("\r\n\r\n");
		delay_ms(1000);		
	}
}






