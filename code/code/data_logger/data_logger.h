/* W25Q128 历史数据记录；采样间隔和导出顺序保存在 MCU 内部 Flash。
 * Flash 固定记录为 32 字节：温度/湿度/ADC 保留 0.01，压力保留 0.1。
 * 新固件兼容旧整数 ADC 记录；旧固件不能读取新的小数 ADC 记录。 */
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
    float adc[4]; /* Filtered ADC codes; Flash preserves 0.01 code. */
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
HAL_StatusTypeDef DataLogger_SetRecordingEnabled(uint8_t enabled);
uint8_t DataLogger_GetRecordingEnabled(void);
uint32_t DataLogger_Count(void);
uint32_t DataLogger_LastSequence(void);
HAL_StatusTypeDef DataLogger_ReadSequence(uint32_t sequence,
                                         DataLogger_Sample *sample,
                                         uint32_t timeout);
uint32_t DataLogger_Capacity(void);
uint32_t DataLogger_GetInterval(void);
uint8_t DataLogger_GetDumpNewestFirst(void);

#endif /* DATA_LOGGER_H */
