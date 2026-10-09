#ifndef BATTERY_ADC_H
#define BATTERY_ADC_H
#include "stm32g0xx_hal.h"
uint32_t BatteryAdc_ReadRaw(ADC_HandleTypeDef *hadc, uint32_t timeout_ms);
float BatteryAdc_ReadVoltage(ADC_HandleTypeDef *hadc, float vref, float divider_ratio, uint32_t timeout_ms);
/* Single-point calibration: 3493 counts measured as 3.648 V. */
float BatteryAdc_CalibratedVoltage(uint32_t raw);
#endif
