#include "sht41.h"

/* SHT41 驱动：发送高精度测量命令，读取温湿度并校验两组 CRC。 */

/* SHT41 的 CRC-8 多项式为 0x31，初值为 0xFF。 */
/**
 * @brief 按 SHT41 的 CRC8 算法校验测量字节，初值 FF、多项式 31。
 * @param data 待处理字节或数据区；指针的输入输出方向由 const 和函数用途确定。
 * @param length 连续处理的字节数，数组尺寸须与该长度匹配。
 * @return 输入数据对应的校验值。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static uint8_t SHT41_Crc8(const uint8_t *data, uint8_t length)
{
    /* 按 Sensirion 规定的 CRC-8 多项式计算数据校验值。 */
    uint8_t crc = 0xFFU;
    uint8_t bit;

    while (length-- != 0U)
    {
        crc ^= *data++;
        for (bit = 0U; bit < 8U; ++bit)
        {
            crc = (crc & 0x80U) ? (uint8_t)((crc << 1U) ^ 0x31U)
                                 : (uint8_t)(crc << 1U);
        }
    }
    return crc;
}

/**
 * @brief 向 SHT41 发送软件复位命令并等待器件恢复。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
HAL_StatusTypeDef SHT41_Reset(SoftI2C_HandleTypeDef *handle, uint32_t timeout)
{
    /* 0x94 为 SHT41 软复位命令。 */
    uint8_t command = 0x94U;
    return SoftI2C_MasterTransmit(handle, SHT41_ADDR, &command, 1U, timeout);
}

/**
 * @brief 启动温湿度测量，等待结果并校验两个 CRC8，成功后换算 °C 和 %RH。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param temperature_c 接收温度结果的输出指针，单位 °C。
 * @param humidity_rh 接收相对湿度结果的输出指针，单位 %RH。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
HAL_StatusTypeDef SHT41_Read(SoftI2C_HandleTypeDef *handle,
                             float *temperature_c, float *humidity_rh,
                             uint32_t timeout)
{
    /* 0xFD 为高精度无加热测量命令，测量完成后读取 6 字节结果。 */
    uint8_t command = 0xFDU;
    uint8_t data[6];
    uint16_t raw_temperature;
    uint16_t raw_humidity;
    HAL_StatusTypeDef status;

    if ((handle == NULL) || (temperature_c == NULL) || (humidity_rh == NULL))
    {
        return HAL_ERROR;
    }
    status = SoftI2C_MasterTransmit(handle, SHT41_ADDR, &command, 1U, timeout);
    if (status != HAL_OK)
    {
        return status;
    }
    HAL_Delay(10U);
    status = SoftI2C_MasterReceive(handle, SHT41_ADDR, data, 6U, timeout);
    if (status != HAL_OK)
    {
        return status;
    }
    if ((SHT41_Crc8(data, 2U) != data[2]) ||
        (SHT41_Crc8(&data[3], 2U) != data[5]))
    {
        return HAL_ERROR;
    }

    raw_temperature = (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
    raw_humidity = (uint16_t)(((uint16_t)data[3] << 8U) | data[4]);
    *temperature_c = -45.0f + (175.0f * (float)raw_temperature / 65535.0f);
    *humidity_rh = -6.0f + (125.0f * (float)raw_humidity / 65535.0f);
    if (*humidity_rh < 0.0f)
    {
        *humidity_rh = 0.0f;
    }
    if (*humidity_rh > 100.0f)
    {
        *humidity_rh = 100.0f;
    }
    return HAL_OK;
}
