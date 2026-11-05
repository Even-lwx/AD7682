/* PCF8563 RTC 驱动接口，支持 BCD 时间读写和 VL 标志读取。 */
#ifndef PCF8563_H
#define PCF8563_H
#include "i2c.h"
#define PCF8563_I2C_ADDR (0x51U << 1) /* HAL 使用左移后的 8 位地址。 */
typedef struct
{
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t weekday;
    uint8_t month;
    uint8_t year;
    uint8_t century;
} PCF8563_DateTime;
/* 读取寄存器 02h 至 08h 的时间数据。 */
/**
 * @brief 读取 RTC 日期时间，屏蔽控制位并将 BCD 字段转为整数。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param datetime RTC 日期时间结构；const 为输入，其他指针用于读取输出。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef PCF8563_ReadDateTime(SoftI2C_HandleTypeDef *handle, PCF8563_DateTime *datetime, uint32_t timeout);
/* 写入时间并保留世纪位配置。 */
/**
 * @brief 校验时间字段范围，将日期时间及世纪位编码后写入 RTC。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param datetime RTC 日期时间结构；const 为输入，其他指针用于读取输出。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef PCF8563_WriteDateTime(SoftI2C_HandleTypeDef *handle, const PCF8563_DateTime *datetime, uint32_t timeout);
/* running 非 0 清除 STOP 位，启动 RTC 振荡器。 */
/**
 * @brief 读改写控制寄存器 STOP 位，启动或停止 RTC 计时。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param running 非 0 启动 RTC，0 停止。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef PCF8563_SetRunning(SoftI2C_HandleTypeDef *handle, uint8_t running, uint32_t timeout);
/* 读取秒寄存器的 VL（电压低）标志。 */
/**
 * @brief 读取 RTC 秒寄存器 VL 标志，供上电和校时有效性检查。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param voltage_low VL 标志输出指针；允许 NULL，此时仅读取不写回。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef PCF8563_GetVoltageLow(SoftI2C_HandleTypeDef *handle, uint8_t *voltage_low, uint32_t timeout);
#endif
