#ifndef __ADC_H
#define __ADC_H	

#include "sys.h"

void Adc_Init(void);
void ADC_DMA_Config(void);
u16  Get_Key_Adc(void); 
u16 Get_Adc_Average(u8 times);
uint16_t Get_Adc_Values(void);
void bat_set_control_status_detection(uint8_t status);
uint8_t bat_get_control_status_detection(void);

void Adc_Test(void);
#endif 

