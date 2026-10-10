#include "pcf8563.h"

#define REG_CONTROL1 0x00U
#define REG_SECONDS  0x02U

static uint8_t PCF8563_BcdToInt(uint8_t value)
{
    return (uint8_t)(((value >> 4U) * 10U) + (value & 0x0FU));
}

static uint8_t PCF8563_IntToBcd(uint8_t value)
{
    return (uint8_t)(((value / 10U) << 4U) | (value % 10U));
}

static HAL_StatusTypeDef PCF8563_ReadRegisters(SoftI2C_HandleTypeDef *handle,
                                                uint8_t reg, uint8_t *data,
                                                uint16_t length, uint32_t timeout)
{
    return SoftI2C_MemRead(handle, PCF8563_I2C_ADDR, reg, data, length, timeout);
}

static HAL_StatusTypeDef PCF8563_WriteRegisters(SoftI2C_HandleTypeDef *handle,
                                                 uint8_t reg, uint8_t *data,
                                                 uint16_t length, uint32_t timeout)
{
    return SoftI2C_MemWrite(handle, PCF8563_I2C_ADDR, reg, data, length, timeout);
}

HAL_StatusTypeDef PCF8563_ReadDateTime(SoftI2C_HandleTypeDef *handle,
                                       PCF8563_DateTime *datetime,
                                       uint32_t timeout)
{
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

HAL_StatusTypeDef PCF8563_WriteDateTime(SoftI2C_HandleTypeDef *handle,
                                        const PCF8563_DateTime *datetime,
                                        uint32_t timeout)
{
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

HAL_StatusTypeDef PCF8563_SetRunning(SoftI2C_HandleTypeDef *handle,
                                      uint8_t running, uint32_t timeout)
{
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

HAL_StatusTypeDef PCF8563_GetVoltageLow(SoftI2C_HandleTypeDef *handle,
                                         uint8_t *voltage_low,
                                         uint32_t timeout)
{
    uint8_t seconds;
    HAL_StatusTypeDef status;

    status = PCF8563_ReadRegisters(handle, REG_SECONDS, &seconds, 1U, timeout);
    if ((status == HAL_OK) && (voltage_low != NULL))
    {
        *voltage_low = (uint8_t)(seconds >> 7U);
    }
    return status;
}
