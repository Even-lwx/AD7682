#ifndef SHT41_H
#define SHT41_H
#include "i2c.h"
#define SHT41_ADDR (0x44U << 1)
HAL_StatusTypeDef SHT41_Read(SoftI2C_HandleTypeDef *hi2c, float *temperature_c, float *humidity_rh, uint32_t timeout);
HAL_StatusTypeDef SHT41_Reset(SoftI2C_HandleTypeDef *hi2c, uint32_t timeout);
#endif
