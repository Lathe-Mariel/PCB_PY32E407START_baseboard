/**
  ******************************************************************************
  * @file    lcd096.c
  * @brief   Software (bit-bang) SPI driver for the Pmod-LCD-0.96 v1.0 module.
  *
  *          LCD controller : ST7735S
  *          Resolution     : 160 x 80 (RGB565, 4-wire serial)
  *          SPI mode       : CPOL = 0, CPHA = 0 (data latched on SCK rising)
  *
  *          The PY32E407 drives the panel with a plain GPIO bit-bang because
  *          the 4-wire serial interface needs CS/DC/RST in addition to the two
  *          SPI wires, and the module is tolerant of a slow clock. Keeping the
  *          SPI in software also means no hardware SPI peripheral / DMA is
  *          required and the sample matches the existing lcd sample style.
  *
  *          Wiring: see lcd096.h. The module plugs into the base board Pmod
  *          socket J3 and can be inserted either way round, so the pin map is
  *          selected at runtime (LCD096_SetPmodRow / LCD096_PMOD_ROW).
  *            row 1 : CS=PD7, MOSI=PB6, DC=PC8, CLK=PC9, RST=PD6
  *            row 2 : CS=PD3, MOSI=PD5, DC=PD6, CLK=PD4, RST=PC8
  *          The module puts RESET opposite DC, so it is NOT on J3 pin10.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "lcd096.h"

/* Private types -------------------------------------------------------------*/

/* One Pmod insertion worth of pins. */
typedef struct
{
  GPIO_TypeDef *cs_port;  uint16_t cs_pin;
  GPIO_TypeDef *sda_port; uint16_t sda_pin;
  GPIO_TypeDef *dc_port;  uint16_t dc_pin;
  GPIO_TypeDef *sck_port; uint16_t sck_pin;
  GPIO_TypeDef *rst_port; uint16_t rst_pin;
} LCD096_Pins_t;

/* Private variables ---------------------------------------------------------*/
static const LCD096_Pins_t LCD096_ROW1 =
{
  LCD096_R1_CS_PORT,  LCD096_R1_CS_PIN,
  LCD096_R1_SDA_PORT, LCD096_R1_SDA_PIN,
  LCD096_R1_DC_PORT,  LCD096_R1_DC_PIN,
  LCD096_R1_SCK_PORT, LCD096_R1_SCK_PIN,
  LCD096_R1_RST_PORT, LCD096_R1_RST_PIN
};

static const LCD096_Pins_t LCD096_ROW2 =
{
  LCD096_R2_CS_PORT,  LCD096_R2_CS_PIN,
  LCD096_R2_SDA_PORT, LCD096_R2_SDA_PIN,
  LCD096_R2_DC_PORT,  LCD096_R2_DC_PIN,
  LCD096_R2_SCK_PORT, LCD096_R2_SCK_PIN,
  LCD096_R2_RST_PORT, LCD096_R2_RST_PIN
};

#if (LCD096_PMOD_ROW == 2U)
static const LCD096_Pins_t *s_pins = &LCD096_ROW2;
#else
static const LCD096_Pins_t *s_pins = &LCD096_ROW1;
#endif

/* Private define ------------------------------------------------------------*/

/* Bit-bang half period, in loops of the SCK half cycle. 0 = as fast as the
   CPU allows (the ST7735S is happy well above 10 MHz, but the loop overhead at
   16 MHz HSI keeps SCK down to a few MHz, which is a safe place to be). */
#define LCD096_BIT_DELAY_LOOPS      (0U)

/* SCK is reset to its idle level at the start of EVERY byte. A free-running
   bit-bang can otherwise begin a byte on an arbitrary half cycle: if it starts
   while SCK is still HIGH the first action is a falling edge, and the panel
   then shifts in one spurious bit (every following byte is off by one). */
#define LCD096_SCK_LOW()            HAL_GPIO_WritePin(s_pins->sck_port, s_pins->sck_pin, GPIO_PIN_RESET)
#define LCD096_SCK_HIGH()           HAL_GPIO_WritePin(s_pins->sck_port, s_pins->sck_pin, GPIO_PIN_SET)

/* CS is held LOW for the whole command/data sequence, as required by the
   ST7735S 4-wire serial interface (raising CS in the middle of a command
   aborts it and the parameters are ignored). */
#define LCD096_CS_LOW()             HAL_GPIO_WritePin(s_pins->cs_port,  s_pins->cs_pin,  GPIO_PIN_RESET)
#define LCD096_CS_HIGH()            HAL_GPIO_WritePin(s_pins->cs_port,  s_pins->cs_pin,  GPIO_PIN_SET)
#define LCD096_DC_CMD()             HAL_GPIO_WritePin(s_pins->dc_port,  s_pins->dc_pin,  GPIO_PIN_RESET)
#define LCD096_DC_DATA()            HAL_GPIO_WritePin(s_pins->dc_port,  s_pins->dc_pin,  GPIO_PIN_SET)
#define LCD096_RST_LOW()            HAL_GPIO_WritePin(s_pins->rst_port, s_pins->rst_pin, GPIO_PIN_RESET)
#define LCD096_RST_HIGH()           HAL_GPIO_WritePin(s_pins->rst_port, s_pins->rst_pin, GPIO_PIN_SET)
#define LCD096_SDA_LOW()            HAL_GPIO_WritePin(s_pins->sda_port, s_pins->sda_pin, GPIO_PIN_RESET)
#define LCD096_SDA_HIGH()           HAL_GPIO_WritePin(s_pins->sda_port, s_pins->sda_pin, GPIO_PIN_SET)

/* Private function prototypes -----------------------------------------------*/
static void LCD096_GpioInit(void);
static void LCD096_WriteByte(uint8_t value);
static void LCD096_WritePixel(uint16_t pixel);
static void LCD096_WriteCommand(uint8_t cmd);
static void LCD096_WriteCmdData(uint8_t cmd, const uint8_t *data, uint32_t len);
static void LCD096_SetWindow(uint16_t x, uint16_t y, uint16_t x_end, uint16_t y_end);
static void LCD096_StartRamWrite(void);
static void LCD096_HardwareReset(void);
static void LCD096_InitSequence(void);

/* Debug hook implemented in main.c. It is weak so the driver can also be used
   without the debug LEDs being wired up. */
__attribute__((weak)) void LCD096_DebugHook(uint8_t stage)
{
  (void)stage;
}

/* Called once per byte pushed out over the software SPI. main.c uses it as a
   "bus alive" indicator; it is weak so the driver works without it too. */
__attribute__((weak)) void LCD096_TxHook(void)
{
}

/* Private user code ---------------------------------------------------------*/

/**
  * @brief  Configure the five LCD control/data GPIOs as push-pull outputs.
  */
static void LCD096_GpioInit(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

  GPIO_InitStruct.Pin = s_pins->cs_pin;
  HAL_GPIO_Init(s_pins->cs_port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = s_pins->sda_pin;
  HAL_GPIO_Init(s_pins->sda_port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = s_pins->dc_pin;
  HAL_GPIO_Init(s_pins->dc_port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = s_pins->sck_pin;
  HAL_GPIO_Init(s_pins->sck_port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = s_pins->rst_pin;
  HAL_GPIO_Init(s_pins->rst_port, &GPIO_InitStruct);

  /* Idle levels: CS high, SCK low (mode 0), DC high (data), RST high. */
  LCD096_CS_HIGH();
  LCD096_SCK_LOW();
  LCD096_SDA_LOW();
  LCD096_DC_DATA();
  LCD096_RST_HIGH();
}

/**
  * @brief  Bit-bang one byte, MSB first, SPI mode 0.
  * @param  value byte to transmit.
  */
static void LCD096_WriteByte(uint8_t value)
{
  /* Start from a known SCK level so the byte always begins with a rising edge. */
  LCD096_SCK_LOW();

  LCD096_TxHook();

  for (uint8_t bit = 0U; bit < 8U; ++bit)
  {
    /* Data is presented while SCK is LOW ... */
    if ((value & 0x80U) != 0U)
    {
      LCD096_SDA_HIGH();
    }
    else
    {
      LCD096_SDA_LOW();
    }

    for (volatile uint32_t i = 0U; i < LCD096_BIT_DELAY_LOOPS; ++i)
    {
      __NOP();
    }

    /* ... and latched by the panel on the rising edge. */
    LCD096_SCK_HIGH();

    for (volatile uint32_t i = 0U; i < LCD096_BIT_DELAY_LOOPS; ++i)
    {
      __NOP();
    }

    LCD096_SCK_LOW();

    value <<= 1U;
  }
}

/**
  * @brief  Transmit one RGB565 pixel (two bytes, MSB first).
  */
static void LCD096_WritePixel(uint16_t pixel)
{
  LCD096_WriteByte((uint8_t)(pixel >> 8U));
  LCD096_WriteByte((uint8_t)(pixel & 0xFFU));
}

/**
  * @brief  Send an 8-bit command (DC = 0) inside its own CS frame.
  */
static void LCD096_WriteCommand(uint8_t cmd)
{
  LCD096_CS_LOW();
  LCD096_DC_CMD();
  LCD096_WriteByte(cmd);
  LCD096_CS_HIGH();
}

/**
  * @brief  Send a command together with its parameter bytes inside ONE CS frame.
  */
static void LCD096_WriteCmdData(uint8_t cmd, const uint8_t *data, uint32_t len)
{
  LCD096_CS_LOW();

  LCD096_DC_CMD();
  LCD096_WriteByte(cmd);

  LCD096_DC_DATA();
  for (uint32_t i = 0U; i < len; ++i)
  {
    LCD096_WriteByte(data[i]);
  }

  LCD096_CS_HIGH();
}

/**
  * @brief  Program the column/row address window (CASET + RASET).
  */
static void LCD096_SetWindow(uint16_t x, uint16_t y, uint16_t x_end, uint16_t y_end)
{
  const uint8_t caset[4] = {(uint8_t)(x >> 8U),     (uint8_t)x,
                            (uint8_t)(x_end >> 8U), (uint8_t)x_end};
  const uint8_t raset[4] = {(uint8_t)(y >> 8U),     (uint8_t)y,
                            (uint8_t)(y_end >> 8U), (uint8_t)y_end};

  LCD096_WriteCmdData(0x2AU, caset, 4U);
  LCD096_WriteCmdData(0x2BU, raset, 4U);
}

/**
  * @brief  Issue RAMWR (0x2C) and leave CS asserted with DC = data so the
  *         caller can stream pixels; the caller must raise CS when done.
  */
static void LCD096_StartRamWrite(void)
{
  LCD096_CS_LOW();
  LCD096_DC_CMD();
  LCD096_WriteByte(0x2CU);
  LCD096_DC_DATA();
}

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Hardware reset: RST LOW for 15 ms, then settle HIGH for 150 ms.
  */
static void LCD096_HardwareReset(void)
{
  LCD096_RST_HIGH();
  HAL_Delay(10);

  LCD096_RST_LOW();
  HAL_Delay(15);

  LCD096_RST_HIGH();
  HAL_Delay(150);
}

/**
  * @brief  ST7735S power-on / configuration sequence.
  *         Values are copied from the reference Pmod-LCD-0.96 design that was
  *         validated against this exact module (RGB565, 160x80, MADCTL = 0x00,
  *         colour inversion off so RGB565 values map straight through).
  */
static void LCD096_InitSequence(void)
{
  /* Software reset, then wait for the internal power-on to settle. */
  LCD096_WriteCommand(0x01U);            /* SWRESET */
  LCD096_DebugHook(5U);
  HAL_Delay(150);

  LCD096_WriteCommand(0x11U);            /* SLPOUT */
  LCD096_DebugHook(6U);
  HAL_Delay(120);

  /* Interface pixel format. 0x05 = 18-bit (RGB666) colour, which is what the
     module's reference firmware uses and what the panel expects: each pixel is
     THREE bytes on the wire (see LCD096_WritePixel). */
  LCD096_WriteCmdData(0x3AU, (const uint8_t[]){0x05U}, 1U);

  /* Memory access control: orientation / mirroring / RGB order. */
  LCD096_WriteCmdData(0x36U, (const uint8_t[]){LCD096_MADCTL_VALUE}, 1U);
  LCD096_DebugHook(7U);

  /* Frame rate control: normal / idle / partial. */
  LCD096_WriteCmdData(0xB1U, (const uint8_t[]){0x01U, 0x2CU, 0x2DU}, 3U);
  LCD096_WriteCmdData(0xB2U, (const uint8_t[]){0x01U, 0x2CU, 0x2DU}, 3U);
  LCD096_WriteCmdData(0xB3U, (const uint8_t[]){0x01U, 0x2CU, 0x2DU,
                                               0x01U, 0x2CU, 0x2DU}, 6U);

  /* Display inversion control. */
  LCD096_WriteCmdData(0xB4U, (const uint8_t[]){0x07U}, 1U);

  /* Power control. */
  LCD096_WriteCmdData(0xC0U, (const uint8_t[]){0xA2U, 0x02U, 0x84U}, 3U);
  LCD096_WriteCmdData(0xC1U, (const uint8_t[]){0xC5U}, 1U);
  LCD096_WriteCmdData(0xC2U, (const uint8_t[]){0x0AU, 0x00U}, 2U);
  LCD096_WriteCmdData(0xC3U, (const uint8_t[]){0x8AU, 0x2AU}, 2U);
  LCD096_WriteCmdData(0xC4U, (const uint8_t[]){0x8AU, 0xEEU}, 2U);

  /* VCOM control. */
  LCD096_WriteCmdData(0xC5U, (const uint8_t[]){0x0EU}, 1U);

  /* Display inversion. RGB565 maps directly when inversion is OFF; panels that
     look colour-inverted need LCD096_USE_INVON = 1. */
#if (LCD096_USE_INVON)
  LCD096_WriteCommand(0x21U);            /* INVON  */
#else
  LCD096_WriteCommand(0x20U);            /* INVOFF */
#endif
  LCD096_DebugHook(8U);

  /* Gamma curves (positive / negative). */
  LCD096_WriteCmdData(0xE0U, (const uint8_t[]){0x0FU, 0x1AU, 0x0FU, 0x18U,
                                               0x2FU, 0x28U, 0x20U, 0x22U,
                                               0x1FU, 0x1BU, 0x23U, 0x37U,
                                               0x00U, 0x07U, 0x02U, 0x10U}, 16U);
  LCD096_WriteCmdData(0xE1U, (const uint8_t[]){0x0FU, 0x1BU, 0x0FU, 0x17U,
                                               0x33U, 0x2CU, 0x29U, 0x2EU,
                                               0x30U, 0x30U, 0x39U, 0x00U,
                                               0x07U, 0x03U, 0x10U}, 15U);
  LCD096_DebugHook(9U);

  LCD096_WriteCommand(0x29U);            /* DISPON */
  LCD096_DebugHook(10U);
  HAL_Delay(100);
}

/* Exported functions --------------------------------------------------------*/

void LCD096_SetPmodRow(uint8_t row)
{
  s_pins = (row == 2U) ? &LCD096_ROW2 : &LCD096_ROW1;
}

void LCD096_Init(void)
{
  LCD096_DebugHook(3U);                  /* LCD GPIO config starting */
  LCD096_GpioInit();

  LCD096_HardwareReset();
  LCD096_DebugHook(4U);                  /* reset pulse done */

  LCD096_InitSequence();
}

void LCD096_PinSelfTest(uint32_t hold_ms)
{
  /* Drive one line HIGH at a time (all the others LOW) so a meter or a bare LED
     on any single LCD pin shows exactly which MCU pin it is wired to. Quickest
     way to find out which Pmod row the module is really plugged into. */
  LCD096_GpioInit();

  LCD096_CS_LOW();  LCD096_DC_CMD();  LCD096_SCK_LOW();  LCD096_RST_LOW();
  LCD096_SDA_LOW();
  LCD096_CS_HIGH();                                      /* stage 81: CS  high */
  LCD096_DebugHook(81U);  HAL_Delay(hold_ms);
  LCD096_CS_LOW();

  LCD096_SDA_HIGH();                                     /* stage 82: SDA high */
  LCD096_DebugHook(82U);  HAL_Delay(hold_ms);
  LCD096_SDA_LOW();

  LCD096_DC_DATA();                                      /* stage 83: DC  high */
  LCD096_DebugHook(83U);  HAL_Delay(hold_ms);
  LCD096_DC_CMD();

  LCD096_SCK_HIGH();                                     /* stage 84: SCK high */
  LCD096_DebugHook(84U);  HAL_Delay(hold_ms);
  LCD096_SCK_LOW();

  LCD096_RST_HIGH();                                     /* stage 85: RST high */
  LCD096_DebugHook(85U);  HAL_Delay(hold_ms);
  LCD096_RST_LOW();

  /* Back to the idle bus state. */
  LCD096_CS_HIGH();
  LCD096_SCK_LOW();
  LCD096_SDA_LOW();
  LCD096_DC_DATA();
  LCD096_RST_HIGH();
}

void LCD096_FillScreen(uint16_t color)
{
  /* Write the whole ST7735S frame memory (GM = "00" -> 132 x 162) instead of
     only the 160 x 80 visible window. This makes a solid fill independent of
     the panel's internal offset and of the MADCTL rotation: whatever sub-window
     the glass actually shows is guaranteed to be painted. If the driver is in a
     smaller addressing mode the extra pixels simply wrap onto already-written
     locations, which is harmless for a single colour. */
  uint32_t pixel_count = (uint32_t)LCD096_FRAME_W * (uint32_t)LCD096_FRAME_H;

  LCD096_SetWindow(0U, 0U, LCD096_FRAME_W - 1U, LCD096_FRAME_H - 1U);
  LCD096_StartRamWrite();

  for (uint32_t i = 0U; i < pixel_count; ++i)
  {
    LCD096_WritePixel(color);
  }

  LCD096_CS_HIGH();
  LCD096_DebugHook(11U);                 /* frame memory fill finished */
}

void LCD096_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  uint32_t pixel_count;

  if ((w == 0U) || (h == 0U))
  {
    return;
  }

  /* Clamp to the panel so a bad caller cannot wrap the address window. */
  if ((x >= LCD096_WIDTH) || (y >= LCD096_HEIGHT))
  {
    return;
  }
  if ((uint32_t)x + w > LCD096_WIDTH)
  {
    w = (uint16_t)(LCD096_WIDTH - x);
  }
  if ((uint32_t)y + h > LCD096_HEIGHT)
  {
    h = (uint16_t)(LCD096_HEIGHT - y);
  }

  pixel_count = (uint32_t)w * (uint32_t)h;

  LCD096_SetWindow(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
  LCD096_StartRamWrite();

  for (uint32_t i = 0U; i < pixel_count; ++i)
  {
    LCD096_WritePixel(color);
  }

  LCD096_CS_HIGH();
}

void LCD096_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                      const uint16_t *pixels)
{
  uint32_t pixel_count = (uint32_t)w * (uint32_t)h;

  if ((w == 0U) || (h == 0U) || (pixels == NULL))
  {
    return;
  }

  LCD096_SetWindow(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
  LCD096_StartRamWrite();

  for (uint32_t i = 0U; i < pixel_count; ++i)
  {
    LCD096_WritePixel(pixels[i]);
  }

  LCD096_CS_HIGH();
}

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
