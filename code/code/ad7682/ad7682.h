/* AD7682 16 位 SAR ADC 驱动接口。 */
#ifndef AD7682_H
#define AD7682_H

#include "stm32g0xx_hal.h"

/* 原理图连接：SCK=PA1，SDO=PA6，DIN=PA7，CNV=PA5。 */
#define AD7682_CNV_GPIO_Port GPIOA
#define AD7682_CNV_Pin       GPIO_PIN_5

/* CNV 拉低后的等待时间；用 TIM2 的 1 MHz 自由计数器测量。 */
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
    uint32_t acquisition_delay_us;
    uint32_t last_cnv_us;
} AD7682_Handle;

/* 初始化 SPI、CNV 引脚和参考电压参数。 */
/**
 * @brief 初始化驱动句柄、CNV 与微秒时基，保存 SPI 和参考电压配置。
 * @param dev AD7682 驱动句柄。
 * @param hspi SPI 外设句柄，须已完成初始化。
 * @param vref_volts ADC 参考电压，单位 V。
 * @return AD7682_OK 成功；AD7682_ERROR 参数或外设错误；AD7682_TIMEOUT 转换通信超时。
 * @note 与 TIM3 采样共享 ADC 资源，主循环诊断调用前须暂停正常采样；禁止并发访问。
 */
AD7682_Status AD7682_Init(AD7682_Handle *dev, SPI_HandleTypeDef *hspi,
                          float vref_volts);
/* TIM2 专用于 ADC 微秒计时：32 位、1 MHz、不产生中断、不占 GPIO。
 * Init 后可用；约 71.6 分钟回绕，时间差应使用无符号减法。 */
/**
 * @brief 读取 TIM2 微秒计数；32 位计数会回绕，时间差需用无符号减法。
 * @return 32 位微秒计数，会自然回绕。
 */
uint32_t AD7682_TimeUs(void);
/* 诊断接口：记录每次有效转换 CNV 上升沿附近的硬件时间戳。
 * 时间戳在写 CNV 前读取，有固定的寄存器访问延迟，分辨率为 1 us。
 * 不滤波；失败时不修改两个输出数组。 */
/**
 * @brief 读取同一通道五个有效样本及 CNV 附近的微秒时间戳；失败时不提交输出数组。
 * @param dev AD7682 驱动句柄。
 * @param channel ADC 输入通道编号，IN0～IN3 对应 0～3。
 * @param samples 接收有效 ADC 样本的数组，容量按声明维度提供。
 * @param timestamps_us 接收 CNV 附近时间戳的数组，单位 µs。
 * @param timeout_ms 单次事务超时，单位 ms。
 * @return AD7682_OK 成功；AD7682_ERROR 参数或外设错误；AD7682_TIMEOUT 转换通信超时。
 * @note 与 TIM3 采样共享 ADC 资源，主循环诊断调用前须暂停正常采样；禁止并发访问。
 * @note timeout_ms 为单次 SPI 超时，上限 1000 ms；失败不提交输出。
 */
AD7682_Status AD7682_ReadChannelTimedSamples(AD7682_Handle *dev,
                                             uint8_t channel,
                                             uint16_t samples[5],
                                             uint32_t timestamps_us[5],
                                             uint32_t timeout_ms);
/* 单次读取一个通道并返回原始码；当前配置关闭 CFG 回读，失败时不修改输出。
 * timeout_ms 是单次 SPI 事务超时，上限 1000 ms，不支持无限等待。 */
/**
 * @brief 读取一路有效 16 位 ADC 原始码；失败时不提交输出。
 * @param dev AD7682 驱动句柄。
 * @param channel ADC 输入通道编号，IN0～IN3 对应 0～3。
 * @param code ADC 码值；指针形式为输出，成功后写入。
 * @param timeout_ms 单次事务超时，单位 ms。
 * @return AD7682_OK 成功；AD7682_ERROR 参数或外设错误；AD7682_TIMEOUT 转换通信超时。
 * @note 与 TIM3 采样共享 ADC 资源，主循环诊断调用前须暂停正常采样；禁止并发访问。
 * @note timeout_ms 为单次 SPI 超时，上限 1000 ms；失败不提交输出。
 */
AD7682_Status AD7682_ReadChannel(AD7682_Handle *dev, uint8_t channel,
                                 uint16_t *code, uint32_t timeout_ms);
/* 切换通道后先丢弃两次流水线结果，再连续读取一个通道 5 次；
 * 失败时不修改输出。 */
/**
 * @brief 读取同一通道五个有效样本，不进行中值滤波。
 * @param dev AD7682 驱动句柄。
 * @param channel ADC 输入通道编号，IN0～IN3 对应 0～3。
 * @param samples 接收有效 ADC 样本的数组，容量按声明维度提供。
 * @param timeout_ms 单次事务超时，单位 ms。
 * @return AD7682_OK 成功；AD7682_ERROR 参数或外设错误；AD7682_TIMEOUT 转换通信超时。
 * @note 与 TIM3 采样共享 ADC 资源，主循环诊断调用前须暂停正常采样；禁止并发访问。
 */
AD7682_Status AD7682_ReadChannelSamples(AD7682_Handle *dev, uint8_t channel,
                                        uint16_t samples[5],
                                        uint32_t timeout_ms);
/* 四通道顺序扫描：每个通道先丢弃两次切换后的结果，再读取 5 个有效样本，
 * 以排序后第 3 个样本（中位数）作为该通道结果，抑制单点尖峰。 */
/**
 * @brief 扫描 IN0～IN3，每路五次有效转换取中位数作为输出。
 * @param dev AD7682 驱动句柄。
 * @param codes 接收 IN0～IN3 ADC 码值的四元素数组。
 * @param timeout_ms 单次事务超时，单位 ms。
 * @return AD7682_OK 成功；AD7682_ERROR 参数或外设错误；AD7682_TIMEOUT 转换通信超时。
 * @note 与 TIM3 采样共享 ADC 资源，主循环诊断调用前须暂停正常采样；禁止并发访问。
 */
AD7682_Status AD7682_ReadFourChannels(AD7682_Handle *dev, uint16_t codes[4],
                                      uint32_t timeout_ms);
/* IN0～IN3 每个通道先丢弃两次切换结果，再连续读取 5 个有效样本，
 * 数组布局为 samples[通道][样本序号]。 */
/**
 * @brief 扫描 IN0～IN3 并返回每路五个有效样本，供上层滤波或诊断。
 * @param dev AD7682 驱动句柄。
 * @param samples 接收有效 ADC 样本的数组，容量按声明维度提供。
 * @param timeout_ms 单次事务超时，单位 ms。
 * @return AD7682_OK 成功；AD7682_ERROR 参数或外设错误；AD7682_TIMEOUT 转换通信超时。
 * @note 与 TIM3 采样共享 ADC 资源，主循环诊断调用前须暂停正常采样；禁止并发访问。
 */
AD7682_Status AD7682_ReadFourChannelsSamples(AD7682_Handle *dev,
                                             uint16_t samples[4][5],
                                             uint32_t timeout_ms);
/* 对一个通道的 5 个有效原始样本取中位数。 */
/**
 * @brief 对五个原始码的副本排序并返回中位数，不修改输入数组。
 * @param values 参与中位数计算的五元素输入数组。
 * @return 五个输入 ADC 码值的中位数。
 */
uint16_t AD7682_Median5(const uint16_t values[5]);
/* 兼容旧接口：读取 IN0。 */
/**
 * @brief 兼容旧接口，读取 IN0 通道原始码。
 * @param dev AD7682 驱动句柄。
 * @param code ADC 码值；指针形式为输出，成功后写入。
 * @param timeout_ms 单次事务超时，单位 ms。
 * @return AD7682_OK 成功；AD7682_ERROR 参数或外设错误；AD7682_TIMEOUT 转换通信超时。
 * @note 与 TIM3 采样共享 ADC 资源，主循环诊断调用前须暂停正常采样；禁止并发访问。
 */
AD7682_Status AD7682_ReadRaw(AD7682_Handle *dev, uint16_t *code,
                             uint32_t timeout_ms);
/* 读取原始码并按参考电压换算为伏特。 */
/**
 * @brief 读取 IN0 原始码，按句柄中的参考电压换算为伏特。
 * @param dev AD7682 驱动句柄。
 * @param voltage 电压值，单位 V；指针形式用于接收输出。
 * @param timeout_ms 单次事务超时，单位 ms。
 * @return AD7682_OK 成功；AD7682_ERROR 参数或外设错误；AD7682_TIMEOUT 转换通信超时。
 * @note 与 TIM3 采样共享 ADC 资源，主循环诊断调用前须暂停正常采样；禁止并发访问。
 */
AD7682_Status AD7682_ReadVoltage(AD7682_Handle *dev, float *voltage,
                                 uint32_t timeout_ms);

#endif /* AD7682_H */
