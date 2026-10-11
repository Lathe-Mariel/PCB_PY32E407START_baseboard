/**
  ******************************************************************************
  * @file    melody_wav.h
  * @author  MCU Application Team
  * @brief   Melody.wav converted to 8 bit unsigned PCM, mono.
  *
  *          Define AUDIO_MELODY_SMALL to use the 8 kHz reduced-size data;
  *          otherwise the 16 kHz full-size data is selected.
  *          Sample value 128 is silence, which maps to a 50 % PWM duty.
  ******************************************************************************
  */

#ifndef __MELODY_WAV_H
#define __MELODY_WAV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define MELODY_WAV_FULL_RATE_HZ   16000U
#define MELODY_WAV_FULL_LENGTH    47519U
#define MELODY_WAV_SMALL_RATE_HZ   8000U
#define MELODY_WAV_SMALL_LENGTH   23760U

extern const uint8_t MELODY_WAV_FULL_SAMPLES[MELODY_WAV_FULL_LENGTH];
extern const uint8_t MELODY_WAV_S_SAMPLES[MELODY_WAV_SMALL_LENGTH];

#if defined(AUDIO_MELODY_SMALL)
  #define MELODY_WAV_RATE_HZ  MELODY_WAV_SMALL_RATE_HZ
  #define MELODY_WAV_LENGTH   MELODY_WAV_SMALL_LENGTH
  #define MELODY_WAV_SAMPLES  MELODY_WAV_S_SAMPLES
#else
  #define MELODY_WAV_RATE_HZ  MELODY_WAV_FULL_RATE_HZ
  #define MELODY_WAV_LENGTH   MELODY_WAV_FULL_LENGTH
  #define MELODY_WAV_SAMPLES  MELODY_WAV_FULL_SAMPLES
#endif

#ifdef __cplusplus
}
#endif

#endif /* __MELODY_WAV_H */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
