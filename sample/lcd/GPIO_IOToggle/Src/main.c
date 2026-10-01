/**
  ******************************************************************************
  * @file    main.c
  * @author  MCU Application Team
  * @brief   Main program body
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "puya_logo.h"

/* Private define ------------------------------------------------------------*/
#define LCD_WIDTH                   320U
#define LCD_HEIGHT                  240U

#define LCD_CS_PORT                 GPIOD
#define LCD_CS_PIN                  GPIO_PIN_3
#define LCD_MOSI_PORT               GPIOD
#define LCD_MOSI_PIN                GPIO_PIN_5
#define LCD_CLK_PORT                GPIOD
#define LCD_CLK_PIN                 GPIO_PIN_4
#define LCD_RS_PORT                 GPIOC
#define LCD_RS_PIN                  GPIO_PIN_8

#define LCD_ORANGE                  0xFBE0U
#define LCD_WHITE                   0xFFFFU

/* Logo bounce speed: pixels moved per frame in each axis. Raise these to make
   the logo travel faster (and/or lower the frame delay in main()). */
#define LOGO_SPEED_X                (3)
#define LOGO_SPEED_Y                (2)
#define LOGO_FRAME_DELAY_MS         (16)  /* ~60 fps upper bound */

/* SPI mode select. ILI9341 is normally mode 0 (CPOL=0/CPHA=0, clock idles LOW,
   latch on rising edge). Set to 1 to test mode 3 (CPOL=1/CPHA=1, clock idles
   HIGH, latch on rising edge). */
#define LCD_SPI_MODE3               (1U)

/* Private variables ---------------------------------------------------------*/
static volatile uint8_t g_lcd_slow_spi = 0U;   /* 1: insert bit delays (debug only) */

/* Debug instrumentation -----------------------------------------------------*/
/* Bit-bang timing.
   LCD_BIT_DELAY_LOOPS is applied on EVERY SPI bit, so raising it slows the whole
   transfer down (both during init and during the pixel fill). LCD_DEBUG_SLOW_SPI
   adds an extra, much larger delay while the short init command stream is being
   sent, so a scope can capture it. */
#define LCD_BIT_DELAY_LOOPS         (0U)   /* base idle loops per SCK half period  */
#define LCD_PIXEL_BIT_DELAY_LOOPS   (0U)   /* throttle so SCK stays within ILI9341 spec */
#define LCD_DEBUG_SLOW_SPI          (1U)   /* 1: allow extra slow mode for init    */
#define LCD_DEBUG_BIT_DELAY_LOOPS   (40U)  /* extra loops per SCK half in slow mode */

/* On-board debug LEDs: 4x ACTIVE LOW (pin driven LOW => LED lit). */
#define DBG_LED_GPIO_PORT           GPIOA
#define DBG_LED1_PIN                GPIO_PIN_3
#define DBG_LED2_PIN                GPIO_PIN_4
#define DBG_LED3_PIN                GPIO_PIN_5
#define DBG_LED4_PIN                GPIO_PIN_6
#define DBG_LED_ALL_PINS            (DBG_LED1_PIN | DBG_LED2_PIN | \
                                     DBG_LED3_PIN | DBG_LED4_PIN)

#define DBG_LED_ON(pin)             HAL_GPIO_WritePin(DBG_LED_GPIO_PORT, (pin), GPIO_PIN_RESET)
#define DBG_LED_OFF(pin)            HAL_GPIO_WritePin(DBG_LED_GPIO_PORT, (pin), GPIO_PIN_SET)

/* Private function prototypes -----------------------------------------------*/
static void APP_SystemClockConfig(void);
static void APP_GpioConfig(void);
static void LCD_GpioInit(void);
static void LCD_DebugLedInit(void);
static void LCD_DebugLedShow(uint8_t stage);
static void LCD_WriteByte(uint8_t value);
static void LCD_WritePixel(uint16_t pixel);
static void LCD_WriteCommand(uint8_t cmd);
static void LCD_WriteCmdData(uint8_t cmd, const uint8_t *data, uint32_t len);
static void LCD_SetRegion(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end);
static void LCD_FillScreen(uint16_t color);
static void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
static void LCD_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels);
static void LCD_Init(void);

/**
  * @brief  Main program.
  * @retval int
  */
int main(void)
{
  int16_t  x = 20;
  int16_t  y = 20;
  int8_t   dx = LOGO_SPEED_X;
  int8_t   dy = LOGO_SPEED_Y;

  HAL_Init();
  APP_SystemClockConfig();
  APP_GpioConfig();
  LCD_DebugLedInit();

  /* Stage 1: MCU is alive and running past clock / GPIO init, before LCD. */
  LCD_DebugLedShow(1U);
  HAL_Delay(400);

  LCD_Init();   /* clears the screen to white */

  /* DVD-style bouncing logo over a white background.
     Erase only the strip the logo leaves behind, one rectangle per axis. */
  while (1)
  {
    int16_t old_x = x;
    int16_t old_y = y;

    x += dx;
    y += dy;

    if (x <= 0)
    {
      x    = 0;
      dx   = (int8_t)(-dx);
    }
    else if (x >= (int16_t)(LCD_WIDTH - PUYA_LOGO_WIDTH))
    {
      x    = (int16_t)(LCD_WIDTH - PUYA_LOGO_WIDTH);
      dx   = (int8_t)(-dx);
    }

    if (y <= 0)
    {
      y    = 0;
      dy   = (int8_t)(-dy);
    }
    else if (y >= (int16_t)(LCD_HEIGHT - PUYA_LOGO_HEIGHT))
    {
      y    = (int16_t)(LCD_HEIGHT - PUYA_LOGO_HEIGHT);
      dy   = (int8_t)(-dy);
    }

    /* Erase the area the logo previously occupied but does not cover now. */
    if (x > old_x)
    {
      LCD_FillRect((uint16_t)old_x, (uint16_t)old_y, (uint16_t)(x - old_x),
                   PUYA_LOGO_HEIGHT, LCD_WHITE);
    }
    else if (x < old_x)
    {
      LCD_FillRect((uint16_t)(x + PUYA_LOGO_WIDTH), (uint16_t)old_y,
                   (uint16_t)(old_x - x), PUYA_LOGO_HEIGHT, LCD_WHITE);
    }

    if (y > old_y)
    {
      LCD_FillRect((uint16_t)old_x, (uint16_t)old_y, PUYA_LOGO_WIDTH,
                   (uint16_t)(y - old_y), LCD_WHITE);
    }
    else if (y < old_y)
    {
      LCD_FillRect((uint16_t)old_x, (uint16_t)(y + PUYA_LOGO_HEIGHT),
                   PUYA_LOGO_WIDTH, (uint16_t)(old_y - y), LCD_WHITE);
    }

    /* Erase regions that are outside the old footprint (only reachable when the
       logo was clamped at an edge). Cheap no-ops in the common case. */

    LCD_DrawImage((uint16_t)x, (uint16_t)y, PUYA_LOGO_WIDTH, PUYA_LOGO_HEIGHT,
                  puya_logo);

    LCD_DebugLedShow(3U);
    HAL_Delay(LOGO_FRAME_DELAY_MS);
  }
}

/**
  * @brief  GPIO configuration.
  * @param  None
  * @retval None
  */
static void APP_GpioConfig(void)
{
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();

  LCD_GpioInit();
}

static void LCD_GpioInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

  GPIO_InitStruct.Pin = LCD_CS_PIN;
  HAL_GPIO_Init(LCD_CS_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LCD_MOSI_PIN;
  HAL_GPIO_Init(LCD_MOSI_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LCD_CLK_PIN;
  HAL_GPIO_Init(LCD_CLK_PORT, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LCD_RS_PIN;
  HAL_GPIO_Init(LCD_RS_PORT, &GPIO_InitStruct);

  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_MOSI_PORT, LCD_MOSI_PIN, GPIO_PIN_RESET);
#if (LCD_SPI_MODE3)
  HAL_GPIO_WritePin(LCD_CLK_PORT, LCD_CLK_PIN, GPIO_PIN_SET);   /* idle HIGH in mode 3 */
#else
  HAL_GPIO_WritePin(LCD_CLK_PORT, LCD_CLK_PIN, GPIO_PIN_RESET); /* idle LOW  in mode 0 */
#endif
  HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, GPIO_PIN_RESET);
}

static void LCD_DebugLedInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitStruct.Pin = DBG_LED_ALL_PINS;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DBG_LED_GPIO_PORT, &GPIO_InitStruct);

  /* Active Low: drive HIGH to keep all debug LEDs OFF at startup. */
  HAL_GPIO_WritePin(DBG_LED_GPIO_PORT, DBG_LED_ALL_PINS, GPIO_PIN_SET);
}

/* Active-Low indicator: lights LED1..LED4 according to the init stage.
   stage 1 = LED1 only, stage 2 = LED1+LED2, stage 3 = all four. */
static void LCD_DebugLedShow(uint8_t stage)
{
  DBG_LED_OFF(DBG_LED1_PIN);
  DBG_LED_OFF(DBG_LED2_PIN);
  DBG_LED_OFF(DBG_LED3_PIN);
  DBG_LED_OFF(DBG_LED4_PIN);

  if (stage >= 1U)
  {
    DBG_LED_ON(DBG_LED1_PIN);
  }
  if (stage >= 2U)
  {
    DBG_LED_ON(DBG_LED2_PIN);
  }
  if (stage >= 3U)
  {
    DBG_LED_ON(DBG_LED3_PIN);
  }
  if (stage >= 4U)
  {
    DBG_LED_ON(DBG_LED4_PIN);
  }
}

/* Bit-bang 4-wire SPI: MSB first. CS, MOSI and CLK are all on GPIOD, so a
   single BSRR write can drive the clock and the data line together. This drops
   the hot loop to 2 register writes per bit.
   mode 0 (LCD_SPI_MODE3=0): clock idles LOW,  data set while LOW,  latch rising
   mode 3 (LCD_SPI_MODE3=1): clock idles HIGH, data set on falling, latch rising
   LCD_PIXEL_BIT_DELAY_LOOPS keeps SCK below the ILI9341 limit now that the core
   runs at 144 MHz. Set it to 0 for absolute maximum speed. */
static void LCD_WriteByte(uint8_t value)
{
  volatile uint32_t *bsrr = &LCD_CLK_PORT->BSRR;

  const uint32_t clk_lo  = (uint32_t)LCD_CLK_PIN << 16U;   /* reset CLK */
  const uint32_t clk_hi  = (uint32_t)LCD_CLK_PIN;          /* set   CLK */
  const uint32_t mosi_lo = (uint32_t)LCD_MOSI_PIN << 16U;  /* reset MOSI */
  const uint32_t mosi_hi = (uint32_t)LCD_MOSI_PIN;         /* set   MOSI */

  for (uint8_t bit = 0U; bit < 8U; ++bit)
  {
    const uint32_t mosi = ((value & 0x80U) != 0U) ? mosi_hi : mosi_lo;

#if (LCD_SPI_MODE3)
    /* One write: clock falls AND the data bit is driven at the same time. */
    *bsrr = clk_lo | mosi;
#else
    /* Clock is already low; just drive the data bit. */
    *bsrr = mosi;
#endif

    for (volatile uint32_t i = 0U; i < LCD_PIXEL_BIT_DELAY_LOOPS; ++i)
    {
      __NOP();
    }
#if (LCD_DEBUG_SLOW_SPI)
    if (g_lcd_slow_spi != 0U)
    {
      for (volatile uint32_t i = 0U; i < LCD_DEBUG_BIT_DELAY_LOOPS; ++i)
      {
        __NOP();
      }
    }
#endif

    *bsrr = clk_hi;   /* rising edge latches the bit into the LCD */

    for (volatile uint32_t i = 0U; i < LCD_PIXEL_BIT_DELAY_LOOPS; ++i)
    {
      __NOP();
    }
#if (LCD_DEBUG_SLOW_SPI)
    if (g_lcd_slow_spi != 0U)
    {
      for (volatile uint32_t i = 0U; i < LCD_DEBUG_BIT_DELAY_LOOPS; ++i)
      {
        __NOP();
      }
    }
#endif
    value <<= 1U;
  }

  /* Leave the clock at its idle level between bytes / frames. */
#if (LCD_SPI_MODE3)
  *bsrr = clk_hi;
#else
  *bsrr = clk_lo;
#endif
}

/* Toggle each control line slowly and independently so you can verify it on a
   scope / LED / continuity check before trusting the whole protocol. */
static void LCD_WriteCommand(uint8_t cmd)
{
  HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);
  LCD_WriteByte(cmd);
  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
}

/* Send a command together with all of its parameters inside ONE CS-low frame.
   The ILI9341 expects CS to stay low from the command byte through the last
   parameter; raising CS in between can terminate the command early and make
   the parameter bytes be ignored. */
static void LCD_WriteCmdData(uint8_t cmd, const uint8_t *data, uint32_t len)
{
  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);

  HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, GPIO_PIN_RESET);
  LCD_WriteByte(cmd);

  HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, GPIO_PIN_SET);
  for (uint32_t i = 0U; i < len; ++i)
  {
    LCD_WriteByte(data[i]);
  }

  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
}

static void LCD_SetRegion(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end)
{
  const uint8_t col[4] = {(uint8_t)(x_start >> 8U), (uint8_t)x_start,
                          (uint8_t)(x_end >> 8U),   (uint8_t)x_end};
  const uint8_t row[4] = {(uint8_t)(y_start >> 8U), (uint8_t)y_start,
                          (uint8_t)(y_end >> 8U),   (uint8_t)y_end};

  LCD_WriteCmdData(0x2AU, col, 4U);
  LCD_WriteCmdData(0x2BU, row, 4U);
}

/* Send one RGB565 pixel (two bytes, MSB first). Keeping the loop here instead
   of calling LCD_WriteByte twice saves a function call per pixel. */
static void LCD_WritePixel(uint16_t pixel)
{
  LCD_WriteByte((uint8_t)(pixel >> 8U));
  LCD_WriteByte((uint8_t)(pixel & 0xFFU));
}

/* Blit an RGB565 image into a (x,y)-(x+w-1,y+h-1) window. CS is asserted once
   for the whole memory-write burst. */
static void LCD_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                          const uint16_t *pixels)
{
  uint32_t count = (uint32_t)w * (uint32_t)h;

  LCD_SetRegion(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));

  LCD_WriteCommand(0x2CU);

  HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);

  for (uint32_t i = 0U; i < count; ++i)
  {
    LCD_WritePixel(pixels[i]);
  }

  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
}

static void LCD_FillScreen(uint16_t color)
{
  uint32_t pixel_count = (uint32_t)LCD_WIDTH * (uint32_t)LCD_HEIGHT;

  LCD_SetRegion(0U, 0U, LCD_WIDTH - 1U, LCD_HEIGHT - 1U);

  LCD_WriteCommand(0x2CU);

  /* Assert CS once for the whole pixel stream. The ILI9341 memory-write
     burst (0x2C) expects CS to stay LOW while every pixel byte is clocked;
     toggling CS per byte can abort the burst and leave the panel blank. */
  HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);

  for (uint32_t i = 0U; i < pixel_count; ++i)
  {
    LCD_WriteByte((uint8_t)(color >> 8U));
    LCD_WriteByte((uint8_t)(color & 0xFFU));
  }

  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
}

/* Fill an arbitrary rectangle with a solid color (used to erase the logo's
   previous position). A degenerate size is ignored. */
static void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                         uint16_t color)
{
  uint32_t count;

  if ((w == 0U) || (h == 0U))
  {
    return;
  }

  count = (uint32_t)w * (uint32_t)h;

  LCD_SetRegion(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));

  LCD_WriteCommand(0x2CU);

  HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);

  for (uint32_t i = 0U; i < count; ++i)
  {
    LCD_WriteByte((uint8_t)(color >> 8U));
    LCD_WriteByte((uint8_t)(color & 0xFFU));
  }

  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
}

static void LCD_Init(void)
{
  HAL_Delay(120);

  /* Stage 2: init command stream starts (LED1+LED2). */
  LCD_DebugLedShow(2U);

#if (LCD_DEBUG_SLOW_SPI)
  g_lcd_slow_spi = 1U;   /* slow only the short init command stream */
#endif

  LCD_WriteCommand(0x01U);          /* SWRESET */
  HAL_Delay(120);

  /* Undocumented but required by ILI9341 modules (power control A). */
  LCD_WriteCmdData(0xEFU, (const uint8_t[]){0x03U, 0x80U, 0x02U}, 3U);

  LCD_WriteCmdData(0xCFU, (const uint8_t[]){0x00U, 0xC1U, 0x30U}, 3U);

  LCD_WriteCmdData(0xEDU, (const uint8_t[]){0x64U, 0x03U, 0x12U, 0x81U}, 4U);

  LCD_WriteCmdData(0xE8U, (const uint8_t[]){0x85U, 0x00U, 0x78U}, 3U);

  LCD_WriteCmdData(0xCBU, (const uint8_t[]){0x39U, 0x2CU, 0x00U, 0x34U, 0x02U}, 5U);

  LCD_WriteCmdData(0xF7U, (const uint8_t[]){0x20U}, 1U);

  LCD_WriteCmdData(0xEAU, (const uint8_t[]){0x00U, 0x00U}, 2U);

  /* Power control 1 / 2 */
  LCD_WriteCmdData(0xC0U, (const uint8_t[]){0x23U}, 1U);
  LCD_WriteCmdData(0xC1U, (const uint8_t[]){0x10U}, 1U);

  /* VCOM control 1 / 2 */
  LCD_WriteCmdData(0xC5U, (const uint8_t[]){0x3EU, 0x28U}, 2U);
  LCD_WriteCmdData(0xC7U, (const uint8_t[]){0x86U}, 1U);

  /* MADCTL: MV|BGR -> landscape, 320 wide x 240 tall */
  LCD_WriteCmdData(0x36U, (const uint8_t[]){0x28U}, 1U);

  /* Vertical scroll start address = 0 (required, matches reference driver) */
  LCD_WriteCmdData(0x37U, (const uint8_t[]){0x00U}, 1U);

  /* Pixel format: 16-bit RGB565 */
  LCD_WriteCmdData(0x3AU, (const uint8_t[]){0x55U}, 1U);

  /* Frame rate control: 70 Hz */
  LCD_WriteCmdData(0xB1U, (const uint8_t[]){0x00U, 0x18U}, 2U);

  /* Display function control (PT/REV/GS/SS/SM + FP/BP) */
  LCD_WriteCmdData(0xB6U, (const uint8_t[]){0x08U, 0x82U, 0x27U}, 3U);

  /* 3Gamma disable + Gamma set */
  LCD_WriteCmdData(0xF2U, (const uint8_t[]){0x00U}, 1U);
  LCD_WriteCmdData(0x26U, (const uint8_t[]){0x01U}, 1U);

  /* Positive gamma correction */
  LCD_WriteCmdData(0xE0U, (const uint8_t[]){0x0FU, 0x31U, 0x2BU, 0x0CU, 0x0EU,
                                            0x08U, 0x4EU, 0xF1U, 0x37U, 0x07U,
                                            0x10U, 0x03U, 0x0EU, 0x09U, 0x00U}, 15U);

  /* Negative gamma correction */
  LCD_WriteCmdData(0xE1U, (const uint8_t[]){0x00U, 0x0EU, 0x14U, 0x03U, 0x11U,
                                            0x07U, 0x31U, 0xC1U, 0x48U, 0x08U,
                                            0x0FU, 0x0CU, 0x31U, 0x36U, 0x0FU}, 15U);

  LCD_WriteCommand(0x11U);          /* SLPOUT */
  HAL_Delay(120);

  LCD_WriteCommand(0x29U);          /* DISPON */
  HAL_Delay(20);

#if (LCD_DEBUG_SLOW_SPI)
  g_lcd_slow_spi = 0U;   /* restore full speed for the bulk pixel transfer */
#endif

  LCD_FillScreen(LCD_WHITE);
}

/**
  * @brief  System clock configuration function.
  * @param  None
  * @retval None
  */
static void APP_SystemClockConfig(void)
{
  RCC_OscInitTypeDef OscInitstruct = {0};
  RCC_ClkInitTypeDef ClkInitstruct = {0};

  OscInitstruct.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI |
                                 RCC_OSCILLATORTYPE_LSE | RCC_OSCILLATORTYPE_LSI |
                                 RCC_OSCILLATORTYPE_HSI48M;
  OscInitstruct.HSEState = RCC_HSE_OFF;
  OscInitstruct.HSI48MState = RCC_HSI48M_OFF;
  OscInitstruct.HSIState = RCC_HSI_ON;
  OscInitstruct.LSEState = RCC_LSE_OFF;
  OscInitstruct.LSIState = RCC_LSI_OFF;

  /* PLL: HSI/2 (8 MHz) * 18 = 144 MHz. This is the main speed-up for the
     bit-bang SPI; the old config ran the core at only 16 MHz. */
  OscInitstruct.PLL.PLLState   = RCC_PLL_ON;
  OscInitstruct.PLL.PLLSource  = RCC_PLLSOURCE_HSI_DIV2;
  OscInitstruct.PLL.PLLPrediv  = RCC_PLL_PREDIV_DIV1;
  OscInitstruct.PLL.PLLMUL     = 18;
  OscInitstruct.PLL.PLLPostdiv = RCC_PLL_POSTDIV_DIV1;

  if (HAL_RCC_OscConfig(&OscInitstruct) != HAL_OK)
  {
    APP_ErrorHandler();
  }

  ClkInitstruct.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                            RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  ClkInitstruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  ClkInitstruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  ClkInitstruct.APB1CLKDivider = RCC_HCLK_DIV2;
  ClkInitstruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&ClkInitstruct, FLASH_LATENCY_6) != HAL_OK)
  {
    APP_ErrorHandler();
  }
}

void APP_ErrorHandler(void)
{
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  while (1)
  {
  }
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
