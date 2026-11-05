/* W25Q128 SPI NOR Flash 驱动接口。 */
#ifndef W25Q128_H
#define W25Q128_H
#include "stm32g0xx_hal.h"
#define W25Q128_CS_PORT GPIOA
#define W25Q128_CS_PIN GPIO_PIN_2 /* PA2，低电平选中。 */
/* 读取 JEDEC 厂商、型号和容量 ID。 */
/**
 * @brief 读取 JEDEC 的厂商、器件类型和容量三个字节。
 * @param spi SPI 外设句柄，须已完成初始化。
 * @param id 接收 JEDEC 厂商、类型和容量的三字节输出数组。
 * @param timeout 底层事务超时，单位 ms；不是整个上层操作的总时间上限。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef W25Q128_ReadJedec(SPI_HandleTypeDef *spi, uint8_t id[3], uint32_t timeout);
/* 从 24 位地址连续读取数据。 */
/**
 * @brief 以 24 位地址发送普通读取命令，将连续数据写入调用者缓冲区。
 * @param spi SPI 外设句柄，须已完成初始化。
 * @param address 目标地址；Flash 为字节地址，软件 I2C 为左移一位的器件地址。
 * @param buffer 数据缓冲区，容量须不小于 length；const 为待发送输入，否则用于接收。
 * @param length 连续处理的字节数，数组尺寸须与该长度匹配。
 * @param timeout 底层事务超时，单位 ms；不是整个上层操作的总时间上限。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef W25Q128_Read(SPI_HandleTypeDef *spi, uint32_t address, uint8_t *buffer, uint32_t length, uint32_t timeout);
/* 页编程，单次长度不能超过 256 字节。 */
/**
 * @brief 先写使能再执行一次页编程并等待完成；调用者须确保不跨 256 字节页且目标已擦除。
 * @param spi SPI 外设句柄，须已完成初始化。
 * @param address 目标地址；Flash 为字节地址，软件 I2C 为左移一位的器件地址。
 * @param buffer 数据缓冲区，容量须不小于 length；const 为待发送输入，否则用于接收。
 * @param length 连续处理的字节数，数组尺寸须与该长度匹配。
 * @param timeout 底层事务超时，单位 ms；不是整个上层操作的总时间上限。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef W25Q128_Write(SPI_HandleTypeDef *spi, uint32_t address, const uint8_t *buffer, uint32_t length, uint32_t timeout);
/* 擦除包含指定地址的 4 KB 扇区。 */
/**
 * @brief 擦除包含指定地址的 4 KB 扇区并等待 BUSY 清零，会破坏该扇区原数据。
 * @param spi SPI 外设句柄，须已完成初始化。
 * @param address 目标地址；Flash 为字节地址，软件 I2C 为左移一位的器件地址。
 * @param timeout 底层事务超时，单位 ms；不是整个上层操作的总时间上限。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef W25Q128_EraseSector(SPI_HandleTypeDef *spi, uint32_t address, uint32_t timeout);
#endif
