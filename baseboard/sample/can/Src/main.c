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
/* Private variables ---------------------------------------------------------*/
FDCAN_HandleTypeDef FdcanHandle = {0};

/* Data counters used by the demo */
__IO uint32_t CanTxCounter = 0;
__IO uint32_t CanRxCounter = 0;

/* Data to be transmitted */
uint8_t CanTxData[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
uint8_t CanRxData[8] = {0};

/* Private user code ---------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
static void APP_SystemClockConfig(void);
static void APP_GpioInit(void);
static void APP_FdcanInit(void);

/**
  * @brief  Main program
  * @retval int
  */
int main(void)
{
  FDCAN_TxHeaderTypeDef TxHeader;

  /* Reset of all peripherals, Initializes the Systick */
  HAL_Init();

  /* System clock configuration: HSI 16 MHz / 2, PLL x20 /2 = 80 MHz SYSCLK */
  APP_SystemClockConfig();

  /* Initialize the board LEDs and the CAN transceiver standby pin */
  APP_GpioInit();

  /* Initialize FDCAN1 and the message RAM layout */
  APP_FdcanInit();

  /* Build the transmit header once, it is reused for every frame */
  TxHeader.Identifier          = CAN_TX_ID_STD;
  TxHeader.IdType              = FDCAN_STANDARD_ID;
  TxHeader.TxFrameType         = FDCAN_DATA_FRAME;
  TxHeader.DataLength          = FDCAN_DLC_BYTES_8;
  TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  TxHeader.BitRateSwitch       = FDCAN_BRS_OFF;
  TxHeader.FDFormat            = FDCAN_CLASSIC_CAN;
  TxHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
  TxHeader.MessageMarker       = 0U;

  while (1)
  {
    /* Update the payload so every frame is distinguishable on the bus */
    CanTxData[0] = (uint8_t)(CanTxCounter & 0xFFU);
    CanTxData[1] = (uint8_t)((CanTxCounter >> 8) & 0xFFU);

    /* Transmit one classic CAN frame -------------------------------------- */
    if (HAL_FDCAN_AddMessageToTxFifoQ(&FdcanHandle, &TxHeader, CanTxData) != HAL_OK)
    {
      APP_ErrorHandler();
    }
    CanTxCounter++;

    /* Toggle LED D1 so the transmit loop is visible without an analyser */
    HAL_GPIO_TogglePin(LED_PORT, LED_D1_PIN);

    /* Wait ~1 s before sending the next frame */
    HAL_Delay(1000);
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
  *         cleanly into a 500 kbit/s CAN bit time.
  * @param  None
  * @retval None
  */
static void APP_SystemClockConfig(void)
{
  RCC_OscInitTypeDef  OscInitstruct = {0};
  RCC_ClkInitTypeDef  ClkInitstruct = {0};

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

  /* Select the CAN kernel clock: PLL / 1 = 80 MHz.
     The APB (control logic) clock must be >= the CAN kernel clock, the
     configuration above keeps both at 80 MHz. */
  {
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_CAN;
    PeriphClkInit.CanClockSelection    = RCC_CANCLKSOURCE_PLL;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      APP_ErrorHandler();
    }
  }
}

/**
  * @brief FDCAN1 Initialization Function
  * @param None
  * @retval None
  */
static void APP_FdcanInit(void)
{
  FDCAN_FilterTypeDef sFilterConfig;

  /* Configure the FDCAN1 instance and its bit timing */
  FdcanHandle.Instance                        = FDCAN1;
  FdcanHandle.Init.FrameFormat                = FDCAN_FRAME_CLASSIC;
  FdcanHandle.Init.Mode                       = FDCAN_MODE_NORMAL;
  FdcanHandle.Init.AutoRetransmission         = ENABLE;
  FdcanHandle.Init.TransmitPause              = DISABLE;
  FdcanHandle.Init.ProtocolException          = DISABLE;
  FdcanHandle.Init.NominalPrescaler           = CAN_PRESCALER;
  FdcanHandle.Init.NominalSyncJumpWidth       = CAN_SYNC_JUMP_WIDTH;
  FdcanHandle.Init.NominalTimeSeg1            = CAN_TIME_SEG1;
  FdcanHandle.Init.NominalTimeSeg2            = CAN_TIME_SEG2;
  FdcanHandle.Init.DataPrescaler              = 0U;
  FdcanHandle.Init.DataSyncJumpWidth          = 0U;
  FdcanHandle.Init.DataTimeSeg1               = 0U;
  FdcanHandle.Init.DataTimeSeg2               = 0U;
  FdcanHandle.Init.MessageRAMOffset           = 0U;
  FdcanHandle.Init.StdFiltersNbr              = 1U;
  FdcanHandle.Init.ExtFiltersNbr              = 0U;
  FdcanHandle.Init.RxFifo0ElmtsNbr            = 1U;
  FdcanHandle.Init.RxFifo0ElmtSize            = FDCAN_DATA_BYTES_8;
  FdcanHandle.Init.RxFifo1ElmtsNbr            = 0U;
  FdcanHandle.Init.RxFifo1ElmtSize            = FDCAN_DATA_BYTES_8;
  FdcanHandle.Init.RxBuffersNbr               = 0U;
  FdcanHandle.Init.RxBufferSize               = FDCAN_DATA_BYTES_8;
  FdcanHandle.Init.TxEventsNbr                = 0U;
  FdcanHandle.Init.TxBuffersNbr               = 0U;
  FdcanHandle.Init.TxFifoQueueElmtsNbr        = 1U;
  FdcanHandle.Init.TxFifoQueueMode            = FDCAN_TX_FIFO_OPERATION;
  FdcanHandle.Init.TxElmtSize                 = FDCAN_DATA_BYTES_8;
  if (HAL_FDCAN_Init(&FdcanHandle) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  /* Accept only the demo RX identifier into Rx FIFO 0 */
  sFilterConfig.IdType       = FDCAN_STANDARD_ID;
  sFilterConfig.FilterIndex  = 0U;
  sFilterConfig.FilterType   = FDCAN_FILTER_MASK;
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilterConfig.FilterID1    = CAN_RX_ID_STD;
  sFilterConfig.FilterID2    = 0x7FFU;   /* Mask: compare all 11 ID bits */
  if (HAL_FDCAN_ConfigFilter(&FdcanHandle, &sFilterConfig) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  /* Drop every non-matching frame and reject remote frames */
  if (HAL_FDCAN_ConfigGlobalFilter(&FdcanHandle, FDCAN_REJECT, FDCAN_REJECT,
                                   FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  /* Route the Rx FIFO 0 "new message" interrupt to interrupt line 0 */
  if (HAL_FDCAN_ConfigInterruptLines(&FdcanHandle,
                                     FDCAN_IT_RX_FIFO0_NEW_MESSAGE,
                                     FDCAN_INTERRUPT_LINE0) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  /* Enable the Rx FIFO 0 new message notification */
  if (HAL_FDCAN_ActivateNotification(&FdcanHandle,
                                     FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0U) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  /* Start the FDCAN peripheral, it leaves initialization mode */
  if (HAL_FDCAN_Start(&FdcanHandle) != HAL_OK)
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
