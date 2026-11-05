#ifndef UART_CRC_H
#define UART_CRC_H

#include "stm32g0xx_hal.h"

/* ASCII stop-and-wait transport for sample rows and dump completion.
 * UART remains 921600 8E1. Enable explicitly; legacy VOFA output is default. */
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
