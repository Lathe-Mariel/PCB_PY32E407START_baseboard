/**
  ******************************************************************************
  * @file    lan8670.c
  * @brief   LAN8670 10BASE-T1S PHY bring-up and PY32E407 ETH (RMII) driver.
  *
  *          The LAN8670 is connected to the PY32E407 ETH RMII pin set, so the
  *          MAC side is driven by the on-chip ETH peripheral while the PHY is
  *          configured through the MDIO management interface.
  *
  *            MDIO/MDC  -> Clause 22 registers 0x00..0x1F
  *                         MMD registers through 0x0D/0x0E (DEVAD 1, 3, 31)
  *
  *          PLCA (Physical Layer Collision Avoidance) is the 10BASE-T1S
  *          mechanism that makes a multidrop mixing segment deterministic.
  *          One node must act as coordinator (PLCA ID 0) and periodically send
  *          the BEACON; the other nodes are followers with IDs 1..N.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "lan8670.h"

/* Private define ------------------------------------------------------------*/
#define ETH_RX_BUF_SIZE               ETH_MAX_PACKET_SIZE
#define ETH_TX_BUF_SIZE               ETH_MAX_PACKET_SIZE

#define LAN8670_RESET_POLL_MS         100U
#define LAN8670_LINK_POLL_MS         1000U

/* Private variables ---------------------------------------------------------*/
ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT];
ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT];

static uint8_t Rx_Buff[ETH_RX_DESC_CNT][ETH_RX_BUF_SIZE];
static uint8_t Tx_Buff[ETH_TX_DESC_CNT][ETH_TX_BUF_SIZE];

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Read a Clause 22 register through the ETH MDIO interface.
  *         HAL_ETH_ReadPHYRegister() returns the 16 data bits in RegValue.
  */
HAL_StatusTypeDef LAN8670_ReadReg(uint16_t Reg, uint16_t *Value)
{
  uint32_t data = 0U;
  HAL_StatusTypeDef status;

  status = HAL_ETH_ReadPHYRegister(&heth, Reg, &data);
  if (status == HAL_OK)
  {
    *Value = (uint16_t)(data & 0xFFFFU);
  }

  return status;
}

/**
  * @brief  Write a Clause 22 register.
  */
HAL_StatusTypeDef LAN8670_WriteReg(uint16_t Reg, uint16_t Value)
{
  return HAL_ETH_WritePHYRegister(&heth, Reg, (uint32_t)Value);
}

/**
  * @brief  Write the MMD register address.
  *         MMDCTRL.FNCTN = 00b (address), MMDAD = register address.
  */
static HAL_StatusTypeDef LAN8670_MMDSetAddress(uint8_t DevAd, uint16_t Addr)
{
  HAL_StatusTypeDef status;

  status = LAN8670_WriteReg(LAN8670_REG_MMDCTRL,
                            (uint16_t)(LAN8670_MMD_FUNC_ADDR | (uint16_t)DevAd));
  if (status != HAL_OK)
  {
    return status;
  }

  return LAN8670_WriteReg(LAN8670_REG_MMDAD, Addr);
}

/**
  * @brief  Read an MMD register (indirect addressing through 0x0D / 0x0E).
  */
HAL_StatusTypeDef LAN8670_ReadMMD(uint8_t DevAd, uint16_t Addr, uint16_t *Value)
{
  HAL_StatusTypeDef status;

  status = LAN8670_MMDSetAddress(DevAd, Addr);
  if (status != HAL_OK)
  {
    return status;
  }

  status = LAN8670_WriteReg(LAN8670_REG_MMDCTRL,
                            (uint16_t)(LAN8670_MMD_FUNC_DATA | (uint16_t)DevAd));
  if (status != HAL_OK)
  {
    return status;
  }

  return LAN8670_ReadReg(LAN8670_REG_MMDAD, Value);
}

/**
  * @brief  Write an MMD register.
  */
HAL_StatusTypeDef LAN8670_WriteMMD(uint8_t DevAd, uint16_t Addr, uint16_t Value)
{
  HAL_StatusTypeDef status;

  status = LAN8670_MMDSetAddress(DevAd, Addr);
  if (status != HAL_OK)
  {
    return status;
  }

  /* FNCTN must select the data function before MMDAD carries the payload */
  status = LAN8670_WriteReg(LAN8670_REG_MMDCTRL,
                            (uint16_t)(LAN8670_MMD_FUNC_DATA | (uint16_t)DevAd));
  if (status != HAL_OK)
  {
    return status;
  }

  return LAN8670_WriteReg(LAN8670_REG_MMDAD, Value);
}

/**
  * @brief  Read the PHY identifier and the basic status registers.
  * @param  Info: filled with what could be read, may be NULL
  * @retval LAN8670_OK when the device answers and the OUI matches
  */
LAN8670_Status LAN8670_Probe(LAN8670_Info *Info)
{
  LAN8670_Info local;
  uint16_t phyid1 = 0U;
  uint16_t phyid2 = 0U;
  uint16_t bmsr = 0U;
  uint16_t bmcr = 0U;
  uint16_t plca = 0U;

  local.PhyId1        = 0U;
  local.PhyId2        = 0U;
  local.Bmsr          = 0U;
  local.Bmcr          = 0U;
  local.LinkUp        = 0U;
  local.PlcaSupported = 0U;
  local.PlcaStatus    = 0U;

  /* Always hand back a fully initialised block, the caller prints it even
     when the probe fails. */
  if (Info != NULL)
  {
    *Info = local;
  }

  if (LAN8670_ReadReg(LAN8670_REG_PHYID1, &phyid1) != HAL_OK)
  {
    return LAN8670_ERR_MDIO;
  }
  if (LAN8670_ReadReg(LAN8670_REG_PHYID2, &phyid2) != HAL_OK)
  {
    return LAN8670_ERR_MDIO;
  }

  local.PhyId1 = phyid1;
  local.PhyId2 = phyid2;

  /* An all-zero or all-one answer means nothing is driving MDIO */
  if (((phyid1 == 0x0000U) || (phyid1 == 0xFFFFU)) &&
      ((phyid2 == 0x0000U) || (phyid2 == 0xFFFFU)))
  {
    if (Info != NULL)
    {
      *Info = local;
    }
    return LAN8670_ERR_MDIO;
  }

  (void)LAN8670_ReadReg(LAN8670_REG_BMSR, &bmsr);
  (void)LAN8670_ReadReg(LAN8670_REG_BMSR, &bmsr); /* link bit is latched low */
  (void)LAN8670_ReadReg(LAN8670_REG_BMCR, &bmcr);
  local.Bmsr    = bmsr;
  local.Bmcr    = bmcr;
  local.LinkUp  = ((bmsr & LAN8670_BMSR_LINK) != 0U) ? 1U : 0U;

  /* The OPEN Alliance map tells us the PLCA registers exist */
  if (LAN8670_ReadMMD(LAN8670_DEVAD_VENDOR2, LAN8670_PLCA_STS, &plca) == HAL_OK)
  {
    local.PlcaSupported = 1U;
    local.PlcaStatus    = plca;
  }

  if (Info != NULL)
  {
    *Info = local;
  }

  /* Verify the manufacturer model number, not the OUI: the OUI depends on the
     exact ordering code while MODEL[5:0] is fixed for the LAN8670/1/2 family. */
  if (((phyid2 & LAN8670_PHYID2_MODEL_MASK) >> LAN8670_PHYID2_MODEL_SHIFT) !=
      LAN8670_PHYID2_MODEL_LAN86xx)
  {
    return LAN8670_ERR_ID;
  }

  return LAN8670_OK;
}

/**
  * @brief  Issue a PHY software reset, then put the PHY in a known state:
  *         10BASE-T1S is always 10 Mbit/s full duplex and there is no
  *         auto-negotiation, so BMCR is programmed explicitly.
  */
LAN8670_Status LAN8670_SoftReset(void)
{
  uint16_t bmcr = 0U;
  uint16_t dummy = 0U;
  uint32_t start;

  if (LAN8670_WriteReg(LAN8670_REG_BMCR, LAN8670_BMCR_RESET) != HAL_OK)
  {
    return LAN8670_ERR_MDIO;
  }

  start = HAL_GetTick();
  do
  {
    if (LAN8670_ReadReg(LAN8670_REG_BMCR, &bmcr) != HAL_OK)
    {
      return LAN8670_ERR_MDIO;
    }
    if ((bmcr & LAN8670_BMCR_RESET) == 0U)
    {
      break;
    }
  } while ((HAL_GetTick() - start) < LAN8670_RESET_POLL_MS);

  if ((bmcr & LAN8670_BMCR_RESET) != 0U)
  {
    return LAN8670_ERR_TIMEOUT;
  }

  /* Post-reset configuration: 10 Mbit/s, full duplex, no auto-negotiation */
  if (LAN8670_WriteReg(LAN8670_REG_BMCR,
                       LAN8670_BMCR_DUPLEX | LAN8670_BMCR_SPEED_LSB) != HAL_OK)
  {
    return LAN8670_ERR_MDIO;
  }

  /* Let the PHY settle before the first data transfer */
  HAL_Delay(PHY_RESET_DELAY);

  /* Clear the latched status bits */
  (void)LAN8670_ReadReg(LAN8670_REG_BMSR, &dummy);
  (void)LAN8670_ReadReg(LAN8670_REG_BMSR, &dummy);

  return LAN8670_OK;
}

/**
  * @brief  Configure the PLCA reconciliation sublayer.
  * @param  NodeId 0 = coordinator (sends the BEACON), 1..254 = follower ID,
  *                 0xFF = disable PLCA and use plain CSMA/CD
  * @param  NodeCount number of transmit opportunities in one PLCA cycle
  * @param  TransmitOpportunityTimer TOTMR value, 0 keeps the default
  */
LAN8670_Status LAN8670_ConfigurePLCA(uint8_t NodeId, uint8_t NodeCount,
                                     uint8_t TransmitOpportunityTimer)
{
  uint16_t ctrl1;

  if (TransmitOpportunityTimer != 0U)
  {
    if (LAN8670_WriteMMD(LAN8670_DEVAD_VENDOR2, LAN8670_PLCA_TOTMR,
                         (uint16_t)TransmitOpportunityTimer) != HAL_OK)
    {
      return LAN8670_ERR_MDIO;
    }
  }

  ctrl1 = (uint16_t)(((uint16_t)(NodeCount & 0xFFU) << LAN8670_PLCA_NCNT_SHIFT) |
                     (uint16_t)(NodeId & LAN8670_PLCA_ID_MASK));
  if (LAN8670_WriteMMD(LAN8670_DEVAD_VENDOR2, LAN8670_PLCA_CTRL1, ctrl1) != HAL_OK)
  {
    return LAN8670_ERR_MDIO;
  }

  if (NodeId == 0xFFU)
  {
    /* Clear the enable bit: back to CSMA/CD */
    if (LAN8670_WriteMMD(LAN8670_DEVAD_VENDOR2, LAN8670_PLCA_CTRL0, 0x0000U) != HAL_OK)
    {
      return LAN8670_ERR_MDIO;
    }
  }
  else
  {
    if (LAN8670_WriteMMD(LAN8670_DEVAD_VENDOR2, LAN8670_PLCA_CTRL0,
                         LAN8670_PLCA_EN) != HAL_OK)
    {
      return LAN8670_ERR_MDIO;
    }
  }

  return LAN8670_OK;
}

/**
  * @brief  Wait until the PHY reports a valid link.
  */
LAN8670_Status LAN8670_WaitForLink(uint32_t TimeoutMs)
{
  uint32_t start = HAL_GetTick();
  uint16_t bmsr = 0U;

  do
  {
    /* BMSR link status is latched low: read twice to obtain the live state */
    if (LAN8670_ReadReg(LAN8670_REG_BMSR, &bmsr) != HAL_OK)
    {
      return LAN8670_ERR_MDIO;
    }
    if (LAN8670_ReadReg(LAN8670_REG_BMSR, &bmsr) != HAL_OK)
    {
      return LAN8670_ERR_MDIO;
    }
    if ((bmsr & LAN8670_BMSR_LINK) != 0U)
    {
      return LAN8670_OK;
    }
    HAL_Delay(10U);
  } while ((HAL_GetTick() - start) < TimeoutMs);

  return LAN8670_ERR_TIMEOUT;
}

/**
  * @brief  Configure the ETH MAC/DMA for 10BASE-T1S. Does not start the MAC.
  *         This must run before any MDIO access because it enables the ETH
  *         clock, configures the RMII pin set and sets the MDIO clock range.
  *         Auto-negotiation is disabled: 10BASE-T1S is always 10 Mbit/s and the
  *         LAN8670 has no Clause 37 negotiation.
  */
HAL_StatusTypeDef LAN8670_EthInit(void)
{
  static uint8_t macAddr[6] = {0x02U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U};

  heth.Instance = ETH;
  heth.Init.AutoNegotiation = ETH_AUTONEGOTIATION_DISABLE;
  heth.Init.Speed           = LAN8670_ETH_SPEED;
  heth.Init.DuplexMode      = LAN8670_ETH_DUPLEX;
  heth.Init.PhyAddress      = LAN8670_PHY_ADDR;
  heth.Init.MACAddr         = macAddr;
  heth.Init.RxMode          = ETH_RXPOLLING_MODE;
  heth.Init.ChecksumMode    = ETH_CHECKSUM_BY_SOFTWARE;
  heth.Init.MediaInterface  = ETH_MEDIA_INTERFACE_RMII;

  if (HAL_ETH_Init(&heth) != HAL_OK)
  {
    return HAL_ERROR;
  }

  if (HAL_ETH_DMATxDescListInit(&heth, DMATxDscrTab, &Tx_Buff[0][0],
                                ETH_TX_DESC_CNT) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_ETH_DMARxDescListInit(&heth, DMARxDscrTab, &Rx_Buff[0][0],
                                   ETH_RX_DESC_CNT);
}

/**
  * @brief  Enable the MAC/DMA transmitter and receiver.
  */
HAL_StatusTypeDef LAN8670_EthStart(void)
{
  return HAL_ETH_Start(&heth);
}

/**
  * @brief  Queue one Ethernet frame for transmission.
  * @param  Frame  complete frame including the 14 byte header
  * @param  Length frame length, 14..ETH_MAX_PACKET_SIZE
  */
HAL_StatusTypeDef LAN8670_SendFrame(const uint8_t *Frame, uint32_t Length)
{
  uint32_t i;

  if ((Frame == NULL) || (Length == 0U) || (Length > ETH_MAX_PACKET_SIZE))
  {
    return HAL_ERROR;
  }

  if ((heth.TxDesc->Status & ETH_DMATXDESC_OWN) != 0U)
  {
    /* The DMA still owns the descriptor, the previous frame is in flight */
    return HAL_BUSY;
  }

  /* Copy the frame into the buffer attached to the current Tx descriptor */
  for (i = 0U; i < Length; i++)
  {
    ((uint8_t *)(uint32_t)heth.TxDesc->Buffer1Addr)[i] = Frame[i];
  }

  /* ControlBufferSize (TCH, FS, LS and TBS1) is programmed by
     HAL_ETH_TransmitFrame(), do not overwrite it here. */

  if (HAL_ETH_TransmitFrame(&heth, Length) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return HAL_OK;
}

/**
  * @brief  Poll for one received frame.
  *         The descriptors are handed back to the DMA once the frame has been
  *         copied out, otherwise reception stalls after the first segment.
  * @param  Buffer     destination buffer
  * @param  BufferSize size of the destination buffer
  * @param  Length     received frame length, may be NULL
  * @retval HAL_OK when a frame was copied, HAL_TIMEOUT when none is pending
  */
HAL_StatusTypeDef LAN8670_GetFrame(uint8_t *Buffer, uint32_t BufferSize,
                                   uint32_t *Length)
{
  ETH_DMADescTypeDef *desc;
  uint8_t *src;
  uint32_t len;
  uint32_t offset = 0U;
  uint32_t copied = 0U;
  uint32_t segment;
  uint32_t i;

  if ((Buffer == NULL) || (BufferSize == 0U))
  {
    return HAL_ERROR;
  }

  if (HAL_ETH_GetReceivedFrame(&heth) != HAL_OK)
  {
    /* Clear a possible Rx buffer unavailable condition and resume the DMA */
    if ((heth.Instance->DMASR & ETH_DMASR_RBUS) != 0U)
    {
      heth.Instance->DMASR   = ETH_DMASR_RBUS;
      heth.Instance->DMARPDR = 0U;
    }
    return HAL_TIMEOUT;
  }

  len = heth.RxFrameInfos.length;

  if (len > BufferSize)
  {
    len = BufferSize;
  }

  /* A frame may span several descriptors of ETH_RX_BUF_SIZE bytes each */
  desc  = heth.RxFrameInfos.FSRxDesc;
  src   = (uint8_t *)(uint32_t)desc->Buffer1Addr;

  while ((copied < len) && (desc != NULL))
  {
    uint32_t chunk = ETH_RX_BUF_SIZE - offset;

    if (chunk > (len - copied))
    {
      chunk = len - copied;
    }

    for (i = 0U; i < chunk; i++)
    {
      Buffer[copied + i] = src[offset + i];
    }

    copied += chunk;
    offset += chunk;

    if (offset >= ETH_RX_BUF_SIZE)
    {
      offset = 0U;
      desc = (ETH_DMADescTypeDef *)(uint32_t)desc->Buffer2NextDescAddr;
      if (desc != NULL)
      {
        src = (uint8_t *)(uint32_t)desc->Buffer1Addr;
      }
    }
  }

  /* Give the segment chain back to the DMA and reset the segment counter */
  desc = heth.RxFrameInfos.FSRxDesc;
  for (segment = 0U; segment < heth.RxFrameInfos.SegCount; segment++)
  {
    desc->Status |= ETH_DMARXDESC_OWN;
    desc = (ETH_DMADescTypeDef *)(uint32_t)desc->Buffer2NextDescAddr;
    if (desc == NULL)
    {
      break;
    }
  }
  heth.RxFrameInfos.SegCount = 0U;

  /* Resume reception when the Rx buffer unavailable flag was raised */
  if ((heth.Instance->DMASR & ETH_DMASR_RBUS) != 0U)
  {
    heth.Instance->DMASR   = ETH_DMASR_RBUS;
    heth.Instance->DMARPDR = 0U;
  }

  if (Length != NULL)
  {
    *Length = len;
  }

  return HAL_OK;
}

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
