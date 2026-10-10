/* 软件 I2C 通用接口：使用 GPIO 开漏模拟总线时序。 */
#ifndef __I2C_H
#define __I2C_H
#include "stm32g0xx_hal.h"
typedef struct
{
    GPIO_TypeDef *scl_port;
    uint16_t scl_pin;
    GPIO_TypeDef *sda_port;
    uint16_t sda_pin;
} SoftI2C_HandleTypeDef;
extern SoftI2C_HandleTypeDef hi2c1;
void MX_I2C1_Init(void);
/* 主机发送/接收，不带寄存器地址。 */
HAL_StatusTypeDef SoftI2C_MasterTransmit(SoftI2C_HandleTypeDef *, uint16_t, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef SoftI2C_MasterReceive(SoftI2C_HandleTypeDef *, uint16_t, uint8_t *, uint16_t, uint32_t);
/* 先发送寄存器地址，再执行读写。 */
HAL_StatusTypeDef SoftI2C_MemRead(SoftI2C_HandleTypeDef *, uint16_t, uint8_t, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef SoftI2C_MemWrite(SoftI2C_HandleTypeDef *, uint16_t, uint8_t, uint8_t *, uint16_t, uint32_t);
#endif
