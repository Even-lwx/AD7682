#ifndef SSD1306_H
#define SSD1306_H

#include "stm32g0xx_hal.h"

/* Ver1.2: SPI2 PA0=SCLK, PA4=DIN; PB0=CS#, PB1=D/C#, PB10=RES#. */
HAL_StatusTypeDef SSD1306_Init(SPI_HandleTypeDef *spi);
/* Main-loop only. BeginFrame targets 10 Hz (100 ms between frame starts). */
uint8_t SSD1306_BeginFrame(uint32_t now);
/* Immediate main-loop redraw for user input; replaces any partial old frame. */
void SSD1306_BeginUiFrame(uint32_t now);
void SSD1306_SetLine(uint8_t row, const char *text);
/* 2x font: four 16-pixel rows, ten characters per row. Set during frame build. */
void SSD1306_SetLargeText(uint8_t enabled);
/* Five dashboard rows with a 13-pixel pitch, double-width glyphs. */
void SSD1306_SetDashboard(void);
/* Sends at most one 128-byte page per call; no framebuffer or ISR SPI access. */
void SSD1306_Process(uint32_t now);

#endif
