#ifndef W25Q128_H
#define W25Q128_H
#include "stm32g0xx_hal.h"
#define W25Q128_CS_PORT GPIOA
#define W25Q128_CS_PIN GPIO_PIN_2
HAL_StatusTypeDef W25Q128_ReadJedec(SPI_HandleTypeDef*,uint8_t id[3],uint32_t);
HAL_StatusTypeDef W25Q128_Read(SPI_HandleTypeDef*,uint32_t,uint8_t*,uint32_t,uint32_t);
HAL_StatusTypeDef W25Q128_Write(SPI_HandleTypeDef*,uint32_t,const uint8_t*,uint32_t,uint32_t);
HAL_StatusTypeDef W25Q128_EraseSector(SPI_HandleTypeDef*,uint32_t,uint32_t);
#endif
