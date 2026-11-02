/**
  ******************************************************************************
  * @file    py32e407_it.c
  * @author  MCU Application Team
  * @brief   Interrupt Service Routines.
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
#include "py32e407_it.h"

/* Private includes ----------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private user code ---------------------------------------------------------*/
/* External variables --------------------------------------------------------*/
extern uint8_t CanRxData[8];
extern __IO uint32_t CanRxCounter;

/******************************************************************************/
/*          Cortex-M4 Processor Interruption and Exception Handlers           */
/******************************************************************************/
/**
  * @brief   This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void)
{
}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
void SysTick_Handler(void)
{
  HAL_IncTick();
}

/******************************************************************************/
/* PY32E407 Peripheral Interrupt Handlers                                     */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file.                                          */
/******************************************************************************/

/**
  * @brief This function handles FDCAN1 interrupt 0 (FDCAN_INT0).
  */
void CAN1_INT0_IRQHandler(void)
{
  HAL_FDCAN_IRQHandler(&FdcanHandle);
}

/**
  * @brief  Rx FIFO 0 new message callback.
  *         Echoes every received frame back with the same identifier.
  * @param  hfdcan: FDCAN handle pointer
  * @param  RxFifo0ITs: the enabled Rx FIFO 0 interrupts
  * @retval None
  */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
  FDCAN_RxHeaderTypeDef RxHeader;
  FDCAN_TxHeaderTypeDef TxHeader;

  if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0U)
  {
    /* Read the pending message out of Rx FIFO 0 */
    if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &RxHeader, CanRxData) == HAL_OK)
    {
      CanRxCounter++;

      /* Echo the frame back with the same identifier and payload */
      TxHeader.Identifier          = RxHeader.Identifier;
      TxHeader.IdType              = RxHeader.IdType;
      TxHeader.TxFrameType         = FDCAN_DATA_FRAME;
      TxHeader.DataLength          = RxHeader.DataLength;
      TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
      TxHeader.BitRateSwitch       = FDCAN_BRS_OFF;
      TxHeader.FDFormat            = FDCAN_CLASSIC_CAN;
      TxHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
      TxHeader.MessageMarker       = 0U;
      (void)HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, &TxHeader, CanRxData);

      /* Signal the reception on LED2 */
      HAL_GPIO_TogglePin(LED_PORT, LED_D2_PIN);
    }
  }
}

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
