/* PCF8563 RTC 驱动接口，支持 BCD 时间读写和 VL 标志读取。 */
#ifndef PCF8563_H
#define PCF8563_H
#include "i2c.h"
#define PCF8563_I2C_ADDR (0x51U << 1) /* HAL 使用左移后的 8 位地址。 */
typedef struct
{
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t weekday;
    uint8_t month;
    uint8_t year;
    uint8_t century;
} PCF8563_DateTime;
/* 读取寄存器 02h 至 08h 的时间数据。 */
HAL_StatusTypeDef PCF8563_ReadDateTime(SoftI2C_HandleTypeDef *handle, PCF8563_DateTime *datetime, uint32_t timeout);
/* 写入时间并保留世纪位配置。 */
HAL_StatusTypeDef PCF8563_WriteDateTime(SoftI2C_HandleTypeDef *handle, const PCF8563_DateTime *datetime, uint32_t timeout);
/* running 非 0 清除 STOP 位，启动 RTC 振荡器。 */
HAL_StatusTypeDef PCF8563_SetRunning(SoftI2C_HandleTypeDef *handle, uint8_t running, uint32_t timeout);
/* 读取秒寄存器的 VL（电压低）标志。 */
HAL_StatusTypeDef PCF8563_GetVoltageLow(SoftI2C_HandleTypeDef *handle, uint8_t *voltage_low, uint32_t timeout);
#endif
