/********************************************************************************
* @File name  : uart4.h
* @Description: 
* @Author     : ZHLE
*  Version Date        Modification Description
	7、单相计量芯片: 串口1，波特率4800，
	   引脚分配为：  USART1_TX： PA9
									USART1_RX： PA10
********************************************************************************/
#ifndef _ELEC_UART_H_
#define _ELEC_UART_H_
#include "sys.h"

void    elec_hard_spi_init(void);			 		   // 初始化SPI口
uint8_t hardSPI_ReadWriteByte(uint8_t TxData); // SPI总线读写一个字节

void hardSPI_WriteByte(uint8_t TxData) ;

void hardSPI_Write_Multi_Byte(uint8_t *buff, uint16_t len);	 
uint8_t hardSPI_ReadByte(void);	 
void hardSPI_test(void);	
void hard_spi_delay(uint16_t time);

#endif
