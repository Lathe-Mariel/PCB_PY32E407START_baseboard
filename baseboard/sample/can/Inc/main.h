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
 * CAN (FDCAN1) pin assignment on this board
 *   PD0 ------> FDCAN1_RX (AF9) -------- ATA6561 RXD
 *   PD1 ------> FDCAN1_TX (AF9) -------- ATA6561 TXD
 *
 * The ATA6561 STBY pin is driven by PB14 (net "B14"). STBY is active high,
 * so PB14 must be held low to put the transceiver in normal mode.
 */
#define CAN_RX_PIN                     GPIO_PIN_0
#define CAN_TX_PIN                     GPIO_PIN_1
#define CAN_GPIO_PORT                  GPIOD
#define CAN_GPIO_AF                    GPIO_AF9_FDCAN1
#define CAN_GPIO_CLK_ENABLE()          __HAL_RCC_GPIOD_CLK_ENABLE()

/*
 * SLCAN link (UART) pin assignment
 *   PA9  ------> USART1_TX (AF7) --> PC RX (PY-LINK virtual COM port)
 *   PA10 ------> USART1_RX (AF7) <-- PC TX
 * PA9 is routed to J6 pin 20, PA10 to J6 pin 21 (also Pmod2 pin 2) on the
 * baseboard, and to the PY-LINK VCP on the PY32E407V1xT-START module.
 */
#define SLCAN_UART                     USART1
#define SLCAN_UART_BAUDRATE            921600U
#define SLCAN_UART_TX_PIN              GPIO_PIN_9
#define SLCAN_UART_RX_PIN              GPIO_PIN_10
#define SLCAN_UART_GPIO_PORT           GPIOA
#define SLCAN_UART_GPIO_AF             GPIO_AF7_USART1
#define SLCAN_UART_GPIO_CLK_ENABLE()   __HAL_RCC_GPIOA_CLK_ENABLE()
#define SLCAN_UART_IT_PRIORITY         2U

/* ATA6561 standby control (active high: 1 = standby, 0 = normal) */
#define CAN_STBY_PIN                   GPIO_PIN_14
#define CAN_STBY_PORT                  GPIOB
#define CAN_STBY_GPIO_CLK_ENABLE()     __HAL_RCC_GPIOB_CLK_ENABLE()

/* On-board LEDs (active low) */
#define LED_D1_PIN                     GPIO_PIN_3   /* PA3 */
#define LED_D2_PIN                     GPIO_PIN_4   /* PA4 */
#define LED_D3_PIN                     GPIO_PIN_5   /* PA5 */
#define LED_D4_PIN                     GPIO_PIN_6   /* PA6 */
#define LED_PORT                       GPIOA
#define LED_ALL_PINS                   (LED_D1_PIN | LED_D2_PIN | LED_D3_PIN | LED_D4_PIN)
#define LED_GPIO_CLK_ENABLE()          __HAL_RCC_GPIOA_CLK_ENABLE()

/* Interrupt priority for FDCAN_INT0 */
#define CAN_IT_PRIORITY                1U

/* Exported variables --------------------------------------------------------*/
extern FDCAN_HandleTypeDef FdcanHandle;

/* Exported functions prototypes ---------------------------------------------*/
void APP_ErrorHandler(void);
#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
