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
   the model field is the reliable check. */
#define LAN8670_PHYID2_MODEL_MASK    0x03F0U
#define LAN8670_PHYID2_MODEL_SHIFT   4U
#define LAN8670_PHYID2_MODEL_LAN86xx 0x16U   /*!< 010110b */

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
LAN8670_Status LAN8670_ConfigurePLCA(uint8_t NodeId, uint8_t NodeCount,
                                     uint8_t TransmitOpportunityTimer);
LAN8670_Status LAN8670_WaitForLink(uint32_t TimeoutMs);

/* MAC/DMA layer */
HAL_StatusTypeDef LAN8670_EthInit(void);
HAL_StatusTypeDef LAN8670_EthStart(void);
HAL_StatusTypeDef LAN8670_SendFrame(const uint8_t *Frame, uint32_t Length);
HAL_StatusTypeDef LAN8670_GetFrame(uint8_t *Buffer, uint32_t BufferSize,
                                   uint32_t *Length);

#ifdef __cplusplus
}
#endif

#endif /* __LAN8670_H */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
