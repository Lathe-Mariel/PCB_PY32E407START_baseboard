/**
  ******************************************************************************
  * @file    main.c
  * @author  MCU Application Team
  * @brief   UDP JPEG receiver over LAN8670 10BASE-T1S (RMII).
  *
  *          Startup sequence:
  *            1. 80 MHz system clock, ETH clocked from HCLK
  *            2. GPIO for the LEDs and the console UART
  *            3. PHY identification over MDIO (vendor / model / revision)
  *            4. PHY soft reset and optional PLCA configuration
  *            5. MAC/DMA configuration and start, link poll
  *            6. Receive JPG1 UDP datagrams and show JPEG images on the LCD
  *
  *          Console: USART1 (PY-LINK VCP) at 921600 8N1
  *            PA9  -> PC RXD
  *            PA10 <- PC TXD
  *
  *          Network addresses are configured in Inc/main.h
  *          (PING_LOCAL_IP is also the receiver's IPv4 address).
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
#include "lcd_spi.h"
#include "ping.h"

/* Private define ------------------------------------------------------------*/

/*
 * PLCA configuration.
 * Exactly one node on the mixing segment is the coordinator (ID 0) and sends
 * the periodic BEACON. Followers take IDs 1..NodeCount-1.
 *
 * The follower ID must be smaller than the NCNT of the coordinator (the number
 * of transmit opportunities per bus cycle); a follower with an out-of-range ID
 * never gets a transmit opportunity and cannot send anything.  The NCNT value
 * programmed here is only used when this node is the coordinator (ID 0) -
 * followers ignore it and take the cycle length from the BEACON.
 * Set PLCA_NODE_ID to 0xFF to disable PLCA and use plain CSMA/CD.
 */
#define PLCA_NODE_ID                  1U     /* 0 = coordinator, 1..254 = follower, 0xFF = CSMA/CD */
#define PLCA_NODE_COUNT               2U     /* coordinator only: NCNT (IDs 0..7) */
#define PLCA_TOTMR                   32U    /* transmit opportunity timer, 0 = default */

/* Set to 0 to remove the 5 s MAC DMA/MMC diagnostic line.  TXok counts good
   frames actually transmitted on the RMII interface: if it stays 0 while ARP
   requests are being queued, the MAC is not transmitting (typically a missing
   50 MHz ETH_REF_CLK on PA1). */
#define ETH_DIAG_PRINT                1

/* Set to 0 to remove the per-frame receive trace (length, EtherType, source
   MAC and, for ICMP, the type and sequence number). */
#define ETH_RX_DEBUG                  0

/* Diagnostic: enable the PHY PCS loopback so the MAC receives its own
   transmitted frames.  Set back to 0 for normal operation. */
#define ETH_PCS_LOOPBACK_TEST         0

#define ORIENTATION_BUTTON_DEBOUNCE_MS 30U

/* Private variables ---------------------------------------------------------*/
ETH_HandleTypeDef heth;

/* Private function prototypes -----------------------------------------------*/
static void APP_SystemClockConfig(void);
static void APP_GpioInit(void);
static void APP_PollOrientationButton(void);
static void APP_PrintPhyInfo(const LAN8670_Info *Info);
static void APP_PrintIp(uint32_t Ip);
#if ETH_RX_DEBUG
static void APP_PrintRxFrame(const uint8_t *Frame, uint32_t Length);
#endif
static void APP_MdioConsistencyTest(uint32_t Iterations, uint8_t AlwaysPrint);

/**
  * @brief  Main program
  * @retval int
  */
int main(void)
{
  LAN8670_Info   info = {0};
  LAN8670_Status status;
  Ping_Config    pingCfg;
  uint8_t        ethReady = 0U;
  uint32_t       lastHeartbeat;
  uint32_t       lastDiag = 0U;
  uint32_t       rxFrames = 0U;
  uint8_t        frameBuffer[ETH_MAX_PACKET_SIZE];
  uint32_t       frameLength = 0U;

  HAL_Init();
  APP_SystemClockConfig();
  APP_GpioInit();
  Console_Init();

  Console_Write("\r\n=== PY32E407 + LAN8670 UDP JPEG display ===\r\n");
  LCD_SPI_Init();
  LCD_SPI_Clear();

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
      if (PLCA_NODE_ID == LAN8670_PLCA_ID_DISABLED)
      {
        Console_Write("PHY: PLCA disabled, CSMA/CD\r\n");
      }
      else
      {
        Console_Printf("PHY: PLCA enabled, ID=%u, node count=%u, TOTMR=%u\r\n",
                       (unsigned)PLCA_NODE_ID, (unsigned)PLCA_NODE_COUNT,
                       (unsigned)PLCA_TOTMR);
      }
    }
    else
    {
      Console_Write("PHY: PLCA configuration FAILED\r\n");
    }
  }

  /* ------------------------------------------------------- MAC start ---*/
  if (LAN8670_EthStart() == HAL_OK)
  {
    ethReady = 1U;
    Console_Write("MAC: RMII 10 Mbit/s half duplex, DMA started\r\n");

#if ETH_DIAG_PRINT
    Console_Printf("MAC: MACCR=0x%08lX MACFFR=0x%08lX DMAOMR=0x%08lX DMABMR=0x%08lX\r\n",
                   (unsigned long)heth.Instance->MACCR,
                   (unsigned long)heth.Instance->MACFFR,
                   (unsigned long)heth.Instance->DMAOMR,
                   (unsigned long)heth.Instance->DMABMR);
#endif
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

    /* PLCA_STS.PST = 1 while BEACONs are being sent or received.  A follower
       with PST = 0 has no coordinator (it falls back to CSMA/CD).  Give the
       PHY a moment to synchronise before judging. */
    if (PLCA_NODE_ID != LAN8670_PLCA_ID_DISABLED)
    {
      uint16_t plcaSts = 0U;
      uint32_t plcaStart = HAL_GetTick();

      do
      {
        if (LAN8670_ReadMMD(LAN8670_DEVAD_VENDOR2, LAN8670_PLCA_STS,
                            &plcaSts) != HAL_OK)
        {
          plcaSts = 0U;
          break;
        }
        if ((plcaSts & LAN8670_PLCA_STS_PST) != 0U)
        {
          break;
        }
        HAL_Delay(10U);
      } while ((HAL_GetTick() - plcaStart) < 1000U);

      Console_Printf("PHY: PLCA %s (PLCA_STS=0x%04X)\r\n",
                     ((plcaSts & LAN8670_PLCA_STS_PST) != 0U)
                         ? "active (BEACON seen)" : "no BEACON (CSMA/CD fallback)",
                     (unsigned)plcaSts);
    }

    /* Link quality (revision C2 SQI): 0 = ~5 dB SNR, 7 = ~18 dB SNR */
    if (LAN8670_SqiStart() == LAN8670_OK)
    {
      uint16_t sqiCtl = 0U;
      uint16_t sqiCfg0 = 0U;
      uint16_t sqiCfg2 = 0U;
      uint16_t sqiSts = 0U;

      (void)LAN8670_ReadMMD(LAN8670_DEVAD_VENDOR2, LAN8670_SQICTL, &sqiCtl);
      (void)LAN8670_ReadMMD(LAN8670_DEVAD_VENDOR2, LAN8670_SQICFG0, &sqiCfg0);
      (void)LAN8670_ReadMMD(LAN8670_DEVAD_VENDOR2, LAN8670_SQICFG2, &sqiCfg2);
      (void)LAN8670_ReadMMD(LAN8670_DEVAD_VENDOR2, LAN8670_SQISTS0, &sqiSts);

      Console_Printf("PHY: SQI measurement started (CTL=0x%04X CFG0=0x%04X "
                     "CFG2=0x%04X STS=0x%04X)\r\n",
                     (unsigned)sqiCtl, (unsigned)sqiCfg0,
                     (unsigned)sqiCfg2, (unsigned)sqiSts);
    }
  }

#if ETH_PCS_LOOPBACK_TEST
  if (LAN8670_SetPcsLoopback(1U) == LAN8670_OK)
  {
    Console_Write("PHY: PCS LOOPBACK ENABLED - medium bypassed\r\n");
  }
#endif

  /* Baseline MDIO stability check (also repeated in the 5 s diagnostics) */
  APP_MdioConsistencyTest(1000U, 1U);

  /* Keep the ARP responder configured so the PC can resolve this MAC. */
  pingCfg.LocalMac     = LAN8670_GetMacAddress();
  pingCfg.LocalIp      = PING_LOCAL_IP;
  pingCfg.NetMask      = PING_NETMASK;
  pingCfg.GatewayIp    = PING_GATEWAY_IP;
  pingCfg.TargetIp     = PING_TARGET_IP;
  pingCfg.IntervalMs   = PING_INTERVAL_MS;
  pingCfg.TimeoutMs    = PING_TIMEOUT_MS;
  pingCfg.ArpTimeoutMs = PING_ARP_TIMEOUT_MS;
  pingCfg.ArpRetries   = PING_ARP_RETRIES;
  Ping_Init(&pingCfg);
  JpegReceiver_Init(JPEG_RX_LOCAL_IP);

  Console_Write("UDP: receive JPEG at ");
  APP_PrintIp(JPEG_RX_LOCAL_IP);
  Console_Printf(":%u (max %lu bytes)\r\n\r\n",
                 (unsigned)JPEG_UDP_PORT,
                 (unsigned long)JPEG_RX_MAX_BYTES);

  lastHeartbeat = HAL_GetTick();

  /* -------------------------------------------------------------- loop ---*/
  while (1)
  {
    APP_PollOrientationButton();

    /* Frames coming from the segment */
    while (LAN8670_GetFrame(frameBuffer, sizeof(frameBuffer), &frameLength) == HAL_OK)
    {
      APP_PollOrientationButton();
      HAL_GPIO_TogglePin(LED_PORT, LED_D2_PIN);
#if ETH_RX_DEBUG
      APP_PrintRxFrame(frameBuffer, frameLength);
#endif
      Ping_HandleFrame(frameBuffer, frameLength);
      JpegReceiver_HandleFrame(frameBuffer, frameLength);
      rxFrames++;
    }

    JpegReceiver_Poll();

    /* 1 Hz heartbeat on D4 so a hung loop is obvious */
    if ((HAL_GetTick() - lastHeartbeat) >= 1000U)
    {
      lastHeartbeat = HAL_GetTick();
      HAL_GPIO_TogglePin(LED_PORT, LED_D4_PIN);

#if ETH_DIAG_PRINT
      /* Every 5 s: DMA status and the MAC MMC counters (read to clear). */
      if ((HAL_GetTick() - lastDiag) >= 5000U)
      {
        uint32_t dmasr  = heth.Instance->DMASR;
        uint32_t txGood = heth.Instance->MMCTGFCR;
        uint32_t rxGood = heth.Instance->MMCRGUFCR;
        uint32_t rxCrc  = heth.Instance->MMCRFCECR;
        uint16_t sts1   = 0U;
        uint16_t prssts = 0U;

        (void)LAN8670_ReadReg(LAN8670_REG_STS1, &sts1);
        (void)LAN8670_ReadMMD(LAN8670_DEVAD_VENDOR2, LAN8670_PRSSTS, &prssts);

        lastDiag = HAL_GetTick();
        Console_Printf("MAC: DMASR=0x%08lX TXok=%lu RXuni=%lu RXcrc=%lu RXall=%lu "
                       "STS1=0x%04X MAXID=%u%s%s%s%s\r\n",
                       (unsigned long)dmasr, (unsigned long)txGood,
                       (unsigned long)rxGood, (unsigned long)rxCrc,
                       (unsigned long)rxFrames, (unsigned)sts1,
                       (unsigned)LAN8670_PRSSTS_MAXID(prssts),
                       ((sts1 & LAN8670_STS1_PLCASYM) != 0U) ? " PLCASYM" : "",
                       ((sts1 & LAN8670_STS1_TXCOL)   != 0U) ? " TXCOL"   : "",
                       ((sts1 & LAN8670_STS1_ESDERR)  != 0U) ? " ESDERR"  : "",
                       ((sts1 & LAN8670_STS1_DEC5B)   != 0U) ? " DEC5B"   : "");

        {
          uint16_t sqiRaw = 0U;

          if (LAN8670_ReadMMD(LAN8670_DEVAD_VENDOR2, LAN8670_SQISTS0,
                              &sqiRaw) == HAL_OK)
          {
            if ((sqiRaw & LAN8670_SQISTS0_VLD) != 0U)
            {
              Console_Printf("PHY: SQI=%u/7 (0=worst..7=best) STS=0x%04X\r\n",
                             (unsigned)LAN8670_SQISTS0_VAL(sqiRaw),
                             (unsigned)sqiRaw);
            }
            else
            {
              Console_Printf("PHY: SQI pending (STS=0x%04X err=%u)\r\n",
                             (unsigned)sqiRaw,
                             (unsigned)(sqiRaw & 0x07U));
            }

            if (((sqiRaw & LAN8670_SQISTS0_ERR) != 0U) &&
                ((sqiRaw & LAN8670_SQISTS0_VLD) == 0U))
            {
              (void)LAN8670_SqiStart();
            }
          }
        }

        /* Short MDIO consistency burst; prints only when something is wrong */
        APP_MdioConsistencyTest(100U, 0U);
      }
#endif
    }
  }
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
  * @brief  Print an IPv4 address in dotted-decimal form.
  */
static void APP_PrintIp(uint32_t Ip)
{
  Console_Printf("%u.%u.%u.%u",
                 (unsigned)((Ip >> 24U) & 0xFFU),
                 (unsigned)((Ip >> 16U) & 0xFFU),
                 (unsigned)((Ip >> 8U) & 0xFFU),
                 (unsigned)(Ip & 0xFFU));
}

/**
  * @brief  Print a one-line summary of a received frame (debug aid).
  */
#if ETH_RX_DEBUG
static void APP_PrintRxFrame(const uint8_t *Frame, uint32_t Length)
{
  uint16_t type;

  if (Length < 14U)
  {
    return;
  }

  type = (uint16_t)(((uint16_t)Frame[12] << 8U) | (uint16_t)Frame[13]);

  Console_Printf("RX t=%lums len=%lu type=0x%04X src=%02X:%02X:%02X:%02X:%02X:%02X",
                 (unsigned long)HAL_GetTick(), (unsigned long)Length,
                 (unsigned)type,
                 Frame[6], Frame[7], Frame[8], Frame[9], Frame[10], Frame[11]);

  if ((type == 0x0800U) && (Length >= (14U + 20U + 8U)) && (Frame[14U + 9U] == 1U))
  {
    Console_Printf(" icmp type=%u seq=%u",
                   (unsigned)Frame[14U + 20U],
                   (unsigned)((uint16_t)((uint16_t)Frame[14U + 20U + 6U] << 8U) |
                              (uint16_t)Frame[14U + 20U + 7U]));
  }
  Console_Write("\r\n");
}
#endif

/**
  * @brief  Read stable PHY registers repeatedly and count read errors and
  *         value changes.  A marginal MDIO connection, reference clock or
  *         ground (e.g. a poorly soldered QFN exposed pad) shows up here.
  */
static void APP_MdioConsistencyTest(uint32_t Iterations, uint8_t AlwaysPrint)
{
  uint16_t id1Ref = 0U;
  uint16_t id2Ref = 0U;
  uint16_t pcsRef = 0U;
  uint16_t firstReg = 0U;
  uint16_t firstGot = 0U;
  uint16_t firstExp = 0U;
  uint32_t errors = 0U;
  uint32_t mismatches = 0U;
  uint32_t i;
  uint16_t v;

  if ((LAN8670_ReadReg(LAN8670_REG_PHYID1, &id1Ref) != HAL_OK) ||
      (LAN8670_ReadReg(LAN8670_REG_PHYID2, &id2Ref) != HAL_OK) ||
      (LAN8670_ReadMMD(LAN8670_DEVAD_PCS, LAN8670_T1SPCSCTL, &pcsRef) != HAL_OK))
  {
    Console_Write("MDIO: consistency test setup FAILED\r\n");
    return;
  }

  for (i = 0U; i < Iterations; i++)
  {
    if (LAN8670_ReadReg(LAN8670_REG_PHYID1, &v) != HAL_OK)
    {
      errors++;
    }
    else if (v != id1Ref)
    {
      mismatches++;
      if (firstReg == 0U)
      {
        firstReg = LAN8670_REG_PHYID1; firstGot = v; firstExp = id1Ref;
      }
    }

    if (LAN8670_ReadReg(LAN8670_REG_PHYID2, &v) != HAL_OK)
    {
      errors++;
    }
    else if (v != id2Ref)
    {
      mismatches++;
      if (firstReg == 0U)
      {
        firstReg = LAN8670_REG_PHYID2; firstGot = v; firstExp = id2Ref;
      }
    }

    if (LAN8670_ReadMMD(LAN8670_DEVAD_PCS, LAN8670_T1SPCSCTL, &v) != HAL_OK)
    {
      errors++;
    }
    else if (v != pcsRef)
    {
      mismatches++;
      if (firstReg == 0U)
      {
        firstReg = LAN8670_T1SPCSCTL; firstGot = v; firstExp = pcsRef;
      }
    }
  }

  if ((AlwaysPrint != 0U) || (errors != 0U) || (mismatches != 0U))
  {
    Console_Printf("MDIO: %lu x3 reads, errors=%lu mismatches=%lu",
                   (unsigned long)Iterations, (unsigned long)errors,
                   (unsigned long)mismatches);
    if (mismatches != 0U)
    {
      Console_Printf(" (first reg=0x%02X got=0x%04X exp=0x%04X)",
                     (unsigned)firstReg, (unsigned)firstGot, (unsigned)firstExp);
    }
    Console_Write("\r\n");
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

  /* Disable the LAN8720: PC8 is its active-low nRST, so drive it low and keep
     it there.  The output latch is cleared before the pin becomes an output so
     the reset is asserted without a glitch. */
  LAN8720_NRST_GPIO_CLK_ENABLE();
  HAL_GPIO_WritePin(LAN8720_NRST_PORT, LAN8720_NRST_PIN, GPIO_PIN_RESET);
  GPIO_InitStruct.Pin   = LAN8720_NRST_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LAN8720_NRST_PORT, &GPIO_InitStruct);

  USER_BUTTON_GPIO_CLK_ENABLE();
  GPIO_InitStruct.Pin = USER_BUTTON_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(USER_BUTTON_GPIO_PORT, &GPIO_InitStruct);
}

static void APP_PollOrientationButton(void)
{
  static GPIO_PinState previousRawState = GPIO_PIN_RESET;
  static GPIO_PinState stableState = GPIO_PIN_RESET;
  static uint32_t rawStateChangedAt;
  static uint8_t initialized;
  const uint32_t now = HAL_GetTick();
  const GPIO_PinState rawState =
      HAL_GPIO_ReadPin(USER_BUTTON_GPIO_PORT, USER_BUTTON_PIN);

  if (initialized == 0U)
  {
    previousRawState = rawState;
    stableState = rawState;
    rawStateChangedAt = now;
    initialized = 1U;
    return;
  }

  if (rawState != previousRawState)
  {
    previousRawState = rawState;
    rawStateChangedAt = now;
  }
  else if ((rawState != stableState) &&
           ((now - rawStateChangedAt) >= ORIENTATION_BUTTON_DEBOUNCE_MS))
  {
    stableState = rawState;
    if (stableState == GPIO_PIN_SET)
    {
      JpegReceiver_CycleOrientation();
    }
  }
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
