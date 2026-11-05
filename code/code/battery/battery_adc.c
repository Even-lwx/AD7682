#include "battery_adc.h"

/* 电池电压采集：MCU 12-bit ADC，VDDA/VREF+ = 3.3 V。 */

/* 分压接线：电源 -- 100 kOhm -- ADC -- 47 kOhm -- GND。
 * MCU 参考电压已改变，旧板单点标定不再适用，按3.3 V理论值换算。
 * AD7682 的外部5 V参考不参与此计算。 */
#define BATTERY_DIVIDER_TOP_KOHM       100.0f
#define BATTERY_DIVIDER_BOTTOM_KOHM     47.0f
#define BATTERY_DIVIDER_RATIO \
    ((BATTERY_DIVIDER_TOP_KOHM + BATTERY_DIVIDER_BOTTOM_KOHM) / \
     BATTERY_DIVIDER_BOTTOM_KOHM)
#define BATTERY_ADC_VREF_VOLTS          3.3f
#define BATTERY_ADC_PIN_GAIN_V_PER_COUNT (BATTERY_ADC_VREF_VOLTS / 4095.0f)
#define BATTERY_ADC_CAL_GAIN_V_PER_COUNT \
    (BATTERY_ADC_PIN_GAIN_V_PER_COUNT * BATTERY_DIVIDER_RATIO)

uint32_t BatteryAdc_ReadRaw(ADC_HandleTypeDef *handle, uint32_t timeout)
{
    uint32_t value = 0U;
    (void)BatteryAdc_ReadRawChecked(handle, &value, timeout);
    return value;
}

HAL_StatusTypeDef BatteryAdc_ReadRawChecked(ADC_HandleTypeDef *handle,
                                          uint32_t *raw, uint32_t timeout)
{
    /* 启动 ADC，等待一次转换，读取后及时停止 ADC。 */
    HAL_StatusTypeDef status;

    if (handle == NULL || raw == NULL) { return HAL_ERROR; }
    status = HAL_ADC_Start(handle);
    if (status != HAL_OK) { return status; }
    status = HAL_ADC_PollForConversion(handle, timeout);
    if (status != HAL_OK)
    {
        (void)HAL_ADC_Stop(handle);
        return status;
    }
    *raw = HAL_ADC_GetValue(handle);
    (void)HAL_ADC_Stop(handle);
    return HAL_OK;
}

float BatteryAdc_ReadVoltage(ADC_HandleTypeDef *handle, float vref,
                             float divider_ratio, uint32_t timeout)
{
    /* 适用于已知 VDDA 和电阻分压比的理论换算。 */
    uint32_t raw = BatteryAdc_ReadRaw(handle, timeout);
    return ((float)raw * vref / 4095.0f) * divider_ratio;
}

float BatteryAdc_CalibratedVoltage(uint32_t raw)
{
    /* 保留接口名；现按3.3 V及100k/47k分压输出电池端电压。 */
    return (float)raw * BATTERY_ADC_CAL_GAIN_V_PER_COUNT;
}
