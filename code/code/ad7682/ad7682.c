#include "ad7682.h"

/* 依据数据手册 Rev.B 第 24、25、28 页：单端接地参考、全带宽、
 * 使用外部 REF 参考源、关闭内部参考缓冲器和温度传感器、
 * 关闭通道序列器、关闭 CFG 回读。REF[5:3] = 110，
 * RB[0] = 1 表示不回读 CFG，基础配置值为 0x3C71。 */
#define AD7682_CFG_BASE 0x3C71U
#define AD7682_CHANNEL_DUMMY_CONVERSIONS 2U

/* STM32G031 的 TIM2 是 32 位计数器。本工程此前未使用 TIM2。
 * 系统时钟配置完成后初始化一次，不开启中断、不配置任何输出引脚。 */
static void AD7682_TimebaseInit(void)
{
    uint32_t clock = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE) != 0U) { clock *= 2U; }
    __HAL_RCC_TIM2_CLK_ENABLE();
    TIM2->CR1 = 0U;
    TIM2->DIER = 0U;
    TIM2->PSC = clock / 1000000U - 1U;
    TIM2->ARR = 0xFFFFFFFFUL;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0U;
    TIM2->CNT = 0U;
    TIM2->CR1 = TIM_CR1_CEN;
}

uint32_t AD7682_TimeUs(void)
{
    return TIM2->CNT;
}

static void AD7682_DelayUs(uint32_t us)
{
    uint32_t start = AD7682_TimeUs();
    while ((uint32_t)(AD7682_TimeUs() - start) < us) { __NOP(); }
}

/* ADC 读写时屏蔽短时中断，不能使用依赖 SysTick 的 HAL SPI 超时。
 * SPI1 为本驱动独占的 8 位、双线主机；以 TIM2 为整个 16 SCK 事务计时。
 * 显式按高字节、低字节访问 DR，保持与原 HAL 的字节顺序一致。 */
static AD7682_Status AD7682_SpiExchange(AD7682_Handle *dev, uint16_t wire,
                                       uint8_t rx[2], uint32_t timeout_ms)
{
    SPI_HandleTypeDef *hspi = dev->hspi;
    SPI_TypeDef *spi = hspi->Instance;
    uint32_t start = AD7682_TimeUs();
    uint32_t limit = timeout_ms > 1000U ? 1000000U : timeout_ms * 1000U;
    uint8_t i;
    uint8_t tx[2];
    AD7682_Status result = AD7682_OK;
    /* 16 clocks need at most 16 us at the supported 1..4 MHz settings.
     * Bound masked IRQ latency even when SPI malfunctions (UART 115200).
     * Normal exchanges finish in 8 us at 2 MHz. */
    if (limit > 50U) { limit = 50U; }
    if (hspi->State != HAL_SPI_STATE_READY || hspi->Lock == HAL_LOCKED)
    {
        return AD7682_ERROR;
    }
    hspi->Lock = HAL_LOCKED;
    hspi->State = HAL_SPI_STATE_BUSY_TX_RX;
    hspi->ErrorCode = HAL_SPI_ERROR_NONE;
    __HAL_SPI_ENABLE(hspi);
    while ((spi->SR & SPI_SR_FRLVL) != 0U)
    {
        (void)*(__IO uint8_t *)&spi->DR;
        if ((uint32_t)(AD7682_TimeUs() - start) >= limit)
        {
            result = AD7682_TIMEOUT;
            goto finish;
        }
    }
    __HAL_SPI_CLEAR_OVRFLAG(hspi);
    tx[0] = (uint8_t)(wire >> 8);
    tx[1] = (uint8_t)wire;
    for (i = 0U; i < 2U; i++)
    {
        while ((spi->SR & SPI_SR_TXE) == 0U)
        {
            if ((uint32_t)(AD7682_TimeUs() - start) >= limit)
            {
                result = AD7682_TIMEOUT;
                goto finish;
            }
        }
        *(__IO uint8_t *)&spi->DR = tx[i];
        while ((spi->SR & SPI_SR_RXNE) == 0U)
        {
            if ((uint32_t)(AD7682_TimeUs() - start) >= limit)
            {
                result = AD7682_TIMEOUT;
                goto finish;
            }
        }
        rx[i] = *(__IO uint8_t *)&spi->DR;
    }
    while ((spi->SR & SPI_SR_BSY) != 0U)
    {
        if ((uint32_t)(AD7682_TimeUs() - start) >= limit)
        {
            result = AD7682_TIMEOUT;
            goto finish;
        }
    }
    if ((spi->SR & (SPI_SR_OVR | SPI_SR_MODF | SPI_SR_FRE)) != 0U)
    {
        result = AD7682_ERROR;
    }
finish:
    if (result != AD7682_OK)
    {
        /* 失败后复位 SPI1 并恢复原配置；不用 HAL 内部的滴答等待。 */
        uint32_t cr1 = spi->CR1 & (uint32_t)~SPI_CR1_SPE;
        uint32_t cr2 = spi->CR2;
        __HAL_RCC_SPI1_FORCE_RESET();
        __HAL_RCC_SPI1_RELEASE_RESET();
        spi->CR2 = cr2;
        spi->CR1 = cr1;
        hspi->ErrorCode = HAL_SPI_ERROR_FLAG;
    }
    hspi->State = HAL_SPI_STATE_READY;
    hspi->Lock = HAL_UNLOCKED;
    return result;
}

/* 完成一次“启动转换、发送 CFG、读取数据”的完整 SPI 事务。
 * 不使用 BUSY，CNV 保持低电平结束，下一次调用产生新的上升沿。
 * 时序余量按 VDD=3 V、tCONV(max)=3.2 us、tACQ(min)=1.8 us 设置。
 * 手册要求 CNV 上升沿前后保持数字引脚安静，因此 CNV 上升沿前的
 * 10 us 安静时间和转换期间一起屏蔽短时中断；不会屏蔽 500 us 采集等待。 */
static AD7682_Status AD7682_Transfer(AD7682_Handle *dev, uint16_t cfg,
                                    uint8_t update_cfg,
                                    uint16_t *raw, uint16_t *readback,
                                    uint32_t timeout_ms)
{
    /* CFG[13]=0 时保持当前配置；配置更新完成后 DIN 全部保持低电平。 */
    uint16_t wire = update_cfg != 0U ? (uint16_t)(cfg << 2) : 0U;
    uint8_t rx[2] = {0U};
    AD7682_Status status;
    uint32_t primask;
    /* 手册建议 CNV 上升沿前保持 SCK/DIN 安静。这里把 SPI 完成后的
     * 安静窗口放在本次事务开头，并与转换窗口一起保护，避免 SysTick、
     * TIM14 或 UART ISR 在 CNV 边沿附近插入数字活动。 */
    primask = __get_PRIMASK();
    __disable_irq();
    AD7682_DelayUs(10U);
    dev->last_cnv_us = AD7682_TimeUs();
    dev->cnv_port->BSRR = dev->cnv_pin;
    /* tCONV 最大值为 3.2 us；CNV 保持高电平，留出足够转换余量。 */
    AD7682_DelayUs(10U);
    HAL_GPIO_WritePin(dev->cnv_port, dev->cnv_pin, GPIO_PIN_RESET);
    __set_PRIMASK(primask);
    /*
     * SPI 传输前保持 CNV 低电平，满足 tACQ；同时给新选通道及其 RC
     * 输入网络约 500 us 的稳定时间。
     */
    AD7682_DelayUs(dev->acquisition_delay_us);
    /* RB=1 时只需要 16 个 SCK：前 16 位为转换结果，之后 SDO 已进入高阻。
     * RB=0 会开启 CFG 回读，需要 30 个 SCK；本驱动不使用该模式。
     * SPI 突发期间不允许中断插入，避免 SCK 被拉长或 UART/TIM 的数字边沿
     * 与 ADC 的最后几位转换数据重叠；500 us 采集等待仍保持可中断。 */
    primask = __get_PRIMASK();
    __disable_irq();
    status = AD7682_SpiExchange(dev, wire, rx, timeout_ms);
    __set_PRIMASK(primask);
    if (status != AD7682_OK)
    {
        return status;
    }
    *raw = (uint16_t)(((uint16_t)rx[0] << 8) | rx[1]);
    *readback = 0U;
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
    if (hspi->Instance != SPI1 || hspi->Init.DataSize != SPI_DATASIZE_8BIT ||
        hspi->Init.Direction != SPI_DIRECTION_2LINES ||
        hspi->Init.Mode != SPI_MODE_MASTER)
    {
        return AD7682_ERROR;
    }
    /* 重试初始化时不重新启动计时器，保持已产生的时间戳连续。 */
    if ((RCC->APBENR1 & RCC_APBENR1_TIM2EN) == 0U)
    {
        AD7682_TimebaseInit();
    }
    dev->hspi = hspi;
    dev->cnv_port = AD7682_CNV_GPIO_Port;
    dev->cnv_pin = AD7682_CNV_Pin;
    dev->vref_volts = vref_volts;
    dev->acquisition_delay_us = AD7682_ACQUISITION_DELAY_US;
    dev->last_cnv_us = 0U;
    HAL_GPIO_WritePin(dev->cnv_port, dev->cnv_pin, GPIO_PIN_RESET);
    HAL_Delay(10U);
    dev->configured_channel = 0U;
    dev->configured_channel_valid = 0U;
    /* 首次转换建立初始 CFG 流水线状态。 */
    return AD7682_ReadChannel(dev, 2U, &discarded_code, 100U);
}

/* 通道切换时第一次发送新 CFG，第二次 DIN=0 保持配置；丢弃两次结果。 */
static AD7682_Status AD7682_EnsureChannel(AD7682_Handle *dev,
                                          uint8_t channel,
                                          uint32_t timeout_ms)
{
    uint16_t raw;
    uint16_t readback;
    uint16_t cfg;
    uint8_t i;
    AD7682_Status status;

    if (dev->configured_channel_valid != 0U &&
        dev->configured_channel == channel)
    {
        return AD7682_OK;
    }
    cfg = (uint16_t)(AD7682_CFG_BASE | ((uint16_t)channel << 7));
    for (i = 0U; i < AD7682_CHANNEL_DUMMY_CONVERSIONS; i++)
    {
        status = AD7682_Transfer(dev, cfg, i == 0U ? 1U : 0U,
                                 &raw, &readback, timeout_ms);
        if (status != AD7682_OK)
        {
            dev->configured_channel_valid = 0U;
            return status;
        }
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
    status = AD7682_Transfer(dev, cfg, 0U, &raw, &readback, timeout_ms);
    if (status != AD7682_OK)
    {
        dev->configured_channel_valid = 0U;
        return status;
    }
    /* RB=1 时没有 CFG 回读数据；通信成功以 SPI 事务返回状态为准。 */
    (void)readback;
    *code = raw;
    return AD7682_OK;
}

AD7682_Status AD7682_ReadChannelSamples(AD7682_Handle *dev, uint8_t channel,
                                        uint16_t samples[5],
                                        uint32_t timeout_ms)
{
    return AD7682_ReadChannelTimedSamples(dev, channel, samples, NULL, timeout_ms);
}

AD7682_Status AD7682_ReadChannelTimedSamples(AD7682_Handle *dev,
                                             uint8_t channel,
                                             uint16_t samples[5],
                                             uint32_t timestamps_us[5],
                                             uint32_t timeout_ms)
{
    uint16_t values[5];
    uint32_t times[5];
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
    for (i = 0U; i < 5U; i++)
    {
        status = AD7682_Transfer(dev, cfg, 0U, &raw, &readback, timeout_ms);
        if (status != AD7682_OK)
        {
            dev->configured_channel_valid = 0U;
            return status;
        }
        values[i] = raw;
        times[i] = dev->last_cnv_us;
    }
    for (i = 0U; i < 5U; i++) { samples[i] = values[i]; }
    if (timestamps_us != NULL)
    {
        for (i = 0U; i < 5U; i++) { timestamps_us[i] = times[i]; }
    }
    return AD7682_OK;
}

uint16_t AD7682_Median5(const uint16_t values[5])
{
    uint16_t sorted[5];
    uint16_t temp;
    uint8_t i;
    uint8_t j;

    for (i = 0U; i < 5U; i++) { sorted[i] = values[i]; }
    for (i = 0U; i < 4U; i++)
    {
        for (j = 0U; j < (uint8_t)(4U - i); j++)
        {
            if (sorted[j] > sorted[j + 1U])
            {
                temp = sorted[j];
                sorted[j] = sorted[j + 1U];
                sorted[j + 1U] = temp;
            }
        }
    }
    return sorted[2];
}

/* 顺序扫描四路输入：每个通道先丢弃两次切换结果，再取 5 次有效转换，
 * 用 5 个样本的中位数输出。只有四路均成功时才更新数组。 */
AD7682_Status AD7682_ReadFourChannels(AD7682_Handle *dev, uint16_t codes[4],
                                      uint32_t timeout_ms)
{
    uint16_t values[4][5];
    uint8_t i;
    AD7682_Status status;
    if (codes == NULL) { return AD7682_ERROR; }
    for (i = 0U; i < 4U; i++)
    {
        status = AD7682_ReadChannelSamples(dev, i, values[i], timeout_ms);
        if (status != AD7682_OK) { return status; }
    }
    for (i = 0U; i < 4U; i++) { codes[i] = AD7682_Median5(values[i]); }
    return AD7682_OK;
}

AD7682_Status AD7682_ReadFourChannelsSamples(AD7682_Handle *dev,
                                             uint16_t samples[4][5],
                                             uint32_t timeout_ms)
{
    uint16_t values[4][5];
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
        for (sample = 0U; sample < 5U; sample++)
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
