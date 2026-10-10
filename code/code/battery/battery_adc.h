/* 电池电压 ADC 采集和单点标定接口。 */
#ifndef BATTERY_ADC_H
#define BATTERY_ADC_H
#include "stm32g0xx_hal.h"
uint32_t BatteryAdc_ReadRaw(ADC_HandleTypeDef *hadc, uint32_t timeout_ms);
/* 按 ADC 参考电压和分压比换算电池电压。 */
float BatteryAdc_ReadVoltage(ADC_HandleTypeDef *hadc, float vref, float divider_ratio, uint32_t timeout_ms);
/* 单点标定：ADC=3493 对应实测电池电压 3.648 V。 */
float BatteryAdc_CalibratedVoltage(uint32_t raw);
#endif
