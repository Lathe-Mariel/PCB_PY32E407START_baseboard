/**
  ******************************************************************************
  * @file    main.h
  * @author  MCU Application Team
  * @brief   Header for main.c file.
  *          This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "py32e4xx_hal.h"
#include "py32e407xx_Start_Kit.h"

/* Private includes ----------------------------------------------------------*/
/* Private defines -----------------------------------------------------------*/
/*
 * Push switches (active low: pressed = GPIO_PIN_RESET)
 *   SW1 : PF3
 *   SW2 : PC13
 *   SW3 : PB7
 *   SW4 : PB9
 */
#define SW_PF3_PIN                     GPIO_PIN_3
#define SW_PC13_PIN                    GPIO_PIN_13
#define SW_PB7_PIN                     GPIO_PIN_7
#define SW_PB9_PIN                     GPIO_PIN_9

#define SW_PF3_PORT                    GPIOF
#define SW_PC13_PORT                   GPIOC
#define SW_PB7_PORT                    GPIOB
#define SW_PB9_PORT                    GPIOB

/* Switches grouped by port for GPIO initialization */
#define SW_PB_PINS                     (SW_PB7_PIN | SW_PB9_PIN)
#define SW_PC_PINS                     (SW_PC13_PIN)
#define SW_PF_PINS                     (SW_PF3_PIN)
#define SW_PB_GPIO_CLK_ENABLE()        __HAL_RCC_GPIOB_CLK_ENABLE()
#define SW_PC_GPIO_CLK_ENABLE()        __HAL_RCC_GPIOC_CLK_ENABLE()
#define SW_PF_GPIO_CLK_ENABLE()        __HAL_RCC_GPIOF_CLK_ENABLE()

/*
 * LEDs (active low: lit = GPIO_PIN_RESET)
 *   LED1 : PA3
 *   LED2 : PA4
 *   LED3 : PA5
 *   LED4 : PA6
 */
#define LED_PA3_PIN                    GPIO_PIN_3
#define LED_PA4_PIN                    GPIO_PIN_4
#define LED_PA5_PIN                    GPIO_PIN_5
#define LED_PA6_PIN                    GPIO_PIN_6

#define LED_PORT                       GPIOA
#define LED_ALL_PINS                   (LED_PA3_PIN | LED_PA4_PIN | LED_PA5_PIN | LED_PA6_PIN)
#define LED_GPIO_CLK_ENABLE()          __HAL_RCC_GPIOA_CLK_ENABLE()

/* Exported variables prototypes ---------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/
void APP_ErrorHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
