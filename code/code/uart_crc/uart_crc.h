#ifndef UART_CRC_H
#define UART_CRC_H

#include "stm32g0xx_hal.h"

/* 用于采样行和导出完成信息的 ASCII 停等传输。
 * 串口保持 921600 8E1，需显式启用；默认使用原有 VOFA 输出。 */
void UartCrc_Enable(uint32_t session);
void UartCrc_Disable(void);
uint8_t UartCrc_IsEnabled(void);
uint8_t UartCrc_IsBusy(void);
uint8_t UartCrc_IsFailed(void);
HAL_StatusTypeDef UartCrc_Send(const char *payload);
void UartCrc_Process(void);
uint8_t UartCrc_HandleControl(const char *line);
uint8_t UartCrc_ParseHex8(const char *text, uint32_t *value);

#endif

