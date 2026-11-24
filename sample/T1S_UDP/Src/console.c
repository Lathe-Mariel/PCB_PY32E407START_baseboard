/**
  ******************************************************************************
  * @file    console.c
  * @brief   Polled USART1 console used for the LAN8670 bring-up diagnostics.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "main.h"

/* Private define ------------------------------------------------------------*/
#define CONSOLE_TX_TIMEOUT_MS        100U

/* Private variables ---------------------------------------------------------*/
SCI_HandleTypeDef hConsoleUart;

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialise USART1 at 921600 8N1 for the diagnostic output.
  */
void Console_Init(void)
{
  hConsoleUart.Instance          = CONSOLE_UART;
  hConsoleUart.Init.BaudRate     = CONSOLE_UART_BAUDRATE;
  hConsoleUart.Init.WordLength   = SCI_WORDLENGTH_8B;
  hConsoleUart.Init.StopBits     = SCI_STOPBITS_1;
  hConsoleUart.Init.Parity       = SCI_PARITY_NONE;
  hConsoleUart.Init.Mode         = SCI_MODE_TX_RX;
  hConsoleUart.Init.HwFlowCtl    = SCI_HWCONTROL_NONE;
  hConsoleUart.Init.OverSampling = SCI_OVERSAMPLING_16;
  hConsoleUart.AdvancedInit.AdvFeatureInit = SCI_ADVFEATURE_NO_INIT;
  hConsoleUart.FifoMode          = SCI_FIFOMODE_DISABLE;

  if (HAL_SCI_Init(&hConsoleUart) != HAL_OK)
  {
    APP_ErrorHandler();
  }
}

/**
  * @brief  Write a null terminated string to the console.
  */
void Console_Write(const char *Str)
{
  if (Str == NULL)
  {
    return;
  }

  (void)HAL_SCI_Transmit(&hConsoleUart, (uint8_t *)Str, (uint32_t)strlen(Str),
                         CONSOLE_TX_TIMEOUT_MS);
}

/**
  * @brief  printf style helper built on top of Console_Write().
  */
void Console_Printf(const char *Fmt, ...)
{
  char buffer[160];
  va_list args;

  va_start(args, Fmt);
  (void)vsnprintf(buffer, sizeof(buffer), Fmt, args);
  va_end(args);

  Console_Write(buffer);
}

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
