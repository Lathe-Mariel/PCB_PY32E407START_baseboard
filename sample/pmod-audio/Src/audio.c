/**
  ******************************************************************************
  * @file    audio.c
  * @author  MCU Application Team
  * @brief   PWM audio playback (TIM2_CH2 on PA1) for the PY32E407 base board.
  ******************************************************************************
  * @attention
  *
  * This software component is licensed under BSD 3-Clause license.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "audio.h"

/* Private define ------------------------------------------------------------*/
/**
  * @brief  Peripheral used to generate the audio output.
  *
  *         PA1 is wired to AF1 on the PY32E407 and AF1 selects TIM2_CH2
  *         (PY32E407 datasheet, Table 3-3 "Port A alternate functions
  *         mapping"). The pin only needs a series resistor into a small
  *         speaker, plus a simple RC low pass filter for sample playback.
  *
  *         Sample playback follows the PCM sample with the PWM duty cycle.
  *
  *         IMPORTANT - the pin CANNOT be chosen freely: PD8 in particular has no
  *         timer output at all on the PY32E407 (PY32E407 datasheet, Table 3-6:
  *         AF7 = USART3_TX, AF12 = LCDC_D13, AF13 = ETH_RMII_CRS_DV, AF15 =
  *         EVENTOUT only), so it cannot generate timer PWM. Use one of the
  *         timer-capable pins below instead:
  *
  *           pin  timer channel   board connection
  *           PA1  TIM2_CH2 (AF1)  base board: ETH_RMII_REF_CLK, not used here
  *           PD4  TIM2_CH2 (AF2)  base board: Pmod3 pin 6
  *           PD6  TIM2_CH4 (AF2)  base board: Pmod3 pin 8
  *           PD7  TIM2_CH3 (AF2)  base board: Pmod3 pin 1
  *           PE9  TIM1_CH1 (AF2)  base board: Pmod4 pin 4
  *           PE11 TIM1_CH2 (AF2)  base board: Pmod4 pin 6
  *
  *         Uncomment exactly one AUDIO_PWM_PIN_* selection below.
  */
/* #define AUDIO_PWM_PIN_PD4 */
/* #define AUDIO_PWM_PIN_PD6 */
#define AUDIO_PWM_PIN_PD7
/* #define AUDIO_PWM_PIN_PE9 */
/* #define AUDIO_PWM_PIN_PE11 */

#if   defined(AUDIO_PWM_PIN_PD4)
  #define AUDIO_PWM_TIM                TIM2
  #define AUDIO_PWM_CHANNEL            TIM_CHANNEL_2
  #define AUDIO_PWM_TIM_CLK_ENABLE()   __HAL_RCC_TIM2_CLK_ENABLE()
  #define AUDIO_PWM_GPIO_PORT          GPIOD
  #define AUDIO_PWM_GPIO_PIN           GPIO_PIN_4
  #define AUDIO_PWM_GPIO_AF            GPIO_AF2_TIM2
  #define AUDIO_PWM_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOD_CLK_ENABLE()
#elif defined(AUDIO_PWM_PIN_PD6)
  #define AUDIO_PWM_TIM                TIM2
  #define AUDIO_PWM_CHANNEL            TIM_CHANNEL_4
  #define AUDIO_PWM_TIM_CLK_ENABLE()   __HAL_RCC_TIM2_CLK_ENABLE()
  #define AUDIO_PWM_GPIO_PORT          GPIOD
  #define AUDIO_PWM_GPIO_PIN           GPIO_PIN_6
  #define AUDIO_PWM_GPIO_AF            GPIO_AF2_TIM2
  #define AUDIO_PWM_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOD_CLK_ENABLE()
#elif defined(AUDIO_PWM_PIN_PD7)
  #define AUDIO_PWM_TIM                TIM2
  #define AUDIO_PWM_CHANNEL            TIM_CHANNEL_3
  #define AUDIO_PWM_TIM_CLK_ENABLE()   __HAL_RCC_TIM2_CLK_ENABLE()
  #define AUDIO_PWM_GPIO_PORT          GPIOD
  #define AUDIO_PWM_GPIO_PIN           GPIO_PIN_7
  #define AUDIO_PWM_GPIO_AF            GPIO_AF2_TIM2
  #define AUDIO_PWM_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOD_CLK_ENABLE()
#elif defined(AUDIO_PWM_PIN_PE9)
  #define AUDIO_PWM_TIM                TIM1
  #define AUDIO_PWM_CHANNEL            TIM_CHANNEL_1
  #define AUDIO_PWM_TIM_CLK_ENABLE()   __HAL_RCC_TIM1_CLK_ENABLE()
  #define AUDIO_PWM_GPIO_PORT          GPIOE
  #define AUDIO_PWM_GPIO_PIN           GPIO_PIN_9
  #define AUDIO_PWM_GPIO_AF            GPIO_AF2_TIM1
  #define AUDIO_PWM_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOE_CLK_ENABLE()
#elif defined(AUDIO_PWM_PIN_PE11)
  #define AUDIO_PWM_TIM                TIM1
  #define AUDIO_PWM_CHANNEL            TIM_CHANNEL_2
  #define AUDIO_PWM_TIM_CLK_ENABLE()   __HAL_RCC_TIM1_CLK_ENABLE()
  #define AUDIO_PWM_GPIO_PORT          GPIOE
  #define AUDIO_PWM_GPIO_PIN           GPIO_PIN_11
  #define AUDIO_PWM_GPIO_AF            GPIO_AF2_TIM1
  #define AUDIO_PWM_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOE_CLK_ENABLE()
#else
  /* PA1 = TIM2_CH2 (AF1). */
  #define AUDIO_PWM_TIM                TIM2
  #define AUDIO_PWM_CHANNEL            TIM_CHANNEL_2
  #define AUDIO_PWM_TIM_CLK_ENABLE()   __HAL_RCC_TIM2_CLK_ENABLE()
  #define AUDIO_PWM_GPIO_PORT          GPIOA
  #define AUDIO_PWM_GPIO_PIN           GPIO_PIN_1
  #define AUDIO_PWM_GPIO_AF            GPIO_AF1_TIM2
  #define AUDIO_PWM_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOA_CLK_ENABLE()
#endif

/**
  * @brief  PWM carrier used for sample playback, in Hz.
  *
  *         Sample mode runs the counter straight from the timer clock, so the
  *         period is large and the duty cycle gets many steps: 256 counts (8
  *         bits, one step per sample value) at a 16 MHz timer clock. 62.5 kHz
  *         is far above the audio band, so the carrier itself is inaudible.
  */
#define AUDIO_SAMPLE_CARRIER_HZ     62500U

/**
  * @brief  Lowest carrier accepted for sample playback, in Hz.
  *
  *         Guards against a very slow timer clock ending up with an audible
  *         carrier.
  */
#define AUDIO_SAMPLE_MIN_CARRIER_HZ 40000U

/**
  * @brief  Duty span used for the sample playback, in percent of full scale.
  *
  *         The audio information of a PWM signal is carried in its duty cycle, so
  *         the span directly sets both the loudness and the resolution: the duty
  *         travels over span% of the timer period, i.e. over span% of the 256
  *         timer counts, which is also the number of quantisation steps that are
  *         available. A small span therefore means a quiet AND noisy output.
  *
  *         This is why the span is 100 % by default. The Pmod-AUDIO V1.2 (and the
  *         reference designs for it) drive the module from a plain 3V3 logic
  *         square wave: the PAM8403 acts as a buffer for the PWM and the speaker
  *         plus its output network recover the audio, so the carrier does not have
  *         to be attenuated and the full duty range can be used. That gives the
  *         full 8 bit resolution of the 256 count period.
  *
  *         Lower it only to reduce the volume (and accept the extra quantisation
  *         noise that comes with it), or when the output is fed to a linear
  *         amplifier that needs a smaller signal.
  */
#define AUDIO_SAMPLE_SPAN_PCT       100U

/**
  * @brief  Drive of the soft limiter applied to the samples, in percent.
  *
  *         The soft limiter lifts quiet passages more than the peaks, which makes
  *         the playback louder at the same peak level:
  *
  *             f(s) = s * (1 + k) / (1 + k * |s|)      (s in -1..1, k >= 0)
  *
  *         with f(0) = 0, f(1) = 1 and f'(0) = 1 + k. It is disabled by default,
  *         because with the span at 100 % the playback is already at full scale
  *         and any compression only costs resolution (the curve squeezes the duty
  *         values together, so fewer of the 256 steps are used, which raises the
  *         noise floor). Enable it (100..400) if the material is quiet and extra
  *         loudness matters more than the last few dB of noise.
  */
#define AUDIO_SAMPLE_COMPRESS_PCT   0U

/**
  * @brief  Peripheral used to pace the PCM samples.
  *
  *         TIM3 only runs internally and drives no pin; its update rate is the
  *         PCM sample rate.
  */
#define AUDIO_SAMPLE_TIM             TIM3
#define AUDIO_SAMPLE_IRQn            TIM3_IRQn
#define AUDIO_SAMPLE_TIM_CLK_ENABLE()  __HAL_RCC_TIM3_CLK_ENABLE()

/** Sample rate limits. */
#define AUDIO_SAMPLE_MIN_RATE_HZ    8000U
#define AUDIO_SAMPLE_MAX_RATE_HZ    48000U

/* Private variables ---------------------------------------------------------*/
static TIM_HandleTypeDef s_pwm;
static TIM_HandleTypeDef s_sample;

static uint32_t s_timer_clk_hz;                /* raw timer clock, for the pacing timer */

/* Sample (PCM) playback state. */
static const uint8_t *s_pcm;                   /* sample buffer being played     */
static uint32_t s_pcm_length;                  /* number of samples              */
static uint32_t s_pcm_index;                   /* next sample to output          */
static uint8_t  s_sample_playing;

/**
  * @brief  Compare register values for the 256 possible sample bytes.
  *
  *         The mapping from a sample to a duty count is a division, which is too
  *         slow to run in a 16 kHz interrupt at a 16 MHz core clock, so it is
  *         precomputed once per mode change. 512 bytes of RAM buy a very short
  *         interrupt handler.
  */
static uint16_t s_duty_lut[256];

/* Private function prototypes -----------------------------------------------*/
static uint32_t AUDIO_GetTimerClock(uint32_t pclk);
static void AUDIO_PwmGpioConfig(void);
static void AUDIO_PwmSampleInit(void);
static void AUDIO_ProgramPaceTimer(uint32_t rate_hz);
static void AUDIO_SetSample(uint8_t value);
static void AUDIO_Mute(void);
static void AUDIO_ResetPwm(void);
static void AUDIO_SampleTimerStop(void);

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialise the PWM output on PA1 and the audio timers.
  * @param  None
  * @retval None
  */
void AUDIO_Init(void)
{
  TIM_OC_InitTypeDef oc = {0};

  /* Derive timer settings from the clocks that actually feed them, so the
     sample rate stays correct if the system clock is changed later on. */
  s_timer_clk_hz = AUDIO_GetTimerClock(HAL_RCC_GetPCLK1Freq());

  /* Sample mode sets the carrier period before the first sample is played. */
  s_pwm.Instance               = AUDIO_PWM_TIM;
  s_pwm.Init.Prescaler         = 0U;
  s_pwm.Init.CounterMode       = TIM_COUNTERMODE_UP;
  s_pwm.Init.Period            = 255U;
  s_pwm.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  s_pwm.Init.RepetitionCounter = 0U;
  s_pwm.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&s_pwm) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  oc.OCMode       = TIM_OCMODE_PWM1;
  oc.Pulse        = 0U;                      /* 0 % duty, i.e. silence */
  oc.OCPolarity   = TIM_OCPOLARITY_HIGH;
  oc.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
  oc.OCFastMode   = TIM_OCFAST_DISABLE;
  oc.OCIdleState  = TIM_OCIDLESTATE_RESET;
  oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&s_pwm, &oc, AUDIO_PWM_CHANNEL) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  if (HAL_TIM_PWM_Start(&s_pwm, AUDIO_PWM_CHANNEL) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  /* The timer that paces the PCM samples. Its prescaler and period are set by
     AUDIO_PlaySample() because they depend on the sample rate; the values below
     are only a safe placeholder. */
  s_sample.Instance               = AUDIO_SAMPLE_TIM;
  s_sample.Init.Prescaler         = 0U;
  s_sample.Init.CounterMode       = TIM_COUNTERMODE_UP;
  s_sample.Init.Period            = 15U;
  s_sample.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  s_sample.Init.RepetitionCounter = 0U;
  s_sample.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&s_sample) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  HAL_NVIC_SetPriority(AUDIO_SAMPLE_IRQn, 3U, 0U);
  HAL_NVIC_EnableIRQ(AUDIO_SAMPLE_IRQn);
}

/* Sample (PCM) playback -----------------------------------------------------*/

/**
  * @brief  Start (or restart) the playback of a PCM sample buffer.
  *
  *         The buffer holds unsigned 8 bit samples where 128 is silence. The
  *         duty cycle of the PWM follows the sample value, so an RC low pass
  *         filter turns the output back into the original signal.
  * @param  samples pointer to the unsigned 8 bit samples.
  * @param  length  number of samples to play.
  * @param  rate_hz playback rate in Hz (8 kHz to 48 kHz).
  * @retval None
  */
void AUDIO_PlaySample(const uint8_t *samples, uint32_t length, uint32_t rate_hz)
{
  if ((samples == NULL) || (length == 0U))
  {
    AUDIO_StopSample();
    return;
  }

  if (rate_hz < AUDIO_SAMPLE_MIN_RATE_HZ)
  {
    rate_hz = AUDIO_SAMPLE_MIN_RATE_HZ;
  }
  if (rate_hz > AUDIO_SAMPLE_MAX_RATE_HZ)
  {
    rate_hz = AUDIO_SAMPLE_MAX_RATE_HZ;
  }

  __HAL_TIM_DISABLE_IT(&s_sample, TIM_IT_UPDATE);
  AUDIO_SampleTimerStop();

  s_pcm            = samples;
  s_pcm_length     = length;
  s_pcm_index      = 0U;
  s_sample_playing = 1U;

  /* Sample mode: fast carrier, duty from the sample value. */
  AUDIO_ResetPwm();
  AUDIO_PwmSampleInit();

  /* One interrupt per sample, with the period chosen as close to the exact
     sample interval as the timer clock allows. */
  AUDIO_ProgramPaceTimer(rate_hz);

  /* Release the first sample right away so the playback starts immediately. */
  AUDIO_SetSample(s_pcm[0]);
  s_pcm_index = 1U;
}

/**
  * @brief  Stop the sample playback and mute the output.
  * @param  None
  * @retval None
  */
void AUDIO_StopSample(void)
{
  __HAL_TIM_DISABLE_IT(&s_sample, TIM_IT_UPDATE);
  AUDIO_SampleTimerStop();

  s_sample_playing = 0U;
  s_pcm            = NULL;

  AUDIO_Mute();
}

/**
  * @brief  Tell whether a sample is currently being played.
  * @param  None
  * @retval 1 if playing, 0 otherwise.
  */
uint8_t AUDIO_IsPlayingSample(void)
{
  return s_sample_playing;
}

/**
  * @brief  Interrupt handler body for the audio timer.
  *
  *         Called from TIM3_IRQHandler(), see py32e407_it.c. The timer paces
  *         PCM samples at their sample rate.
  * @param  None
  * @retval None
  */
void AUDIO_TimerIRQHandler(void)
{
  if (__HAL_TIM_GET_FLAG(&s_sample, TIM_FLAG_UPDATE) == RESET)
  {
    return;
  }
  __HAL_TIM_CLEAR_FLAG(&s_sample, TIM_FLAG_UPDATE);

  if (s_sample_playing == 0U)
  {
    return;
  }

  if (s_pcm_index >= s_pcm_length)
  {
    AUDIO_StopSample();
    return;
  }

  AUDIO_SetSample(s_pcm[s_pcm_index]);
  ++s_pcm_index;
}

/* Private functions --------------------------------------------------------*/

/**
  * @brief  Return the clock that feeds the APB1 timers.
  *
  *         As on the whole PY32E4 family, the APB1 timers run at PCLK1 when the
  *         APB1 prescaler is 1 and at twice PCLK1 otherwise.
  * @param  pclk PCLK1 frequency in Hz.
  * @retval Timer input clock in Hz.
  */
static uint32_t AUDIO_GetTimerClock(uint32_t pclk)
{
  if (pclk == 0U)
  {
    pclk = 16000000U;
  }

  if (((RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos) < 4U)
  {
    return pclk;
  }

  return pclk * 2U;
}

/**
  * @brief  Output one unsigned 8 bit PCM sample.
  *
  *         128 (silence) maps to a 50 % duty cycle, 0 to the minimum duty and
  *         255 to the maximum one. The value is looked up in a table that was
  *         filled when the mode was selected, so this is a single store.
  * @param  value unsigned 8 bit sample.
  * @retval None
  */
static void AUDIO_SetSample(uint8_t value)
{
  __HAL_TIM_SET_COMPARE(&s_pwm, AUDIO_PWM_CHANNEL, s_duty_lut[value]);
}

/**
  * @brief  Drive the output permanently low, i.e. silence.
  * @param  None
  * @retval None
  */
static void AUDIO_Mute(void)
{
  __HAL_TIM_SET_COMPARE(&s_pwm, AUDIO_PWM_CHANNEL, 0U);
}

/**
  * @brief  Restore the PWM period before starting sample playback.
  * @param  None
  * @retval None
  */
static void AUDIO_ResetPwm(void)
{
  AUDIO_Mute();
  __HAL_TIM_SET_AUTORELOAD(&s_pwm, 999U);
}

/**
  * @brief  Prepare the PWM for sample generation.
  *
  *         Sample mode runs the counter straight from the timer clock so that
  *         the 16 bit period gives many duty steps, i.e. a proper 8 bit PWM
  *         DAC. The carrier is kept above the audio band.
  * @param  None
  * @retval None
  */
static void AUDIO_PwmSampleInit(void)
{
  uint32_t carrier = AUDIO_SAMPLE_CARRIER_HZ;
  uint32_t period;
  uint32_t value;
  int32_t  mid;
  int32_t  span;

  if (s_timer_clk_hz < (AUDIO_SAMPLE_MIN_CARRIER_HZ * 2U))
  {
    /* Very slow timer clock: keep the maximum resolution the clock allows. */
    period = 2U;
  }
  else
  {
    period = s_timer_clk_hz / carrier;
    if (period < 2U)
    {
      period = 2U;
    }
    else if (period > 65536U)
    {
      period = 65536U;
    }
  }

  __HAL_TIM_SET_PRESCALER(&s_pwm, 0U);
  __HAL_TIM_SET_AUTORELOAD(&s_pwm, period - 1U);

  /* Precompute the sample to duty mapping. The sample is centred on a 50 %
     duty (which the AC coupling of the amplifier turns into silence) and the
     span around that midpoint is set by AUDIO_SAMPLE_SPAN_PCT. The duty is kept
     just below the period so that the output can never be left permanently
     high.
   *
   * The mapping also applies the compressor selected by
   * AUDIO_SAMPLE_COMPRESS_PCT. The curve used is
   *
   *     f(s) = s * (1 + k) / (1 + k * |s|)          with s in [-1, 1]
   *
   * which is a smooth saturation: f(0) = 0, f(1) = 1 and f'(0) = 1 + k. Small
   * signals are therefore lifted by 1 + k while the peak stays anchored at 1,
   * which is exactly the ratio that raises the average level. The arithmetic is
   * done in integers so no floating point library is pulled in. */
  mid  = (int32_t)(period / 2U);
  span = (int32_t)((period / 2U) * AUDIO_SAMPLE_SPAN_PCT / 100U);

  for (value = 0U; value < 256U; ++value)
  {
    int32_t s    = (int32_t)value - 128;      /* -128 .. +127               */
    int32_t k    = (int32_t)AUDIO_SAMPLE_COMPRESS_PCT;
    int32_t num  = s * (128 + k);
    int32_t den  = 128 + ((k * (s < 0 ? -s : s)) / 128);
    int32_t shaped;
    int32_t duty;

    /* q15 value in the same -128..+128 range as the input. */
    shaped = (den != 0) ? (num / den) : s;
    if (shaped >  128) shaped =  128;
    if (shaped < -128) shaped = -128;

    duty = mid + ((shaped * span) / 128);

    if (duty < 0)
    {
      duty = 0;
    }
    else if (duty > (int32_t)period - 1)
    {
      duty = (int32_t)period - 1;
    }
    s_duty_lut[value] = (uint16_t)duty;
  }
}

/**
  * @brief  Stop the sample pacing timer.
  * @param  None
  * @retval None
  */
static void AUDIO_SampleTimerStop(void)
{
  __HAL_TIM_DISABLE(&s_sample);
  __HAL_TIM_SET_COUNTER(&s_sample, 0U);
  __HAL_TIM_CLEAR_FLAG(&s_sample, TIM_FLAG_UPDATE);
}

/**
  * @brief  Program the pacing timer to fire at the requested rate.
  *
  *         The timer clock is divided as accurately as possible by searching a
  *         small window of prescaler values for the combination with the
  *         smallest rate error. The error stays below 0.1 % for every supported
  *         rate (it is exactly zero for the common standard rates).
  * @param  rate_hz rate of the update interrupt, in Hz.
  * @retval None
  */
static void AUDIO_ProgramPaceTimer(uint32_t rate_hz)
{
  uint64_t clk = s_timer_clk_hz;
  uint32_t guess;
  uint32_t best_pre = 1U;
  uint32_t best_arr = 1U;
  uint64_t best_err = UINT64_MAX;
  uint32_t lo;
  uint32_t hi;
  uint32_t i;

  /* Smallest prescaler that brings the period for *this* rate into the 16 bit
     range of the auto-reload register. */
  guess = 1U;
  while ((clk / ((uint64_t)guess * (uint64_t)rate_hz)) > 65536ULL)
  {
    ++guess;
  }

  /* Below that prescaler the period no longer fits, so only search upwards. */
  lo = guess;
  hi = guess + 8U;

  for (i = lo; i <= hi; ++i)
  {
    uint64_t ticks = clk / i;
    uint64_t ideal;
    uint64_t got;
    uint64_t diff;
    uint64_t err;
    uint32_t arr;

    if (ticks == 0ULL)
    {
      continue;
    }

    arr = (uint32_t)((ticks + (rate_hz / 2U)) / rate_hz);
    if (arr == 0U)
    {
      arr = 1U;
    }
    if (arr > 65536U)
    {
      continue;
    }

    /* The achieved period is the prescaler times the auto-reload value. The
       error is expressed in parts per million of the ideal period. */
    ideal = clk / rate_hz;
    got   = (uint64_t)i * (uint64_t)arr;
    diff  = (got > ideal) ? (got - ideal) : (ideal - got);
    err   = (diff * 1000000ULL) / (ideal ? ideal : 1ULL);

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

  __HAL_TIM_SET_PRESCALER(&s_sample, best_pre - 1U);
  __HAL_TIM_SET_AUTORELOAD(&s_sample, best_arr - 1U);
  __HAL_TIM_SET_COUNTER(&s_sample, 0U);
  __HAL_TIM_CLEAR_FLAG(&s_sample, TIM_FLAG_UPDATE);
  __HAL_TIM_ENABLE(&s_sample);
  __HAL_TIM_ENABLE_IT(&s_sample, TIM_IT_UPDATE);
}

/**
  * @brief  Configure PA1 as the TIM2_CH2 alternate function output.
  * @param  None
  * @retval None
  */
static void AUDIO_PwmGpioConfig(void)
{
  GPIO_InitTypeDef gpio = {0};

  AUDIO_PWM_GPIO_CLK_ENABLE();

  gpio.Pin       = AUDIO_PWM_GPIO_PIN;
  gpio.Mode      = GPIO_MODE_AF_PP;
  gpio.Pull      = GPIO_NOPULL;
  gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = AUDIO_PWM_GPIO_AF;
  HAL_GPIO_Init(AUDIO_PWM_GPIO_PORT, &gpio);
}

/**
  * @brief  Initialise the low level hardware of the PWM timer, called by
  *         HAL_TIM_PWM_Init().
  * @param  htim TIM handle.
  * @retval None
  */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == AUDIO_PWM_TIM)
  {
    AUDIO_PWM_TIM_CLK_ENABLE();
    AUDIO_PwmGpioConfig();
  }
}

/**
  * @brief  Initialise the low level hardware of the audio timer, called by
  *         HAL_TIM_Base_Init().
  * @param  htim TIM handle.
  * @retval None
  */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == AUDIO_SAMPLE_TIM)
  {
    AUDIO_SAMPLE_TIM_CLK_ENABLE();
  }
}

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
