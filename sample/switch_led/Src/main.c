/**
  ******************************************************************************
  * @file    main.c
  * @author  MCU Application Team
  * @brief   Main program body
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

/* Private define ------------------------------------------------------------*/
/* Pin definitions are provided in main.h */

/* Private variables ---------------------------------------------------------*/
/* Private user code ---------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void APP_SystemClockConfig(void);
static void APP_GpioConfig(void);

/**
  * @brief  Main program.
  * @retval int
  */
int main(void)
{
  uint8_t sw1Pressed;
  uint8_t sw2Pressed;
  uint8_t sw3Pressed;
  uint8_t sw4Pressed;

  /* Reset of all peripherals, Initializes the Systick. */ 
  HAL_Init();
  
  /* System clock configuration */
  APP_SystemClockConfig(); 
  
  /* Initialize GPIO */
  APP_GpioConfig();

  while (1)
  {
    /* Read switches (active low: pressed = GPIO_PIN_RESET) */
    sw1Pressed = (HAL_GPIO_ReadPin(SW_PF3_PORT,  SW_PF3_PIN)  == GPIO_PIN_RESET);
    sw2Pressed = (HAL_GPIO_ReadPin(SW_PC13_PORT, SW_PC13_PIN) == GPIO_PIN_RESET);
    sw3Pressed = (HAL_GPIO_ReadPin(SW_PB7_PORT,  SW_PB7_PIN)  == GPIO_PIN_RESET);
    sw4Pressed = (HAL_GPIO_ReadPin(SW_PB9_PORT,  SW_PB9_PIN)  == GPIO_PIN_RESET);

    /* Drive LEDs (active low: lit = GPIO_PIN_RESET) */
    HAL_GPIO_WritePin(LED_PORT, LED_PA3_PIN, sw1Pressed ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_PORT, LED_PA4_PIN, sw2Pressed ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_PORT, LED_PA5_PIN, sw3Pressed ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_PORT, LED_PA6_PIN, sw4Pressed ? GPIO_PIN_RESET : GPIO_PIN_SET);
  }  
}

/**
  * @brief  GPIO configuration.
  * @param  None
  * @retval None
  */
static void APP_GpioConfig(void)
{
  GPIO_InitTypeDef  GPIO_InitStruct = {0};

  /* Enable clocks for the ports used by switches and LEDs */
  SW_PB_GPIO_CLK_ENABLE();                               /* Enable GPIOB clock */
  SW_PC_GPIO_CLK_ENABLE();                               /* Enable GPIOC clock */
  SW_PF_GPIO_CLK_ENABLE();                               /* Enable GPIOF clock */
  LED_GPIO_CLK_ENABLE();                                 /* Enable GPIOA clock */

  /* Switches: input with pull-up (active low) */
  GPIO_InitStruct.Pin = SW_PB_PINS;                      /* PB7  (SW3), PB9  (SW4) */
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;                /* Input mode */
  GPIO_InitStruct.Pull = GPIO_PULLUP;                    /* Enable pull-up */
  HAL_GPIO_Init(SW_PB7_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SW_PC_PINS;                      /* PC13 (SW2) */
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;                /* Input mode */
  GPIO_InitStruct.Pull = GPIO_PULLUP;                    /* Enable pull-up */
  HAL_GPIO_Init(SW_PC13_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = SW_PF_PINS;                      /* PF3  (SW1) */
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;                /* Input mode */
  GPIO_InitStruct.Pull = GPIO_PULLUP;                    /* Enable pull-up */
  HAL_GPIO_Init(SW_PF3_PORT, &GPIO_InitStruct);

  /* LEDs: push-pull output (active low) */
  GPIO_InitStruct.Pin = LED_ALL_PINS;                    /* PA3, PA4, PA5, PA6 */
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;            /* Push-pull output */
  GPIO_InitStruct.Pull = GPIO_PULLUP;                    /* Enable pull-up */
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;          /* GPIO speed */  
  HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);

  /* Turn all LEDs off (active low) */
  HAL_GPIO_WritePin(LED_PORT, LED_ALL_PINS, GPIO_PIN_SET);
}

/**
  * @brief  System clock configuration function.
  * @param  None
  * @retval None
  */
static void APP_SystemClockConfig(void)
{
  RCC_OscInitTypeDef  OscInitstruct = {0};
  RCC_ClkInitTypeDef  ClkInitstruct = {0};
  
  OscInitstruct.OscillatorType  = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSE | 
                                  RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_HSI48M;
  OscInitstruct.HSEState        = RCC_HSE_OFF;                              /* Close HSE */
/* OscInitstruct.HSEFreq         = RCC_HSE_16_32MHz; */                       /* Choose HSE frequency of 16-32MHz */
  OscInitstruct.HSI48MState     = RCC_HSI48M_OFF;                           /* Close HSI48M */
  OscInitstruct.HSIState        = RCC_HSI_ON;                               /* Enable HSI */
  OscInitstruct.LSEState        = RCC_LSE_OFF;                              /* Close LSE */
/* OscInitstruct.LSEDriver       = RCC_LSEDRIVE_HIGH; */                    /* Drive capability level: high */
  OscInitstruct.LSIState        = RCC_LSI_OFF;                              /* Close LSI */
  OscInitstruct.PLL.PLLState    = RCC_PLL_OFF;                              /* Close PLL */
/* OscInitstruct.PLL.PLLSource   = RCC_PLLSOURCE_HSI_DIV2; */               /* PLL clock source selection HSI/2 */
/* OscInitstruct.PLL.PLLPrediv   = RCC_PLL_PREDIV_DIV1; */                  /* PLL clock Prediv factor */
/* OscInitstruct.PLL.PLLMUL      = 12; */                                   /* PLL clock source 12-fold frequency */
/* OscInitstruct.PLL.PLLPostdiv  = RCC_PLL_POSTDIV_DIV1; */                 /* PLL clock Postdiv factor */
  /* Configure oscillator */
  if(HAL_RCC_OscConfig(&OscInitstruct) != HAL_OK)
  {
    APP_ErrorHandler();
  }
  
  ClkInitstruct.ClockType       = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  ClkInitstruct.SYSCLKSource    = RCC_SYSCLKSOURCE_HSI;                 /* System clock selection HSI */
  ClkInitstruct.AHBCLKDivider   = RCC_SYSCLK_DIV1;                      /* AHB clock 1 division */
  ClkInitstruct.APB1CLKDivider  = RCC_HCLK_DIV1;                        /* APB1 clock 1 division */
  ClkInitstruct.APB2CLKDivider  = RCC_HCLK_DIV2;                        /* APB2 clock 2 division */
  /* Configure Clock */
  if(HAL_RCC_ClockConfig(&ClkInitstruct, FLASH_LATENCY_0) != HAL_OK)
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
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
