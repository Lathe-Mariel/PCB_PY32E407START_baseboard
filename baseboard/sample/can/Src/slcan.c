/**
  ******************************************************************************
  * @file    slcan.c
  * @brief   SLCAN (LAWICEL) protocol bridge between USART1 and FDCAN1.
  *
  *          The PC drives the on-board ATA6561 CAN transceiver through a plain
  *          ASCII protocol:
  *
  *            t1232AABB\r          standard data frame, ID 0x123, 2 bytes
  *            T123456788AABBCC\r   extended data frame
  *            r1234\r              standard remote frame, DLC 4
  *            O / C                open / close the CAN channel
  *            L                    open in listen only mode
  *            S0..S9               select bit rate (10 kbit/s .. 1 Mbit/s)
  *            sXXYY                select bit rate through SJA1000 BTR0/BTR1
  *            F                    read status flags
  *            V / v / N            hardware+software version / serial number
  *            Z0 / Z1              timestamp off / on
  *
  *          Received CAN frames are pushed to the PC as "t...", "T...",
  *          "r..." or "R..." lines.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "slcan.h"

/* Private define ------------------------------------------------------------*/
#define SLCAN_TX_RING_SIZE          1024U
#define SLCAN_RX_RING_SIZE          256U
#define SLCAN_FRAME_QUEUE_SIZE      16U
#define SLCAN_LINE_SIZE             40U

#define SLCAN_CR                    0x0DU
#define SLCAN_BEL                   0x07U

#define SLCAN_HW_VERSION            "1013"
#define SLCAN_SW_VERSION            "1300"
#define SLCAN_SERIAL_NUMBER         "PY407"

#define SLCAN_CAN_CLOCK_HZ          80000000U
#define SLCAN_RX_FIFO_ELMTS         3U
#define SLCAN_TX_FIFO_ELMTS         3U

#define SLCAN_TIMESTAMP_MODULO      10000U

#define SLCAN_TQ_MIN                8U
#define SLCAN_TQ_MAX                128U

/* Private types -------------------------------------------------------------*/
typedef struct
{
  uint32_t Id;
  uint32_t IdType;
  uint32_t FrameType;
  uint8_t  Dlc;
  uint8_t  Data[8];
} SLCAN_FrameTypeDef;

/* Private variables ---------------------------------------------------------*/
SCI_HandleTypeDef hSlcanUart;

static uint8_t SlcanTxRing[SLCAN_TX_RING_SIZE];
static volatile uint32_t SlcanTxHead;
static volatile uint32_t SlcanTxTail;
static uint32_t SlcanTxChunk;
static volatile uint8_t SlcanTxBusy;

static uint8_t SlcanRxRing[SLCAN_RX_RING_SIZE];
static volatile uint32_t SlcanRxHead;
static volatile uint32_t SlcanRxTail;
static uint8_t SlcanRxByte;

static SLCAN_FrameTypeDef SlcanFrameQueue[SLCAN_FRAME_QUEUE_SIZE];
static volatile uint32_t SlcanFrameHead;
static volatile uint32_t SlcanFrameTail;

static char SlcanLine[SLCAN_LINE_SIZE];
static uint32_t SlcanLineLen;
static uint8_t SlcanLineOverflow;
static uint8_t SlcanTimestamp;

static uint32_t SlcanBitrate = 500000U;
static uint32_t SlcanMode = FDCAN_MODE_NORMAL;
static volatile uint8_t SlcanIsOpen;

static uint32_t SlcanTimingPre = 10U;
static uint32_t SlcanTimingSeg1 = 13U;
static uint32_t SlcanTimingSeg2 = 2U;
static uint32_t SlcanTimingSjw = 2U;

/* Private function prototypes -----------------------------------------------*/
static HAL_StatusTypeDef SLCAN_CanApply(uint8_t Start);
static HAL_StatusTypeDef SLCAN_SetBitrate(uint32_t Bitrate);
static void SLCAN_Queue(const char *Data, uint32_t Len);
static void SLCAN_QueueChar(char Ch);
static void SLCAN_TxKick(void);
static void SLCAN_HandleLine(const char *Line, uint32_t Len);
static void SLCAN_QueueFrame(const FDCAN_RxHeaderTypeDef *Header, const uint8_t *Data);
static void SLCAN_SendFrame(const SLCAN_FrameTypeDef *Frame);
static void SLCAN_Hex(char *Out, uint32_t Value, uint32_t Digits);
static int32_t SLCAN_ParseHex(char Ch);
static int32_t SLCAN_ParseHexValue(const char *Str, uint32_t Digits, uint32_t *Value);

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialize the SLCAN bridge: USART1 for the PC link and FDCAN1.
  *         The CAN channel stays closed until the host sends 'O' or 'L'.
  * @param  None
  * @retval None
  */
void SLCAN_Init(void)
{
  if (SLCAN_SetBitrate(SlcanBitrate) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  hSlcanUart.Instance          = SLCAN_UART;
  hSlcanUart.Init.BaudRate     = SLCAN_UART_BAUDRATE;
  hSlcanUart.Init.WordLength   = SCI_WORDLENGTH_8B;
  hSlcanUart.Init.StopBits     = SCI_STOPBITS_1;
  hSlcanUart.Init.Parity       = SCI_PARITY_NONE;
  hSlcanUart.Init.Mode         = SCI_MODE_TX_RX;
  hSlcanUart.Init.HwFlowCtl    = SCI_HWCONTROL_NONE;
  hSlcanUart.Init.OverSampling = SCI_OVERSAMPLING_16;
  hSlcanUart.AdvancedInit.AdvFeatureInit = SCI_ADVFEATURE_NO_INIT;
  hSlcanUart.FifoMode          = SCI_FIFOMODE_DISABLE;

  if (HAL_SCI_Init(&hSlcanUart) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  (void)HAL_SCI_Receive_IT(&hSlcanUart, &SlcanRxByte, 1U);

  if (SLCAN_CanApply(0U) != HAL_OK)
  {
    APP_ErrorHandler();
  }
}

/**
  * @brief  Main loop hook: parse pending UART commands and forward CAN frames.
  * @param  None
  * @retval None
  */
void SLCAN_Poll(void)
{
  uint8_t byte;

  /* Command characters coming from the PC */
  for (;;)
  {
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    if (SlcanRxTail == SlcanRxHead)
    {
      __set_PRIMASK(primask);
      break;
    }
    byte = SlcanRxRing[SlcanRxTail];
    SlcanRxTail = (SlcanRxTail + 1U) % SLCAN_RX_RING_SIZE;
    __set_PRIMASK(primask);

    if ((byte == SLCAN_CR) || (byte == '\n'))
    {
      if (SlcanLineOverflow != 0U)
      {
        SlcanLineOverflow = 0U;
        SlcanLineLen = 0U;
        SLCAN_QueueChar((char)SLCAN_BEL);
      }
      else if (SlcanLineLen > 0U)
      {
        SLCAN_HandleLine(SlcanLine, SlcanLineLen);
        SlcanLineLen = 0U;
      }
      else
      {
        /* Ignore CR+LF pairs and stray terminators */
      }
    }
    else if (SlcanLineLen < SLCAN_LINE_SIZE)
    {
      SlcanLine[SlcanLineLen] = (char)byte;
      SlcanLineLen++;
    }
    else
    {
      SlcanLineOverflow = 1U;
    }
  }

  /* Frames received from the CAN bus */
  while (SlcanFrameTail != SlcanFrameHead)
  {
    SLCAN_FrameTypeDef frame;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    frame = SlcanFrameQueue[SlcanFrameTail];
    SlcanFrameTail = (SlcanFrameTail + 1U) % SLCAN_FRAME_QUEUE_SIZE;
    __set_PRIMASK(primask);

    SLCAN_SendFrame(&frame);
  }

  SLCAN_TxKick();
}

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Compute the FDCAN bit timing for a bit rate.
  *         The prescaler and the number of tq per bit are chosen so the
  *         resulting bit rate stays within 0.5% of the requested one, with
  *         a sample point close to 87.5%.
  * @retval HAL_OK if the bit rate can be generated.
  */
static HAL_StatusTypeDef SLCAN_SetBitrate(uint32_t Bitrate)
{
  uint32_t bestTq = 0U;
  uint32_t bestPre = 0U;
  uint32_t bestActual = 0U;
  uint32_t bestErr = 0xFFFFFFFFU;
  uint32_t bestDelta = 0xFFFFFFFFU;
  uint32_t tq;

  if ((Bitrate < 1000U) || (Bitrate > 2000000U))
  {
    return HAL_ERROR;
  }

  for (tq = SLCAN_TQ_MIN; tq <= SLCAN_TQ_MAX; tq++)
  {
    uint32_t pre = (SLCAN_CAN_CLOCK_HZ + (Bitrate * tq) / 2U) / (Bitrate * tq);
    uint32_t actual;
    uint32_t err;
    uint32_t delta;

    if ((pre < 1U) || (pre > 512U))
    {
      continue;
    }

    actual = SLCAN_CAN_CLOCK_HZ / (pre * tq);
    err = (actual > Bitrate) ? (actual - Bitrate) : (Bitrate - actual);

    /* Prefer an exact match, then the tq count closest to 16 */
    delta = (tq > 16U) ? (tq - 16U) : (16U - tq);
    if ((err < bestErr) || ((err == bestErr) && (delta < bestDelta)))
    {
      bestErr = err;
      bestDelta = delta;
      bestTq = tq;
      bestPre = pre;
      bestActual = actual;
    }

    /* An exact bit rate built from 16 tq per bit cannot be improved */
    if ((bestErr == 0U) && (bestDelta == 0U))
    {
      break;
    }
  }

  /* Reject bit rates that cannot be reached within 0.5% */
  if ((bestTq == 0U) || ((bestErr * 200U) > Bitrate))
  {
    return HAL_ERROR;
  }

  /* Keep the sample point near 87.5% of the bit time */
  SlcanTimingSeg2 = (bestTq >= 12U) ? (bestTq / 8U) : 2U;
  SlcanTimingSeg1 = bestTq - 1U - SlcanTimingSeg2;
  SlcanTimingSjw  = (SlcanTimingSeg2 > 4U) ? 4U : SlcanTimingSeg2;
  SlcanTimingPre  = bestPre;
  SlcanBitrate    = bestActual;

  return HAL_OK;
}

/**
  * @brief  (Re)configure FDCAN1 with the current bit timing and mode.
  * @param  Start 0 = stay closed (configuration mode), 1 = start the channel
  * @retval HAL status
  */
static HAL_StatusTypeDef SLCAN_CanApply(uint8_t Start)
{
  FDCAN_FilterTypeDef filter;
  HAL_StatusTypeDef status = HAL_OK;

  /* Keep the CAN interrupt out while the peripheral is reconfigured */
  HAL_NVIC_DisableIRQ(CAN1_INT0_IRQn);

  if (SlcanIsOpen != 0U)
  {
    (void)HAL_FDCAN_Stop(&FdcanHandle);
    SlcanIsOpen = 0U;
  }

  FdcanHandle.Instance                  = FDCAN1;
  FdcanHandle.Init.FrameFormat          = FDCAN_FRAME_CLASSIC;
  FdcanHandle.Init.Mode                 = SlcanMode;
  FdcanHandle.Init.AutoRetransmission   = ENABLE;
  FdcanHandle.Init.TransmitPause        = DISABLE;
  FdcanHandle.Init.ProtocolException    = DISABLE;
  FdcanHandle.Init.NominalPrescaler     = SlcanTimingPre;
  FdcanHandle.Init.NominalSyncJumpWidth = SlcanTimingSjw;
  FdcanHandle.Init.NominalTimeSeg1      = SlcanTimingSeg1;
  FdcanHandle.Init.NominalTimeSeg2      = SlcanTimingSeg2;
  FdcanHandle.Init.DataPrescaler        = 0U;
  FdcanHandle.Init.DataSyncJumpWidth    = 0U;
  FdcanHandle.Init.DataTimeSeg1         = 0U;
  FdcanHandle.Init.DataTimeSeg2         = 0U;
  FdcanHandle.Init.MessageRAMOffset     = 0U;
  FdcanHandle.Init.StdFiltersNbr        = 1U;
  FdcanHandle.Init.ExtFiltersNbr        = 0U;
  FdcanHandle.Init.RxFifo0ElmtsNbr      = SLCAN_RX_FIFO_ELMTS;
  FdcanHandle.Init.RxFifo0ElmtSize      = FDCAN_DATA_BYTES_8;
  FdcanHandle.Init.RxFifo1ElmtsNbr      = 0U;
  FdcanHandle.Init.RxFifo1ElmtSize      = FDCAN_DATA_BYTES_8;
  FdcanHandle.Init.RxBuffersNbr         = 0U;
  FdcanHandle.Init.RxBufferSize         = FDCAN_DATA_BYTES_8;
  FdcanHandle.Init.TxEventsNbr          = 0U;
  FdcanHandle.Init.TxBuffersNbr         = 0U;
  FdcanHandle.Init.TxFifoQueueElmtsNbr  = SLCAN_TX_FIFO_ELMTS;
  FdcanHandle.Init.TxFifoQueueMode      = FDCAN_TX_FIFO_OPERATION;
  FdcanHandle.Init.TxElmtSize           = FDCAN_DATA_BYTES_8;

  do
  {
    if (HAL_FDCAN_Init(&FdcanHandle) != HAL_OK)
    {
      status = HAL_ERROR;
      break;
    }

    /* One pass-all standard filter */
    filter.IdType       = FDCAN_STANDARD_ID;
    filter.FilterIndex  = 0U;
    filter.FilterType   = FDCAN_FILTER_MASK;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1    = 0U;
    filter.FilterID2    = 0U;
    if (HAL_FDCAN_ConfigFilter(&FdcanHandle, &filter) != HAL_OK)
    {
      status = HAL_ERROR;
      break;
    }

    /* Accept everything, remote frames included, so they can be forwarded */
    if (HAL_FDCAN_ConfigGlobalFilter(&FdcanHandle,
                                     FDCAN_ACCEPT_IN_RX_FIFO0,
                                     FDCAN_ACCEPT_IN_RX_FIFO0,
                                     FDCAN_FILTER_REMOTE,
                                     FDCAN_FILTER_REMOTE) != HAL_OK)
    {
      status = HAL_ERROR;
      break;
    }

    /* Keep the newest frames when the small Rx FIFO overflows */
    if (HAL_FDCAN_ConfigRxFifoOverwrite(&FdcanHandle, FDCAN_RX_FIFO0,
                                        FDCAN_RX_FIFO_OVERWRITE) != HAL_OK)
    {
      status = HAL_ERROR;
      break;
    }

    if (HAL_FDCAN_ConfigInterruptLines(&FdcanHandle, FDCAN_IT_RX_FIFO0_NEW_MESSAGE,
                                       FDCAN_INTERRUPT_LINE0) != HAL_OK)
    {
      status = HAL_ERROR;
      break;
    }

    if (HAL_FDCAN_ActivateNotification(&FdcanHandle, FDCAN_IT_RX_FIFO0_NEW_MESSAGE,
                                       0U) != HAL_OK)
    {
      status = HAL_ERROR;
      break;
    }

    if (Start != 0U)
    {
      if (HAL_FDCAN_Start(&FdcanHandle) != HAL_OK)
      {
        status = HAL_ERROR;
        break;
      }
      SlcanIsOpen = 1U;
    }
  } while (0);

  HAL_NVIC_EnableIRQ(CAN1_INT0_IRQn);

  return status;
}

/**
  * @brief  Append raw bytes to the transmit ring buffer.
  */
static void SLCAN_Queue(const char *Data, uint32_t Len)
{
  uint32_t primask = __get_PRIMASK();
  uint32_t i;

  __disable_irq();
  for (i = 0U; i < Len; i++)
  {
    uint32_t next = (SlcanTxHead + 1U) % SLCAN_TX_RING_SIZE;
    if (next == SlcanTxTail)
    {
      /* Ring full: drop the remaining bytes */
      break;
    }
    SlcanTxRing[SlcanTxHead] = (uint8_t)Data[i];
    SlcanTxHead = next;
  }
  __set_PRIMASK(primask);

  SLCAN_TxKick();
}

static void SLCAN_QueueChar(char Ch)
{
  SLCAN_Queue(&Ch, 1U);
}

/**
  * @brief  Start the next UART transmission if the port is idle.
  */
static void SLCAN_TxKick(void)
{
  uint32_t primask = __get_PRIMASK();
  uint32_t chunk;

  __disable_irq();

  if ((SlcanTxBusy != 0U) || (SlcanTxHead == SlcanTxTail))
  {
    __set_PRIMASK(primask);
    return;
  }

  chunk = (SlcanTxHead > SlcanTxTail) ? (SlcanTxHead - SlcanTxTail)
                                      : (SLCAN_TX_RING_SIZE - SlcanTxTail);
  SlcanTxChunk = chunk;

  if (HAL_SCI_Transmit_IT(&hSlcanUart, &SlcanTxRing[SlcanTxTail], chunk) != HAL_OK)
  {
    SlcanTxChunk = 0U;
  }
  else
  {
    SlcanTxBusy = 1U;
  }

  __set_PRIMASK(primask);
}

/**
  * @brief  Write an unsigned value as zero padded, upper case hexadecimal.
  */
static void SLCAN_Hex(char *Out, uint32_t Value, uint32_t Digits)
{
  static const char hex[] = "0123456789ABCDEF";
  uint32_t i;

  for (i = 0U; i < Digits; i++)
  {
    Out[Digits - 1U - i] = hex[Value & 0x0FU];
    Value >>= 4U;
  }
}

/**
  * @brief  Convert a hexadecimal character, returns -1 when invalid.
  */
static int32_t SLCAN_ParseHex(char Ch)
{
  if ((Ch >= '0') && (Ch <= '9'))
  {
    return (int32_t)(Ch - '0');
  }
  if ((Ch >= 'A') && (Ch <= 'F'))
  {
    return (int32_t)(Ch - 'A') + 10;
  }
  if ((Ch >= 'a') && (Ch <= 'f'))
  {
    return (int32_t)(Ch - 'a') + 10;
  }
  return -1;
}

/**
  * @brief  Parse N hexadecimal characters into a value.
  * @retval 0 on success, -1 on invalid digit
  */
static int32_t SLCAN_ParseHexValue(const char *Str, uint32_t Digits, uint32_t *Value)
{
  uint32_t result = 0U;
  uint32_t i;

  for (i = 0U; i < Digits; i++)
  {
    int32_t digit = SLCAN_ParseHex(Str[i]);

    if (digit < 0)
    {
      return -1;
    }
    result = (result << 4U) | (uint32_t)digit;
  }

  *Value = result;
  return 0;
}

/**
  * @brief  Push a received CAN frame into the software queue (interrupt side).
  */
static void SLCAN_QueueFrame(const FDCAN_RxHeaderTypeDef *Header, const uint8_t *Data)
{
  uint32_t next = (SlcanFrameHead + 1U) % SLCAN_FRAME_QUEUE_SIZE;
  uint32_t dlc = Header->DataLength >> 16U;
  uint32_t i;

  if (next == SlcanFrameTail)
  {
    /* Queue full: drop the frame */
    return;
  }

  if (dlc > 8U)
  {
    dlc = 8U;
  }

  SlcanFrameQueue[SlcanFrameHead].Id        = Header->Identifier;
  SlcanFrameQueue[SlcanFrameHead].IdType    = Header->IdType;
  SlcanFrameQueue[SlcanFrameHead].FrameType = Header->RxFrameType;
  SlcanFrameQueue[SlcanFrameHead].Dlc       = (uint8_t)dlc;
  for (i = 0U; i < dlc; i++)
  {
    SlcanFrameQueue[SlcanFrameHead].Data[i] = Data[i];
  }

  SlcanFrameHead = next;
}

/**
  * @brief  Format one received CAN frame as an SLCAN line and queue it.
  */
static void SLCAN_SendFrame(const SLCAN_FrameTypeDef *Frame)
{
  char out[SLCAN_LINE_SIZE];
  uint32_t pos = 0U;
  uint32_t i;

  if (Frame->IdType == FDCAN_EXTENDED_ID)
  {
    out[pos] = (Frame->FrameType == FDCAN_REMOTE_FRAME) ? 'R' : 'T';
    pos++;
    SLCAN_Hex(&out[pos], Frame->Id, 8U);
    pos += 8U;
  }
  else
  {
    out[pos] = (Frame->FrameType == FDCAN_REMOTE_FRAME) ? 'r' : 't';
    pos++;
    SLCAN_Hex(&out[pos], Frame->Id, 3U);
    pos += 3U;
  }

  out[pos] = (char)('0' + Frame->Dlc);
  pos++;

  if (Frame->FrameType != FDCAN_REMOTE_FRAME)
  {
    for (i = 0U; i < Frame->Dlc; i++)
    {
      SLCAN_Hex(&out[pos], Frame->Data[i], 2U);
      pos += 2U;
    }
  }

  if (SlcanTimestamp != 0U)
  {
    SLCAN_Hex(&out[pos], HAL_GetTick() % SLCAN_TIMESTAMP_MODULO, 4U);
    pos += 4U;
  }

  out[pos] = (char)SLCAN_CR;
  pos++;

  SLCAN_Queue(out, pos);
  HAL_GPIO_TogglePin(LED_PORT, LED_D1_PIN);
}

/**
  * @brief  Handle one complete command line.
  */
static void SLCAN_HandleLine(const char *Line, uint32_t Len)
{
  FDCAN_TxHeaderTypeDef header;
  uint8_t data[8] = {0};
  uint32_t id = 0U;
  uint32_t dlc;
  uint32_t i;
  uint8_t ok = 0U;
  char ch = Line[0];

  switch (ch)
  {
    case 'O':
    case 'L':
      if (SlcanIsOpen == 0U)
      {
        SlcanMode = (ch == 'L') ? FDCAN_MODE_BUS_MONITORING : FDCAN_MODE_NORMAL;
        ok = (SLCAN_CanApply(1U) == HAL_OK) ? 1U : 0U;
      }
      else
      {
        ok = 1U;
      }
      break;

    case 'C':
      if (SlcanIsOpen != 0U)
      {
        (void)HAL_FDCAN_Stop(&FdcanHandle);
        SlcanIsOpen = 0U;
      }
      ok = 1U;
      break;

    case 'S':
      if ((Len == 2U) && (Line[1] >= '0') && (Line[1] <= '9') && (SlcanIsOpen == 0U))
      {
        static const uint32_t rates[] = {10000U, 20000U, 50000U, 100000U, 125000U,
                                         250000U, 500000U, 750000U, 1000000U, 83300U};
        ok = (SLCAN_SetBitrate(rates[(uint32_t)(Line[1] - '0')]) == HAL_OK) ? 1U : 0U;
      }
      break;

    case 's':
      if ((Len == 5U) && (SlcanIsOpen == 0U))
      {
        uint32_t btr0;
        uint32_t btr1;
        uint32_t brp;
        uint32_t tq;

        /* SJA1000 BTR0/BTR1 referenced to an 8 MHz CAN clock */
        if ((SLCAN_ParseHexValue(&Line[1], 2U, &btr0) == 0) &&
            (SLCAN_ParseHexValue(&Line[3], 2U, &btr1) == 0))
        {
          brp = (btr0 & 0x3FU) + 1U;
          /* Sync segment (1 tq) + TSEG1 + TSEG2 make up one bit time */
          tq  = 1U + (1U + (btr1 & 0x0FU)) + (1U + ((btr1 >> 4U) & 0x07U));
          ok  = (SLCAN_SetBitrate(8000000U / (brp * tq)) == HAL_OK) ? 1U : 0U;
        }
      }
      break;

    case 'F':
      if (Len == 1U)
      {
        FDCAN_ProtocolStatusTypeDef status;
        char out[4];
        uint32_t flags = 0U;

        if (HAL_FDCAN_GetRxFifoFillLevel(&FdcanHandle, FDCAN_RX_FIFO0) >= SLCAN_RX_FIFO_ELMTS)
        {
          flags |= 0x01U;
        }
        if (HAL_FDCAN_GetTxFifoFreeLevel(&FdcanHandle) == 0U)
        {
          flags |= 0x02U;
        }
        if (HAL_FDCAN_GetProtocolStatus(&FdcanHandle, &status) == HAL_OK)
        {
          if (status.Warning != 0U)
          {
            flags |= 0x04U;
          }
          if (status.ErrorPassive != 0U)
          {
            flags |= 0x10U;
          }
          if (status.BusOff != 0U)
          {
            flags |= 0x80U;
          }
        }

        out[0] = 'F';
        SLCAN_Hex(&out[1], flags, 2U);
        out[3] = (char)SLCAN_CR;
        SLCAN_Queue(out, 4U);
        return;
      }
      break;

    case 'V':
    case 'v':
      {
        char out[6];

        out[0] = ch;
        out[1] = SLCAN_HW_VERSION[0];
        out[2] = SLCAN_HW_VERSION[1];
        out[3] = SLCAN_SW_VERSION[0];
        out[4] = SLCAN_SW_VERSION[1];
        out[5] = (char)SLCAN_CR;
        SLCAN_Queue(out, 6U);
      }
      return;

    case 'N':
      if (Len == 1U)
      {
        char out[10];
        uint32_t pos = 0U;

        out[pos] = 'N';
        pos++;
        for (i = 0U; SLCAN_SERIAL_NUMBER[i] != '\0'; i++)
        {
          out[pos] = SLCAN_SERIAL_NUMBER[i];
          pos++;
        }
        out[pos] = (char)SLCAN_CR;
        pos++;
        SLCAN_Queue(out, pos);
        return;
      }
      break;

    case 'Z':
      if ((Len == 2U) && ((Line[1] == '0') || (Line[1] == '1')))
      {
        SlcanTimestamp = (Line[1] == '1') ? 1U : 0U;
        ok = 1U;
      }
      break;

    case 't':
    case 'T':
    case 'r':
    case 'R':
      {
        uint32_t idDigits = ((ch == 't') || (ch == 'r')) ? 3U : 8U;
        uint32_t hdrLen = 1U + idDigits + 1U;

        if ((Len < hdrLen) ||
            (SLCAN_ParseHexValue(&Line[1], idDigits, &id) != 0) ||
            (Line[1U + idDigits] < '0') || (Line[1U + idDigits] > '8'))
        {
          break;
        }

        dlc = (uint32_t)(Line[1U + idDigits] - '0');

        if ((ch == 't') || (ch == 'T'))
        {
          /* Data frames carry the payload hex right after the DLC digit */
          if (Len != hdrLen + (dlc * 2U))
          {
            break;
          }

          for (i = 0U; i < dlc; i++)
          {
            uint32_t value;

            if (SLCAN_ParseHexValue(&Line[hdrLen + (i * 2U)], 2U, &value) != 0)
            {
              break;
            }
            data[i] = (uint8_t)value;
          }
          if (i != dlc)
          {
            break;
          }
        }
        else if (Len != hdrLen)
        {
          /* Remote frames only carry the identifier and the DLC */
          break;
        }
        else
        {
          /* No payload, keep the zeroed data buffer */
        }

        header.Identifier          = id;
        header.IdType              = ((ch == 't') || (ch == 'r')) ? FDCAN_STANDARD_ID
                                                                  : FDCAN_EXTENDED_ID;
        header.TxFrameType         = ((ch == 'r') || (ch == 'R')) ? FDCAN_REMOTE_FRAME
                                                                  : FDCAN_DATA_FRAME;
        header.DataLength          = dlc << 16U;
        header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
        header.BitRateSwitch       = FDCAN_BRS_OFF;
        header.FDFormat            = FDCAN_CLASSIC_CAN;
        header.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
        header.MessageMarker       = 0U;

        if ((SlcanIsOpen != 0U) &&
            (HAL_FDCAN_AddMessageToTxFifoQ(&FdcanHandle, &header, data) == HAL_OK))
        {
          HAL_GPIO_TogglePin(LED_PORT, LED_D2_PIN);
          ok = 1U;
        }
      }
      break;

    default:
      break;
  }

  SLCAN_QueueChar((ok != 0U) ? (char)SLCAN_CR : (char)SLCAN_BEL);
}

/* HAL callbacks -------------------------------------------------------------*/

/**
  * @brief  Store one received byte into the command ring buffer.
  */
static void SLCAN_PushRx(uint8_t Byte)
{
  uint32_t next = (SlcanRxHead + 1U) % SLCAN_RX_RING_SIZE;

  if (next != SlcanRxTail)
  {
    SlcanRxRing[SlcanRxHead] = Byte;
    SlcanRxHead = next;
  }
}

/**
  * @brief  One byte was received from the PC.
  */
void HAL_SCI_RxCpltCallback(SCI_HandleTypeDef *hsci)
{
  if (hsci->Instance == SLCAN_UART)
  {
    uint32_t extra = 0U;

    SLCAN_PushRx(SlcanRxByte);

    /* Pick up bytes that arrived while the interrupt was being re-armed.
       RXNEIE is cleared by the HAL at this point, so they are only read
       here and cannot be handled twice. The count is bounded to keep the
       interrupt short when the host saturates the link. */
    while ((extra < 8U) && (__HAL_SCI_GET_FLAG(hsci, SCI_FLAG_RXNE) != RESET))
    {
      SLCAN_PushRx((uint8_t)(hsci->Instance->DR & 0xFFU));
      extra++;
    }

    (void)HAL_SCI_Receive_IT(hsci, &SlcanRxByte, 1U);
  }
}

/**
  * @brief  The last queued block was sent, continue with the ring buffer.
  */
void HAL_SCI_TxCpltCallback(SCI_HandleTypeDef *hsci)
{
  if (hsci->Instance == SLCAN_UART)
  {
    SlcanTxTail = (SlcanTxTail + SlcanTxChunk) % SLCAN_TX_RING_SIZE;
    SlcanTxChunk = 0U;
    SlcanTxBusy = 0U;
    SLCAN_TxKick();
  }
}

/**
  * @brief  A UART error happened (typically an overrun): resynchronize.
  */
void HAL_SCI_ErrorCallback(SCI_HandleTypeDef *hsci)
{
  if (hsci->Instance == SLCAN_UART)
  {
    if ((hsci->ErrorCode & HAL_SCI_ERROR_ORE) != 0U)
    {
      /* The overrun blocks the receiver, HAL already ended the transfer */
      __HAL_SCI_CLEAR_OREFLAG(hsci);
      hsci->ErrorCode = HAL_SCI_ERROR_NONE;
    }

    if (hsci->RxState == HAL_SCI_STATE_READY)
    {
      (void)HAL_SCI_Receive_IT(hsci, &SlcanRxByte, 1U);
    }
  }
}

/**
  * @brief  New message(s) written to Rx FIFO 0: move them to the software queue.
  */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
  FDCAN_RxHeaderTypeDef header;
  uint8_t data[8];

  if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0U)
  {
    return;
  }

  while (HAL_FDCAN_GetRxFifoFillLevel(hfdcan, FDCAN_RX_FIFO0) > 0U)
  {
    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &header, data) != HAL_OK)
    {
      break;
    }
    SLCAN_QueueFrame(&header, data);
  }
}

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/