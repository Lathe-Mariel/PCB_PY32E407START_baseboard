/**
  ******************************************************************************
  * @file    py32e407_hal_msp.c
  * @author  MCU Application Team
  * @brief   MSP initialization for ETH (RMII) and the diagnostic USART1.
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
* @brief ETH MSP Initialization
* This function configures the hardware resources used by the RMII interface
* @param heth: ETH handle pointer
* @retval None
*/
void HAL_ETH_MspInit(ETH_HandleTypeDef *heth)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (heth->Instance == ETH)
  {
    /* Peripheral clock enable. The ETH MAC additionally needs the separate
       TX/RX clock gates, otherwise the MAC stays silent. */
    __HAL_RCC_ETH_CLK_ENABLE();
    __HAL_RCC_ETHTX_CLK_ENABLE();
    __HAL_RCC_ETHRX_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /**ETH GPIO Configuration
    PA1      ------> ETH_REF_CLK
    PA2      ------> ETH_MDIO
    PB11     ------> ETH_TX_EN
    PB12     ------> ETH_TXD0
    PB13     ------> ETH_TXD1
    PC1      ------> ETH_MDC
    PC4      ------> ETH_RXD0
    PC5      ------> ETH_RXD1
    PD8      ------> ETH_CRS_DV
    */
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = ETH_GPIO_AF;

    GPIO_InitStruct.Pin = ETH_REF_CLK_PIN | ETH_MDIO_PIN;
    HAL_GPIO_Init(ETH_GPIO_PORT_MDIO, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = ETH_TX_EN_PIN | ETH_TXD0_PIN | ETH_TXD1_PIN;
    HAL_GPIO_Init(ETH_GPIO_PORT_TX, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = ETH_MDC_PIN | ETH_RXD0_PIN | ETH_RXD1_PIN;
    HAL_GPIO_Init(ETH_GPIO_PORT_RX, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = ETH_CRS_DV_PIN;
    HAL_GPIO_Init(ETH_GPIO_PORT_CRS, &GPIO_InitStruct);

    /* ETH interrupt Init */
    HAL_NVIC_SetPriority(ETH_IRQn, ETH_IT_PRIORITY, 0);
    HAL_NVIC_EnableIRQ(ETH_IRQn);
  }
}

/**
* @brief ETH MSP De-Initialization
* @param heth: ETH handle pointer
* @retval None
*/
void HAL_ETH_MspDeInit(ETH_HandleTypeDef *heth)
{
  if (heth->Instance == ETH)
  {
    __HAL_RCC_ETH_CLK_DISABLE();
    __HAL_RCC_ETHTX_CLK_DISABLE();
    __HAL_RCC_ETHRX_CLK_DISABLE();

    HAL_GPIO_DeInit(ETH_GPIO_PORT_MDIO, ETH_REF_CLK_PIN | ETH_MDIO_PIN);
    HAL_GPIO_DeInit(ETH_GPIO_PORT_TX, ETH_TX_EN_PIN | ETH_TXD0_PIN | ETH_TXD1_PIN);
    HAL_GPIO_DeInit(ETH_GPIO_PORT_RX, ETH_MDC_PIN | ETH_RXD0_PIN | ETH_RXD1_PIN);
    HAL_GPIO_DeInit(ETH_GPIO_PORT_CRS, ETH_CRS_DV_PIN);

    HAL_NVIC_DisableIRQ(ETH_IRQn);
  }
}

/**
* @brief SCI (USART1) MSP Initialization
* This function configures the hardware resources used by the diagnostic link
* @param hsci: SCI handle pointer
* @retval None
*/
void HAL_SCI_MspInit(SCI_HandleTypeDef *hsci)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (hsci->Instance == CONSOLE_UART)
  {
    __HAL_RCC_USART1_CLK_ENABLE();

    CONSOLE_UART_GPIO_CLK_ENABLE();

    /**USART1 GPIO Configuration
    PA9      ------> USART1_TX
    PA10     ------> USART1_RX
    */
    GPIO_InitStruct.Pin       = CONSOLE_UART_TX_PIN | CONSOLE_UART_RX_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = CONSOLE_UART_GPIO_AF;
    HAL_GPIO_Init(CONSOLE_UART_GPIO_PORT, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(USART1_IRQn, CONSOLE_UART_IT_PRIORITY, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
  }
}

/**
* @brief SCI (USART1) MSP De-Initialization
* @param hsci: SCI handle pointer
* @retval None
*/
void HAL_SCI_MspDeInit(SCI_HandleTypeDef *hsci)
{
  if (hsci->Instance == CONSOLE_UART)
  {
    __HAL_RCC_USART1_CLK_DISABLE();

    HAL_GPIO_DeInit(CONSOLE_UART_GPIO_PORT,
                    CONSOLE_UART_TX_PIN | CONSOLE_UART_RX_PIN);

    HAL_NVIC_DisableIRQ(USART1_IRQn);
  }
}

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
