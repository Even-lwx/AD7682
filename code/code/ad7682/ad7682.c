#include "ad7682.h"

/* AD7682 datasheet: CNV high starts acquisition/conversion; after tCONV,
 * 16 SCK falling edges shift out MSB-first conversion data. */
static void AD7682_DelayUs(uint32_t us)
{
    uint32_t loops;

    /* The loop is deliberately conservative; conversion timing is not a
     * precision timebase. The SPI transaction remains the timing authority. */
    loops = (SystemCoreClock / 1000000UL / 5UL) * us;
    while (loops-- != 0U)
    {
        __NOP();
    }
}

AD7682_Status AD7682_Init(AD7682_Handle *dev, SPI_HandleTypeDef *hspi,
                          float vref_volts)
{
    if ((dev == NULL) || (hspi == NULL) || (vref_volts <= 0.0f))
    {
        return AD7682_ERROR;
    }

    dev->hspi = hspi;
    dev->cnv_port = AD7682_CNV_GPIO_Port;
    dev->cnv_pin = AD7682_CNV_Pin;
    dev->vref_volts = vref_volts;
    HAL_GPIO_WritePin(dev->cnv_port, dev->cnv_pin, GPIO_PIN_RESET);
    return AD7682_OK;
}

AD7682_Status AD7682_ReadRaw(AD7682_Handle *dev, uint16_t *code,
                             uint32_t timeout_ms)
{
    uint8_t rx[2] = {0U, 0U};
    HAL_StatusTypeDef hal_status;

    if ((dev == NULL) || (code == NULL) || (dev->hspi == NULL))
    {
        return AD7682_ERROR;
    }

    HAL_GPIO_WritePin(dev->cnv_port, dev->cnv_pin, GPIO_PIN_SET);
    /* tCNVH is 10 ns; 1 us is a safe GPIO pulse at this abstraction level. */
    AD7682_DelayUs(1U);
    /* tCONV is 2.2 us maximum at the documented operating conditions. */
    AD7682_DelayUs(3U);
    HAL_GPIO_WritePin(dev->cnv_port, dev->cnv_pin, GPIO_PIN_RESET);

    hal_status = HAL_SPI_Receive(dev->hspi, rx, sizeof(rx), timeout_ms);
    if (hal_status != HAL_OK)
    {
        return (hal_status == HAL_TIMEOUT) ? AD7682_TIMEOUT : AD7682_ERROR;
    }

    *code = ((uint16_t)rx[0] << 8) | rx[1];
    return AD7682_OK;
}

AD7682_Status AD7682_ReadVoltage(AD7682_Handle *dev, float *voltage,
                                 uint32_t timeout_ms)
{
    uint16_t code;
    AD7682_Status status;

    if ((dev == NULL) || (voltage == NULL))
    {
        return AD7682_ERROR;
    }

    status = AD7682_ReadRaw(dev, &code, timeout_ms);
    if (status == AD7682_OK)
    {
        *voltage = ((float)code * dev->vref_volts) / 65536.0f;
    }
    return status;
}
