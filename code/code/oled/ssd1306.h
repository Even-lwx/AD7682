#ifndef SSD1306_H
#define SSD1306_H

#include "stm32g0xx_hal.h"

/* 第 1.2 版引脚映射：SPI2 PA0=SCLK、PA4=DIN；PB0=CS#、PB1=D/C#、PB10=RES#。 */
HAL_StatusTypeDef SSD1306_Init(SPI_HandleTypeDef *spi);
/* 仅允许主循环调用，BeginFrame 的目标频率为 10 Hz（帧起点间隔 100 ms）。 */
uint8_t SSD1306_BeginFrame(uint32_t now);
/* 响应用户输入时在主循环立即重绘，并替换尚未完成的旧帧。 */
void SSD1306_BeginUiFrame(uint32_t now);
void SSD1306_SetLine(uint8_t row, const char *text);
/* 二倍字体：四行，每行高 16 像素、宽 10 个字符；在构建帧时设置。 */
void SSD1306_SetLargeText(uint8_t enabled);
/* 仪表盘共五行，行距为 13 像素，字模宽度加倍。 */
void SSD1306_SetDashboard(void);
/* 每次调用最多发送一个 128 字节页面；不使用帧缓冲区，也不在中断中访问 SPI。 */
void SSD1306_Process(uint32_t now);

#endif

