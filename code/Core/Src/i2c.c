#include "i2c.h"

/* 软件 I2C 时序实现：GPIO 释放为高电平，外部上拉完成总线回升。 */

/* 软件 I2C：PB11 为 SCL，PB12 为 SDA，外部上拉到 3.3 V。 */
SoftI2C_HandleTypeDef hi2c1 = {GPIOB, GPIO_PIN_11, GPIOB, GPIO_PIN_12};

/**
 * @brief 通过短忙等待控制软件 I2C 位时序，不提供精确微秒延时。
 * @return 无返回值。
 * @note 当前实现为 GPIO 忙等待，不支持时钟拉伸检测；须由上层串行调用。
 */
static void SoftI2C_Delay(void)
{
    /* 简单延时用于限制 GPIO 翻转速度，不作为精密时基。 */
    for (volatile uint32_t i = 0U; i < 32U; ++i) { __NOP(); }
}

/**
 * @brief 设置软件 I2C 引脚电平并等待一个位时序延迟。
 * @param port GPIO 端口。
 * @param pin GPIO 引脚掩码。
 * @param state 目标 GPIO 电平，RESET 为低、SET 为高。
 * @return 无返回值。
 * @note 当前实现为 GPIO 忙等待，不支持时钟拉伸检测；须由上层串行调用。
 */
static void SoftI2C_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
    HAL_GPIO_WritePin(port, pin, state);
    SoftI2C_Delay();
}

/**
 * @brief 读取软件 I2C 数据线的当前电平。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @return GPIO_PIN_SET 为高电平，GPIO_PIN_RESET 为低电平。
 * @note 当前实现为 GPIO 忙等待，不支持时钟拉伸检测；须由上层串行调用。
 */
static GPIO_PinState SoftI2C_ReadSda(const SoftI2C_HandleTypeDef *handle)
{
    return HAL_GPIO_ReadPin(handle->sda_port, handle->sda_pin);
}

/**
 * @brief 产生软件 I2C 起始或重复起始条件。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @return 无返回值。
 * @note 当前实现为 GPIO 忙等待，不支持时钟拉伸检测；须由上层串行调用。
 */
static void SoftI2C_Start(SoftI2C_HandleTypeDef *handle)
{
    /* SDA 在 SCL 为高时由高变低，产生 I2C START。 */
    SoftI2C_WritePin(handle->sda_port, handle->sda_pin, GPIO_PIN_SET);
    SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_SET);
    SoftI2C_WritePin(handle->sda_port, handle->sda_pin, GPIO_PIN_RESET);
    SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_RESET);
}

/**
 * @brief 产生软件 I2C 停止条件并释放数据线。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @return 无返回值。
 * @note 当前实现为 GPIO 忙等待，不支持时钟拉伸检测；须由上层串行调用。
 */
static void SoftI2C_Stop(SoftI2C_HandleTypeDef *handle)
{
    /* SDA 在 SCL 为高时由低变高，产生 I2C STOP。 */
    SoftI2C_WritePin(handle->sda_port, handle->sda_pin, GPIO_PIN_RESET);
    SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_SET);
    SoftI2C_WritePin(handle->sda_port, handle->sda_pin, GPIO_PIN_SET);
}

/**
 * @brief 按高位优先发送一个字节，在第九个时钟读取从机 ACK。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param data 待处理字节或数据区；指针的输入输出方向由 const 和函数用途确定。
 * @return 1 收到从机 ACK，0 未收到 ACK。
 * @note 当前实现为 GPIO 忙等待，不支持时钟拉伸检测；须由上层串行调用。
 */
static uint8_t SoftI2C_WriteByte(SoftI2C_HandleTypeDef *handle, uint8_t data)
{
    /* 高位先发送，并在第 9 个时钟采样从机 ACK。 */
    uint8_t i;
    uint8_t ack;
    for (i = 0U; i < 8U; ++i)
    {
        SoftI2C_WritePin(handle->sda_port, handle->sda_pin,
                         (data & 0x80U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_SET);
        SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_RESET);
        data <<= 1U;
    }
    SoftI2C_WritePin(handle->sda_port, handle->sda_pin, GPIO_PIN_SET);
    SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_SET);
    ack = (SoftI2C_ReadSda(handle) == GPIO_PIN_RESET) ? 1U : 0U;
    SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_RESET);
    return ack;
}

/**
 * @brief 按高位优先读取一个字节，并根据参数向从机发送 ACK 或 NACK。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param ack 非 0 发送 ACK，0 发送 NACK。
 * @return 从总线接收的一个字节。
 * @note 当前实现为 GPIO 忙等待，不支持时钟拉伸检测；须由上层串行调用。
 */
static uint8_t SoftI2C_ReadByte(SoftI2C_HandleTypeDef *handle, uint8_t ack)
{
    /* 高位先接收，ack 参数决定本字节后发送 ACK 或 NACK。 */
    uint8_t i;
    uint8_t data = 0U;
    SoftI2C_WritePin(handle->sda_port, handle->sda_pin, GPIO_PIN_SET);
    for (i = 0U; i < 8U; ++i)
    {
        data <<= 1U;
        SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_SET);
        if (SoftI2C_ReadSda(handle) == GPIO_PIN_SET) { data |= 1U; }
        SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_RESET);
    }
    SoftI2C_WritePin(handle->sda_port, handle->sda_pin,
                     ack ? GPIO_PIN_RESET : GPIO_PIN_SET);
    SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_SET);
    SoftI2C_WritePin(handle->scl_port, handle->scl_pin, GPIO_PIN_RESET);
    SoftI2C_WritePin(handle->sda_port, handle->sda_pin, GPIO_PIN_SET);
    return data;
}

/**
 * @brief 将 PB11/PB12 配置为软件 I2C 开漏输出并释放总线。
 * @return 无返回值。
 */
void MX_I2C1_Init(void)
{
    /* 软件 I2C 两根线均配置为开漏输出并释放为高电平。 */
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_11 | GPIO_PIN_12;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11 | GPIO_PIN_12, GPIO_PIN_SET);
}

/**
 * @brief 发送设备写地址及连续数据，地址或数据未获 ACK 时停止并返回错误。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param address 目标地址；Flash 为字节地址，软件 I2C 为左移一位的器件地址。
 * @param data 待处理字节或数据区；指针的输入输出方向由 const 和函数用途确定。
 * @param length 连续处理的字节数，数组尺寸须与该长度匹配。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 当前实现为 GPIO 忙等待，不支持时钟拉伸检测；须由上层串行调用。
 */
HAL_StatusTypeDef SoftI2C_MasterTransmit(SoftI2C_HandleTypeDef *handle,
                                         uint16_t address, uint8_t *data,
                                         uint16_t length, uint32_t timeout)
{
    uint16_t i;
    (void)timeout;
    SoftI2C_Start(handle);
    if (!SoftI2C_WriteByte(handle, (uint8_t)(address & 0xFEU))) { SoftI2C_Stop(handle); return HAL_ERROR; }
    for (i = 0U; i < length; ++i)
    {
        if (!SoftI2C_WriteByte(handle, data[i])) { SoftI2C_Stop(handle); return HAL_ERROR; }
    }
    SoftI2C_Stop(handle);
    return HAL_OK;
}

/**
 * @brief 发送设备读地址并连续接收数据，末字节后发送 NACK。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param address 目标地址；Flash 为字节地址，软件 I2C 为左移一位的器件地址。
 * @param data 待处理字节或数据区；指针的输入输出方向由 const 和函数用途确定。
 * @param length 连续处理的字节数，数组尺寸须与该长度匹配。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 当前实现为 GPIO 忙等待，不支持时钟拉伸检测；须由上层串行调用。
 */
HAL_StatusTypeDef SoftI2C_MasterReceive(SoftI2C_HandleTypeDef *handle,
                                        uint16_t address, uint8_t *data,
                                        uint16_t length, uint32_t timeout)
{
    uint16_t i;
    (void)timeout;
    SoftI2C_Start(handle);
    if (!SoftI2C_WriteByte(handle, (uint8_t)(address | 0x01U))) { SoftI2C_Stop(handle); return HAL_ERROR; }
    for (i = 0U; i < length; ++i) { data[i] = SoftI2C_ReadByte(handle, (i + 1U < length) ? 1U : 0U); }
    SoftI2C_Stop(handle);
    return HAL_OK;
}

/**
 * @brief 写入寄存器地址后发重复起始，连续读取指定寄存器数据。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param address 目标地址；Flash 为字节地址，软件 I2C 为左移一位的器件地址。
 * @param reg 起始寄存器地址。
 * @param data 待处理字节或数据区；指针的输入输出方向由 const 和函数用途确定。
 * @param length 连续处理的字节数，数组尺寸须与该长度匹配。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 当前实现为 GPIO 忙等待，不支持时钟拉伸检测；须由上层串行调用。
 */
HAL_StatusTypeDef SoftI2C_MemRead(SoftI2C_HandleTypeDef *handle, uint16_t address,
                                  uint8_t reg, uint8_t *data, uint16_t length,
                                  uint32_t timeout)
{
    (void)timeout;
    SoftI2C_Start(handle);
    if (!SoftI2C_WriteByte(handle, (uint8_t)(address & 0xFEU)) || !SoftI2C_WriteByte(handle, reg))
    { SoftI2C_Stop(handle); return HAL_ERROR; }
    SoftI2C_Start(handle);
    if (!SoftI2C_WriteByte(handle, (uint8_t)(address | 0x01U))) { SoftI2C_Stop(handle); return HAL_ERROR; }
    while (length-- > 0U) { *data++ = SoftI2C_ReadByte(handle, (length != 0U) ? 1U : 0U); }
    SoftI2C_Stop(handle);
    return HAL_OK;
}

/**
 * @brief 发送寄存器地址及连续数据，任何写字节未获 ACK 时停止。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param address 目标地址；Flash 为字节地址，软件 I2C 为左移一位的器件地址。
 * @param reg 起始寄存器地址。
 * @param data 待处理字节或数据区；指针的输入输出方向由 const 和函数用途确定。
 * @param length 连续处理的字节数，数组尺寸须与该长度匹配。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 当前实现为 GPIO 忙等待，不支持时钟拉伸检测；须由上层串行调用。
 */
HAL_StatusTypeDef SoftI2C_MemWrite(SoftI2C_HandleTypeDef *handle, uint16_t address,
                                   uint8_t reg, uint8_t *data, uint16_t length,
                                   uint32_t timeout)
{
    uint16_t i;
    (void)timeout;
    SoftI2C_Start(handle);
    if (!SoftI2C_WriteByte(handle, (uint8_t)(address & 0xFEU)) || !SoftI2C_WriteByte(handle, reg))
    { SoftI2C_Stop(handle); return HAL_ERROR; }
    for (i = 0U; i < length; ++i)
    {
        if (!SoftI2C_WriteByte(handle, data[i])) { SoftI2C_Stop(handle); return HAL_ERROR; }
    }
    SoftI2C_Stop(handle);
    return HAL_OK;
}
