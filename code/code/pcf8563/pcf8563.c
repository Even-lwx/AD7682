#include "pcf8563.h"

/* PCF8563 驱动：寄存器使用 BCD 编码，底层总线为软件 I2C。 */

#define REG_CONTROL1 0x00U
#define REG_SECONDS  0x02U

/**
 * @brief 将已屏蔽控制位的 BCD 数字解码为十进制整数。
 * @param value 待编码、待解析或待写入的值；指针形式用于接收解析输出。
 * @return 解码或转换后的整数。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static uint8_t PCF8563_BcdToInt(uint8_t value)
{
    /* 将 RTC 寄存器中的 BCD 数字转换为普通十进制数。 */
    return (uint8_t)(((value >> 4U) * 10U) + (value & 0x0FU));
}

/**
 * @brief 将十进制整数编码为 RTC 寄存器使用的 BCD 数字。
 * @param value 待编码、待解析或待写入的值；指针形式用于接收解析输出。
 * @return 解码或转换后的整数。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static uint8_t PCF8563_IntToBcd(uint8_t value)
{
    /* 将普通十进制数转换为 PCF8563 使用的 BCD 格式。 */
    return (uint8_t)(((value / 10U) << 4U) | (value % 10U));
}

/**
 * @brief 从 RTC 指定起始寄存器连续读取字节。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param reg 起始寄存器地址。
 * @param data 待处理字节或数据区；指针的输入输出方向由 const 和函数用途确定。
 * @param length 连续处理的字节数，数组尺寸须与该长度匹配。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static HAL_StatusTypeDef PCF8563_ReadRegisters(SoftI2C_HandleTypeDef *handle,
                                                uint8_t reg, uint8_t *data,
                                                uint16_t length, uint32_t timeout)
{
    /* 统一封装带寄存器地址的连续读取。 */
    return SoftI2C_MemRead(handle, PCF8563_I2C_ADDR, reg, data, length, timeout);
}

/**
 * @brief 向 RTC 指定起始寄存器连续写入字节。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param reg 起始寄存器地址。
 * @param data 待处理字节或数据区；指针的输入输出方向由 const 和函数用途确定。
 * @param length 连续处理的字节数，数组尺寸须与该长度匹配。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static HAL_StatusTypeDef PCF8563_WriteRegisters(SoftI2C_HandleTypeDef *handle,
                                                 uint8_t reg, uint8_t *data,
                                                 uint16_t length, uint32_t timeout)
{
    /* 统一封装带寄存器地址的连续写入。 */
    return SoftI2C_MemWrite(handle, PCF8563_I2C_ADDR, reg, data, length, timeout);
}

/**
 * @brief 读取 RTC 日期时间，屏蔽控制位并将 BCD 字段转为整数。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param datetime RTC 日期时间结构；const 为输入，其他指针用于读取输出。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
HAL_StatusTypeDef PCF8563_ReadDateTime(SoftI2C_HandleTypeDef *handle,
                                       PCF8563_DateTime *datetime,
                                       uint32_t timeout)
{
    /* 从秒寄存器开始连续读取秒、分、时、日、星期、月和年。 */
    uint8_t data[7];
    HAL_StatusTypeDef status;

    if ((handle == NULL) || (datetime == NULL))
    {
        return HAL_ERROR;
    }
    status = PCF8563_ReadRegisters(handle, REG_SECONDS, data, 7U, timeout);
    if (status != HAL_OK)
    {
        return status;
    }

    datetime->seconds = PCF8563_BcdToInt(data[0] & 0x7FU);
    datetime->minutes = PCF8563_BcdToInt(data[1] & 0x7FU);
    datetime->hours = PCF8563_BcdToInt(data[2] & 0x3FU);
    datetime->day = PCF8563_BcdToInt(data[3] & 0x3FU);
    datetime->weekday = PCF8563_BcdToInt(data[4] & 0x07U);
    datetime->century = (uint8_t)(data[5] >> 7U);
    datetime->month = PCF8563_BcdToInt(data[5] & 0x1FU);
    datetime->year = PCF8563_BcdToInt(data[6]);
    return HAL_OK;
}

/**
 * @brief 校验时间字段范围，将日期时间及世纪位编码后写入 RTC。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param datetime RTC 日期时间结构；const 为输入，其他指针用于读取输出。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
HAL_StatusTypeDef PCF8563_WriteDateTime(SoftI2C_HandleTypeDef *handle,
                                        const PCF8563_DateTime *datetime,
                                        uint32_t timeout)
{
    /* 校验字段范围后，将时间编码并连续写入 RTC。 */
    uint8_t data[7];

    if ((handle == NULL) || (datetime == NULL) ||
        (datetime->seconds > 59U) || (datetime->minutes > 59U) ||
        (datetime->hours > 23U) || (datetime->day < 1U) ||
        (datetime->day > 31U) || (datetime->weekday > 6U) ||
        (datetime->month < 1U) || (datetime->month > 12U))
    {
        return HAL_ERROR;
    }

    data[0] = PCF8563_IntToBcd(datetime->seconds);
    data[1] = PCF8563_IntToBcd(datetime->minutes);
    data[2] = PCF8563_IntToBcd(datetime->hours);
    data[3] = PCF8563_IntToBcd(datetime->day);
    data[4] = PCF8563_IntToBcd(datetime->weekday);
    data[5] = (uint8_t)(PCF8563_IntToBcd(datetime->month) |
                        ((datetime->century & 1U) << 7U));
    data[6] = PCF8563_IntToBcd(datetime->year);
    return PCF8563_WriteRegisters(handle, REG_SECONDS, data, 7U, timeout);
}

/**
 * @brief 读改写控制寄存器 STOP 位，启动或停止 RTC 计时。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param running 非 0 启动 RTC，0 停止。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
HAL_StatusTypeDef PCF8563_SetRunning(SoftI2C_HandleTypeDef *handle,
                                      uint8_t running, uint32_t timeout)
{
    /* 控制寄存器 bit5 为 STOP，running 非 0 时清除该位。 */
    uint8_t control;
    HAL_StatusTypeDef status;

    status = PCF8563_ReadRegisters(handle, REG_CONTROL1, &control, 1U, timeout);
    if (status != HAL_OK)
    {
        return status;
    }
    if (running != 0U)
    {
        control &= (uint8_t)~0x20U;
    }
    else
    {
        control |= 0x20U;
    }
    return PCF8563_WriteRegisters(handle, REG_CONTROL1, &control, 1U, timeout);
}

/**
 * @brief 读取 RTC 秒寄存器 VL 标志，供上电和校时有效性检查。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param voltage_low VL 标志输出指针；允许 NULL，此时仅读取不写回。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
HAL_StatusTypeDef PCF8563_GetVoltageLow(SoftI2C_HandleTypeDef *handle,
                                         uint8_t *voltage_low,
                                         uint32_t timeout)
{
    /* 秒寄存器 bit7 为 VL 电压低/振荡器停止标志。 */
    uint8_t seconds;
    HAL_StatusTypeDef status;

    status = PCF8563_ReadRegisters(handle, REG_SECONDS, &seconds, 1U, timeout);
    if ((status == HAL_OK) && (voltage_low != NULL))
    {
        *voltage_low = (uint8_t)(seconds >> 7U);
    }
    return status;
}
