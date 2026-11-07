/**
  ******************************************************************************
  * @file    slcan.h
  * @brief   SLCAN (LAWICEL) bridge between a PC serial port and FDCAN1.
  ******************************************************************************
  */

#ifndef __SLCAN_H
#define __SLCAN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Exported functions prototypes ---------------------------------------------*/

/**
  * @brief  Initialize the SLCAN UART (USART1) and the CAN bit timing.
  *         The FDCAN1 peripheral is initialized but stays closed until the
  *         host sends the 'O' (open) command.
  */
void SLCAN_Init(void);

/**
  * @brief  Process pending UART commands and forward received CAN frames.
  *         Must be called continuously from the main loop.
  */
void SLCAN_Poll(void);

#ifdef __cplusplus
}
#endif

#endif /* __SLCAN_H */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
