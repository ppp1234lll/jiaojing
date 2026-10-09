#ifndef _GSM_USART_H_
#define _GSM_USART_H_

#include "sys.h"

/* 参数 */
#define GSM_USART_RX_MAX (1024+512)  /* 需容纳无线HTTP升级分块+MIPURC前缀, 原1024+64偏小 */
#define GSM_USART_TX_MAX  1600

/* 函数声明 */

void gsm_usart_init(uint32_t bound);
void gsm_usart_send_str(uint8_t *buff, uint16_t len);

#endif
