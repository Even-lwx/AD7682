/* AD7682 16 位 SAR ADC 驱动接口。 */
#ifndef AD7682_H
#define AD7682_H

#include "stm32g0xx_hal.h"

/* AD7682 schematic nets: SCK=PA1, SDO=PA6, DIN=PA7, CNV=PA5. */
#define AD7682_CNV_GPIO_Port GPIOA
#define AD7682_CNV_Pin       GPIO_PIN_5

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
} AD7682_Handle;

/* 初始化 SPI、CNV 引脚和参考电压参数。 */
AD7682_Status AD7682_Init(AD7682_Handle *dev, SPI_HandleTypeDef *hspi,
                          float vref_volts);
/* Read IN0..IN3, including configuration pipeline latency and readback clocking.
 * Output is unchanged on error. timeout_ms is per SPI transaction. */
AD7682_Status AD7682_ReadChannel(AD7682_Handle *dev, uint8_t channel,
                                 uint16_t *code, uint32_t timeout_ms);
AD7682_Status AD7682_ReadFourChannels(AD7682_Handle *dev, uint16_t codes[4],
                                      uint32_t timeout_ms);
/* Compatibility interface: reads IN0. */
AD7682_Status AD7682_ReadRaw(AD7682_Handle *dev, uint16_t *code,
                             uint32_t timeout_ms);
/* 读取原始码并按参考电压换算为伏特。 */
AD7682_Status AD7682_ReadVoltage(AD7682_Handle *dev, float *voltage,
                                 uint32_t timeout_ms);

#endif /* AD7682_H */
