/**
  ******************************************************************************
  * @file    main.c
  * @author  MCU Application Team
  * @brief   LAN8670 10BASE-T1S (RMII) bring-up demo.
  *
  *          Startup sequence:
  *            1. 80 MHz system clock, ETH clocked from HCLK
  *            2. GPIO for the LEDs and the console UART
  *            3. PHY identification over MDIO (vendor / model / revision)
  *            4. PHY soft reset and optional PLCA configuration
  *            5. MAC/DMA configuration and start, link poll
  *            6. Periodic demo frames, received frames echoed on the console
  *
  *          Console: USART1 (PY-LINK VCP) at 921600 8N1
  *            PA9  -> PC RXD
  *            PA10 <- PC TXD
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
#include <string.h>

#include "main.h"
#include "lan8670.h"

/* Private define ------------------------------------------------------------*/

/*
 * PLCA configuration.
 * Exactly one node on the mixing segment is the coordinator (ID 0) and sends
 * the periodic BEACON. Followers take IDs 1..NodeCount-1.
 */
#define PLCA_NODE_ID                  0U    /* 0 = coordinator, 1..254 = follower */
#define PLCA_NODE_COUNT               8U
#define PLCA_TOTMR                   32U   /* transmit opportunity timer, 0 = default */

/* Demo traffic */
#define DEMO_FRAME_PERIOD_MS         1000U
#define DEMO_FRAME_LEN               20U

/* Private variables ---------------------------------------------------------*/
ETH_HandleTypeDef heth;

static uint32_t DemoTxCounter;

/* Private function prototypes -----------------------------------------------*/
static void APP_SystemClockConfig(void);
static void APP_GpioInit(void);
static void APP_PrintPhyInfo(const LAN8670_Info *Info);
static void APP_SendDemoFrame(void);
static void APP_PrintReceivedFrame(const uint8_t *Frame, uint32_t Length);

/**
  * @brief  Main program
  * @retval int
  */
int main(void)
{
  LAN8670_Info info = {0};
  LAN8670_Status status;
  uint8_t ethReady = 0U;
  uint32_t lastTx = 0U;

  HAL_Init();
  APP_SystemClockConfig();
  APP_GpioInit();
  Console_Init();

  Console_Write("\r\n=== PY32E407 + LAN8670 10BASE-T1S bring-up ===\r\n");

  /* ------------------------------------------------- MAC/DMA (no start) ---*/
  /* This must happen first: it enables the ETH clock, configures the RMII
     pins and programs the MDIO clock range used by every PHY access below. */
  if (LAN8670_EthInit() != HAL_OK)
  {
    Console_Write("MAC: initialisation FAILED (RMII clock missing?)\r\n");
    Console_Write("     check the 50 MHz oscillator and R6/R38/R39/R40\r\n");
    while (1)
    {
      HAL_GPIO_TogglePin(LED_PORT, LED_D1_PIN);
      HAL_Delay(200U);
    }
  }
  Console_Write("MAC: ETH clock, RMII pins and MDIO ready\r\n");

  /* ---------------------------------------------------------------- PHY ---*/
  status = LAN8670_Probe(&info);
  APP_PrintPhyInfo(&info);

  if (status == LAN8670_ERR_MDIO)
  {
    Console_Write("PHY: no answer on MDIO (check PHYAD strap / MDIO pull-up)\r\n");
  }
  else if (status == LAN8670_ERR_ID)
  {
    Console_Write("PHY: answered but the identifier is not a LAN8670\r\n");
  }
  else
  {
    Console_Write("PHY: LAN8670 detected\r\n");
  }

  if (status == LAN8670_OK)
  {
    /* ------------------------------------------------------ PHY reset ---*/
    status = LAN8670_SoftReset();
    Console_Printf("PHY: soft reset %s\r\n",
                   (status == LAN8670_OK) ? "OK" : "FAILED");

    /* ----------------------------------------------------------- PLCA ---*/
    status = LAN8670_ConfigurePLCA(PLCA_NODE_ID, PLCA_NODE_COUNT, PLCA_TOTMR);
    if (status == LAN8670_OK)
    {
      Console_Printf("PHY: PLCA enabled, ID=%u, node count=%u, TOTMR=%u\r\n",
                     (unsigned)PLCA_NODE_ID, (unsigned)PLCA_NODE_COUNT,
                     (unsigned)PLCA_TOTMR);
    }
    else
    {
      Console_Write("PHY: PLCA configuration FAILED\r\n");
    }

    /* ------------------------------------------------------- MAC start ---*/
    if (LAN8670_EthStart() == HAL_OK)
    {
      ethReady = 1U;
      Console_Write("MAC: RMII 10 Mbit/s full duplex, DMA started\r\n");
    }
    else
    {
      Console_Write("MAC: start FAILED\r\n");
    }

    /* ----------------------------------------------------------- link ---*/
    if (ethReady != 0U)
    {
      status = LAN8670_WaitForLink(3000U);
      Console_Printf("PHY: link %s\r\n",
                     (status == LAN8670_OK) ? "UP" : "DOWN (no partner?)");
    }
  }

  Console_Write("Ready. Sending a demo frame every second.\r\n\r\n");

  /* -------------------------------------------------------------- loop ---*/
  while (1)
  {
    uint8_t  frame[ETH_MAX_PACKET_SIZE];
    uint32_t length = 0U;

    /* Frames coming from the segment */
    while (LAN8670_GetFrame(frame, sizeof(frame), &length) == HAL_OK)
    {
      HAL_GPIO_TogglePin(LED_PORT, LED_D2_PIN);
      APP_PrintReceivedFrame(frame, length);
    }

    /* Periodic demo frame so the segment shows traffic */
    if ((HAL_GetTick() - lastTx) >= DEMO_FRAME_PERIOD_MS)
    {
      lastTx = HAL_GetTick();
      HAL_GPIO_TogglePin(LED_PORT, LED_D4_PIN);

      if (ethReady != 0U)
      {
        APP_SendDemoFrame();
      }
    }
  }
}

/**
  * @brief  Build and transmit one demo frame.
  */
static void APP_SendDemoFrame(void)
{
  uint8_t frame[DEMO_FRAME_LEN];
  static const uint8_t src[6] = {0x02U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U};
  static const uint8_t dst[6] = {0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU};
  uint32_t i;

  /* Ethernet header */
  for (i = 0U; i < 6U; i++)
  {
    frame[i]      = dst[i];
    frame[6U + i] = src[i];
  }
  frame[12] = (uint8_t)(DEMO_ETHERTYPE >> 8U);
  frame[13] = (uint8_t)(DEMO_ETHERTYPE & 0xFFU);

  /* Payload: sequence number + a few counters */
  DemoTxCounter++;
  frame[14] = (uint8_t)(DemoTxCounter >> 24U);
  frame[15] = (uint8_t)(DemoTxCounter >> 16U);
  frame[16] = (uint8_t)(DemoTxCounter >> 8U);
  frame[17] = (uint8_t)(DemoTxCounter);
  frame[18] = (uint8_t)(HAL_GetTick() >> 8U);
  frame[19] = (uint8_t)(HAL_GetTick());

  if (LAN8670_SendFrame(frame, DEMO_FRAME_LEN) == HAL_OK)
  {
    HAL_GPIO_TogglePin(LED_PORT, LED_D3_PIN);
    Console_Printf("TX %lu bytes, seq=%lu\r\n",
                    (unsigned long)DEMO_FRAME_LEN, (unsigned long)DemoTxCounter);
  }
  else
  {
    Console_Write("TX failed (descriptor busy)\r\n");
  }
}

/**
  * @brief  Print the interesting fields of a received frame.
  */
static void APP_PrintReceivedFrame(const uint8_t *Frame, uint32_t Length)
{
  uint16_t etherType;
  uint32_t i;
  uint32_t shown;

  if (Length < 14U)
  {
    Console_Printf("RX %lu bytes (runt)\r\n", (unsigned long)Length);
    return;
  }

  etherType = (uint16_t)(((uint16_t)Frame[12] << 8U) | Frame[13]);

  Console_Printf("RX %lu bytes type=0x%04X\r\n", (unsigned long)Length,
                 (unsigned)etherType);
  Console_Write("   ");

  shown = (Length - 14U < 16U) ? (Length - 14U) : 16U;
  for (i = 0U; i < shown; i++)
  {
    Console_Printf("%02X ", Frame[14U + i]);
  }
  Console_Write("\r\n");
}

/**
  * @brief  Print the PHY identification block read over MDIO.
  */
static void APP_PrintPhyInfo(const LAN8670_Info *Info)
{
  Console_Printf("PHY: ID1=0x%04X ID2=0x%04X (OUI 0x%06lX, model %u, rev %u)\r\n",
                 (unsigned)Info->PhyId1, (unsigned)Info->PhyId2,
                 (unsigned long)(((uint32_t)(Info->PhyId1 & 0xFFFFU) << 6U) |
                                 ((uint32_t)(Info->PhyId2 & 0xFC00U) >> 10U)),
                 (unsigned)((Info->PhyId2 >> 4U) & 0x3FU),
                 (unsigned)(Info->PhyId2 & 0x0FU));
  Console_Printf("PHY: BMSR=0x%04X BMCR=0x%04X link=%u\r\n",
                 (unsigned)Info->Bmsr, (unsigned)Info->Bmcr,
                 (unsigned)Info->LinkUp);

  if (Info->PlcaSupported != 0U)
  {
    Console_Printf("PHY: PLCA status register = 0x%04X\r\n",
                   (unsigned)Info->PlcaStatus);
  }
  else
  {
    Console_Write("PHY: PLCA registers not readable\r\n");
  }
}

/**
  * @brief  Configure the board LEDs.
  *         The four LEDs are active low.
  */
static void APP_GpioInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  LED_GPIO_CLK_ENABLE();

  HAL_GPIO_WritePin(LED_PORT, LED_ALL_PINS, GPIO_PIN_SET);
  GPIO_InitStruct.Pin   = LED_ALL_PINS;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);
}

/**
  * @brief  System clock configuration.
  *         HSI(16 MHz)/2 * 20 = 160 MHz VCO, then /2 -> 80 MHz SYSCLK.
  *         The ETH MAC is clocked from HCLK (80 MHz).
  */
static void APP_SystemClockConfig(void)
{
  RCC_OscInitTypeDef OscInitstruct = {0};
  RCC_ClkInitTypeDef ClkInitstruct = {0};

  OscInitstruct.OscillatorType  = RCC_OSCILLATORTYPE_HSI;
  OscInitstruct.HSIState        = RCC_HSI_ON;
  OscInitstruct.HSEState        = RCC_HSE_OFF;
  OscInitstruct.HSI48MState     = RCC_HSI48M_OFF;
  OscInitstruct.LSEState        = RCC_LSE_OFF;
  OscInitstruct.LSIState        = RCC_LSI_OFF;
  OscInitstruct.PLL.PLLState    = RCC_PLL_ON;
  OscInitstruct.PLL.PLLSource   = RCC_PLLSOURCE_HSI_DIV2;
  OscInitstruct.PLL.PLLPrediv   = RCC_PLL_PREDIV_DIV1;
  OscInitstruct.PLL.PLLMUL      = 20;
  OscInitstruct.PLL.PLLPostdiv  = RCC_PLL_POSTDIV_DIV2;
  if (HAL_RCC_OscConfig(&OscInitstruct) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  ClkInitstruct.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                                 RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  ClkInitstruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  ClkInitstruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  ClkInitstruct.APB1CLKDivider = RCC_HCLK_DIV1;
  ClkInitstruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&ClkInitstruct, FLASH_LATENCY_3) != HAL_OK)
  {
    APP_ErrorHandler();
  }
}

/**
  * @brief  This function is executed in case of error occurrence.
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
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  while (1)
  {
  }
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
