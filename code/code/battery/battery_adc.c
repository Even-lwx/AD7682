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

/**
 * @brief 兼容旧接口读取电池 ADC；失败返回 0，需区分错误时使用 ReadRawChecked。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param timeout 底层事务超时，单位 ms；不是整个上层操作的总时间上限。
 * @return ADC 原始码；读取失败也返回 0。
 */
uint32_t BatteryAdc_ReadRaw(ADC_HandleTypeDef *handle, uint32_t timeout)
{
    uint32_t value = 0U;
    (void)BatteryAdc_ReadRawChecked(handle, &value, timeout);
    return value;
}

/**
 * @brief 启动内部 ADC、等待转换并停止 ADC，成功后提交原始码。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param raw ADC 原始码；指针形式用于接收输出。
 * @param timeout 底层事务超时，单位 ms；不是整个上层操作的总时间上限。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
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

/**
 * @brief 读取内部 ADC，按参考电压和电阻分压比换算电池端电压。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param vref ADC 参考电压，单位 V。
 * @param divider_ratio 电池端电压与 ADC 引脚电压的比例。
 * @param timeout 底层事务超时，单位 ms；不是整个上层操作的总时间上限。
 * @return 计算后的值，量纲与用途说明一致。
 */
float BatteryAdc_ReadVoltage(ADC_HandleTypeDef *handle, float vref,
                             float divider_ratio, uint32_t timeout)
{
    /* 适用于已知 VDDA 和电阻分压比的理论换算。 */
    uint32_t raw = BatteryAdc_ReadRaw(handle, timeout);
    return ((float)raw * vref / 4095.0f) * divider_ratio;
}

/**
 * @brief 按 3.3 V 参考、12 位 ADC 和 100k/47k 分压电阻计算电池电压。
 * @param raw ADC 原始码；指针形式用于接收输出。
 * @return 计算后的值，量纲与用途说明一致。
 */
float BatteryAdc_CalibratedVoltage(uint32_t raw)
{
    /* 保留接口名；现按3.3 V及100k/47k分压输出电池端电压。 */
    return (float)raw * BATTERY_ADC_CAL_GAIN_V_PER_COUNT;
}
