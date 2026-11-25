/**
  ******************************************************************************
  * @file    lan8670.h
  * @brief   LAN8670 10BASE-T1S PHY + PY32E407 ETH (RMII) bring-up helpers.
  ******************************************************************************
  */

#ifndef __LAN8670_H
#define __LAN8670_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Standard IEEE 802.3 Clause 22 registers -----------------------------------*/
#define LAN8670_REG_BMCR             0x00U   /*!< Basic mode control          */
#define LAN8670_REG_BMSR             0x01U   /*!< Basic mode status           */
#define LAN8670_REG_PHYID1           0x02U   /*!< PHY identifier 1            */
#define LAN8670_REG_PHYID2           0x03U   /*!< PHY identifier 2            */
#define LAN8670_REG_MMDCTRL          0x0DU   /*!< MMD access control          */
#define LAN8670_REG_MMDAD            0x0EU   /*!< MMD access address / data   */

/* MMDCTRL function field FNCTN[1:0] (bits 15:14)
   00b = Address, 01b = Data (no post increment),
   10b = Data (post increment on read/write), 11b = Data (post increment on write) */
#define LAN8670_MMD_FUNC_ADDR        0x0000U /*!< 00b: next MMDAD write sets the address */
#define LAN8670_MMD_FUNC_DATA        0x4000U /*!< 01b: next MMDAD access hits the data */

/* MMD 31 (vendor) registers -------------------------------------------------
   NOTE: STS1/STS2/IMSK1 live in MMD 31 at address 0x0018/0x0019/0x001C.
   Clause 22 register 0x18 is reserved space and always reads 0x0000, so
   these must be accessed with LAN8670_ReadMMD(), not LAN8670_ReadReg(). */
#define LAN8670_MMD_STS1               0x0018U /*!< Status 1 (read clear)       */
#define LAN8670_STS1_SQI               0x1000U /*!< Bit 12: SQI status changed   */
#define LAN8670_STS1_PSTC              0x0800U /*!< Bit 11: PLCA status changed  */
#define LAN8670_STS1_TXCOL             0x0400U /*!< Bit 10: physical tx collision */
#define LAN8670_STS1_TXJAB             0x0200U /*!< Bit 9 : transmit jabber      */
#define LAN8670_STS1_TSSI              0x0100U /*!< Bit 8 : packet tx/rx via TSSI */
#define LAN8670_STS1_EMPCYC            0x0080U /*!< Bit 7 : empty PLCA cycle     */
#define LAN8670_STS1_RXINTO            0x0040U /*!< Bit 6 : rx in own TO         */
#define LAN8670_STS1_UNEXPB            0x0020U /*!< Bit 5 : unexpected BEACON    */
#define LAN8670_STS1_BCNBFTO           0x0010U /*!< Bit 4 : BEACON before own TO */
#define LAN8670_STS1_UNCRS              0x0008U /*!< Bit 3 : unexpected carrier   */
#define LAN8670_STS1_PLCASYM            0x0004U /*!< Bit 2 : BEACON seen, PLCA off */
#define LAN8670_STS1_ESDERR             0x0002U /*!< Bit 1 : end-of-stream delim err */
#define LAN8670_STS1_DEC5B              0x0001U /*!< Bit 0 : 5B decode error      */

/* PLCA event counters (MMD 31, read clear, low word latches on high read) */
#define LAN8670_CTRCTRL                0x0020U /*!< Counter control              */
#define LAN8670_CTRCTRL_TOCTRE         0x0002U /*!< Bit 1: TO counter enable    */
#define LAN8670_CTRCTRL_BCNCTRE        0x0001U /*!< Bit 0: BEACON counter enable */
#define LAN8670_TOCNTH                 0x0024U /*!< Transmit opportunity count H */
#define LAN8670_TOCNTL                 0x0025U /*!< Transmit opportunity count L */
#define LAN8670_BCNCNTH                0x0026U /*!< BEACON count high            */
#define LAN8670_BCNCNTL                0x0027U /*!< BEACON count low             */

/* Link Status Control (MMD 31 address 0x0012, revision D0 and later).
   LSCFG bits 12:11: 00 = fixed (LNKSTS always 0), 01 = PLCA status,
   10 = firmware semaphore (LSSEM bit 0).  The reset default is "fixed",
   i.e. link reads DOWN forever until this register is programmed. */
#define LAN8670_REG_LSCTL               0x0012U
#define LAN8670_LSCTL_LSCFG_PLCA       ((uint16_t)1U << 11)
#define LAN8670_LSCTL_LSCFG_SEMAPHORE   ((uint16_t)2U << 11)
#define LAN8670_LSCTL_LSSEM            ((uint16_t)1U << 0)

/* Collision Detection Control 0 (MMD 31).  CDEN bit 15 is set by default;
   AN1699 recommends disabling collision detection while PLCA is active for
   noise immunity and re-enabling it in the CSMA/CD fallback. */
#define LAN8670_CDCTL0                 0x0087U
#define LAN8670_CDCTL0_CDEN            0x8000U

/* MMD device addresses */
#define LAN8670_DEVAD_PMA_PMD        1U
#define LAN8670_DEVAD_PCS            3U
#define LAN8670_DEVAD_VENDOR2        31U

/* MMD 31 (vendor) registers -------------------------------------------------*/
#define LAN8670_PINCTRL              0x0011U /*!< Pin control                 */
#define LAN8670_PLCA_CTRL0           0xCA01U /*!< PLCA control 0              */
#define LAN8670_PLCA_CTRL1           0xCA02U /*!< PLCA control 1              */
#define LAN8670_PLCA_STS             0xCA03U /*!< PLCA status                 */
#define LAN8670_PLCA_TOTMR           0xCA04U /*!< PLCA transmit opportunity timer */

/* PLCA_CTRL0 bits (register 0xCA01, MMD 31) */
#define LAN8670_PLCA_EN              0x8000U /*!< Bit 15: PLCA enable        */
#define LAN8670_PLCA_RST             0x4000U /*!< Bit 14: PLCA reset (W1SC)  */

/* PLCA_CTRL1 fields (register 0xCA02, MMD 31)
   The datasheet puts NCNT in bits 15:8 and the local ID in bits 7:0. */
#define LAN8670_PLCA_NCNT_MASK       0xFF00U /*!< Bits 15:8 node count       */
#define LAN8670_PLCA_NCNT_SHIFT      8U
#define LAN8670_PLCA_ID_MASK         0x00FFU /*!< Bits 7:0  local ID         */
#define LAN8670_PLCA_ID_DISABLED     0x00FFU /*!< ID = 0xFF: fall back to CSMA/CD */

/* PLCA_STS bits (register 0xCA03, MMD 31) */
#define LAN8670_PLCA_STS_PST         0x8000U /*!< Bit 15: BEACON seen        */

/* PLCA reconciliation sublayer status (MMD 31, address 0x0036) */
#define LAN8670_PRSSTS               0x0036U
#define LAN8670_PRSSTS_MAXID(v)      (uint8_t)(((v) >> 8U) & 0xFFU)

/* PCS control (MMD 3) */
#define LAN8670_T1SPCSCTL            0x08F3U
#define LAN8670_T1SPCSCTL_LBE        0x4000U /*!< Bit 14: PCS loopback enable */

/* SQI registers (MMD 31, silicon revision C1/C2; deprecated on D0) */
#define LAN8670_SQICTL               0x00A0U
#define LAN8670_SQISTS0              0x00A1U
#define LAN8670_SQICFG0              0x00AAU
#define LAN8670_SQICFG2              0x00ACU
#define LAN8670_SQICTL_RST           0x8000U /*!< Bit 15: reset (self clear) */
#define LAN8670_SQICTL_EN            0x4000U /*!< Bit 14: measurement enable */
#define LAN8670_SQISTS0_ERR          0x0080U /*!< Bit 7: accumulation error  */
#define LAN8670_SQISTS0_VLD          0x0040U /*!< Bit 6: value valid         */
#define LAN8670_SQISTS0_VAL(v)       (uint8_t)(((v) >> 3U) & 0x07U) /*!< value 0..7 */

/* Dynamic Channel Quality SQI (MMD 31, silicon revision D0 and later) */
#define LAN8670_DCQ_TOID             0xCC02U /*!< Transmit Opportunity ID    */
#define LAN8670_DCQ_SQI              0xCC03U /*!< SQI: bit15 UPD, bits 2:0 value */
#define LAN8670_DCQ_SQIP             0xCC04U /*!< SQI+: bit15 UPD, bits 4:0 value */
#define LAN8670_DCQ_SQI_UPD          0x8000U
#define LAN8670_DCQ_SQIP_VAL(v)      (uint8_t)((v) & 0x1FU)

/* BMCR bits */
#define LAN8670_BMCR_RESET           0x8000U
#define LAN8670_BMCR_LOOPBACK        0x4000U
#define LAN8670_BMCR_SPEED_LSB       0x2000U
#define LAN8670_BMCR_AUTONEG_EN      0x1000U
#define LAN8670_BMCR_POWER_DOWN      0x0800U
#define LAN8670_BMCR_ISOLATE         0x0400U
#define LAN8670_BMCR_RESTART_AN      0x0200U
#define LAN8670_BMCR_DUPLEX          0x0100U
#define LAN8670_BMCR_SPEED_MSB       0x0040U

/* BMSR bits */
#define LAN8670_BMSR_LINK            0x0004U
#define LAN8670_BMSR_AN_COMPLETE     0x0020U
#define LAN8670_BMSR_EXT_STATUS      0x0100U

/* PHY identifier: the datasheet defines MODEL[5:0] = 010110b for LAN8670/1/2,
   located in PHYID2 bits 9:4. The OUI depends on the exact ordering code, so
   the model field is the reliable check.  REV[3:0] in PHYID2 bits 3:0 gives
   the silicon revision: 0101 = C2, 0110 = D0. */
#define LAN8670_PHYID2_MODEL_MASK    0x03F0U
#define LAN8670_PHYID2_MODEL_SHIFT   4U
#define LAN8670_PHYID2_MODEL_LAN86xx 0x16U   /*!< 010110b */
#define LAN8670_PHYID2_REV_MASK      0x000FU
#define LAN8670_PHYID2_REV_B1         0x0002U
#define LAN8670_PHYID2_REV_C1        0x0004U
#define LAN8670_PHYID2_REV_C2        0x0005U
#define LAN8670_PHYID2_REV_D0        0x0006U

/* Exported types ------------------------------------------------------------*/
typedef enum
{
  LAN8670_OK = 0,
  LAN8670_ERR_MDIO,       /*!< No answer on the management interface       */
  LAN8670_ERR_ID,         /*!< PHY identifier does not match a LAN8670     */
  LAN8670_ERR_TIMEOUT     /*!< A polled operation timed out                */
} LAN8670_Status;

typedef struct
{
  uint16_t PhyId1;
  uint16_t PhyId2;
  uint16_t Bmsr;
  uint16_t Bmcr;
  uint8_t  LinkUp;
  uint8_t  PlcaSupported;
  uint16_t PlcaStatus;
  uint8_t  SiliconRev;            /*!< LAN8670_PHYID2_REV_x, 0 when unknown */
} LAN8670_Info;

/* Exported functions prototypes ---------------------------------------------*/

/* Direct Clause 22 register access (also used for the bring-up self test) */
HAL_StatusTypeDef LAN8670_ReadReg(uint16_t Reg, uint16_t *Value);
HAL_StatusTypeDef LAN8670_WriteReg(uint16_t Reg, uint16_t Value);

/* MMD (indirect) register access, MMD address 1 / 3 / 31 */
HAL_StatusTypeDef LAN8670_ReadMMD(uint8_t DevAd, uint16_t Addr, uint16_t *Value);
HAL_StatusTypeDef LAN8670_WriteMMD(uint8_t DevAd, uint16_t Addr, uint16_t Value);

/* Bring-up sequence */
LAN8670_Status LAN8670_Probe(LAN8670_Info *Info);
LAN8670_Status LAN8670_SoftReset(void);
LAN8670_Status LAN8670_ApplyConfig(uint8_t SiliconRev);
LAN8670_Status LAN8670_ConfigurePLCA(uint8_t NodeId, uint8_t NodeCount,
                                     uint8_t TransmitOpportunityTimer);
LAN8670_Status LAN8670_WaitForLink(uint32_t TimeoutMs);

/* PLCA health counters (MMD 31, read clear) */
LAN8670_Status LAN8670_EnablePlcaCounters(void);
LAN8670_Status LAN8670_ReadPlcaCounters(uint32_t *BeaconCount,
                                        uint32_t *TransmitOpportunityCount);

/* Signal quality indicator (revision C1/C2): start a measurement, then poll. */
LAN8670_Status LAN8670_SqiStart(void);
LAN8670_Status LAN8670_SqiRead(uint8_t *Valid, uint8_t *Value, uint8_t *ErrorCode);

/* Dynamic Channel Quality SQI (revision D0 and later): writing the transmit
   opportunity ID starts a measurement; SQI_UPD (read clear) signals results. */
LAN8670_Status LAN8670_DcqSqiStart(void);
LAN8670_Status LAN8670_DcqSqiRead(uint8_t *Updated, uint8_t *Value,
                                  uint8_t *ValuePlus);

/* PCS loopback: returns the MAC's own transmitted frames (PLCA must be off). */
LAN8670_Status LAN8670_SetPcsLoopback(uint8_t Enable);

/* MAC/DMA layer */
HAL_StatusTypeDef LAN8670_EthInit(void);
HAL_StatusTypeDef LAN8670_EthStart(void);
const uint8_t *LAN8670_GetMacAddress(void);
HAL_StatusTypeDef LAN8670_SendFrame(const uint8_t *Frame, uint32_t Length);
HAL_StatusTypeDef LAN8670_GetFrame(uint8_t *Buffer, uint32_t BufferSize,
                                   uint32_t *Length);

#ifdef __cplusplus
}
#endif

#endif /* __LAN8670_H */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
