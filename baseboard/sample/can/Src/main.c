/**
  ******************************************************************************
  * @file    main.c
  * @author  MCU Application Team
  * @brief   SLCAN (LAWICEL) CAN adapter bridge.
  *
  *          PC --UART(SLCAN ASCII)--> USART1 --FDCAN1--> ATA6561 --> CAN bus
  *
  *          The board behaves like a CANable / CANUSB style adapter: the host
  *          opens the virtual COM port, sends the SLCAN command set and the
  *          firmware forwards the frames to the on-board CAN transceiver.
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
#include "slcan.h"

/* Private variables ---------------------------------------------------------*/
FDCAN_HandleTypeDef FdcanHandle = {0};

/* Private function prototypes -----------------------------------------------*/
static void APP_SystemClockConfig(void);
static void APP_GpioInit(void);

/**
  * @brief  Main program
  * @retval int
  */
int main(void)
{
  /* Reset of all peripherals, Initializes the Systick */
  HAL_Init();

  /* System clock configuration: HSI 16 MHz / 2, PLL x20 /2 = 80 MHz SYSCLK */
  APP_SystemClockConfig();

  /* Initialize the board LEDs and the CAN transceiver standby pin */
  APP_GpioInit();

  /* Initialize the SLCAN link (USART1) and FDCAN1, CAN stays closed until
     the host sends 'O' or 'L' */
  SLCAN_Init();

  while (1)
  {
    SLCAN_Poll();
  }
}

/**
  * @brief  Configure the board LEDs and the CAN transceiver standby pin.
  *         LEDs are active low, the ATA6561 STBY pin is active high.
  * @param  None
  * @retval None
  */
static void APP_GpioInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  LED_GPIO_CLK_ENABLE();
  CAN_STBY_GPIO_CLK_ENABLE();

  /* LEDs off (active low), then configure them as push-pull outputs */
  HAL_GPIO_WritePin(LED_PORT, LED_ALL_PINS, GPIO_PIN_SET);
  GPIO_InitStruct.Pin   = LED_ALL_PINS;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);

  /* ATA6561 STBY low -> normal mode */
  HAL_GPIO_WritePin(CAN_STBY_PORT, CAN_STBY_PIN, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin   = CAN_STBY_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CAN_STBY_PORT, &GPIO_InitStruct);
}

/**
  * @brief  System clock configuration function.
  *         HSI(16 MHz)/2 * 20 = 160 MHz VCO, then /2 -> 80 MHz SYSCLK.
  *         80 MHz is used so the CAN kernel clock (PLL/1 = 80 MHz) divides
  *         cleanly into the supported CAN bit times.
  * @param  None
  * @retval None
  */
static void APP_SystemClockConfig(void)
{
  RCC_OscInitTypeDef  OscInitstruct = {0};
  RCC_ClkInitTypeDef  ClkInitstruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  OscInitstruct.OscillatorType  = RCC_OSCILLATORTYPE_HSI;
  OscInitstruct.HSIState        = RCC_HSI_ON;                               /* Enable HSI */
  OscInitstruct.HSEState        = RCC_HSE_OFF;                              /* Close HSE */
  OscInitstruct.HSI48MState     = RCC_HSI48M_OFF;                           /* Close HSI48M */
  OscInitstruct.LSEState        = RCC_LSE_OFF;                              /* Close LSE */
  OscInitstruct.LSIState        = RCC_LSI_OFF;                              /* Close LSI */
  OscInitstruct.PLL.PLLState    = RCC_PLL_ON;                               /* Enable PLL */
  OscInitstruct.PLL.PLLSource   = RCC_PLLSOURCE_HSI_DIV2;                   /* PLL clock source HSI/2 = 8 MHz */
  OscInitstruct.PLL.PLLPrediv   = RCC_PLL_PREDIV_DIV1;                      /* PLL clock Prediv factor */
  OscInitstruct.PLL.PLLMUL      = 20;                                       /* 8 MHz * 20 = 160 MHz VCO */
  OscInitstruct.PLL.PLLPostdiv  = RCC_PLL_POSTDIV_DIV2;                     /* 160 MHz / 2 = 80 MHz */
  if (HAL_RCC_OscConfig(&OscInitstruct) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  ClkInitstruct.ClockType       = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  ClkInitstruct.SYSCLKSource    = RCC_SYSCLKSOURCE_PLLCLK;                  /* System clock selection PLL */
  ClkInitstruct.AHBCLKDivider   = RCC_SYSCLK_DIV1;                          /* AHB clock = 80 MHz */
  ClkInitstruct.APB1CLKDivider  = RCC_HCLK_DIV1;                            /* APB1 clock = 80 MHz */
  ClkInitstruct.APB2CLKDivider  = RCC_HCLK_DIV1;                            /* APB2 clock = 80 MHz */
  if (HAL_RCC_ClockConfig(&ClkInitstruct, FLASH_LATENCY_3) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  /* CAN kernel clock = PLL / 1 = 80 MHz, USART1 stays on PCLK2 = 80 MHz */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_CAN;
  PeriphClkInit.CanClockSelection    = RCC_CANCLKSOURCE_PLL;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    APP_ErrorHandler();
  }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void APP_ErrorHandler(void)
{
  while (1)
  {
  }
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* Users can add their own printing information as needed,
     for example: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* Infinite loop */
  while (1)
  {
  }
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/