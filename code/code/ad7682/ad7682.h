/* AD7682 16 位 SAR ADC 驱动接口。 */
#ifndef AD7682_H
#define AD7682_H

#include "stm32g0xx_hal.h"

/* 原理图连接：SCK=PA1，SDO=PA6，DIN=PA7，CNV=PA5。 */
#define AD7682_CNV_GPIO_Port GPIOA
#define AD7682_CNV_Pin       GPIO_PIN_5

/* CNV 拉低后的采集保持时间，单位 us。手册规定 tACQ 最小 1.8 us；
 * 当前设置为 500 us，用于验证传感器输出和模拟网络的稳定性。 */
#define AD7682_ACQUISITION_DELAY_US 500U

typedef enum
{
    AD7682_OK = 0,
    AD7682_ERROR,
    AD7682_TIMEOUT
} AD7682_Status;

typedef struct
{
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cnv_port;
    uint16_t cnv_pin;
    float vref_volts;
    uint8_t configured_channel;
    uint8_t configured_channel_valid;
} AD7682_Handle;

/* 初始化 SPI、CNV 引脚和参考电压参数。 */
AD7682_Status AD7682_Init(AD7682_Handle *dev, SPI_HandleTypeDef *hspi,
                          float vref_volts);
/* 单次读取一个通道并返回原始码；当前配置关闭 CFG 回读，失败时不修改输出。
 * timeout_ms 是单次 SPI 事务的超时时间。 */
AD7682_Status AD7682_ReadChannel(AD7682_Handle *dev, uint8_t channel,
                                 uint16_t *code, uint32_t timeout_ms);
/* 切换通道后先丢弃两次流水线结果，再连续读取一个通道 5 次；
 * 失败时不修改输出。 */
AD7682_Status AD7682_ReadChannelSamples(AD7682_Handle *dev, uint8_t channel,
                                        uint16_t samples[5],
                                        uint32_t timeout_ms);
/* 四通道顺序扫描：每个通道先丢弃两次切换后的结果，再读取 5 个有效样本，
 * 以第 3 个样本的中位数作为该通道结果，抑制单点尖峰。 */
AD7682_Status AD7682_ReadFourChannels(AD7682_Handle *dev, uint16_t codes[4],
                                      uint32_t timeout_ms);
/* IN0～IN3 每个通道先丢弃两次切换结果，再连续读取 5 个有效样本，
 * 数组布局为 samples[通道][样本序号]。 */
AD7682_Status AD7682_ReadFourChannelsSamples(AD7682_Handle *dev,
                                             uint16_t samples[4][5],
                                             uint32_t timeout_ms);
/* 对一个通道的 5 个有效原始样本取中位数。 */
uint16_t AD7682_Median5(const uint16_t values[5]);
/* 兼容旧接口：读取 IN0。 */
AD7682_Status AD7682_ReadRaw(AD7682_Handle *dev, uint16_t *code,
                             uint32_t timeout_ms);
/* 读取原始码并按参考电压换算为伏特。 */
AD7682_Status AD7682_ReadVoltage(AD7682_Handle *dev, float *voltage,
                                 uint32_t timeout_ms);

#endif /* AD7682_H */
