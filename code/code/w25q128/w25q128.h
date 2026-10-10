/* W25Q128 SPI NOR Flash 驱动接口。 */
#ifndef W25Q128_H
#define W25Q128_H
#include "stm32g0xx_hal.h"
#define W25Q128_CS_PORT GPIOA
#define W25Q128_CS_PIN GPIO_PIN_2 /* PA2，低电平选中。 */
/* 读取 JEDEC 厂商、型号和容量 ID。 */
HAL_StatusTypeDef W25Q128_ReadJedec(SPI_HandleTypeDef *spi, uint8_t id[3], uint32_t timeout);
/* 从 24 位地址连续读取数据。 */
HAL_StatusTypeDef W25Q128_Read(SPI_HandleTypeDef *spi, uint32_t address, uint8_t *buffer, uint32_t length, uint32_t timeout);
/* 页编程，单次长度不能超过 256 字节。 */
HAL_StatusTypeDef W25Q128_Write(SPI_HandleTypeDef *spi, uint32_t address, const uint8_t *buffer, uint32_t length, uint32_t timeout);
/* 擦除包含指定地址的 4 KB 扇区。 */
HAL_StatusTypeDef W25Q128_EraseSector(SPI_HandleTypeDef *spi, uint32_t address, uint32_t timeout);
#endif
