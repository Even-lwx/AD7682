#ifndef PCF8563_H
#define PCF8563_H
#include "i2c.h"
#define PCF8563_I2C_ADDR (0x51U << 1)
typedef struct { uint8_t seconds, minutes, hours, day, weekday, month, year, century; } PCF8563_DateTime;
HAL_StatusTypeDef PCF8563_ReadDateTime(SoftI2C_HandleTypeDef*, PCF8563_DateTime*, uint32_t);
HAL_StatusTypeDef PCF8563_WriteDateTime(SoftI2C_HandleTypeDef*, const PCF8563_DateTime*, uint32_t);
HAL_StatusTypeDef PCF8563_SetRunning(SoftI2C_HandleTypeDef*, uint8_t, uint32_t);
HAL_StatusTypeDef PCF8563_GetVoltageLow(SoftI2C_HandleTypeDef*, uint8_t*, uint32_t);
#endif
