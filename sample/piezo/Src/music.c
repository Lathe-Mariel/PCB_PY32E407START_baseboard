/**
  ******************************************************************************
  * @file    music.c
  * @author  MCU Application Team
  * @brief   PWM-DAC speech/music player with 6-band equaliser
  *          (TIM1_CH2/CH2N complementary PWM, pins PA9/PA12).
  ******************************************************************************
  * @attention
  *
  * Playback chain
  * --------------
  *   platform_pcm.c (8 bit PCM, 16 kHz, EQ'd offline by tools/wav2pcm.py)
  *        |
  *   [runtime 6-band EQ, RBJ biquads in Q15]     <- MUSIC_EqSetBand()
  *        |
  *   [PWM DAC] TIM1_CH2/CH2N, carrier ~62.5 kHz, duty = sample
  *        |
  *   piezo buzzer between PA9 and PA12 (differential drive = 2x swing)
  *
  * Two EQ layers cooperate:
  *  - the offline EQ (tools/wav2pcm.py --eq) does the heavy, static speech
  *    shaping while generating the data;
  *  - the runtime EQ below refines it on the target without regenerating
  *    anything. Both use the same band layout (MUSIC_EQ_BAND_DEFS in
  *    music.h): -6 dB shelf below 200 Hz, +3/+2 dB vowel body at 300/900 Hz,
  *    +5/+3 dB consonants/presence at 2.5/3.5 kHz, -6 dB shelf above 6 kHz.
  *
  * Sample pacing: TIM2 fires one update interrupt per PCM sample at
  * PLATFORM_PCM_RATE_HZ (16 kHz). The ISR runs the 6 biquads on one sample
  * (42 int32 MACs) and latches the duty - comfortably inside the 64 us
  * sample period at 16 MHz.
  *
  * Pin mapping: PA9 = TIM1_CH2, PA12 = TIM1_CH2N (AF6). No dead time is set:
  * the complementary pair here forms a differential drive of one piezo
  * element, not a half bridge.
  ******************************************************************************
  * This software component is licensed under BSD 3-Clause license.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "music.h"
#include "platform_pcm.h"

#include <math.h>

/* Private define ------------------------------------------------------------*/
/**
  * @brief  Peripherals used by the player.
  *
  *         TIM1: APB2, PWM DAC carrier on PA9/PA12.
  *         TIM2: APB1, paces the PCM samples.
  */
#define MUSIC_PWM_TIM                 TIM1
#define MUSIC_NOTE_TIM                TIM2
#define MUSIC_NOTE_IRQn               TIM2_IRQn

#define MUSIC_PWM_TIM_CLK_ENABLE()    __HAL_RCC_TIM1_CLK_ENABLE()
#define MUSIC_NOTE_TIM_CLK_ENABLE()   __HAL_RCC_TIM2_CLK_ENABLE()

/**
  * @brief  GPIO used by the buzzer: PA9 = TIM1_CH2, PA12 = TIM1_CH2N (AF6).
  */
#define MUSIC_GPIO_PORT               GPIOA
#define MUSIC_GPIO_PIN_CH2            GPIO_PIN_9
#define MUSIC_GPIO_PIN_CH2N           GPIO_PIN_12
#define MUSIC_GPIO_AF                 GPIO_AF6_TIM1
#define MUSIC_GPIO_CLK_ENABLE()       __HAL_RCC_GPIOA_CLK_ENABLE()

/**
  * @brief  PWM period in ticks for sample playback.
  *
  *         256 ticks = exact 8 bit duty resolution (one step per sample
  *         quantum). The carrier is then timer_clk/256; at 16 MHz that is
  *         62.5 kHz, far above the audio band, so the carrier is inaudible.
  */
#define MUSIC_PWM_PERIOD              256U
#define MUSIC_DUTY_MAX                (MUSIC_PWM_PERIOD - 1U)

/**
  * @brief  Duty span, in percent of the full 8 bit range.
  *
  *         100 % uses the whole PWM range (loudest); the piezo + its direct
  *         drive handle the full swing.
  */
#define MUSIC_DUTY_SPAN_PCT           100U

/* Private types -------------------------------------------------------------*/
/**
  * @brief  One RBJ biquad (Q15 coefficients) plus its state, for one EQ band.
  */
typedef struct
{
  int32_t b0, b1, b2, a1, a2;          /* Q15 coefficients                   */
  int32_t x1, x2, y1, y2;              /* filter state in Q15                */
} MUSIC_Biquad;

/* Private variables ---------------------------------------------------------*/
static TIM_HandleTypeDef s_pwm;      /* TIM1: PWM DAC carrier               */
static TIM_HandleTypeDef s_note;     /* TIM2: sample pacing                 */

static uint32_t s_pwm_timer_clk_hz;  /* raw clock feeding TIM1              */
static uint32_t s_note_timer_clk_hz; /* raw clock feeding TIM2              */

static volatile uint8_t s_playing;   /* 1 while the sample is playing       */

/* PCM playback state                                                         */
static const uint8_t *s_pcm;         /* sample buffer being played          */
static uint32_t s_pcm_length;        /* number of samples                   */
static uint32_t s_pcm_index;         /* next sample to output               */

/* Runtime equaliser: one biquad per band, cascaded                            */
static MUSIC_Biquad s_eq[MUSIC_EQ_BANDS];
static uint8_t      s_eq_enabled = 1U;

/**
  * @brief  Volume factor in Q15 (32768 = 100 % = the offline normalised
  *         level). Set from MUSIC_DEFAULT_VOLUME_PCT and changeable at
  *         runtime through MUSIC_SetVolume().
  */
static int32_t s_volume_q15 = (MUSIC_DEFAULT_VOLUME_PCT * 32768) / 100;

/* Private function prototypes -----------------------------------------------*/
static void MUSIC_GpioConfig(void);
static void MUSIC_SetSample(uint8_t value);
static void MUSIC_Mute(void);
static int32_t MUSIC_SoftLimit(int32_t s);

static void MUSIC_EqCoeffs(uint32_t band, const MUSIC_EqBandDef *def);
static int32_t MUSIC_EqSample(int32_t s);

static uint32_t MUSIC_GetApb2TimerClock(uint32_t pclk2);
static uint32_t MUSIC_GetApb1TimerClock(uint32_t pclk1);

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialise the buzzer PWM DAC (PA9/PA12, TIM1 CH2/CH2N) and the
  *         sample pacing timer. Builds the default 6-band EQ. The output
  *         starts muted.
  * @retval None
  */
void MUSIC_Init(void)
{
  TIM_OC_InitTypeDef oc = {0};
  TIM_BreakDeadTimeConfigTypeDef bd = {0};
  uint32_t band;

  /* Derive the timer settings from the clocks that actually feed them, so
     the carrier stays correct if the clock tree changes. */
  s_pwm_timer_clk_hz  = MUSIC_GetApb2TimerClock(HAL_RCC_GetPCLK2Freq());
  s_note_timer_clk_hz = MUSIC_GetApb1TimerClock(HAL_RCC_GetPCLK1Freq());

  /* TIM1 time base: prescaler 1, 256-count period (8 bit PWM DAC). */
  s_pwm.Instance               = MUSIC_PWM_TIM;
  s_pwm.Init.Prescaler         = 0U;
  s_pwm.Init.CounterMode       = TIM_COUNTERMODE_UP;
  s_pwm.Init.Period            = MUSIC_PWM_PERIOD - 1U;
  s_pwm.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  s_pwm.Init.RepetitionCounter = 0U;
  s_pwm.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&s_pwm) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  /* PWM1 mode with complementary output. Both outputs are active high, so
     CH2N is the exact complement of CH2: the two buzzer terminals see
     opposite phases (differential drive). The duty changes per sample. */
  oc.OCMode       = TIM_OCMODE_PWM1;
  oc.Pulse        = 0U;                             /* 0 % duty = silence    */
  oc.OCPolarity   = TIM_OCPOLARITY_HIGH;
  oc.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
  oc.OCFastMode   = TIM_OCFAST_DISABLE;
  oc.OCIdleState  = TIM_OCIDLESTATE_RESET;
  oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&s_pwm, &oc, TIM_CHANNEL_2) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  /* No break input, no dead time (a piezo has no shoot-through path). MOE is
     required to release CH2/CH2N at all; OSSR/OSSI keep the outputs
     timer-controlled while muted so the two terminals sit low. */
  bd.OffStateRunMode  = TIM_OSSR_ENABLE;
  bd.OffStateIDLEMode = TIM_OSSI_ENABLE;
  bd.LockLevel        = TIM_LOCKLEVEL_OFF;
  bd.DeadTime         = 0U;
  bd.BreakState       = TIM_BREAK_DISABLE;
  bd.BreakPolarity    = TIM_BREAKPOLARITY_LOW;
  bd.BreakFilter      = 0U;
  bd.AutomaticOutput  = TIM_AUTOMATICOUTPUT_DISABLE;
  bd.Break2State      = TIM_BREAK2_DISABLE;
  bd.Break2Polarity   = TIM_BREAK2POLARITY_LOW;
  bd.Break2Filter     = 0U;
  if (HAL_TIMEx_ConfigBreakDeadTime(&s_pwm, &bd) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  /* Sample pacing: prescaler/period are set in MUSIC_Play() since they
     depend on the rate; the values below are a safe placeholder. */
  s_note.Instance               = MUSIC_NOTE_TIM;
  s_note.Init.Prescaler         = 0U;
  s_note.Init.CounterMode       = TIM_COUNTERMODE_UP;
  s_note.Init.Period            = 15U;
  s_note.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  s_note.Init.RepetitionCounter = 0U;
  s_note.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&s_note) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  HAL_NVIC_SetPriority(MUSIC_NOTE_IRQn, 3U, 0U);
  HAL_NVIC_EnableIRQ(MUSIC_NOTE_IRQn);

  MUSIC_GpioConfig();

  /* Build the runtime EQ with the default speech tuning. */
  for (band = 0U; band < MUSIC_EQ_BANDS; band++)
  {
    MUSIC_EqCoeffs(band, &MUSIC_EQ_BAND_DEFS[band]);
  }
  MUSIC_EqEnable(1U);
}

/**
  * @brief  Start (or restart) the playback of the built-in PCM sample.
  *
  *         The buffer holds unsigned 8 bit samples where 128 is silence.
  *         The duty cycle of the PWM follows the (equalised) sample value,
  *         so the piezo driven between PA9 and PA12 reproduces the waveform.
  * @retval None
  */
void MUSIC_Play(void)
{
  uint64_t clk;
  uint64_t ideal;
  uint32_t guess;
  uint32_t best_pre = 1U;
  uint32_t best_arr = 1U;
  uint64_t best_err = UINT64_MAX;
  uint32_t lo, hi, i;

  s_pcm        = PLATFORM_PCM_SAMPLES;
  s_pcm_length = PLATFORM_PCM_LENGTH;
  s_pcm_index  = 0U;
  s_playing    = 1U;

  /* Carrier: prescaler 0, period 256 ticks (8 bit duty resolution). */
  __HAL_TIM_SET_PRESCALER(&s_pwm, 0U);
  __HAL_TIM_SET_AUTORELOAD(&s_pwm, MUSIC_PWM_PERIOD - 1U);
  HAL_TIM_GenerateEvent(&s_pwm, TIM_EVENTSOURCE_UPDATE);
  __HAL_TIM_CLEAR_FLAG(&s_pwm, TIM_FLAG_UPDATE);

  /* Pace timer: pick the prescaler/period pair with the smallest rate
     error, like the pmod-audio sample does. */
  clk   = s_note_timer_clk_hz;
  ideal = clk / PLATFORM_PCM_RATE_HZ;

  guess = 1U;
  while ((clk / ((uint64_t)guess * PLATFORM_PCM_RATE_HZ)) > 65536ULL)
  {
    guess++;
  }

  lo = guess;
  hi = guess + 8U;
  for (i = lo; i <= hi; i++)
  {
    uint64_t ticks = clk / i;
    uint64_t got, diff, err;
    uint32_t arr;

    if (ticks == 0ULL)
    {
      continue;
    }

    arr = (uint32_t)((ticks + (PLATFORM_PCM_RATE_HZ / 2U)) / PLATFORM_PCM_RATE_HZ);
    if ((arr == 0U) || (arr > 65536U))
    {
      continue;
    }

    got  = (uint64_t)i * arr;
    diff = (got > ideal) ? (got - ideal) : (ideal - got);
    err  = (diff * 1000000ULL) / (ideal ? ideal : 1ULL);
    if (err < best_err)
    {
      best_err = err;
      best_pre = i;
      best_arr = arr;
      if (err == 0ULL)
      {
        break;
      }
    }
  }

  __HAL_TIM_DISABLE(&s_note);
  __HAL_TIM_SET_PRESCALER(&s_note, best_pre - 1U);
  __HAL_TIM_SET_AUTORELOAD(&s_note, best_arr - 1U);
  __HAL_TIM_SET_COUNTER(&s_note, 0U);
  __HAL_TIM_CLEAR_FLAG(&s_note, TIM_FLAG_UPDATE);

  /* Enable both outputs (CC2E + CC2NE), the main output (MOE) and the
     counter (CEN) with direct register writes: the HAL start/stop functions
     manage a channel state machine that cannot run twice in a row without
     an interleaved stop, and this path runs again from the repeat loop. */
  s_pwm.Instance->CCER |= (TIM_CCER_CC2E | TIM_CCER_CC2NE);
  __HAL_TIM_MOE_ENABLE(&s_pwm);
  __HAL_TIM_ENABLE(&s_pwm);

  __HAL_TIM_ENABLE(&s_note);
  __HAL_TIM_ENABLE_IT(&s_note, TIM_IT_UPDATE);

  /* Release the first sample right away so playback starts immediately. */
  MUSIC_SetSample(s_pcm[0]);
  s_pcm_index = 1U;
}

/**
  * @brief  Stop playback and mute the output.
  * @retval None
  */
void MUSIC_Stop(void)
{
  __HAL_TIM_DISABLE_IT(&s_note, TIM_IT_UPDATE);
  __HAL_TIM_DISABLE(&s_note);
  __HAL_TIM_CLEAR_FLAG(&s_note, TIM_FLAG_UPDATE);

  s_playing = 0U;
  MUSIC_Mute();
}

/**
  * @brief  Tell whether a sample is currently being played.
  * @retval 1 if playing, 0 otherwise.
  */
uint8_t MUSIC_IsPlaying(void)
{
  return s_playing;
}

/**
  * @brief  Set the gain of one runtime EQ band (live tweak on the target).
  * @param  band    band index 0..MUSIC_EQ_BANDS-1.
  * @param  gain_db gain in dB, clamped to -12..+12.
  * @retval None
  */
void MUSIC_EqSetBand(uint32_t band, int8_t gain_db)
{
  MUSIC_EqBandDef def;

  if (band >= MUSIC_EQ_BANDS)
  {
    return;
  }

  def = MUSIC_EQ_BAND_DEFS[band];
  if (gain_db >  12)
  {
    gain_db =  12;
  }
  if (gain_db < -12)
  {
    gain_db = -12;
  }

  def.gain_db = gain_db;
  MUSIC_EqCoeffs(band, &def);
}

/**
  * @brief  Set the playback volume.
  *
  *         100 % plays the data at its offline-normalised level. Values
  *         above 100 % apply a makeup gain; the soft limiter afterwards
  *         keeps peaks from hard-clipping, so louder does not sound
  *         distorted.
  * @param  volume_pct gain in percent: 100 = normalised level,
  *         200 = about +6 dB louder. Clamped to 1..400.
  * @retval None
  */
void MUSIC_SetVolume(uint32_t volume_pct)
{
  if (volume_pct == 0U)
  {
    volume_pct = 1U;
  }
  if (volume_pct > 400U)
  {
    volume_pct = 400U;
  }

  s_volume_q15 = (int32_t)((volume_pct * 32768U) / 100U);
}

/**
  * @brief  Enable or disable the runtime EQ (0 = dry samples).
  * @param  enable 1 to process, 0 to bypass.
  * @retval None
  */
void MUSIC_EqEnable(uint8_t enable)
{
  uint32_t band;

  s_eq_enabled = enable ? 1U : 0U;

  /* A mode change invalidates the filter state. */
  for (band = 0U; band < MUSIC_EQ_BANDS; band++)
  {
    s_eq[band].x1 = 0;
    s_eq[band].x2 = 0;
    s_eq[band].y1 = 0;
    s_eq[band].y2 = 0;
  }
}

/**
  * @brief  Bring-up self-test: drive the buzzer from raw timer duty cycles,
  *         without the EQ, the sample data or the pacing timer.
  * @retval None
  */
void MUSIC_SelfTest(void)
{
  uint32_t k;

  /* Enable both outputs, the main output and the counter - the same line
     MUSIC_Play() uses; without it the pins would stay high-impedance. */
  s_pwm.Instance->CCER |= (TIM_CCER_CC2E | TIM_CCER_CC2NE);
  __HAL_TIM_MOE_ENABLE(&s_pwm);
  __HAL_TIM_ENABLE(&s_pwm);

  /* Phase 1: fix 50 % duty for a second - the plain carrier. On a scope
     PA9 shows 62.5 kHz PWM and PA12 its complement. */
  __HAL_TIM_SET_COMPARE(&s_pwm, TIM_CHANNEL_2, 128U);
  HAL_Delay(1000U);

  /* Phase 2: switch the duty between ~35 % and ~65 % every millisecond.
     Through the PWM DAC that is a 500 Hz square wave - a clearly audible
     beep that involves none of the EQ / sample / TIM2 machinery. */
  for (k = 0U; k < 2000U; k++)
  {
    __HAL_TIM_SET_COMPARE(&s_pwm, TIM_CHANNEL_2, ((k & 1U) != 0U) ? 88U : 168U);
    HAL_Delay(1U);
  }

  /* Leave at centre duty; real playback takes over next. */
  __HAL_TIM_SET_COMPARE(&s_pwm, TIM_CHANNEL_2, 128U);
}

/**
  * @brief  Interrupt handler body for the sample pacing timer.
  *
  *         Called from TIM2_IRQHandler(), see py32e407_it.c. One update
  *         interrupt per PCM sample: the ISR runs the runtime EQ on the
  *         sample and latches the new duty. Sized comfortably inside the
  *         64 us sample period at 16 MHz (42 int32 MACs + writes).
  * @retval None
  */
void MUSIC_NoteTimerIRQHandler(void)
{
  if (__HAL_TIM_GET_FLAG(&s_note, TIM_FLAG_UPDATE) == RESET)
  {
    return;
  }
  __HAL_TIM_CLEAR_FLAG(&s_note, TIM_FLAG_UPDATE);

  if (s_playing == 0U)
  {
    return;
  }

  if (s_pcm_index >= s_pcm_length)
  {
    /* End of the sample: stop here, muting the buzzer and halting the pace
       timer. The main loop waits for MUSIC_IsPlaying() == 0, pauses, then
       calls MUSIC_Play() to repeat. */
    MUSIC_Stop();
    return;
  }

  MUSIC_SetSample(s_pcm[s_pcm_index]);
  s_pcm_index++;
}

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Configure PA9/PA12 as TIM1_CH2/CH2N alternate function outputs.
  * @retval None
  */
static void MUSIC_GpioConfig(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  MUSIC_GPIO_CLK_ENABLE();

  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = MUSIC_GPIO_AF;

  /* PA9 = TIM1_CH2, buzzer terminal A */
  GPIO_InitStruct.Pin = MUSIC_GPIO_PIN_CH2;
  HAL_GPIO_Init(MUSIC_GPIO_PORT, &GPIO_InitStruct);

  /* PA12 = TIM1_CH2N, buzzer terminal B */
  GPIO_InitStruct.Pin = MUSIC_GPIO_PIN_CH2N;
  HAL_GPIO_Init(MUSIC_GPIO_PORT, &GPIO_InitStruct);
}

/**
  * @brief  Output one PCM sample (optionally through the runtime EQ).
  *
  *         Signal path: EQ -> volume makeup gain -> soft limiter -> duty.
  *         The sample is centred on 128 (silence); the duty map puts the
  *         value onto the full 8 bit span of the 256-count period.
  * @param  value unsigned 8 bit sample, 128 = silence.
  * @retval None
  */
static void MUSIC_SetSample(uint8_t value)
{
  int32_t duty;
  int32_t s;

  /* signed sample in -128..+127, scaled to Q15 */
  s = ((int32_t)value - 128) << 8;

  if (s_eq_enabled)
  {
    s = MUSIC_EqSample(s);
  }

  /* Volume makeup gain (Q15 multiply), then the soft limiter keeps the
     peaks from hard-clipping when the gain pushes them past full scale. */
  s = (int32_t)(((int64_t)s * s_volume_q15) >> 15);
  s = MUSIC_SoftLimit(s);

  /* back to the -128..+127 range with rounding and saturation */
  s = (s + (1 << 7)) >> 8;
  if (s >  127)
  {
    s =  127;
  }
  if (s < -128)
  {
    s = -128;
  }

  /* Map the 0..255 sample onto the duty span around 0..MUSIC_DUTY_MAX. */
  duty = ((int32_t)(s + 128) * (int32_t)MUSIC_DUTY_MAX) / 255;

  /* CCR2 preload (OC2PE) is on, so the new value latches at the next
     update event - no phase jitter inside the carrier cycles. */
  __HAL_TIM_SET_COMPARE(&s_pwm, TIM_CHANNEL_2, (uint32_t)duty);
}

/**
  * @brief  Soft limiter: smooth saturation used as the loudness maximiser.
  *
  *         f(s) = s * (1 + K) / (1 + K * |s| / FS), s and FS in Q15:
  *           f(0) = 0, f(+-FS) = +-FS, f'(0) = 1 + K, monotonic.
  *         Quiet parts are lifted by (1 + K) while peaks bend smoothly
  *         towards full scale, so the makeup gain from MUSIC_SetVolume()
  *         comes out as loudness instead of hard-clipped mush. Only
  *         samples that still exceed FS are finally clamped.
  *
  *         Integer form with a single 64-bit division per sample - cheap
  *         enough inside the 16 kHz pacing ISR.
  * @param  s sample in Q15 (already volume-gained).
  * @retval limited sample in Q15, |y| <= FS.
  */
static int32_t MUSIC_SoftLimit(int32_t s)
{
  const int32_t FS = 32767;
  const int32_t K  = 2;

  int32_t a   = (s < 0) ? -s : s;
  int32_t y   = (int32_t)(((int64_t)s * (1 + K) * FS) /
                          (FS + (int64_t)a * K));

  if (y >  FS)
  {
    y =  FS;
  }
  if (y < -FS)
  {
    y = -FS;
  }
  return y;
}

/**
  * @brief  Drive both outputs permanently low, i.e. silence.
  * @retval None
  */
static void MUSIC_Mute(void)
{
  __HAL_TIM_SET_COMPARE(&s_pwm, TIM_CHANNEL_2, 0U);
}

/* Runtime equaliser ---------------------------------------------------------*/

/**
  * @brief  Compute the Q15 coefficients of one EQ band from its definition.
  * @param  band band index.
  * @param  def  band definition (type, freq, Q, gain dB).
  * @retval None
  */
static void MUSIC_EqCoeffs(uint32_t band, const MUSIC_EqBandDef *def)
{
  MUSIC_Biquad *b = &s_eq[band];
  double f0    = (double)def->freq_hz;
  double q     = (double)def->q;
  double gdb   = (double)def->gain_db;
  double srate = (double)PLATFORM_PCM_RATE_HZ;
  double A     = 1.0;
  double w0, cw, sw, alpha;
  double b0, b1, b2, a0, a1, a2;
  int32_t steps = (int32_t)((gdb < 0.0) ? -gdb : gdb);
  int32_t i;

  /* A = 10^(gdb/40), accumulated per-dB without libm.pow; the remainder
     below 1 dB is negligible for EQ tuning purposes. */
  for (i = 0; i < steps; i++)
  {
    A *= 1.1220184543019633;             /* 10^(1/40)                      */
  }
  if (gdb < 0.0)
  {
    A = 1.0 / A;
  }

  w0  = 6.283185307179586 * f0 / srate;
  cw  = cos(w0);
  sw  = sin(w0);

  if (def->type == MUSIC_EQ_TYPE_LOWSHELF)
  {
    /* RBJ shelf: alpha scales with cos of the corner for a shelf slope */
    alpha = sqrt((A + 1.0 / A) * (1.0 / q - 1.0) + 2.0) / 2.0;

    b0 =        A * ((A + 1) - (A - 1) * cw + 2.0 * sqrt(A) * alpha);
    b1 =    2 * A * ((A - 1) - (A + 1) * cw              );
    b2 =        A * ((A + 1) - (A - 1) * cw - 2.0 * sqrt(A) * alpha);
    a0 =           (A + 1) + (A - 1) * cw + 2.0 * sqrt(A) * alpha;
    a1 =       -2 * ((A - 1) + (A + 1) * cw              );
    a2 =           (A + 1) + (A - 1) * cw - 2.0 * sqrt(A) * alpha;
  }
  else if (def->type == MUSIC_EQ_TYPE_HIGHSHELF)
  {
    alpha = sqrt((A + 1.0 / A) * (1.0 / q - 1.0) + 2.0) / 2.0;

    b0 =        A * ((A + 1) + (A - 1) * cw + 2.0 * sqrt(A) * alpha);
    b1 =   -2 * A * ((A - 1) + (A + 1) * cw              );
    b2 =        A * ((A + 1) + (A - 1) * cw - 2.0 * sqrt(A) * alpha);
    a0 =           (A + 1) - (A - 1) * cw + 2.0 * sqrt(A) * alpha;
    a1 =        2 * ((A - 1) - (A + 1) * cw              );
    a2 =           (A + 1) - (A - 1) * cw - 2.0 * sqrt(A) * alpha;
  }
  else /* peaking */
  {
    alpha = sw / (2.0 * q);

    b0 = 1 + alpha * A;
    b1 = -2 * cw;
    b2 = 1 - alpha * A;
    a0 = 1 + alpha / A;
    a1 = -2 * cw;
    a2 = 1 - alpha / A;
  }

  b->b0 = (int32_t)(b0 / a0 * 32768.0);
  b->b1 = (int32_t)(b1 / a0 * 32768.0);
  b->b2 = (int32_t)(b2 / a0 * 32768.0);
  b->a1 = (int32_t)(a1 / a0 * 32768.0);
  b->a2 = (int32_t)(a2 / a0 * 32768.0);
}

/**
  * @brief  Run the 6-band cascade over one sample (Q15 in, Q15 out).
  * @param  s sample in Q15 (-32768..32767).
  * @retval filtered sample in Q15.
  */
static int32_t MUSIC_EqSample(int32_t s)
{
  uint32_t k;

  for (k = 0U; k < MUSIC_EQ_BANDS; k++)
  {
    MUSIC_Biquad *b = &s_eq[k];
    int64_t acc = (int64_t)b->b0 * s
                + (int64_t)b->b1 * b->x1
                + (int64_t)b->b2 * b->x2
                - (int64_t)b->a1 * b->y1
                - (int64_t)b->a2 * b->y2;

    int32_t y = (int32_t)(acc >> 15);

    if (y >  32767)
    {
      y =  32767;
    }
    if (y < -32768)
    {
      y = -32768;
    }

    b->x2 = b->x1;
    b->x1 = s;
    b->y2 = b->y1;
    b->y1 = y;
    s     = y;
  }

  return s;
}

/* Clock helpers -------------------------------------------------------------*/

/**
  * @brief  Return the clock that feeds the APB2 timers (TIM1).
  * @param  pclk2 PCLK2 frequency in Hz.
  * @retval Timer input clock in Hz.
  */
static uint32_t MUSIC_GetApb2TimerClock(uint32_t pclk2)
{
  if (pclk2 == 0U)
  {
    pclk2 = 16000000U;
  }

  if (((RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos) < 4U)
  {
    return pclk2;
  }

  return pclk2 * 2U;
}

/**
  * @brief  Return the clock that feeds the APB1 timers (TIM2).
  * @param  pclk1 PCLK1 frequency in Hz.
  * @retval Timer input clock in Hz.
  */
static uint32_t MUSIC_GetApb1TimerClock(uint32_t pclk1)
{
  if (pclk1 == 0U)
  {
    pclk1 = 16000000U;
  }

  if (((RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos) < 4U)
  {
    return pclk1;
  }

  return pclk1 * 2U;
}

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/