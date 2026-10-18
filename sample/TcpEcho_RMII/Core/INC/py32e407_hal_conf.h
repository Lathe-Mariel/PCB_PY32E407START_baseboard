/* Includes ------------------------------------------------------------------*/
/**
  ******************************************************************************
  * @file    py32e407_hal_conf.h
  * @author  MCU Application Team
  * @brief   HAL configuration file.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 Puya Semiconductor Co.
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
#ifndef __PY32E407_HAL_CONF_HAL_CONF_H
#define __PY32E407_HAL_CONF_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/

/* ########################## Module Selection ############################## */
/**
  * @brief This is the list of modules to be used in the HAL driver
  */
#define HAL_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
/* #define HAL_ADC_MODULE_ENABLED */
/* #define HAL_I2C_MODULE_ENABLED */
/* #define HAL_SPI_MODULE_ENABLED */
/* #define HAL_USART_MODULE_ENABLED */
/* #define HAL_SCI_MODULE_ENABLED */
#define HAL_UART_MODULE_ENABLED
/* #define HAL_LPUART_MODULE_ENABLED */
/* #define HAL_IRDA_MODULE_ENABLED */
/* #define HAL_SMARTCARD_MODULE_ENABLED */
/* #define HAL_CORDIC_MODULE_ENABLED */
/* #define HAL_I2S_MODULE_ENABLED */
/* #define HAL_TIM_MODULE_ENABLED */
/* #define HAL_LPTIM_MODULE_ENABLED */
/* #define HAL_RTC_MODULE_ENABLED */
/* #define HAL_COMP_MODULE_ENABLED */
/* #define HAL_OPA_MODULE_ENABLED */
/* #define HAL_CTC_MODULE_ENABLED */
/* #define HAL_IWDG_MODULE_ENABLED */
/* #define HAL_WWDG_MODULE_ENABLED */
/* #define HAL_LCD_MODULE_ENABLED */
/* #define HAL_LCDC_MODULE_ENABLED */
/* #define HAL_CRC_MODULE_ENABLED */
/* #define HAL_RNG_MODULE_ENABLED */
/* #define HAL_DIV_MODULE_ENABLED */
/* #define HAL_FDCAN_MODULE_ENABLED */
/* #define HAL_DAC_MODULE_ENABLED */
/* #define HAL_SD_MODULE_ENABLED */
/* #define HAL_ESMC_MODULE_ENABLED */
/* #define HAL_USBH_MODULE_ENABLED */
/* #define HAL_USBD_MODULE_ENABLED */
#define HAL_ETH_MODULE_ENABLED

/* ########################## Oscillator Values adaptation ####################*/
#if !defined  (HSI_VALUE)
#define HSI_VALUE              ((uint32_t)16000000)     /*!< Value of the Internal oscillator in Hz */
#endif /* HSI_VALUE */

/**
  * @brief Adjust the value of External High Speed oscillator (HSE) used in your application.
  *        This value is used by the RCC HAL module to compute the system frequency
  */
#if !defined  (HSE_VALUE)
#define HSE_VALUE              ((uint32_t)24000000) /*!< Value of the External oscillator in Hz */
#endif /* HSE_VALUE */

#if !defined  (HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT    ((uint32_t)200)   /*!< Time out for HSE start up, in ms */
#endif /* HSE_STARTUP_TIMEOUT */

/**
  * @brief Internal High Speed oscillator (HSI48) value for USB.
  *        This internal oscillator is mainly dedicated to provide a high precision clock to
  *        the USB peripheral by means of a special Clock Recovery System (CRS) circuitry.
  *        When the CRS is not used, the HSI48 RC oscillator runs on it default frequency
  *        which is subject to manufacturing process variations.
  */
#if !defined  (HSI48_VALUE)
  #define HSI48_VALUE           ((uint32_t)48000000)  /*!< Value of the Internal High Speed oscillator for USB in Hz.
                                               The real value my vary depending on manufacturing process variations.*/
#endif /* HSI48_VALUE */

/**
  * @brief Internal Low Speed Internal oscillator (LSI) value.
  */
#if !defined  (LSI_VALUE)
#define LSI_VALUE               ((uint32_t)40000)    /*!< LSI Typical Value in Hz */
#endif /* LSI_VALUE */                                /*!< Value of the Internal Low Speed oscillator in Hz
                                                    The real value may vary depending on the variations
                                                    in voltage and temperature. */

/**
  * @brief Adjust the value of External Low Speed oscillator (LSE) used in your application.
  *        This value is used by the RCC HAL module to compute the system frequency
  */
#if !defined  (LSE_VALUE)
#define LSE_VALUE              ((uint32_t)32768) /*!< Value of the External oscillator in Hz*/
#endif /* LSE_VALUE */

#if !defined  (LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT    ((uint32_t)5000)   /*!< Time out for LSE start up, in ms */
#endif /* LSE_STARTUP_TIMEOUT */

/* Tip: To avoid modifying this file each time you need to use different HSE,
===  you can define the HSE value in your toolchain compiler preprocessor. */

/* ########################### System Configuration ######################### */
/**
 * @brief This is the HAL system configuration section
 */
#define  VDD_VALUE               ((uint32_t)3300) /*!< Value of VDD in mv */
#define  PRIORITY_HIGHEST        0
#define  PRIORITY_HIGH           1
#define  PRIORITY_LOW            2
#define  PRIORITY_LOWEST         3
#define  TICK_INT_PRIORITY       ((uint32_t)PRIORITY_LOWEST)    /*!< tick interrupt priority (lowest by default)  */
#define  USE_RTOS                0
#define  PREFETCH_ENABLE         0

/* ########################## Assert Selection ############################## */
/**
 * @brief Uncomment the line below to expanse the "assert_param" macro in the
 *        HAL drivers code
 */
/* #define USE_FULL_ASSERT       1U */

/* ################## Ethernet peripheral configuration ##################### */

/* Section 1 : Ethernet peripheral configuration */

/* MAC ADDRESS: MAC_ADDR0:MAC_ADDR1:MAC_ADDR2:MAC_ADDR3:MAC_ADDR4:MAC_ADDR5 */
#define MAC_ADDR0                     2U
#define MAC_ADDR1                     0U
#define MAC_ADDR2                     0U
#define MAC_ADDR3                     0U
#define MAC_ADDR4                     0U
#define MAC_ADDR5                     0U

/* Definition of the Ethernet driver buffers size and count */
#ifdef ETH_RX_BUF_SIZE
#undef ETH_RX_BUF_SIZE
#endif
#define ETH_RX_BUF_SIZE               ETH_MAX_PACKET_SIZE /* buffer size for receive  */

#ifdef ETH_TX_BUF_SIZE
#undef ETH_TX_BUF_SIZE
#endif
#define ETH_TX_BUF_SIZE               ETH_MAX_PACKET_SIZE /* buffer size for transmit */

#ifdef ETH_RXBUFNB
#undef ETH_RXBUFNB
#endif
#define ETH_RXBUFNB                   ((uint32_t)8U)      /* 4 Rx buffers of size ETH_RX_BUF_SIZE */

#ifdef ETH_TXBUFNB
#undef ETH_TXBUFNB
#endif
#define ETH_TXBUFNB                   ((uint32_t)8U)      /* 4 Tx buffers of size ETH_TX_BUF_SIZE */

/* Section 2: PHY configuration section */

/* LAN8720_PHY_ADDRESS Address*/
#define LAN8720_PHY_ADDRESS           0U
/* PHY Reset delay these values are based on a 1 ms Systick interrupt*/
#define PHY_RESET_DELAY               ((uint32_t)0x00000005U)
/* PHY Configuration delay */
#define PHY_CONFIG_DELAY              ((uint32_t)0x00000005U)

#define PHY_READ_TO                   ((uint32_t)0x0000FFFFU)
#define PHY_WRITE_TO                  ((uint32_t)0x0000FFFFU)

/* Section 3: Common PHY Registers */

#define PHY_BCR                       ((uint16_t)0x00U)    /*!< Transceiver Basic Control Register */
#define PHY_BSR                       ((uint16_t)0x01U)    /*!< Transceiver Basic Status Register  */

#define PHY_RESET                     ((uint16_t)0x8000U)  /*!< PHY Reset */
#define PHY_LOOPBACK                  ((uint16_t)0x4000U)  /*!< Select loop-back mode */
#define PHY_FULLDUPLEX_100M           ((uint16_t)0x2100U)  /*!< Set the full-duplex mode at 100 Mb/s */
#define PHY_HALFDUPLEX_100M           ((uint16_t)0x2000U)  /*!< Set the half-duplex mode at 100 Mb/s */
#define PHY_FULLDUPLEX_10M            ((uint16_t)0x0100U)  /*!< Set the full-duplex mode at 10 Mb/s  */
#define PHY_HALFDUPLEX_10M            ((uint16_t)0x0000U)  /*!< Set the half-duplex mode at 10 Mb/s  */
#define PHY_AUTONEGOTIATION           ((uint16_t)0x1000U)  /*!< Enable auto-negotiation function     */
#define PHY_RESTART_AUTONEGOTIATION   ((uint16_t)0x0200U)  /*!< Restart auto-negotiation function    */
#define PHY_POWERDOWN                 ((uint16_t)0x0800U)  /*!< Select the power down mode           */
#define PHY_ISOLATE                   ((uint16_t)0x0400U)  /*!< Isolate PHY from MII                 */

#define PHY_AUTONEGO_COMPLETE         ((uint16_t)0x0020U)  /*!< Auto-Negotiation process completed */
#define PHY_LINKED_STATUS             ((uint16_t)0x0004U)  /*!< Valid link established             */
#define PHY_JABBER_DETECTION          ((uint16_t)0x0002U)  /*!< Jabber condition detected          */

/* Section 4: Extended PHY Registers */
#define PHY_SR                        ((uint16_t)0x1FU)    /*!< PHY status register Offset */

#define PHY_SPEED_STATUS              ((uint16_t)0x0004U)  /*!< PHY Speed mask  */
#define PHY_DUPLEX_STATUS             ((uint16_t)0x0010U)  /*!< PHY Duplex mask */

/* ################## SPI peripheral configuration ########################## */

/* CRC FEATURE: Use to activate CRC feature inside HAL SPI Driver
* Activated: CRC code is present inside driver
* Deactivated: CRC code cleaned from driver
*/

#define USE_SPI_CRC                     0U

/* Includes ------------------------------------------------------------------*/
/**
 * @brief Include module's header file
 */
#ifdef HAL_MODULE_ENABLED
#include "py32e4xx_hal.h"
#endif /* HAL_MODULE_ENABLED */

#ifdef HAL_RCC_MODULE_ENABLED
#include "py32e407_hal_rcc.h"
#endif /* HAL_RCC_MODULE_ENABLED */

#ifdef HAL_CORTEX_MODULE_ENABLED
#include "py32e407_hal_cortex.h"
#endif /* HAL_CORTEX_MODULE_ENABLED */

#ifdef HAL_DMA_MODULE_ENABLED
#include "py32e407_hal_dma.h"
#endif /* HAL_DMA_MODULE_ENABLED */

#ifdef HAL_FLASH_MODULE_ENABLED
#include "py32e407_hal_flash.h"
#endif /* HAL_FLASH_MODULE_ENABLED */

#ifdef HAL_GPIO_MODULE_ENABLED
#include "py32e407_hal_gpio.h"
#endif /* HAL_GPIO_MODULE_ENABLED */

#ifdef HAL_PWR_MODULE_ENABLED
#include "py32e407_hal_pwr.h"
#endif /* HAL_PWR_MODULE_ENABLED */

#ifdef HAL_EXTI_MODULE_ENABLED
#include "py32e407_hal_exti.h"
#endif /* HAL_EXTI_MODULE_ENABLED */

#ifdef HAL_ADC_MODULE_ENABLED
#include "py32e407_hal_adc.h"
#endif /* HAL_ADC_MODULE_ENABLED */

#ifdef HAL_I2C_MODULE_ENABLED
#include "py32e407_hal_i2c.h"
#endif /* HAL_I2C_MODULE_ENABLED */

#ifdef HAL_SPI_MODULE_ENABLED
#include "py32e407_hal_spi.h"
#endif /* HAL_SPI_MODULE_ENABLED */

#ifdef HAL_USART_MODULE_ENABLED
#include "py32e407_hal_usart.h"
#endif /* HAL_USART_MODULE_ENABLED */

#ifdef HAL_SCI_MODULE_ENABLED
#include "py32e407_hal_sci.h"
#endif /* HAL_SCI_MODULE_ENABLED */

#ifdef HAL_UART_MODULE_ENABLED
#include "py32e407_hal_uart.h"
#endif /* HAL_UART_MODULE_ENABLED */

#ifdef HAL_LPUART_MODULE_ENABLED
#include "py32e407_hal_lpuart.h"
#endif /* HAL_LPUART_MODULE_ENABLED */

#ifdef HAL_IRDA_MODULE_ENABLED
#include "py32e407_hal_irda.h"
#endif /* HAL_IRDA_MODULE_ENABLED */

#ifdef HAL_SMARTCARD_MODULE_ENABLED
#include "py32e407_hal_smartcard.h"
#endif /* HAL_SMARTCARD_MODULE_ENABLED */

#ifdef HAL_CORDIC_MODULE_ENABLED
#include "py32e407_hal_cordic.h"
#endif /* HAL_CORDIC_MODULE_ENABLED */

#ifdef HAL_I2S_MODULE_ENABLED
#include "py32e407_hal_i2s.h"
#endif /* HAL_I2S_MODULE_ENABLED */

#ifdef HAL_TIM_MODULE_ENABLED
#include "py32e407_hal_tim.h"
#endif /* HAL_TIM_MODULE_ENABLED */

#ifdef HAL_LPTIM_MODULE_ENABLED
#include "py32e407_hal_lptim.h"
#endif /* HAL_LPTIM_MODULE_ENABLED */

#ifdef HAL_RTC_MODULE_ENABLED
#include "py32e407_hal_rtc.h"
#endif /* HAL_RTC_MODULE_ENABLED */

#ifdef HAL_COMP_MODULE_ENABLED
#include "py32e407_hal_comp.h"
#endif /* HAL_COMP_MODULE_ENABLED */

#ifdef HAL_OPA_MODULE_ENABLED
#include "py32e407_hal_opa.h"
#endif /* HAL_OPA_MODULE_ENABLED */

#ifdef HAL_CTC_MODULE_ENABLED
#include "py32e407_hal_ctc.h"
#endif /* HAL_CTC_MODULE_ENABLED */

#ifdef HAL_IWDG_MODULE_ENABLED
#include "py32e407_hal_iwdg.h"
#endif /* HAL_IWDG_MODULE_ENABLED */

#ifdef HAL_WWDG_MODULE_ENABLED
#include "py32e407_hal_wwdg.h"
#endif /* HAL_WWDG_MODULE_ENABLED */

#ifdef HAL_LCD_MODULE_ENABLED
#include "py32e407_hal_lcd.h"
#endif /* HAL_LCD_MODULE_ENABLED */

#ifdef HAL_LCDC_MODULE_ENABLED
#include "py32e407_hal_lcdc.h"
#endif /* HAL_LCDC_MODULE_ENABLED */

#ifdef HAL_CRC_MODULE_ENABLED
#include "py32e407_hal_crc.h"
#endif /* HAL_CRC_MODULE_ENABLED */

#ifdef HAL_RNG_MODULE_ENABLED
#include "py32e407_hal_rng.h"
#endif /* HAL_RNG_MODULE_ENABLED */

#ifdef HAL_DIV_MODULE_ENABLED
#include "py32e407_hal_div.h"
#endif /* HAL_DIV_MODULE_ENABLED */

#ifdef HAL_FDCAN_MODULE_ENABLED
#include "py32e407_hal_fdcan.h"
#endif /* HAL_FDCAN_MODULE_ENABLED */

#ifdef HAL_DAC_MODULE_ENABLED
#include "py32e407_hal_dac.h"
#endif /* HAL_DAC_MODULE_ENABLED */

#ifdef HAL_SD_MODULE_ENABLED
#include "py32e407_hal_sd.h"
#endif /* HAL_SD_MODULE_ENABLED */

#ifdef HAL_ESMC_MODULE_ENABLED
#include "py32e407_hal_esmc.h"
#endif /* HAL_ESMC_MODULE_ENABLED */

#ifdef HAL_USBH_MODULE_ENABLED
#include "py32e407_hal_usbh.h"
#endif /* HAL_USBH_MODULE_ENABLED */

#ifdef HAL_USBD_MODULE_ENABLED
#include "py32e407_hal_usbd.h"
#endif /* HAL_USBD_MODULE_ENABLED */

#ifdef HAL_ETH_MODULE_ENABLED
#include "py32e407_hal_eth.h"
#endif /* HAL_ETH_MODULE_ENABLED */

/* Exported macro ------------------------------------------------------------*/
#ifdef  USE_FULL_ASSERT
/**
 * @brief  The assert_param macro is used for function's parameters check.
 * @param  expr: If expr is false, it calls assert_failed function
 *         which reports the name of the source file and the source
 *         line number of the call that failed.
 *         If expr is true, it returns no value.
 * @retval None
 */
#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
/* Exported functions ------------------------------------------------------- */
void assert_failed(uint8_t* file, uint32_t line);
#else
#define assert_param(expr) ((void)0U)
#endif /* USE_FULL_ASSERT */

#ifdef __cplusplus
}
#endif

#endif  /* __PY32E407_HAL_CONF_HAL_CONF_H */
