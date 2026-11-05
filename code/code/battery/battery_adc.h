/* 电池电压 ADC 采集和换算接口。 */
#ifndef BATTERY_ADC_H
#define BATTERY_ADC_H
#include "stm32g0xx_hal.h"
/**
 * @brief 兼容旧接口读取电池 ADC；失败返回 0，需区分错误时使用 ReadRawChecked。
 * @param hadc 内部 ADC 句柄。
 * @param timeout_ms 单次事务超时，单位 ms。
 * @return ADC 原始码；读取失败也返回 0。
 */
uint32_t BatteryAdc_ReadRaw(ADC_HandleTypeDef *hadc, uint32_t timeout_ms);
/**
 * @brief 启动内部 ADC、等待转换并停止 ADC，成功后提交原始码。
 * @param hadc 内部 ADC 句柄。
 * @param raw ADC 原始码；指针形式用于接收输出。
 * @param timeout_ms 单次事务超时，单位 ms。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef BatteryAdc_ReadRawChecked(ADC_HandleTypeDef *hadc,
                                          uint32_t *raw, uint32_t timeout_ms);
/* 按 ADC 参考电压和分压比换算电池电压。 */
/**
 * @brief 读取内部 ADC，按参考电压和电阻分压比换算电池端电压。
 * @param hadc 内部 ADC 句柄。
 * @param vref ADC 参考电压，单位 V。
 * @param divider_ratio 电池端电压与 ADC 引脚电压的比例。
 * @param timeout_ms 单次事务超时，单位 ms。
 * @return 计算后的值，量纲与用途说明一致。
 */
float BatteryAdc_ReadVoltage(ADC_HandleTypeDef *hadc, float vref, float divider_ratio, uint32_t timeout_ms);
/* MCU参考3.3 V、12位ADC，上臂100k/下臂47k；返回电池端电压。
 * 保留旧接口名称，已停止使用旧板的单点标定系数。 */
/**
 * @brief 按 3.3 V 参考、12 位 ADC 和 100k/47k 分压电阻计算电池电压。
 * @param raw ADC 原始码；指针形式用于接收输出。
 * @return 计算后的值，量纲与用途说明一致。
 */
float BatteryAdc_CalibratedVoltage(uint32_t raw);
#endif
