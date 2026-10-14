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
#include "music.h"

/* Private define ------------------------------------------------------------*/
/**
  * @brief  Idle time between two playbacks, in milliseconds.
  */
#define APP_SONG_GAP_MS             1500U

#define LED_GPIO_PIN                 LED2_PIN
#define LED_GPIO_PORT                LED2_GPIO_PORT
#define LED_GPIO_CLK_ENABLE()        LED2_GPIO_CLK_ENABLE()

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef    TimHandle;
TIM_OC_InitTypeDef              sPWMConfig;
TIM_BreakDeadTimeConfigTypeDef sBreakConfig;

/* Private user code ---------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void APP_SystemClockConfig(void);
static void APP_GpioConfig(void);

/**
  * @brief  Main program.
  *
  *         The board plays platform.wav (converted to 8 bit PCM at 16 kHz
  *         by tools/wav2pcm.py, offline-equalised and level-compressed for
  *         speech intelligibility) on the passive piezo buzzer connected to
  *         PA9/PA12. The buzzer is driven differentially by the complementary
  *         PWM pair TIM1_CH2 (PA9) / TIM1_CH2N (PA12) as a PWM DAC, and a
  *         runtime 6-band equaliser (same band layout as the offline one)
  *         refines the tone on the target. See music.c / platform_pcm.c.
  * @retval int
  */
int main(void)
{
  /* Reset of all peripherals, Initializes the Systick */
  HAL_Init();

  /* Configure Systemclock */
  APP_SystemClockConfig();

  /* Initialize GPIO */
  APP_GpioConfig();

  /* Initialize the buzzer output and start playing */
  MUSIC_Init();
  MUSIC_Play();

  /* Infinite loop */
  while (1)
  {
    /* Restart the playback when it is over */
    if (MUSIC_IsPlaying() == 0U)
    {
      HAL_Delay(APP_SONG_GAP_MS);
      MUSIC_Play();
    }

    /* Heartbeat: 2 Hz blink while the melody is playing */
    HAL_Delay(250);
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
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

  LED_GPIO_CLK_ENABLE();                                 /* Enable GPIOB clock */

  GPIO_InitStruct.Pin = LED_GPIO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;            /* Push-pull output */
  GPIO_InitStruct.Pull = GPIO_PULLUP;                    /* Enable pull-up */
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;          /* GPIO speed */
  /* GPIO initialization */
  HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);
}

/**
  * @brief  System Clock Configuration
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
  ClkInitstruct.APB2CLKDivider  = RCC_HCLK_DIV1;                        /* APB2 clock 1 division */
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
  /* Infinite loop */
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
     for example: printf("Wrong parameters value: file %s on line %d\r\n", file, line)  */
  /* Infinite loop */
  while (1)
  {
  }
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
