/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    uart
  * @brief   This file provides code for the configuration
  *          of all used UART.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2025 Puya Semiconductor Co.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by Puya under BSD 3-Clause license,
  * the License ; You may not use this file except in compliance with the
  * License.You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "uart.h"
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN Define */

/* USER CODE END Define */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN Macro */

/* USER CODE END Macro */

/* Public variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Private */

/* USER CODE END Private */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* External variables --------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/***************************************
 * @brief UART1 Init
 **************************************/
void Studio_UART1_Init(void)
{
  /* USER CODE BEGIN Studio_UART1_Init 0 */

  /* USER CODE END Studio_UART1_Init 0 */

  huart1.Instance = UART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.MsbFirst = UART_MSB_FIRST_DISABLE;

  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Studio_UART1_Init 1 */

  /* USER CODE END Studio_UART1_Init 1 */
}

void HAL_UART_MspInit(UART_HandleTypeDef* huart)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (huart->Instance == UART1)
  {
  /* USER CODE BEGIN UART1_MspInit 0 */

  /* USER CODE END UART1_MspInit 0 */

    __HAL_RCC_UART1_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**UART1 GPIO Configuration
    *PC10     ------> UART1_TX
    *PC11     ------> UART1_RX
    */

    GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF5_UART1;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN UART1_MspInit 1 */

  /* USER CODE END UART1_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* huart)
{
  if (huart->Instance == UART1)
  {
  /* USER CODE BEGIN UART1_MspDeInit 0 */

  /* USER CODE END UART1_MspDeInit 0 */

    __HAL_RCC_UART1_CLK_DISABLE();

    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_10|GPIO_PIN_11);
  /* USER CODE BEGIN UART1_MspDeInit 1 */

  /* USER CODE END UART1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN ExternalFunctions */

/* USER CODE END ExternalFunctions */

/* USER CODE BEGIN 0 */
#if (defined (__CC_ARM)) || (defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050))
/**
  * @brief  writes a character to the usart
  * @param  ch
  *         *f
  * @retval the character
  */
int fputc(int ch, FILE *f)
{
  /* Send a byte to USART */
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 1000);

  return (ch);
}

#elif defined(__ICCARM__)
/**
  * @brief  writes a character to the usart
  * @param  ch
  *         *f
  * @retval the character
  */
int putchar(int ch)
{
  /* Send a byte to USART */
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 1000);

  return (ch);
}
#elif  defined(__GNUC__)
/**
  * @brief  writes a character to the usart
  * @param  ch
  * @retval the character
  */
int __io_putchar(int ch)
{
  /* Send a byte to USART */
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 1000);

  return ch;
}

int _write(int file, char *ptr, int len)
{
  int DataIdx;
  for (DataIdx=0;DataIdx<len;DataIdx++)
  {
    __io_putchar(*ptr++);
  }
  return len;
}
#endif
/* USER CODE END 0 */

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
