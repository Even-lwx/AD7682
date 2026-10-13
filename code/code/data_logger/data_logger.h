/* W25Q128 每分钟历史数据记录。 */
#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include "pcf8563.h"
#include "stm32g0xx_hal.h"

typedef struct
{
    PCF8563_DateTime datetime;
    float temperature_c;
    float humidity_rh;
    float pressure_u7_hpa;
    float pressure_u11_hpa;
    uint16_t adc[4];
} DataLogger_Sample;

HAL_StatusTypeDef DataLogger_Init(SPI_HandleTypeDef *spi, uint32_t timeout);
HAL_StatusTypeDef DataLogger_Append(const DataLogger_Sample *sample,
                                    uint32_t timeout);
HAL_StatusTypeDef DataLogger_Read(uint32_t ordinal,
                                  DataLogger_Sample *sample,
                                  uint32_t timeout);
HAL_StatusTypeDef DataLogger_Clear(uint32_t timeout);
HAL_StatusTypeDef DataLogger_SetInterval(uint32_t seconds, uint32_t timeout);
HAL_StatusTypeDef DataLogger_SetDumpNewestFirst(uint8_t newest_first,
                                                uint32_t timeout);
uint32_t DataLogger_Count(void);
uint32_t DataLogger_Capacity(void);
uint32_t DataLogger_GetInterval(void);
uint8_t DataLogger_GetDumpNewestFirst(void);

#endif /* DATA_LOGGER_H */
