/* W25Q128 历史数据记录；采样间隔和导出顺序保存在 MCU 内部 Flash。
 * Flash 固定记录为 32 字节：温度/湿度/电池电压保留 0.01，压力保留0.1，
 * ADC保留0.1。旧记录仍可读取；旧固件不能正确读取新增字段格式。 */
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
    float battery_voltage;
    float adc[4]; /* Filtered ADC codes; new Flash records preserve 0.1 code. */
} DataLogger_Sample;

HAL_StatusTypeDef DataLogger_Init(SPI_HandleTypeDef *spi, uint32_t timeout);
HAL_StatusTypeDef DataLogger_Append(const DataLogger_Sample *sample,
                                    uint32_t timeout);
HAL_StatusTypeDef DataLogger_Read(uint32_t ordinal,
                                  DataLogger_Sample *sample,
                                  uint32_t timeout);
HAL_StatusTypeDef DataLogger_Clear(uint32_t timeout);
HAL_StatusTypeDef DataLogger_SetIntervalMs(uint32_t milliseconds, uint32_t timeout);
HAL_StatusTypeDef DataLogger_SetDumpNewestFirst(uint8_t newest_first,
                                                uint32_t timeout);
HAL_StatusTypeDef DataLogger_SetRecordingEnabled(uint8_t enabled);
uint8_t DataLogger_GetRecordingEnabled(void);
/* Battery single-point gain, persisted in MCU config page. log clear preserves it. */
float DataLogger_GetBatteryGain(void);
HAL_StatusTypeDef DataLogger_SetBatteryGain(float gain);
uint8_t DataLogger_GetDisplayAdcOnly(void);
/* One MCU Flash record for all menu settings; interval in ms, multiples of 500.
 * Preserves battery calibration; 500..86400000 ms (0.5 seconds..24 hours). */
HAL_StatusTypeDef DataLogger_SetMenuConfig(uint8_t adc_only, uint8_t recording,
                                         uint8_t newest_first, uint32_t interval);
uint32_t DataLogger_Count(void);
uint32_t DataLogger_LastSequence(void);
HAL_StatusTypeDef DataLogger_ReadSequence(uint32_t sequence,
                                         DataLogger_Sample *sample,
                                         uint32_t timeout);
uint32_t DataLogger_Capacity(void);
uint32_t DataLogger_GetIntervalMs(void);
uint8_t DataLogger_GetDumpNewestFirst(void);

#endif /* DATA_LOGGER_H */
