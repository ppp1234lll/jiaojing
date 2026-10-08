#ifndef _DMA_H_
#define _DMA_H_

#include "sys.h"


/** 数据输入函数 **/
void DMA_Config(DMA_Stream_TypeDef *DMA_Streamx,uint32_t chx,uint32_t par,uint32_t mar,uint32_t dir,uint16_t ndtr);
void DMA_Enable(DMA_Stream_TypeDef *DMA_Streamx,uint16_t ndtr);

/** 数据输出函数 **/

#endif
