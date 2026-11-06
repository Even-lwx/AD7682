#ifndef TEST_HAL_H
#define TEST_HAL_H
#include <stdint.h>
typedef int HAL_StatusTypeDef;
#define HAL_OK 0
#define HAL_ERROR 1
#define HAL_BUSY 2
typedef int UART_HandleTypeDef;
extern UART_HandleTypeDef huart1;
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef*,uint8_t*,uint16_t,uint32_t);
uint32_t HAL_GetTick(void);
#endif
