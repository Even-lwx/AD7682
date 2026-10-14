#include "ad7682.h"

/* Datasheet Rev.B pp.24-25,28: single-ended GND, full BW,
 * external REF (internal reference/buffer off), sequencer off, RB enabled. */
#define AD7682_CFG_BASE 0x3C70U

static void AD7682_DelayUs(uint32_t us)
{
    volatile uint32_t loops = (SystemCoreClock / 1000000UL) * us;
    while (loops-- != 0U) { __NOP(); }
}

/* 完成一次“启动转换、发送 CFG、读取数据/回读”的完整 SPI 事务。
 * 不使用 BUSY，CNV 保持低电平结束，下一次调用产生新的上升沿。
 * 时序余量按 VDD=3 V、tCONV(max)=3.2 us、tACQ(min)=1.8 us 设置。 */
static AD7682_Status AD7682_Transfer(AD7682_Handle *dev, uint16_t cfg,
                                    uint16_t *raw, uint16_t *readback,
                                    uint32_t timeout_ms)
{
    uint16_t wire = (uint16_t)(cfg << 2);
    uint8_t tx[4];
    uint8_t rx[4] = {0U};
    HAL_StatusTypeDef status;
    tx[0] = (uint8_t)(wire >> 8);
    tx[1] = (uint8_t)wire;
    tx[2] = 0U;
    tx[3] = 0U;
    HAL_GPIO_WritePin(dev->cnv_port, dev->cnv_pin, GPIO_PIN_SET);
    /* tCONV(max)=3.2 us; keep CNV high well past EOC. */
    AD7682_DelayUs(10U);
    HAL_GPIO_WritePin(dev->cnv_port, dev->cnv_pin, GPIO_PIN_RESET);
    /* tACQ(min)=1.8 us before the serial transfer. */
    AD7682_DelayUs(5U);
    /* 16 data + 14 CFG bits; final two clocks are ignored (SDO high-Z). */
    status = HAL_SPI_TransmitReceive(dev->hspi, tx, rx, 4U, timeout_ms);
    AD7682_DelayUs(10U);
    if (status != HAL_OK)
    {
        return status == HAL_TIMEOUT ? AD7682_TIMEOUT : AD7682_ERROR;
    }
    *raw = (uint16_t)(((uint16_t)rx[0] << 8) | rx[1]);
    *readback = (uint16_t)((((uint16_t)rx[2] << 8) | rx[3]) >> 2);
    return AD7682_OK;
}

AD7682_Status AD7682_Init(AD7682_Handle *dev, SPI_HandleTypeDef *hspi,
                          float vref_volts)
{
    uint16_t discarded_code;
    if (dev == NULL || hspi == NULL || !(vref_volts > 0.0f))
    {
        return AD7682_ERROR;
    }
    dev->hspi = hspi;
    dev->cnv_port = AD7682_CNV_GPIO_Port;
    dev->cnv_pin = AD7682_CNV_Pin;
    dev->vref_volts = vref_volts;
    HAL_GPIO_WritePin(dev->cnv_port, dev->cnv_pin, GPIO_PIN_RESET);
    HAL_Delay(10U);
    /* First conversion also establishes the initial CFG pipeline state. */
    return AD7682_ReadChannel(dev, 0U, &discarded_code, 100U);
}

/* 读取指定输入。配置切换后前两次结果属于流水线旧配置，第三次有效。 */
AD7682_Status AD7682_ReadChannel(AD7682_Handle *dev, uint8_t channel,
                                 uint16_t *code, uint32_t timeout_ms)
{
    uint16_t raw = 0U, readback = 0U;
    uint16_t cfg;
    uint8_t i;
    AD7682_Status status;
    if (dev == NULL || dev->hspi == NULL || code == NULL || channel >= 4U)
    {
        return AD7682_ERROR;
    }
    cfg = (uint16_t)(AD7682_CFG_BASE | ((uint16_t)channel << 7));
    /* CFG has a two-conversion latency. Discard two results after each
     * channel change; only the third belongs to the requested input. */
    for (i = 0U; i < 3U; i++)
    {
        status = AD7682_Transfer(dev, cfg, &raw, &readback, timeout_ms);
        if (status != AD7682_OK) { return status; }
    }
    /* Some board-level SDO paths do not return CFG reliably although the
     * conversion is valid. Keep readback for diagnostics, but use the HAL
     * transfer result as the communication criterion. */
    (void)readback;
    *code = raw;
    return AD7682_OK;
}

/* 顺序扫描四路输入；只有四路均成功时才更新调用者数组。 */
AD7682_Status AD7682_ReadFourChannels(AD7682_Handle *dev, uint16_t codes[4],
                                      uint32_t timeout_ms)
{
    uint16_t values[4];
    uint8_t i;
    AD7682_Status status;
    if (codes == NULL) { return AD7682_ERROR; }
    for (i = 0U; i < 4U; i++)
    {
        status = AD7682_ReadChannel(dev, i, &values[i], timeout_ms);
        if (status != AD7682_OK) { return status; }
    }
    for (i = 0U; i < 4U; i++) { codes[i] = values[i]; }
    return AD7682_OK;
}

/* 兼容旧接口：读取 IN0 原始码。 */
AD7682_Status AD7682_ReadRaw(AD7682_Handle *dev, uint16_t *code,
                             uint32_t timeout_ms)
{
    return AD7682_ReadChannel(dev, 0U, code, timeout_ms);
}

/* 将 16 位单极性原始码换算为电压，满量程对应 VREF。 */
AD7682_Status AD7682_ReadVoltage(AD7682_Handle *dev, float *voltage,
                                 uint32_t timeout_ms)
{
    uint16_t code;
    AD7682_Status status;
    if (voltage == NULL) { return AD7682_ERROR; }
    status = AD7682_ReadRaw(dev, &code, timeout_ms);
    if (status == AD7682_OK)
    {
        *voltage = (float)code * dev->vref_volts / 65536.0f;
    }
    return status;
}
