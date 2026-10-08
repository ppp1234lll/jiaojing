#ifndef _USART_DEBUG_H_
#define _USART_DEBUG_H_

#include "sys.h"
#include "stdio.h"

#define USART_DEBUG_RX_MODE (1) // 0-中断字节模式 1-DMA空闲中断模式
#define USART_DEBUG_RX_MAX 100

void usart_debug_init_function(uint32_t baudrate);
void usart_debug_send_str(uint8_t *buff, uint16_t len);
void usart_debug_test(void);


#endif
