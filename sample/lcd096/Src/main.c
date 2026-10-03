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
#include "lcd096.h"

/* Private define ------------------------------------------------------------*/
#define LED_GPIO_PIN                 LED2_PIN
#define LED_GPIO_PORT                LED2_GPIO_PORT
#define LED_GPIO_CLK_ENABLE()        LED2_GPIO_CLK_ENABLE()

/* --------------------------------------------------------------------------
   On-board debug LEDs (4x, ACTIVE LOW: pin driven LOW => LED lit).
   They display the bring-up STAGE NUMBER in binary so a black screen can be
   localised without a scope (bit0 = LED1/PA3, bit1 = LED2/PA4,
   bit2 = LED3/PA5, bit3 = LED4/PA6). Read the pattern left-to-right as
   LED4 LED3 LED2 LED1 = bit3 bit2 bit1 bit0.

   Stage codes emitted by this file and by lcd096.c via LCD096_DebugHook():
     1  clock + debug-LED init done
     2  APP_GpioConfig() done
     3  LCD GPIO config done          (start of LCD096_Init)
     4  hardware reset pulse done
     5  SWRESET (0x01) sent
     6  SLPOUT  (0x11) sent
     7  COLMOD+MADCTL sent
     8  FRMCTR/power/VCOM sent
     9  gamma sent
    10  DISPON (0x29) sent
    11  frame-memory fill finished
    81  pin self-test: CS  driven HIGH
    82  pin self-test: SDA (MOSI) driven HIGH
    83  pin self-test: DC  driven HIGH
    84  pin self-test: SCK driven HIGH
    85  pin self-test: RST driven HIGH

   During LCD096_Init() the four LEDs hold the stage code, so a pattern that
   never changes means the code stopped there. Once the colour sweep starts the
   LEDs are free: LED1 (PA3) mirrors the SPI byte counter (see
   APP_SHOW_BUS_ACTIVITY) and flickers while data is going out.
   -------------------------------------------------------------------------- */
#define DBG_LED_PORT                 GPIOA
#define DBG_LED1_PIN                 GPIO_PIN_3
#define DBG_LED2_PIN                 GPIO_PIN_4
#define DBG_LED3_PIN                 GPIO_PIN_5
#define DBG_LED4_PIN                 GPIO_PIN_6
#define DBG_LED_ALL_PINS             (DBG_LED1_PIN | DBG_LED2_PIN | \
                                      DBG_LED3_PIN | DBG_LED4_PIN)

/* Private variables ---------------------------------------------------------*/
static volatile uint8_t g_dbg_stage = 0U;
static volatile uint32_t g_bus_bytes = 0U;   /* bytes pushed out by lcd096.c */

/* Set to 1 to run the pin self-test (stage 81..85) instead of the normal boot,
   REPEATING for ever. Each LCD pin is then driven HIGH on its own for 2 s, so a
   logic analyzer can easily capture which MCU pin drives which signal:
     stage 81 = CS (row2 = PD3), 82 = MOSI (PD5), 83 = DC (PD6),
     84 = CLK (PD4), 85 = RST (PC8).
   Probe every MCU pin and the one pulsing during stage 81 is CS, etc. */
#define APP_RUN_PIN_SELFTEST         0

/* Set to 1 to light LED1 (PA3) whenever the software SPI is pushing bytes, i.e.
   a "bus alive" indicator. If LED1 never lights the MCU is not transmitting at
   all; if it lights the panel is receiving data and the fault is on the panel
   side (wiring, reset, or colour polarity). */
#define APP_SHOW_BUS_ACTIVITY        1

/* Private user code ---------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void APP_SystemClockConfig(void);
static void APP_GpioConfig(void);
static void APP_DebugLedInit(void);
static void APP_DebugLedShow(uint8_t code);

/**
  * @brief  Debug hook called from lcd096.c at each bring-up step.
  * @param  stage stage code (see the table above).
  * @retval None
  */
void LCD096_DebugHook(uint8_t stage);

/**
  * @brief  Called by lcd096.c for every byte pushed out over the software SPI.
  *         Used to prove that the MCU really is transmitting.
  * @retval None
  */
void LCD096_TxHook(void);

/**
  * @brief  Main program.
  *
  *         Brings up the Pmod-LCD-0.96 panel (ST7735S, 160x80) over software
  *         SPI. The four on-board LEDs show the bring-up stage in binary so a
  *         failure can be localised.
  *
  *         A Pmod can be inserted either way round, so BOTH electrically valid
  *         pin maps are initialised, then the one that actually paints the panel
  *         wins. The display then sweeps through a few colours for ever, which
  *         makes the panel's colour polarity obvious at a glance (a red fill
  *         appearing as cyan, or a white fill as black, is a polarity problem,
  *         not a wiring problem).
  * @retval int
  */
int main(void)
{
  /* Reset of all peripherals, Initializes the Systick. */ 
  HAL_Init();
  
  /* System clock configuration */
  APP_SystemClockConfig(); 

  /* Debug LEDs first so stage 1 proves the clock + GPIO bring-up. */
  APP_DebugLedInit();
  LCD096_DebugHook(1U);

  /* Initialize GPIO */
  APP_GpioConfig();
  LCD096_DebugHook(2U);

#if (APP_RUN_PIN_SELFTEST)
  /* Drive one line at a time, for ever. The LEDs show which line it is:
       0x0A = stage 81 = CS,  0x0B = 82 = MOSI, 0x0C = 83 = DC,
       0x0D = 84 = CLK,       0x0E = 85 = RST.
     With a logic analyzer you can see exactly which MCU pin moves during each
     2 s window, which settles the mapping and the Pmod orientation at once. */
  for (;;)
  {
    LCD096_SetPmodRow(2U);
    LCD096_PinSelfTest(2000U);
  }
#else
  /* Only the requested mapping is driven, and it is then left alone, so the
     result cannot be confused by a second code path repainting the panel.
     CS=PD3 MOSI=PD5 DC=PD6 CLK=PD4 RST=PC8. */
  LCD096_SetPmodRow(2U);
  LCD096_Init();
  LCD096_FillScreen(LCD096_WHITE);

  for (;;)
  {
#if (APP_SHOW_BUS_ACTIVITY)
    /* LED1 mirrors the SPI byte counter's low bit. A steady ON or OFF means no
       bytes are being sent; a visible flicker means the bus is alive. */
    HAL_GPIO_WritePin(DBG_LED_PORT, DBG_LED1_PIN,
                      ((g_bus_bytes & 1U) != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
#endif
    HAL_Delay(100);
  }
#endif
}

void LCD096_TxHook(void)
{
  ++g_bus_bytes;
}

/**
  * @brief  Configure the four on-board debug LEDs as push-pull outputs.
  * @param  None
  * @retval None
  */
static void APP_DebugLedInit(void)
{
  GPIO_InitTypeDef  GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitStruct.Pin = DBG_LED_ALL_PINS;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;            /* Push-pull output */
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DBG_LED_PORT, &GPIO_InitStruct);

  /* Active Low: drive all HIGH to keep the LEDs dark at startup. */
  HAL_GPIO_WritePin(DBG_LED_PORT, DBG_LED_ALL_PINS, GPIO_PIN_SET);
}

/**
  * @brief  Show a 4-bit code on the debug LEDs (active low => bit set = lit).
  * @param  code 0..15, bit0 -> LED1 (PA3) ... bit3 -> LED4 (PA6).
  * @retval None
  */
static void APP_DebugLedShow(uint8_t code)
{
  HAL_GPIO_WritePin(DBG_LED_PORT, DBG_LED1_PIN,
                    ((code & 0x01U) != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
  HAL_GPIO_WritePin(DBG_LED_PORT, DBG_LED2_PIN,
                    ((code & 0x02U) != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
  HAL_GPIO_WritePin(DBG_LED_PORT, DBG_LED3_PIN,
                    ((code & 0x04U) != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
  HAL_GPIO_WritePin(DBG_LED_PORT, DBG_LED4_PIN,
                    ((code & 0x08U) != 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/**
  * @brief  Debug hook called from lcd096.c: latch the stage and show it.
  * @param  stage stage code (see the table above).
  * @retval None
  */
void LCD096_DebugHook(uint8_t stage)
{
  g_dbg_stage = stage;
  APP_DebugLedShow(stage);
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
