#ifndef SSD1306_H
#define SSD1306_H

#include "stm32g0xx_hal.h"

/* 第 1.2 版引脚映射：SPI2 PA0=SCLK、PA4=DIN；PB0=CS#、PB1=D/C#、PB10=RES#。 */
/**
 * @brief 绑定 SPI、复位 OLED 并发送显示控制器初始化命令。
 * @param spi SPI 外设句柄，须已完成初始化。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef SSD1306_Init(SPI_HandleTypeDef *spi);
/* 仅允许主循环调用，BeginFrame 的目标频率为 10 Hz（帧起点间隔 100 ms）。 */
/**
 * @brief 在刷新周期到达且旧帧已完成时开始普通显示帧。
 * @param now 当前 HAL 毫秒时间戳。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 */
uint8_t SSD1306_BeginFrame(uint32_t now);
/* 响应用户输入时在主循环立即重绘，并替换尚未完成的旧帧。 */
/**
 * @brief 立即开始用户操作触发的新帧，替换未完成的旧显示帧。
 * @param now 当前 HAL 毫秒时间戳。
 * @return 无返回值。
 */
void SSD1306_BeginUiFrame(uint32_t now);
/**
 * @brief 复制一行文本到显示任务的行缓存，供后续逐页绘制。
 * @param row 文本显示行编号，从 0 开始。
 * @param text 以零结尾的文本或命令字符串。
 * @return 无返回值。
 */
void SSD1306_SetLine(uint8_t row, const char *text);
/* 二倍字体：四行，每行高 16 像素、宽 10 个字符；在构建帧时设置。 */
/**
 * @brief 设置当前帧是否采用二倍大字体。
 * @param enabled 功能开关或模式编码，具体取值见函数用途。
 * @return 无返回值。
 */
void SSD1306_SetLargeText(uint8_t enabled);
/* 仪表盘共五行，行距为 13 像素，字模宽度加倍。 */
/**
 * @brief 将当前帧切换为五行仪表盘显示布局。
 * @return 无返回值。
 */
void SSD1306_SetDashboard(void);
/* 每次调用最多发送一个 128 字节页面；不使用帧缓冲区，也不在中断中访问 SPI。 */
/**
 * @brief 在主循环生成并发送至多一个显示页，避免长时间占用 SPI。
 * @param now 当前 HAL 毫秒时间戳。
 * @return 无返回值。
 */
void SSD1306_Process(uint32_t now);

#endif

