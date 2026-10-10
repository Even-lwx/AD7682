#include "sht41.h"

/* SHT41 的 CRC-8 多项式为 0x31，初值为 0xFF。 */
static uint8_t SHT41_Crc8(const uint8_t *data, uint8_t length)
{
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

HAL_StatusTypeDef SHT41_Reset(SoftI2C_HandleTypeDef *handle, uint32_t timeout)
{
    uint8_t command = 0x94U;
    return SoftI2C_MasterTransmit(handle, SHT41_ADDR, &command, 1U, timeout);
}

HAL_StatusTypeDef SHT41_Read(SoftI2C_HandleTypeDef *handle,
                             float *temperature_c, float *humidity_rh,
                             uint32_t timeout)
{
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
