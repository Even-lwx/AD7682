#include "i2c.h"

/* 软件 I2C：PB11 为 SCL，PB12 为 SDA，外部上拉到 3.3 V。 */
SoftI2C_HandleTypeDef hi2c1 = {GPIOB, GPIO_PIN_11, GPIOB, GPIO_PIN_12};

static void SoftI2C_Delay(void)
{
    for (volatile uint32_t i = 0U; i < 32U; ++i) { __NOP(); }
}

static void SoftI2C_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
    HAL_GPIO_WritePin(port, pin, state);
    SoftI2C_Delay();
}

static GPIO_PinState SoftI2C_ReadSda(const SoftI2C_HandleTypeDef *handle)
{
    return HAL_GPIO_ReadPin(handle->sda_port, handle->sda_pin);
}

static void SoftI2C_Start(SoftI2C_HandleTypeDef *handle)
{
    SoftI2C_WritePin(handle->sda_port, handle->sda_pin, GPIO_PIN_SET);
    SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_SET);
    SoftI2C_WritePin(handle->sda_port, handle->sda_pin, GPIO_PIN_RESET);
    SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_RESET);
}

static void SoftI2C_Stop(SoftI2C_HandleTypeDef *handle)
{
    SoftI2C_WritePin(handle->sda_port, handle->sda_pin, GPIO_PIN_RESET);
    SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_SET);
    SoftI2C_WritePin(handle->sda_port, handle->sda_pin, GPIO_PIN_SET);
}

static uint8_t SoftI2C_WriteByte(SoftI2C_HandleTypeDef *handle, uint8_t data)
{
    uint8_t i;
    uint8_t ack;
    for (i = 0U; i < 8U; ++i)
    {
        SoftI2C_WritePin(handle->sda_port, handle->sda_pin,
                         (data & 0x80U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_SET);
        SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_RESET);
        data <<= 1U;
    }
    SoftI2C_WritePin(handle->sda_port, handle->sda_pin, GPIO_PIN_SET);
    SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_SET);
    ack = (SoftI2C_ReadSda(handle) == GPIO_PIN_RESET) ? 1U : 0U;
    SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_RESET);
    return ack;
}

static uint8_t SoftI2C_ReadByte(SoftI2C_HandleTypeDef *handle, uint8_t ack)
{
    uint8_t i;
    uint8_t data = 0U;
    SoftI2C_WritePin(handle->sda_port, handle->sda_pin, GPIO_PIN_SET);
    for (i = 0U; i < 8U; ++i)
    {
        data <<= 1U;
        SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_SET);
        if (SoftI2C_ReadSda(handle) == GPIO_PIN_SET) { data |= 1U; }
        SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_RESET);
    }
    SoftI2C_WritePin(handle->sda_port, handle->sda_pin,
                     ack ? GPIO_PIN_RESET : GPIO_PIN_SET);
    SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_SET);
    SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_RESET);
    SoftI2C_WritePin(handle->sda_port, handle->sda_pin, GPIO_PIN_SET);
    return data;
}

void MX_I2C1_Init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_11 | GPIO_PIN_12;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11 | GPIO_PIN_12, GPIO_PIN_SET);
}

HAL_StatusTypeDef SoftI2C_MasterTransmit(SoftI2C_HandleTypeDef *handle,
                                         uint16_t address, uint8_t *data,
                                         uint16_t length, uint32_t timeout)
{
    uint16_t i;
    (void)timeout;
    SoftI2C_Start(handle);
    if (!SoftI2C_WriteByte(handle, (uint8_t)(address & 0xFEU))) { SoftI2C_Stop(handle); return HAL_ERROR; }
    for (i = 0U; i < length; ++i)
    {
        if (!SoftI2C_WriteByte(handle, data[i])) { SoftI2C_Stop(handle); return HAL_ERROR; }
    }
    SoftI2C_Stop(handle);
    return HAL_OK;
}

HAL_StatusTypeDef SoftI2C_MasterReceive(SoftI2C_HandleTypeDef *handle,
                                        uint16_t address, uint8_t *data,
                                        uint16_t length, uint32_t timeout)
{
    uint16_t i;
    (void)timeout;
    SoftI2C_Start(handle);
    if (!SoftI2C_WriteByte(handle, (uint8_t)(address | 0x01U))) { SoftI2C_Stop(handle); return HAL_ERROR; }
    for (i = 0U; i < length; ++i) { data[i] = SoftI2C_ReadByte(handle, (i + 1U < length) ? 1U : 0U); }
    SoftI2C_Stop(handle);
    return HAL_OK;
}

HAL_StatusTypeDef SoftI2C_MemRead(SoftI2C_HandleTypeDef *handle, uint16_t address,
                                  uint8_t reg, uint8_t *data, uint16_t length,
                                  uint32_t timeout)
{
    (void)timeout;
    SoftI2C_Start(handle);
    if (!SoftI2C_WriteByte(handle, (uint8_t)(address & 0xFEU)) || !SoftI2C_WriteByte(handle, reg))
    { SoftI2C_Stop(handle); return HAL_ERROR; }
    SoftI2C_Start(handle);
    if (!SoftI2C_WriteByte(handle, (uint8_t)(address | 0x01U))) { SoftI2C_Stop(handle); return HAL_ERROR; }
    while (length-- > 0U) { *data++ = SoftI2C_ReadByte(handle, (length != 0U) ? 1U : 0U); }
    SoftI2C_Stop(handle);
    return HAL_OK;
}

HAL_StatusTypeDef SoftI2C_MemWrite(SoftI2C_HandleTypeDef *handle, uint16_t address,
                                   uint8_t reg, uint8_t *data, uint16_t length,
                                   uint32_t timeout)
{
    uint16_t i;
    (void)timeout;
    SoftI2C_Start(handle);
    if (!SoftI2C_WriteByte(handle, (uint8_t)(address & 0xFEU)) || !SoftI2C_WriteByte(handle, reg))
    { SoftI2C_Stop(handle); return HAL_ERROR; }
    for (i = 0U; i < length; ++i)
    {
        if (!SoftI2C_WriteByte(handle, data[i])) { SoftI2C_Stop(handle); return HAL_ERROR; }
    }
    SoftI2C_Stop(handle);
    return HAL_OK;
}
