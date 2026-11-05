/* 电池电压 ADC 采集和换算接口。 */
#ifndef BATTERY_ADC_H
#define BATTERY_ADC_H
#include "stm32g0xx_hal.h"
uint32_t BatteryAdc_ReadRaw(ADC_HandleTypeDef *hadc, uint32_t timeout_ms);
HAL_StatusTypeDef BatteryAdc_ReadRawChecked(ADC_HandleTypeDef *hadc,
                                          uint32_t *raw, uint32_t timeout_ms);
/* 按 ADC 参考电压和分压比换算电池电压。 */
float BatteryAdc_ReadVoltage(ADC_HandleTypeDef *hadc, float vref, float divider_ratio, uint32_t timeout_ms);
/* MCU参考3.3 V、12位ADC，上臂100k/下臂47k；返回电池端电压。
 * 保留旧接口名称，已停止使用旧板的单点标定系数。 */
float BatteryAdc_CalibratedVoltage(uint32_t raw);
#endif
