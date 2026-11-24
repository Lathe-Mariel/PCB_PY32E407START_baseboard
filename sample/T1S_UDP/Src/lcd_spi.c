#include "main.h"
#include "lcd_spi.h"

#define LCD_CS_PORT        GPIOE
#define LCD_CS_PIN         GPIO_PIN_13
#define LCD_MOSI_PORT      GPIOE
#define LCD_MOSI_PIN       GPIO_PIN_11
#define LCD_DC_PORT        GPIOE
#define LCD_DC_PIN         GPIO_PIN_8
#define LCD_SCK_PORT       GPIOE
#define LCD_SCK_PIN        GPIO_PIN_7
#define LCD_RESET_PORT     GPIOB
#define LCD_RESET_PIN      GPIO_PIN_2

static void LCD_WriteByte(uint8_t value)
{
  volatile uint32_t *bsrr = &LCD_SCK_PORT->BSRR;
  const uint32_t clockLow = (uint32_t)LCD_SCK_PIN << 16U;
  const uint32_t clockHigh = (uint32_t)LCD_SCK_PIN;
  const uint32_t mosiLow = (uint32_t)LCD_MOSI_PIN << 16U;
  const uint32_t mosiHigh = (uint32_t)LCD_MOSI_PIN;

  for (uint8_t bit = 0U; bit < 8U; bit++)
  {
    const uint32_t mosi = ((value & 0x80U) != 0U) ? mosiHigh : mosiLow;

    *bsrr = clockLow | mosi;
    *bsrr = clockHigh;
    value <<= 1U;
  }

  *bsrr = clockHigh;
}

static void LCD_WriteCommand(uint8_t command)
{
  HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);
  LCD_WriteByte(command);
  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
}

static void LCD_WriteCommandData(uint8_t command, const uint8_t *data, uint32_t length)
{
  uint32_t i;

  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_RESET);
  LCD_WriteByte(command);
  HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET);
  for (i = 0U; i < length; i++)
  {
    LCD_WriteByte(data[i]);
  }
  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
}

static void LCD_SetWindow(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
  uint16_t xEnd = (uint16_t)(x + width - 1U);
  uint16_t yEnd = (uint16_t)(y + height - 1U);
  const uint8_t columns[4] = {
    (uint8_t)(x >> 8U), (uint8_t)x, (uint8_t)(xEnd >> 8U), (uint8_t)xEnd
  };
  const uint8_t rows[4] = {
    (uint8_t)(y >> 8U), (uint8_t)y, (uint8_t)(yEnd >> 8U), (uint8_t)yEnd
  };

  LCD_WriteCommandData(0x2AU, columns, sizeof(columns));
  LCD_WriteCommandData(0x2BU, rows, sizeof(rows));
}

static void LCD_StartMemoryWrite(void)
{
  LCD_WriteCommand(0x2CU);
  HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);
}

static void LCD_WriteRgb888(const uint8_t *pixel)
{
  uint16_t rgb565 = (uint16_t)(((uint16_t)(pixel[0] & 0xF8U) << 8U) |
                               ((uint16_t)(pixel[1] & 0xFCU) << 3U) |
                               ((uint16_t)pixel[2] >> 3U));
  LCD_WriteByte((uint8_t)(rgb565 >> 8U));
  LCD_WriteByte((uint8_t)rgb565);
}

void LCD_SPI_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  gpio.Pin = LCD_CS_PIN;
  HAL_GPIO_Init(LCD_CS_PORT, &gpio);
  gpio.Pin = LCD_MOSI_PIN;
  HAL_GPIO_Init(LCD_MOSI_PORT, &gpio);
  gpio.Pin = LCD_SCK_PIN;
  HAL_GPIO_Init(LCD_SCK_PORT, &gpio);
  gpio.Pin = LCD_DC_PIN;
  HAL_GPIO_Init(LCD_DC_PORT, &gpio);
  gpio.Pin = LCD_RESET_PIN;
  HAL_GPIO_Init(LCD_RESET_PORT, &gpio);

  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_SCK_PORT, LCD_SCK_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_MOSI_PORT, LCD_MOSI_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_RESET_PORT, LCD_RESET_PIN, GPIO_PIN_SET);
  HAL_Delay(10U);
  HAL_GPIO_WritePin(LCD_RESET_PORT, LCD_RESET_PIN, GPIO_PIN_RESET);
  HAL_Delay(15U);
  HAL_GPIO_WritePin(LCD_RESET_PORT, LCD_RESET_PIN, GPIO_PIN_SET);
  HAL_Delay(150U);

  LCD_WriteCommand(0x01U);
  HAL_Delay(120U);
  LCD_WriteCommandData(0xEFU, (const uint8_t[]){0x03U, 0x80U, 0x02U}, 3U);
  LCD_WriteCommandData(0xCFU, (const uint8_t[]){0x00U, 0xC1U, 0x30U}, 3U);
  LCD_WriteCommandData(0xEDU, (const uint8_t[]){0x64U, 0x03U, 0x12U, 0x81U}, 4U);
  LCD_WriteCommandData(0xE8U, (const uint8_t[]){0x85U, 0x00U, 0x78U}, 3U);
  LCD_WriteCommandData(0xCBU,
                       (const uint8_t[]){0x39U, 0x2CU, 0x00U, 0x34U, 0x02U}, 5U);
  LCD_WriteCommandData(0xF7U, (const uint8_t[]){0x20U}, 1U);
  LCD_WriteCommandData(0xEAU, (const uint8_t[]){0x00U, 0x00U}, 2U);
  LCD_WriteCommandData(0xC0U, (const uint8_t[]){0x23U}, 1U);
  LCD_WriteCommandData(0xC1U, (const uint8_t[]){0x10U}, 1U);
  LCD_WriteCommandData(0xC5U, (const uint8_t[]){0x3EU, 0x28U}, 2U);
  LCD_WriteCommandData(0xC7U, (const uint8_t[]){0x86U}, 1U);
  LCD_WriteCommandData(0x36U, (const uint8_t[]){0x28U}, 1U);
  LCD_WriteCommandData(0x37U, (const uint8_t[]){0x00U}, 1U);
  LCD_WriteCommandData(0x3AU, (const uint8_t[]){0x55U}, 1U);
  LCD_WriteCommandData(0xB1U, (const uint8_t[]){0x00U, 0x18U}, 2U);
  LCD_WriteCommandData(0xB6U, (const uint8_t[]){0x08U, 0x82U, 0x27U}, 3U);
  LCD_WriteCommandData(0xF2U, (const uint8_t[]){0x00U}, 1U);
  LCD_WriteCommandData(0x26U, (const uint8_t[]){0x01U}, 1U);
  LCD_WriteCommandData(0xE0U,
                       (const uint8_t[]){0x0FU, 0x31U, 0x2BU, 0x0CU, 0x0EU,
                                         0x08U, 0x4EU, 0xF1U, 0x37U, 0x07U,
                                         0x10U, 0x03U, 0x0EU, 0x09U, 0x00U}, 15U);
  LCD_WriteCommandData(0xE1U,
                       (const uint8_t[]){0x00U, 0x0EU, 0x14U, 0x03U, 0x11U,
                                         0x07U, 0x31U, 0xC1U, 0x48U, 0x08U,
                                         0x0FU, 0x0CU, 0x31U, 0x36U, 0x0FU}, 15U);
  LCD_WriteCommand(0x11U);
  HAL_Delay(120U);
  LCD_WriteCommand(0x29U);
  HAL_Delay(20U);
}

void LCD_SPI_Clear(void)
{
  uint32_t i;
  const uint8_t black[3] = {0U, 0U, 0U};

  LCD_SetWindow(0U, 0U, LCD_SPI_WIDTH, LCD_SPI_HEIGHT);
  LCD_StartMemoryWrite();
  for (i = 0U; i < (LCD_SPI_WIDTH * LCD_SPI_HEIGHT); i++)
  {
    LCD_WriteRgb888(black);
  }
  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
}

void LCD_SPI_DrawRgb888(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                        const uint8_t *pixels)
{
  uint16_t sourceWidth = width;
  uint16_t row;
  uint16_t column;

  if ((pixels == NULL) || (width == 0U) || (height == 0U) ||
      (x >= LCD_SPI_WIDTH) || (y >= LCD_SPI_HEIGHT))
  {
    return;
  }
  if ((uint32_t)x + width > LCD_SPI_WIDTH)
  {
    width = (uint16_t)(LCD_SPI_WIDTH - x);
  }
  if ((uint32_t)y + height > LCD_SPI_HEIGHT)
  {
    height = (uint16_t)(LCD_SPI_HEIGHT - y);
  }

  LCD_SetWindow(x, y, width, height);
  LCD_StartMemoryWrite();
  for (row = 0U; row < height; row++)
  {
    const uint8_t *pixel = pixels + ((uint32_t)row * sourceWidth * 3U);
    for (column = 0U; column < width; column++)
    {
      LCD_WriteRgb888(pixel);
      pixel += 3U;
    }
  }
  HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
}
