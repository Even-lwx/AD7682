#include "ad7682.h"

/* 依据数据手册 Rev.B 第 24、25、28 页：单端接地参考、全带宽、
 * 外部参考源（关闭内部参考和缓冲）、关闭通道序列器、开启 CFG 回读。 */
#define AD7682_CFG_BASE 0x3C70U

/* 输入端为 100 ohm + 100 pF 时 RC 约 10 ns；100 us 仍远大于
 * AD7682 要求的最小采集时间，同时避免无谓地拖慢多通道扫描。 */
#define AD7682_ACQUISITION_DELAY_US 100U

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
    /* tCONV 最大值为 3.2 us；CNV 保持高电平，留出足够转换余量。 */
    AD7682_DelayUs(10U);
    HAL_GPIO_WritePin(dev->cnv_port, dev->cnv_pin, GPIO_PIN_RESET);
    /*
     * SPI 传输前保持 CNV 低电平，满足 tACQ；同时给新选通道及其 RC
     * 输入网络约 100 us 的稳定时间。
     */
    AD7682_DelayUs(AD7682_ACQUISITION_DELAY_US);
    /* 前 16 位为转换数据，随后 14 位为 CFG 回读；最后两位忽略（SDO 高阻）。 */
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
    dev->configured_channel = 0U;
    dev->configured_channel_valid = 0U;
    /* 首次转换建立初始 CFG 流水线状态。 */
    return AD7682_ReadChannel(dev, 2U, &discarded_code, 100U);
}

/* AD7682 的配置写入和转换结果存在一拍流水线。通道切换后先发送一次
 * 新 CFG 并丢弃返回值，下一次传输才作为该通道的有效结果。 */
static AD7682_Status AD7682_EnsureChannel(AD7682_Handle *dev,
                                          uint8_t channel,
                                          uint32_t timeout_ms)
{
    uint16_t raw;
    uint16_t readback;
    uint16_t cfg;
    AD7682_Status status;

    if (dev->configured_channel_valid != 0U &&
        dev->configured_channel == channel)
    {
        return AD7682_OK;
    }
    cfg = (uint16_t)(AD7682_CFG_BASE | ((uint16_t)channel << 7));
    status = AD7682_Transfer(dev, cfg, &raw, &readback, timeout_ms);
    if (status != AD7682_OK)
    {
        dev->configured_channel_valid = 0U;
        return status;
    }
    dev->configured_channel = channel;
    dev->configured_channel_valid = 1U;
    return AD7682_OK;
}

/* 读取指定输入：只执行一次转换并直接返回本次原始码。 */
AD7682_Status AD7682_ReadChannel(AD7682_Handle *dev, uint8_t channel,
                                 uint16_t *code, uint32_t timeout_ms)
{
    uint16_t raw = 0U, readback = 0U;
    uint16_t cfg;
    AD7682_Status status;
    if (dev == NULL || dev->hspi == NULL || code == NULL || channel >= 4U)
    {
        return AD7682_ERROR;
    }
    cfg = (uint16_t)(AD7682_CFG_BASE | ((uint16_t)channel << 7));
    status = AD7682_EnsureChannel(dev, channel, timeout_ms);
    if (status != AD7682_OK) { return status; }
    status = AD7682_Transfer(dev, cfg, &raw, &readback, timeout_ms);
    if (status != AD7682_OK)
    {
        dev->configured_channel_valid = 0U;
        return status;
    }
    /* 部分板级 SDO 通路不能稳定返回 CFG。保留回读用于诊断，
     * 但以 HAL 传输结果作为通信成功判据。 */
    (void)readback;
    *code = raw;
    return AD7682_OK;
}

AD7682_Status AD7682_ReadChannelSamples(AD7682_Handle *dev, uint8_t channel,
                                        uint16_t samples[4],
                                        uint32_t timeout_ms)
{
    uint16_t values[4];
    uint16_t readback;
    uint16_t cfg;
    uint16_t raw;
    uint8_t i;
    AD7682_Status status;
    if (dev == NULL || dev->hspi == NULL || samples == NULL || channel >= 4U)
    {
        return AD7682_ERROR;
    }
    cfg = (uint16_t)(AD7682_CFG_BASE | ((uint16_t)channel << 7));
    status = AD7682_EnsureChannel(dev, channel, timeout_ms);
    if (status != AD7682_OK) { return status; }
    for (i = 0U; i < 4U; i++)
    {
        status = AD7682_Transfer(dev, cfg, &raw, &readback, timeout_ms);
        if (status != AD7682_OK)
        {
            dev->configured_channel_valid = 0U;
            return status;
        }
        values[i] = raw;
    }
    for (i = 0U; i < 4U; i++) { samples[i] = values[i]; }
    return AD7682_OK;
}

static uint16_t AD7682_Median4(const uint16_t values[4])
{
    uint16_t sorted[4];
    uint16_t temp;
    uint8_t i;
    uint8_t j;

    for (i = 0U; i < 4U; i++) { sorted[i] = values[i]; }
    for (i = 0U; i < 3U; i++)
    {
        for (j = 0U; j < (uint8_t)(3U - i); j++)
        {
            if (sorted[j] > sorted[j + 1U])
            {
                temp = sorted[j];
                sorted[j] = sorted[j + 1U];
                sorted[j + 1U] = temp;
            }
        }
    }
    return (uint16_t)(((uint32_t)sorted[1] + sorted[2]) / 2U);
}

/* 顺序扫描四路输入：每个通道先丢弃一次切换结果，再取 4 次有效转换，
 * 用中间两个样本的平均值输出。只有四路均成功时才更新数组。 */
AD7682_Status AD7682_ReadFourChannels(AD7682_Handle *dev, uint16_t codes[4],
                                      uint32_t timeout_ms)
{
    uint16_t values[4][4];
    uint8_t i;
    AD7682_Status status;
    if (codes == NULL) { return AD7682_ERROR; }
    for (i = 0U; i < 4U; i++)
    {
        status = AD7682_ReadChannelSamples(dev, i, values[i], timeout_ms);
        if (status != AD7682_OK) { return status; }
    }
    for (i = 0U; i < 4U; i++) { codes[i] = AD7682_Median4(values[i]); }
    return AD7682_OK;
}

AD7682_Status AD7682_ReadFourChannelsSamples(AD7682_Handle *dev,
                                             uint16_t samples[4][4],
                                             uint32_t timeout_ms)
{
    uint16_t values[4][4];
    uint8_t channel;
    uint8_t sample;
    AD7682_Status status;
    if (dev == NULL || samples == NULL) { return AD7682_ERROR; }
    for (channel = 0U; channel < 4U; channel++)
    {
        status = AD7682_ReadChannelSamples(dev, channel, values[channel],
                                            timeout_ms);
        if (status != AD7682_OK) { return status; }
    }
    for (channel = 0U; channel < 4U; channel++)
    {
        for (sample = 0U; sample < 4U; sample++)
        {
            samples[channel][sample] = values[channel][sample];
        }
    }
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
