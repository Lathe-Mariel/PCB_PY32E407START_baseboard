/**
  ******************************************************************************
  * @file    lcd096.h
  * @brief   Software (bit-bang) SPI driver for the Pmod-LCD-0.96 v1.0 module
  *          (160x80 RGB565, ST7735S controller).
  *
  *          The module's 12-pin Pmod header only carries
  *            CS, MOSI(SDA), DC(RS), CLK(SCL), GND, VCC
  *          on its signal row; RESET sits on the opposite row, in the position
  *          directly opposite DC (see LCD096_PMOD_ROW below).
  ******************************************************************************
  */

#ifndef __LCD096_H
#define __LCD096_H

#ifdef __cplusplus
extern "C" {
#endif

#include "py32e4xx_hal.h"

/* Panel geometry (native portrait orientation of the ST7735S panel). */
#define LCD096_WIDTH          160U
#define LCD096_HEIGHT         80U

/* Full ST7735S frame memory size (GM[1:0] = "00" -> 132 x 162). The fill uses
   this so a solid colour is independent of the glass offset / addressing mode.

   NOTE: the driver uses COLMOD = 0x05, which is the ST7735S's 18-bit (RGB666)
   interface, so each pixel is THREE bytes on the wire even though the API takes
   a 16-bit RGB565 value (the byte split is done in LCD096_WritePixel()). That is
   what the module's own reference firmware does, and a logic capture of the bus
   confirms 3 bytes/pixel, so it is intentional. */
#define LCD096_FRAME_W        132U
#define LCD096_FRAME_H        162U

/* -------------------------------------------------------------------------- */
/* Pin map                                                                    */
/* -------------------------------------------------------------------------- */
/*
   A Pmod module can be plugged into a Pmod socket EITHER WAY ROUND, and both
   insertions are electrically valid. This driver therefore selects the mapping
   with LCD096_PMOD_ROW (or LCD096_SetPmodRow() at runtime) instead of
   hard-coding one of them.

   Base board Pmod socket J3 (silkscreen "I2C3"), pad -> net -> MCU:
     row 1 : pin1 /Pmod3_1=PD7, pin2 /Pmod3_2=PB6, pin3 /C8=PC8,
             pin4 /C9    =PC9, pin5 GND,          pin6 +3V3
     row 2 : pin7 /Pmod3_5=PD3, pin8 /Pmod3_6=PD5, pin9 /Pmod3_7=PD6,
             pin10 /Pmod3_8=PD4, pin11 GND,         pin12 +3V3

   Pmod-LCD-0.96 module header J2 (2x6 Pmod, pins 1-6 one row / 7-12 the other):
     pin 1,2,3          = NC, NC, NC
     pin 4,5,6          = RESET, GND, VCC
     pin 7,8,9,10,11,12 = VCC, GND, CLK, DC, MOSI, CS
   So the module's signal row is CS, MOSI, DC, CLK, GND, VCC and its RESET is
   on the OTHER row, directly OPPOSITE DC (not opposite CLK).

   Base board J3 pads are paired (1,7)(2,8)(3,9)(4,10)(5,11)(6,12). Putting the
   module's signal row on J3 row 1 gives CS=PD7 MOSI=PB6 DC=PC8 CLK=PC9, and
   RESET then sits on the module's pin 4 -> J3 pin 9 -> PD6. That is the
   insertion the base board silkscreen labels describe.

   Row 1 insertion (default, matches the base board silkscreen labels)
     CS   -> J3 pin1  = PD7
     MOSI -> J3 pin2  = PB6
     DC   -> J3 pin3  = PC8
     CLK  -> J3 pin4  = PC9
     RST  -> J3 pin9  = PD6      <-- NOT PD4!
   Row 2 insertion (module rotated 180 deg)
     CS   -> J3 pin7  = PD3
     MOSI -> J3 pin8  = PD5
     DC   -> J3 pin9  = PD6
     CLK  -> J3 pin10 = PD4
     RST  -> J3 pin3  = PC8

   NOTE: J3 pin10 (PD4) is NOT the module's RESET. In BOTH insertions it lines
   up with the module's pin 3, which is unused. Driving it changes nothing.
*/
/* Row 1 insertion (default): CS=PD7, MOSI=PB6, DC=PC8, CLK=PC9, RESET=PD6. */
#define LCD096_R1_CS_PORT     GPIOD
#define LCD096_R1_CS_PIN      GPIO_PIN_7      /* J3 pin1  */
#define LCD096_R1_SDA_PORT    GPIOB
#define LCD096_R1_SDA_PIN     GPIO_PIN_6      /* J3 pin2  */
#define LCD096_R1_DC_PORT     GPIOC
#define LCD096_R1_DC_PIN      GPIO_PIN_8      /* J3 pin3  */
#define LCD096_R1_SCK_PORT    GPIOC
#define LCD096_R1_SCK_PIN     GPIO_PIN_9      /* J3 pin4  */
#define LCD096_R1_RST_PORT    GPIOD
#define LCD096_R1_RST_PIN     GPIO_PIN_6      /* J3 pin9  */

/* Row 2 insertion (module rotated 180 deg): CS=PD3, MOSI=PD5, DC=PD6,
   CLK=PD4, RESET=PC8. */
#define LCD096_R2_CS_PORT     GPIOD
#define LCD096_R2_CS_PIN      GPIO_PIN_3      /* J3 pin7  */
#define LCD096_R2_SDA_PORT    GPIOD
#define LCD096_R2_SDA_PIN     GPIO_PIN_5      /* J3 pin8  */
#define LCD096_R2_DC_PORT     GPIOD
#define LCD096_R2_DC_PIN      GPIO_PIN_6      /* J3 pin9  */
#define LCD096_R2_SCK_PORT    GPIOD
#define LCD096_R2_SCK_PIN     GPIO_PIN_4      /* J3 pin10 */
#define LCD096_R2_RST_PORT    GPIOC
#define LCD096_R2_RST_PIN     GPIO_PIN_8      /* J3 pin3  */

/* Insertion used by LCD096_Init() unless LCD096_SetPmodRow() says otherwise.
   Row 2 = CS=PD3 MOSI=PD5 DC=PD6 CLK=PD4 RST=PC8. */
#ifndef LCD096_PMOD_ROW
#define LCD096_PMOD_ROW       2U
#endif


/* -------------------------------------------------------------------------- */
/* Panel tuning                                                               */
/* -------------------------------------------------------------------------- */
/* MADCTL (0x36): 0x00 = no mirror / RGB order. Some 0.96" ST7735S panels need
   0xA0 (mirror X+Y) or 0xC0 (mirror Y) to show the visible 160x80 window in the
   right place. Harmless for a solid fill, matters for text / images. */
#ifndef LCD096_MADCTL_VALUE
#define LCD096_MADCTL_VALUE   0x00U
#endif

/* Display inversion. 0 = INVOFF (0x20), 1 = INVON (0x21). Colour-inverted
   panels (common on the small IPS-like modules) need 1; without it a white fill
   shows up black. */
#ifndef LCD096_USE_INVON
#define LCD096_USE_INVON      1
#endif

/* -------------------------------------------------------------------------- */
/* Common colours (RGB565)                                                    */
/* -------------------------------------------------------------------------- */
#define LCD096_BLACK          0x0000U
#define LCD096_WHITE          0xFFFFU
#define LCD096_RED            0xF800U
#define LCD096_GREEN          0x07E0U
#define LCD096_BLUE           0x001FU
#define LCD096_ORANGE         0xFC00U
#define LCD096_YELLOW         0xFFE0U
#define LCD096_CYAN           0x07FFU
#define LCD096_MAGENTA        0xF81FU

/* -------------------------------------------------------------------------- */
/* API                                                                        */
/* -------------------------------------------------------------------------- */

/**
  * @brief  Select which Pmod insertion the driver talks to.
  * @param  row 1 = CS=PD7/MOSI=PB6/DC=PC8/CLK=PC9/RST=PD4 (the default),
  *             2 = CS=PD3/MOSI=PD5/DC=PD6/CLK=PD4/RST=PC9 (module rotated).
  * @note   Call LCD096_Init() (and re-fill) after switching.
  */
void LCD096_SetPmodRow(uint8_t row);

/**
  * @brief  Initialise the LCD (GPIO + ST7735S power-on sequence).
  * @note   Leaves the panel display-on with the frame memory containing
  *         undefined data. Call LCD096_FillScreen() afterwards.
  */
void LCD096_Init(void);

/**
  * @brief  Bring-up aid: configure the five LCD GPIOs and drive them HIGH one at
  *         a time, so each pin can be checked with a meter/LED without a scope.
  * @param  hold_ms how long each pin stays HIGH (and the others stay LOW).
  * @note   Reports the pin under test through LCD096_DebugHook() as stage
  *         81=CS, 82=SDA(MOSI), 83=DC, 84=SCK, 85=RST. Leaves the bus in its
  *         idle state (CS high, SCK/SDA low, DC high, RST high) when done.
  */
void LCD096_PinSelfTest(uint32_t hold_ms);

/**
  * @brief  Fill the whole panel with a single RGB565 colour.
  * @param  color RGB565 colour value.
  */
void LCD096_FillScreen(uint16_t color);

/**
  * @brief  Fill an axis aligned rectangle with a single RGB565 colour.
  * @param  x,y   Top-left corner (0-based).
  * @param  w,h   Width / height in pixels (must be >= 1).
  * @param  color RGB565 colour value.
  */
void LCD096_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
  * @brief  Blit an RGB565 bitmap into a (x,y)-(x+w-1,y+h-1) window.
  * @param  pixels Row-major RGB565 pixel array, w*h entries.
  */
void LCD096_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                      const uint16_t *pixels);

#ifdef __cplusplus
}
#endif

#endif /* __LCD096_H */

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/
