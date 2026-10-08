/**
  ******************************************************************************
  * @file    cmox_low_level_template.c
  * @brief
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#include "cmox_init.h"
#include "cmox_low_level.h"
#include "stm32f4xx.h"
/* #include "stm32<series>xx_hal.h" */

/**
  * @brief          CMOX library low level initialization
  * @param          pArg User defined parameter that is transmitted from initialize service
  * @retval         Initialization status: @ref CMOX_INIT_SUCCESS / @ref CMOX_INIT_FAIL
  */
cmox_init_retval_t cmox_ll_init(void *pArg)
{
  (void)pArg;
  /* Ensure CRC is enabled for cryptographic processing */
	RCC_AHB1PeriphResetCmd(RCC_AHB1Periph_CRC, DISABLE);   //失能CRC
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_CRC, ENABLE);    //使能CRC时钟
  return CMOX_INIT_SUCCESS;
}

/**
  * @brief          CMOX library low level de-initialization
  * @param          pArg User defined parameter that is transmitted from finalize service
  * @retval         De-initialization status: @ref CMOX_INIT_SUCCESS / @ref CMOX_INIT_FAIL
  */
cmox_init_retval_t cmox_ll_deInit(void *pArg)
{
  (void)pArg;
  /* Do not turn off CRC to avoid side effect on other SW parts using it */
  return CMOX_INIT_SUCCESS;
}
