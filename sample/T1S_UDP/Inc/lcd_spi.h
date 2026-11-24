#ifndef LCD_SPI_H
#define LCD_SPI_H

#include <stdint.h>

void LCD_SPI_Init(void);
void LCD_SPI_Clear(void);
void LCD_SPI_DrawRgb888(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                        const uint8_t *pixels);

#endif
