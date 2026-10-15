/**
  ******************************************************************************
  * @file    music.h
  * @author  MCU Application Team
  * @brief   PWM-DAC player with 6-band equaliser (TIM1_CH2/CH2N,
  *          pins PA9/PA12) for the PY32E407 base board.
  ******************************************************************************
  * @attention
  *
  * This software component is licensed under BSD 3-Clause license.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MUSIC_H
#define __MUSIC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "py32e4xx_hal.h"

/* Exported types ------------------------------------------------------------*/
/**
  * @brief  EQ band types.
  */
typedef enum
{
  MUSIC_EQ_TYPE_LOWSHELF  = 0,
  MUSIC_EQ_TYPE_PEAKING   = 1,
  MUSIC_EQ_TYPE_HIGHSHELF = 2,
} MUSIC_EqType;

/**
  * @brief  Definition of one EQ band.
  */
typedef struct
{
  MUSIC_EqType type;
  uint32_t     freq_hz;
  double       q;
  int8_t       gain_db;
} MUSIC_EqBandDef;

/**
  * @brief  Number of EQ bands of the runtime equaliser.
  */
#define MUSIC_EQ_BANDS                6U

/**
  * @brief  Volume applied at start-up, in percent of the offline level.
  *
  *         The offline data is peak-normalised, but speech spends most of
  *         its time far below that peak (crest factor ~12 dB), so the plain
  *         playback is quiet. A makeup gain in front of the soft limiter
  *         turns that headroom into loudness: 200 % is about +6 dB of
  *         average level, with the peaks smoothly limited instead of
  *         hard-clipped. MUSIC_SetVolume() can change it at runtime.
  *
  *         NOTE: 200 % is the level confirmed audible on the board. 100 %
  *         (the untouched offline level, crest factor ~12 dB) is far too
  *         quiet on the piezo.
  */
#define MUSIC_DEFAULT_VOLUME_PCT      120U

/**
  * @brief  Default band layout of the speech equaliser (16 kHz sample rate).
  *
  *         Same layout as tools/wav2pcm.py, so offline and runtime EQ stack
  *         up to roughly double the tuned dB in every band. Edit either one
  *         to taste; regenerating the PCM is only needed for the offline one.
  */
static const MUSIC_EqBandDef MUSIC_EQ_BAND_DEFS[MUSIC_EQ_BANDS] =
{
  { MUSIC_EQ_TYPE_LOWSHELF,    200U,   0.7, -6 },  /* PWM-wasting rumble */
  { MUSIC_EQ_TYPE_PEAKING,     300U,   1.0, +3 },  /* vowel body         */
  { MUSIC_EQ_TYPE_PEAKING,     900U,   1.0, +2 },  /* vowel body upper   */
  { MUSIC_EQ_TYPE_PEAKING,    2500U,   1.0, +5 },  /* consonants         */
  { MUSIC_EQ_TYPE_PEAKING,    3500U,   1.0, +3 },  /* presence           */
  { MUSIC_EQ_TYPE_HIGHSHELF, 6000U,   0.7, -6 },  /* harsh hiss         */
};

/* Exported functions prototypes ---------------------------------------------*/
/**
  * @brief  Initialise the complementary PWM output (PA9 = TIM1_CH2,
  *         PA12 = TIM1_CH2N) and the sample pacing timer. Builds the EQ.
  */
void MUSIC_Init(void);

/**
  * @brief  Start (or restart) playing the built-in PCM sample.
  */
void MUSIC_Play(void);

/**
  * @brief  Bring-up self-test, independent of the EQ, the sample data and
  *         the pacing timer. Runs ~3 s:
  *         1. ~1 s constant 50 % duty: plain 62.5 kHz carrier on PA9/PA12
  *            (scope-visible).
  *         2. ~2 s of 500 Hz beeps: duty switched between 35 % and 65 %
  *            every millisecond - an audible square wave that proves the
  *            whole output chain (timer, both pins, piezo) by itself.
  */
void MUSIC_SelfTest(void);

/**
  * @brief  Stop playback and mute both outputs.
  */
void MUSIC_Stop(void);

/**
  * @brief  Tell whether the sample is currently playing.
  * @retval 1 if playing, 0 otherwise.
  */
uint8_t MUSIC_IsPlaying(void);

/**
  * @brief  Change the gain of one runtime EQ band (live tweak).
  * @param  band    0..MUSIC_EQ_BANDS-1.
  * @param  gain_db -12..+12 dB.
  */
void MUSIC_EqSetBand(uint32_t band, int8_t gain_db);

/**
  * @brief  Set the playback volume in percent of the offline level.
  *         100 = normalised level, 200 = about +6 dB louder. Above 400 %
  *         the value is clamped; peaks are soft-limited, not clipped.
  */
void MUSIC_SetVolume(uint32_t volume_pct);

/**
  * @brief  Enable/disable the runtime EQ (0 = dry samples).
  */
void MUSIC_EqEnable(uint8_t enable);

/**
  * @brief  Interrupt handler body for the sample pacing timer (TIM2 update),
  *         called from TIM2_IRQHandler().
  */
void MUSIC_NoteTimerIRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MUSIC_H */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/