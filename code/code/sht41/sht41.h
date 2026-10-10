/* SHT41 温湿度传感器驱动接口，使用软件 I2C。 */
#ifndef SHT41_H
#define SHT41_H
#include "i2c.h"
#define SHT41_ADDR (0x44U << 1) /* HAL 使用左移后的 8 位地址。 */
/* 读取温度和相对湿度，函数内部完成 CRC 校验。 */
HAL_StatusTypeDef SHT41_Read(SoftI2C_HandleTypeDef *hi2c, float *temperature_c, float *humidity_rh, uint32_t timeout);
/* 发送软复位命令。 */
HAL_StatusTypeDef SHT41_Reset(SoftI2C_HandleTypeDef *hi2c, uint32_t timeout);
#endif
