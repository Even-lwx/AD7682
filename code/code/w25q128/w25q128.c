#include "w25q128.h"

/* W25Q128 驱动：SPI 模式 0，所有事务使用 PA2 手动控制 CS。 */

/**
 * @brief 控制 Flash 片选，低电平选中芯片。
 * @param state 目标 GPIO 电平，RESET 为低、SET 为高。
 * @return 无返回值。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static void W25Q128_SetCs(GPIO_PinState state)
{
    /* Flash CS# 低有效。 */
    HAL_GPIO_WritePin(W25Q128_CS_PORT, W25Q128_CS_PIN, state);
}

/**
 * @brief 在一次片选事务中发送不带地址的单字节 Flash 命令。
 * @param spi SPI 外设句柄，须已完成初始化。
 * @param command 命令字或以零结尾的串口命令。
 * @param timeout 底层事务超时，单位 ms；不是整个上层操作的总时间上限。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
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

/**
 * @brief 轮询状态寄存器 BUSY，等待编程或擦除结束，达到时限返回超时。
 * @param spi SPI 外设句柄，须已完成初始化。
 * @param timeout 底层事务超时，单位 ms；不是整个上层操作的总时间上限。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
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

/**
 * @brief 读取 JEDEC 的厂商、器件类型和容量三个字节。
 * @param spi SPI 外设句柄，须已完成初始化。
 * @param id 接收 JEDEC 厂商、类型和容量的三字节输出数组。
 * @param timeout 底层事务超时，单位 ms；不是整个上层操作的总时间上限。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
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

/**
 * @brief 以 24 位地址发送普通读取命令，将连续数据写入调用者缓冲区。
 * @param spi SPI 外设句柄，须已完成初始化。
 * @param address 目标地址；Flash 为字节地址，软件 I2C 为左移一位的器件地址。
 * @param buffer 数据缓冲区，容量须不小于 length；const 为待发送输入，否则用于接收。
 * @param length 连续处理的字节数，数组尺寸须与该长度匹配。
 * @param timeout 底层事务超时，单位 ms；不是整个上层操作的总时间上限。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
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

/**
 * @brief 先写使能再执行一次页编程并等待完成；调用者须确保不跨 256 字节页且目标已擦除。
 * @param spi SPI 外设句柄，须已完成初始化。
 * @param address 目标地址；Flash 为字节地址，软件 I2C 为左移一位的器件地址。
 * @param buffer 数据缓冲区，容量须不小于 length；const 为待发送输入，否则用于接收。
 * @param length 连续处理的字节数，数组尺寸须与该长度匹配。
 * @param timeout 底层事务超时，单位 ms；不是整个上层操作的总时间上限。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
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

/**
 * @brief 擦除包含指定地址的 4 KB 扇区并等待 BUSY 清零，会破坏该扇区原数据。
 * @param spi SPI 外设句柄，须已完成初始化。
 * @param address 目标地址；Flash 为字节地址，软件 I2C 为左移一位的器件地址。
 * @param timeout 底层事务超时，单位 ms；不是整个上层操作的总时间上限。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
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
