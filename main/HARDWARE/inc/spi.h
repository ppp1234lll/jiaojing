#ifndef __SPI_H
#define __SPI_H

#include "sys.h"
								  
void w25qxx_SPI_INIT(void);			 		   // 初始化SPI口
uint8_t SPI_ReadWriteByte(uint8_t TxData); // SPI总线读写一个字节
		 
#endif

