#include "battery_adc.h"

/* 单点标定：3493 个 ADC 计数对应实测电池电压 3.648 V。 */
#define BATTERY_ADC_CAL_GAIN_V_PER_COUNT (0.001044374f)

uint32_t BatteryAdc_ReadRaw(ADC_HandleTypeDef *handle, uint32_t timeout)
{
    uint32_t value;

    if (HAL_ADC_Start(handle) != HAL_OK)
    {
        return 0U;
    }
    if (HAL_ADC_PollForConversion(handle, timeout) != HAL_OK)
    {
        (void)HAL_ADC_Stop(handle);
        return 0U;
    }
    value = HAL_ADC_GetValue(handle);
    (void)HAL_ADC_Stop(handle);
    return value;
}

float BatteryAdc_ReadVoltage(ADC_HandleTypeDef *handle, float vref,
                             float divider_ratio, uint32_t timeout)
{
    uint32_t raw = BatteryAdc_ReadRaw(handle, timeout);
    return ((float)raw * vref / 4095.0f) * divider_ratio;
}

float BatteryAdc_CalibratedVoltage(uint32_t raw)
{
    return (float)raw * BATTERY_ADC_CAL_GAIN_V_PER_COUNT;
}
