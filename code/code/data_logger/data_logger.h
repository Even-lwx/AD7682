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
    float adc[4]; /* 滤波后的 ADC 码值；新的 Flash 记录保留 0.1 码精度。 */
} DataLogger_Sample;

/**
 * @brief 绑定 Flash SPI，恢复元数据和配置，扫描有效记录以重建环形日志状态。
 * @param spi SPI 外设句柄，须已完成初始化。
 * @param timeout 底层事务超时，单位 ms；不是整个上层操作的总时间上限。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef DataLogger_Init(SPI_HandleTypeDef *spi, uint32_t timeout);
/**
 * @brief 编码并追加一条日志，按扇区擦除策略更新环形写指针和元数据。
 * @param sample 传感器快照；const 指针为待写入输入，其他指针为读取输出。
 * @param timeout 底层事务超时，单位 ms；不是整个上层操作的总时间上限。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef DataLogger_Append(const DataLogger_Sample *sample,
                                    uint32_t timeout);
/**
 * @brief 按当前导出顺序读取相对序号对应的记录，并校验记录 CRC。
 * @param ordinal 从 0 开始的相对读取位置，顺序由导出设置决定。
 * @param sample 传感器快照；const 指针为待写入输入，其他指针为读取输出。
 * @param timeout 底层事务超时，单位 ms；不是整个上层操作的总时间上限。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef DataLogger_Read(uint32_t ordinal,
                                  DataLogger_Sample *sample,
                                  uint32_t timeout);
/**
 * @brief 清空日志元数据及运行时索引，保留内部 Flash 的设备配置。
 * @param timeout 底层事务超时，单位 ms；不是整个上层操作的总时间上限。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef DataLogger_Clear(uint32_t timeout);
/**
 * @brief 校验并持久化记录间隔；合法值为 500～86400000 ms 且为 500 的倍数。
 * @param milliseconds 记录间隔，单位 ms。
 * @param timeout 底层事务超时，单位 ms；不是整个上层操作的总时间上限。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef DataLogger_SetIntervalMs(uint32_t milliseconds, uint32_t timeout);
/**
 * @brief 设置并保存历史读取顺序，保存失败时恢复原设置。
 * @param newest_first 0 从旧到新，非 0 从新到旧。
 * @param timeout 底层事务超时，单位 ms；不是整个上层操作的总时间上限。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef DataLogger_SetDumpNewestFirst(uint8_t newest_first,
                                                uint32_t timeout);
/**
 * @brief 设置并持久化记录开关，保存失败时恢复原值。
 * @param enabled 功能开关或模式编码，具体取值见函数用途。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef DataLogger_SetRecordingEnabled(uint8_t enabled);
/**
 * @brief 读取当前记录开关。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 */
uint8_t DataLogger_GetRecordingEnabled(void);
/* 电池单点增益，保存在 MCU 配置页中。log clear 不会清除该值。 */
/**
 * @brief 读取配置中保存的电池校准增益。
 * @return 电池校准增益。
 */
float DataLogger_GetBatteryGain(void);
/**
 * @brief 保存 0.5～1.5 范围的电池单点增益；失败时恢复旧值。
 * @param gain 电池单点校准增益，允许 0.5～1.5。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef DataLogger_SetBatteryGain(float gain);
/**
 * @brief 读取是否仅显示 ADC 的菜单配置。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 */
uint8_t DataLogger_GetDisplayAdcOnly(void);
/* 所有菜单设置共用一条 MCU Flash 记录；间隔单位为 ms，且为 500 的倍数。
 * 保留电池校准值；范围为 500～86400000 ms（0.5 秒～24 小时）。 */
/**
 * @brief 统一校验并保存显示模式、记录开关、导出顺序和间隔；失败回滚运行时配置。
 * @param adc_only 0 显示全部，1 仅显示 ADC。
 * @param recording 记录开关，0 关闭、1 开启。
 * @param newest_first 0 从旧到新，非 0 从新到旧。
 * @param interval 记录间隔，单位 ms，按 500 ms 步进。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef DataLogger_SetMenuConfig(uint8_t adc_only, uint8_t recording,
                                         uint8_t newest_first, uint32_t interval);
/**
 * @brief 读取当前环形日志的有效记录数量。
 * @return 当前有效记录数。
 */
uint32_t DataLogger_Count(void);
/**
 * @brief 读取最近一条日志的绝对序号，用于建立导出快照。
 * @return 最近记录的绝对序号。
 */
uint32_t DataLogger_LastSequence(void);
/**
 * @brief 按绝对序号读取日志，核对存储序号及 CRC，拒绝已被覆盖的记录。
 * @param sequence 日志或链路的绝对序号。
 * @param sample 传感器快照；const 指针为待写入输入，其他指针为读取输出。
 * @param timeout 底层事务超时，单位 ms；不是整个上层操作的总时间上限。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef DataLogger_ReadSequence(uint32_t sequence,
                                         DataLogger_Sample *sample,
                                         uint32_t timeout);
/**
 * @brief 返回外部 Flash 数据区可容纳的最大记录条数。
 * @return 可容纳的最大记录条数。
 */
uint32_t DataLogger_Capacity(void);
/**
 * @brief 读取当前记录间隔，单位毫秒。
 * @return 记录间隔，单位 ms。
 */
uint32_t DataLogger_GetIntervalMs(void);
/**
 * @brief 读取当前导出顺序配置。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 */
uint8_t DataLogger_GetDumpNewestFirst(void);

#endif /* DATA_LOGGER_H */

