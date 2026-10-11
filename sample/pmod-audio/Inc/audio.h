/**
  ******************************************************************************
  * @file    audio.h
  * @author  MCU Application Team
  * @brief   PWM audio playback (TIM2_CH2 on PA1) for the PY32E407 base board.
  ******************************************************************************
  * @attention
  *
  * This software component is licensed under BSD 3-Clause license.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __AUDIO_H
#define __AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "py32e4xx_hal.h"

/* Exported functions prototypes ---------------------------------------------*/
/**
  * @brief  Sample timing interrupt body, called from TIM3_IRQHandler().
  */
void AUDIO_TimerIRQHandler(void);

void AUDIO_Init(void);

/* Sample (PCM) playback: the PWM duty follows the sample value. */
void AUDIO_PlaySample(const uint8_t *samples, uint32_t length, uint32_t rate_hz);
void AUDIO_StopSample(void);
uint8_t AUDIO_IsPlayingSample(void);

#ifdef __cplusplus
}
#endif

#endif /* __AUDIO_H */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
