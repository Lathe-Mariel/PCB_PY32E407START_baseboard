/**
  ******************************************************************************
  * @file    platform_pcm.h
  * @author  MCU Application Team
  * @brief   platform.wav as unsigned 8 bit PCM (16 kHz mono), for the piezo
  *          buzzer player (see tools/wav2pcm.py).
  ******************************************************************************
  * @attention
  *
  * Data was processed offline: equalised (6 band, speech tuning) and level
  * compressed, then peak normalised. See tools/wav2pcm.py --eq --agc.
  *
  * PLATFORM_PCM_LENGTH is declared here (the definition lives in
  * platform_pcm.c, derived with sizeof from the unsized array, so the count
  * can never disagree with the actual data).
  *
  * This software component is licensed under BSD 3-Clause license.
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __PLATFORM_PCM_H
#define __PLATFORM_PCM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

/* Exported constants --------------------------------------------------------*/
/**
  * @brief  Playback rate of the PCM data, in Hz.
  */
#define PLATFORM_PCM_RATE_HZ        16000U

/* Exported variables prototypes ---------------------------------------------*/
extern const unsigned char PLATFORM_PCM_SAMPLES[];
extern const unsigned int  PLATFORM_PCM_LENGTH;

#ifdef __cplusplus
}
#endif

#endif /* __PLATFORM_PCM_H */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/