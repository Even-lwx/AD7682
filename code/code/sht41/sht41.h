/* SHT41 温湿度传感器驱动接口，使用软件 I2C。 */
#ifndef SHT41_H
#define SHT41_H
#include "i2c.h"
#define SHT41_ADDR (0x44U << 1) /* HAL 使用左移后的 8 位地址。 */
/* 读取温度和相对湿度，函数内部完成 CRC 校验。 */
/**
 * @brief 启动温湿度测量，等待结果并校验两个 CRC8，成功后换算 °C 和 %RH。
 * @param hi2c 软件 I2C 总线句柄。
 * @param temperature_c 接收温度结果的输出指针，单位 °C。
 * @param humidity_rh 接收相对湿度结果的输出指针，单位 %RH。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef SHT41_Read(SoftI2C_HandleTypeDef *hi2c, float *temperature_c, float *humidity_rh, uint32_t timeout);
/* 发送软复位命令。 */
/**
 * @brief 向 SHT41 发送软件复位命令并等待器件恢复。
 * @param hi2c 软件 I2C 总线句柄。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef SHT41_Reset(SoftI2C_HandleTypeDef *hi2c, uint32_t timeout);
#endif
