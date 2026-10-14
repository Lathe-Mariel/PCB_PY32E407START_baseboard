/**
  ******************************************************************************
  * @file    py32e407_hal_msp.c
  * @author  MCU Application Team
  * @brief   This file provides code for the MSP Initialization
  *          and de-Initialization codes.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2023 Puya Semiconductor Co.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by Puya under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* External functions --------------------------------------------------------*/

/**
  * @brief Initialize global MSP
  */
void HAL_MspInit(void)
{
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();
}

/**
  * @brief Initialize TIM1 related MSP (buzzer complementary PWM)
  */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
  GPIO_InitTypeDef   GPIO_InitStruct = {0};
  /* Enable TIM1 clock */
  __HAL_RCC_TIM1_CLK_ENABLE();
  /* Enable GPIOA clock */
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* Configure PA9/PA12 alternate Function: TIM1 */
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF6_TIM1;
  /* A9-TIM1_CH2  (buzzer terminal A) */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  /* A12-TIM1_CH2N (buzzer terminal B) */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/**
  * @brief Initialize TIM2 related MSP (note pacing timer, no pins)
  */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2)
  {
    /* Enable TIM2 clock */
    __HAL_RCC_TIM2_CLK_ENABLE();
  }
}

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
