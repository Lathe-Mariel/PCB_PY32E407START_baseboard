/**
  ******************************************************************************
  * @file    main.h
  * @author  MCU Application Team
  * @brief   Header for main.c file.
  *          LAN8670 10BASE-T1S (RMII) bring-up on the PY32E407 baseboard.
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

/* Private defines -----------------------------------------------------------*/

/*
 * LAN8670 10BASE-T1S PHY (IC2) on the baseboard.
 * It is wired to the ETH RMII pin set, which is fixed on this device:
 *
 *   PB11 ------> ETH_TX_EN
 *   PB12 ------> ETH_TXD0
 *   PB13 ------> ETH_TXD1
 *   PC4  ------> ETH_RXD0
 *   PC5  ------> ETH_RXD1
 *   PD8  ------> ETH_CRS_DV
 *   PC1  ------> ETH_MDC
 *   PA2  ------> ETH_MDIO
 *   PA1  <----- ETH_REF_CLK   (50 MHz from the on-board CB3LV oscillator)
 *
 * LAN8670 strap pins on this board:
 *   MODE0 = 0, MODE1 = 1  -> MODE[1:0] = 10b -> RMII with 50 MHz REFCLKIN
 *   PHYAD[4:0] all pulled low -> PHY address 0
 *   INH/ACMA/GPIO0/nINT/nRST are pulled by 10k resistors.
 */
#define ETH_GPIO_PORT_TX              GPIOB
#define ETH_GPIO_PORT_RX              GPIOC
#define ETH_GPIO_PORT_CRS             GPIOD
#define ETH_GPIO_PORT_MDIO            GPIOA

#define ETH_TX_EN_PIN                 GPIO_PIN_11
#define ETH_TXD0_PIN                  GPIO_PIN_12
#define ETH_TXD1_PIN                  GPIO_PIN_13
#define ETH_RXD0_PIN                  GPIO_PIN_4
#define ETH_RXD1_PIN                  GPIO_PIN_5
#define ETH_CRS_DV_PIN                GPIO_PIN_8
#define ETH_MDC_PIN                   GPIO_PIN_1
#define ETH_MDIO_PIN                  GPIO_PIN_2
#define ETH_REF_CLK_PIN               GPIO_PIN_1

#define ETH_GPIO_AF                   GPIO_AF13_ETH

/* PHY address, PHYAD[4:0] = 0 on this baseboard */
#define LAN8670_PHY_ADDR              0U

/* 10BASE-T1S runs at 10 Mbit/s, full duplex */
#define LAN8670_ETH_SPEED             ETH_SPEED_10M
#define LAN8670_ETH_DUPLEX            ETH_MODE_FULLDUPLEX

/* Number of DMA descriptors, keep in sync with py32e407_hal_conf.h */
#define ETH_RX_DESC_CNT               4U
#define ETH_TX_DESC_CNT               4U

/* Interrupt priority for the ETH global interrupt */
#define ETH_IT_PRIORITY               3U

/* USART1 (PY-LINK virtual COM port) used for the diagnostic console */
#define CONSOLE_UART                  USART1
#define CONSOLE_UART_BAUDRATE         921600U
#define CONSOLE_UART_TX_PIN           GPIO_PIN_9
#define CONSOLE_UART_RX_PIN           GPIO_PIN_10
#define CONSOLE_UART_GPIO_PORT        GPIOA
#define CONSOLE_UART_GPIO_AF          GPIO_AF7_USART1
#define CONSOLE_UART_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()
#define CONSOLE_UART_IT_PRIORITY      2U

/* On-board LEDs (active low).
   Board silk order is D5=PA3, D4=PA4, D3=PA5, D2=PA6, so the schematic
   reference and the port bit run in opposite directions.  The macros below
   keep the application naming (LED_D1..LED_D4 in port order PA3..PA6). */
#define LED_D1_PIN                    GPIO_PIN_3   /* PA3 : board D5, MAC init failure */
#define LED_D2_PIN                    GPIO_PIN_4   /* PA4 : board D4, frames received  */
#define LED_D3_PIN                    GPIO_PIN_5   /* PA5 : board D3, frames sent      */
#define LED_D4_PIN                    GPIO_PIN_6   /* PA6 : board D2, demo TX blink    */
#define LED_PORT                      GPIOA
#define LED_ALL_PINS                  (LED_D1_PIN | LED_D2_PIN | LED_D3_PIN | LED_D4_PIN)
#define LED_GPIO_CLK_ENABLE()         __HAL_RCC_GPIOA_CLK_ENABLE()

/* EtherType used by the PHY bring-up demo frames */
#define DEMO_ETHERTYPE                0x88B5U

/* Exported variables --------------------------------------------------------*/
extern ETH_HandleTypeDef heth;

/* Exported functions prototypes ---------------------------------------------*/
void APP_ErrorHandler(void);

/* Diagnostic console -------------------------------------------------------*/
void Console_Init(void);
void Console_Write(const char *Str);
void Console_Printf(const char *Fmt, ...);
extern SCI_HandleTypeDef hConsoleUart;

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
