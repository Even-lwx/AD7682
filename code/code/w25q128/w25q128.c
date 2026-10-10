#include "w25q128.h"

/* W25Q128 驱动：SPI 模式 0，所有事务使用 PA2 手动控制 CS。 */

static void W25Q128_SetCs(GPIO_PinState state)
{
    /* Flash CS# 低有效。 */
    HAL_GPIO_WritePin(W25Q128_CS_PORT, W25Q128_CS_PIN, state);
}

static HAL_StatusTypeDef W25Q128_SendCommand(SPI_HandleTypeDef *spi,
                                             uint8_t command, uint32_t timeout)
{
    /* 发送不带地址和数据的单字节命令，例如写使能。 */
    HAL_StatusTypeDef status;

    W25Q128_SetCs(GPIO_PIN_RESET);
    status = HAL_SPI_Transmit(spi, &command, 1U, timeout);
    W25Q128_SetCs(GPIO_PIN_SET);
    return status;
}

static HAL_StatusTypeDef W25Q128_WaitReady(SPI_HandleTypeDef *spi,
                                           uint32_t timeout)
{
    /* 轮询状态寄存器 BUSY 位，直到芯片完成编程或擦除。 */
    uint8_t command = 0x05U;
    uint8_t status_register = 0U;
    uint32_t start = HAL_GetTick();

    do
    {
        W25Q128_SetCs(GPIO_PIN_RESET);
        if (HAL_SPI_Transmit(spi, &command, 1U, timeout) != HAL_OK)
        {
            W25Q128_SetCs(GPIO_PIN_SET);
            return HAL_ERROR;
        }
        if (HAL_SPI_Receive(spi, &status_register, 1U, timeout) != HAL_OK)
        {
            W25Q128_SetCs(GPIO_PIN_SET);
            return HAL_ERROR;
        }
        W25Q128_SetCs(GPIO_PIN_SET);
    } while (((status_register & 0x01U) != 0U) &&
             ((HAL_GetTick() - start) < timeout));

    return ((status_register & 0x01U) != 0U) ? HAL_TIMEOUT : HAL_OK;
}

HAL_StatusTypeDef W25Q128_ReadJedec(SPI_HandleTypeDef *spi, uint8_t id[3],
                                    uint32_t timeout)
{
    /* 0x9F 读取厂商、存储器类型和容量代码。 */
    uint8_t command = 0x9FU;
    HAL_StatusTypeDef status;

    W25Q128_SetCs(GPIO_PIN_RESET);
    status = HAL_SPI_Transmit(spi, &command, 1U, timeout);
    if (status == HAL_OK)
    {
        status = HAL_SPI_Receive(spi, id, 3U, timeout);
    }
    W25Q128_SetCs(GPIO_PIN_SET);
    return status;
}

HAL_StatusTypeDef W25Q128_Read(SPI_HandleTypeDef *spi, uint32_t address,
                               uint8_t *buffer, uint32_t length,
                               uint32_t timeout)
{
    /* 0x03 普通读取命令，地址为 24 位。 */
    uint8_t command[4] = {
        0x03U, (uint8_t)(address >> 16U), (uint8_t)(address >> 8U),
        (uint8_t)address
    };
    HAL_StatusTypeDef status;

    W25Q128_SetCs(GPIO_PIN_RESET);
    status = HAL_SPI_Transmit(spi, command, 4U, timeout);
    if (status == HAL_OK)
    {
        status = HAL_SPI_Receive(spi, buffer, length, timeout);
    }
    W25Q128_SetCs(GPIO_PIN_SET);
    return status;
}

HAL_StatusTypeDef W25Q128_Write(SPI_HandleTypeDef *spi, uint32_t address,
                                const uint8_t *buffer, uint32_t length,
                                uint32_t timeout)
{
    /* 页编程前必须先发送写使能，完成后等待 BUSY 清零。 */
    uint8_t command[4] = {
        0x02U, (uint8_t)(address >> 16U), (uint8_t)(address >> 8U),
        (uint8_t)address
    };
    uint8_t write_enable = 0x06U;
    HAL_StatusTypeDef status;

    if (length > 256U)
    {
        return HAL_ERROR;
    }
    if (W25Q128_SendCommand(spi, write_enable, timeout) != HAL_OK)
    {
        return HAL_ERROR;
    }
    W25Q128_SetCs(GPIO_PIN_RESET);
    status = HAL_SPI_Transmit(spi, command, 4U, timeout);
    if (status == HAL_OK)
    {
        status = HAL_SPI_Transmit(spi, (uint8_t *)buffer, length, timeout);
    }
    W25Q128_SetCs(GPIO_PIN_SET);
    if (status != HAL_OK)
    {
        return status;
    }
    return W25Q128_WaitReady(spi, timeout);
}

HAL_StatusTypeDef W25Q128_EraseSector(SPI_HandleTypeDef *spi, uint32_t address,
                                      uint32_t timeout)
{
    /* 0x20 擦除 4 KB 扇区，地址可使用该扇区内任意位置。 */
    uint8_t command[4] = {
        0x20U, (uint8_t)(address >> 16U), (uint8_t)(address >> 8U),
        (uint8_t)address
    };
    uint8_t write_enable = 0x06U;
    HAL_StatusTypeDef status;

    if (W25Q128_SendCommand(spi, write_enable, timeout) != HAL_OK)
    {
        return HAL_ERROR;
    }
    W25Q128_SetCs(GPIO_PIN_RESET);
    status = HAL_SPI_Transmit(spi, command, 4U, timeout);
    W25Q128_SetCs(GPIO_PIN_SET);
    if (status != HAL_OK)
    {
        return status;
    }
    return W25Q128_WaitReady(spi, timeout);
}
