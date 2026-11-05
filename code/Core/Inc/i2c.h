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
/**
 * @brief 将 PB11/PB12 配置为软件 I2C 开漏输出并释放总线。
 * @return 无返回值。
 */
void MX_I2C1_Init(void);
/* 主机发送/接收，不带寄存器地址。 */
/**
 * @brief 发送设备写地址及连续数据，地址或数据未获 ACK 时停止并返回错误。
 * @param handle 已初始化的软件 I2C 总线句柄。
 * @param address 左移一位后的器件地址。
 * @param data 待发送字节缓冲区，容量至少为 length。
 * @param length 处理的字节数。
 * @param timeout 保留参数，当前 GPIO 位时序实现不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 当前实现为 GPIO 忙等待，不支持时钟拉伸检测；须由上层串行调用。
 */
HAL_StatusTypeDef SoftI2C_MasterTransmit(SoftI2C_HandleTypeDef *handle, uint16_t address, uint8_t *data, uint16_t length, uint32_t timeout);
/**
 * @brief 发送设备读地址并连续接收数据，末字节后发送 NACK。
 * @param handle 已初始化的软件 I2C 总线句柄。
 * @param address 左移一位后的器件地址。
 * @param data 接收输出字节缓冲区，容量至少为 length。
 * @param length 处理的字节数。
 * @param timeout 保留参数，当前 GPIO 位时序实现不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 当前实现为 GPIO 忙等待，不支持时钟拉伸检测；须由上层串行调用。
 */
HAL_StatusTypeDef SoftI2C_MasterReceive(SoftI2C_HandleTypeDef *handle, uint16_t address, uint8_t *data, uint16_t length, uint32_t timeout);
/* 先发送寄存器地址，再执行读写。 */
/**
 * @brief 写入寄存器地址后发重复起始，连续读取指定寄存器数据。
 * @param handle 已初始化的软件 I2C 总线句柄。
 * @param address 左移一位后的器件地址。
 * @param reg 起始寄存器地址。
 * @param data 接收输出字节缓冲区，容量至少为 length。
 * @param length 处理的字节数。
 * @param timeout 保留参数，当前 GPIO 位时序实现不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 当前实现为 GPIO 忙等待，不支持时钟拉伸检测；须由上层串行调用。
 */
HAL_StatusTypeDef SoftI2C_MemRead(SoftI2C_HandleTypeDef *handle, uint16_t address, uint8_t reg, uint8_t *data, uint16_t length, uint32_t timeout);
/**
 * @brief 发送寄存器地址及连续数据，任何写字节未获 ACK 时停止。
 * @param handle 已初始化的软件 I2C 总线句柄。
 * @param address 左移一位后的器件地址。
 * @param reg 起始寄存器地址。
 * @param data 待发送字节缓冲区，容量至少为 length。
 * @param length 处理的字节数。
 * @param timeout 保留参数，当前 GPIO 位时序实现不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 当前实现为 GPIO 忙等待，不支持时钟拉伸检测；须由上层串行调用。
 */
HAL_StatusTypeDef SoftI2C_MemWrite(SoftI2C_HandleTypeDef *handle, uint16_t address, uint8_t reg, uint8_t *data, uint16_t length, uint32_t timeout);
#endif
