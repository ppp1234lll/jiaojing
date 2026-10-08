/********************************************************************************
* @File name  : 
* @Description: 
* @Author     : ZHLE
*  Version Date        Modification Description
*  1.0     
********************************************************************************/
#ifndef _RS485_H_
#define _RS485_H_
#include "sys.h"


void rs485_init(uint32_t bound);
void rs485_send_char(uint8_t ch);
void rs485_send_str(uint8_t *data, uint16_t len);

#endif
