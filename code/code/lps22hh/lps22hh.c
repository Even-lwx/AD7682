#include "lps22hh.h"

#define LPS22HH_REG_WHO_AM_I      0x0FU
#define LPS22HH_REG_CTRL_REG1     0x10U
#define LPS22HH_REG_CTRL_REG2     0x11U
#define LPS22HH_REG_STATUS        0x27U
#define LPS22HH_REG_PRESS_OUT_XL  0x28U
#define LPS22HH_REG_TEMP_OUT_L    0x2BU
#define LPS22HH_STATUS_P_DA       0x01U
#define LPS22HH_CTRL1_BDU         0x02U
#define LPS22HH_CTRL1_ODR_50HZ   0x40U
#define LPS22HH_CTRL2_IF_ADD_INC 0x10U
#define LPS22HH_CTRL2_SWRESET     0x04U

LPS22HH_Device g_lps22hh_u7 = { LPS22HH_I2C_ADDR_LOW };
LPS22HH_Device g_lps22hh_u11 = { LPS22HH_I2C_ADDR_HIGH };

/**
 * @brief 从指定器件连续读取气压计寄存器。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param address 目标地址；Flash 为字节地址，软件 I2C 为左移一位的器件地址。
 * @param reg 起始寄存器地址。
 * @param data 待处理字节或数据区；指针的输入输出方向由 const 和函数用途确定。
 * @param length 连续处理的字节数，数组尺寸须与该长度匹配。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static HAL_StatusTypeDef LPS22HH_ReadRegisters(SoftI2C_HandleTypeDef *handle,
                                               uint16_t address, uint8_t reg,
                                               uint8_t *data, uint16_t length,
                                               uint32_t timeout)
{
    return SoftI2C_MemRead(handle, address, reg, data, length, timeout);
}

/**
 * @brief 向指定器件写入一个气压计寄存器。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param address 目标地址；Flash 为字节地址，软件 I2C 为左移一位的器件地址。
 * @param reg 起始寄存器地址。
 * @param value 待编码、待解析或待写入的值；指针形式用于接收解析输出。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static HAL_StatusTypeDef LPS22HH_WriteRegister(SoftI2C_HandleTypeDef *handle,
                                               uint16_t address, uint8_t reg,
                                               uint8_t value, uint32_t timeout)
{
    return SoftI2C_MemWrite(handle, address, reg, &value, 1U, timeout);
}

/**
 * @brief 读取指定 I2C 地址的 WHO_AM_I 并将结果交给调用者。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param address 目标地址；Flash 为字节地址，软件 I2C 为左移一位的器件地址。
 * @param device_id 接收 WHO_AM_I 的输出指针。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static HAL_StatusTypeDef LPS22HH_ReadIdAt(SoftI2C_HandleTypeDef *handle,
                                          uint16_t address, uint8_t *device_id,
                                          uint32_t timeout)
{
    if ((handle == NULL) || (device_id == NULL)) { return HAL_ERROR; }
    return LPS22HH_ReadRegisters(handle, address, LPS22HH_REG_WHO_AM_I,
                                 device_id, 1U, timeout);
}

/**
 * @brief 提供指定地址的器件 ID 读取接口，便于诊断接线和地址。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param address 目标地址；Flash 为字节地址，软件 I2C 为左移一位的器件地址。
 * @param device_id 接收 WHO_AM_I 的输出指针。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
HAL_StatusTypeDef LPS22HH_ReadIdAtAddress(SoftI2C_HandleTypeDef *handle,
                                          uint16_t address, uint8_t *device_id,
                                          uint32_t timeout)
{
    return LPS22HH_ReadIdAt(handle, address, device_id, timeout);
}

/**
 * @brief 兼容单器件接口，读取默认低地址气压计的 ID。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param device_id 接收 WHO_AM_I 的输出指针。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
HAL_StatusTypeDef LPS22HH_ReadId(SoftI2C_HandleTypeDef *handle,
                                 uint8_t *device_id, uint32_t timeout)
{
    return LPS22HH_ReadIdAt(handle, g_lps22hh_u7.address, device_id, timeout);
}

/**
 * @brief 检查指定气压计 ID，执行复位并配置 1 Hz 连续采样。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param device 目标气压计器件及其 I2C 地址。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
HAL_StatusTypeDef LPS22HH_InitDevice(SoftI2C_HandleTypeDef *handle,
                                     LPS22HH_Device *device, uint32_t timeout)
{
    uint8_t device_id = 0U;
    HAL_StatusTypeDef status;

    if ((handle == NULL) || (device == NULL)) { return HAL_ERROR; }
    status = LPS22HH_ReadIdAt(handle, device->address, &device_id, timeout);
    if ((status != HAL_OK) || (device_id != LPS22HH_WHO_AM_I_VALUE)) { return HAL_ERROR; }

    status = LPS22HH_WriteRegister(handle, device->address, LPS22HH_REG_CTRL_REG2,
                                   LPS22HH_CTRL2_SWRESET, timeout);
    if (status != HAL_OK) { return status; }
    HAL_Delay(5U);

    status = LPS22HH_WriteRegister(handle, device->address, LPS22HH_REG_CTRL_REG2,
                                   LPS22HH_CTRL2_IF_ADD_INC, timeout);
    if (status == HAL_OK)
    {
        status = LPS22HH_WriteRegister(handle, device->address, LPS22HH_REG_CTRL_REG1,
                                       LPS22HH_CTRL1_BDU | LPS22HH_CTRL1_ODR_50HZ,
                                       timeout);
    }
    return status;
}

/**
 * @brief 初始化默认全局气压计器件。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
HAL_StatusTypeDef LPS22HH_Init(SoftI2C_HandleTypeDef *handle, uint32_t timeout)
{
    HAL_StatusTypeDef status = LPS22HH_InitDevice(handle, &g_lps22hh_u7, timeout);
    if (status != HAL_OK) { return status; }
    return LPS22HH_InitDevice(handle, &g_lps22hh_u11, timeout);
}

/**
 * @brief 保存 CTRL_REG1 后关闭 ODR 转换，供 ADC 干扰诊断使用。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param device 目标气压计器件及其 I2C 地址。
 * @param saved_ctrl_reg1 接收暂停前 CTRL_REG1 的输出指针，恢复时使用。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
HAL_StatusTypeDef LPS22HH_SetPowerDown(SoftI2C_HandleTypeDef *handle,
                                       const LPS22HH_Device *device,
                                       uint8_t *saved_ctrl_reg1,
                                       uint32_t timeout)
{
    uint8_t ctrl_reg1;
    HAL_StatusTypeDef status;

    if (handle == NULL || device == NULL || saved_ctrl_reg1 == NULL)
    {
        return HAL_ERROR;
    }
    status = LPS22HH_ReadRegisters(handle, device->address,
                                   LPS22HH_REG_CTRL_REG1, &ctrl_reg1,
                                   1U, timeout);
    if (status != HAL_OK) { return status; }
    *saved_ctrl_reg1 = ctrl_reg1;
    /* ODR[2:0]=000 时关闭气压和温度转换。 */
    ctrl_reg1 &= (uint8_t)~0x70U;
    return LPS22HH_WriteRegister(handle, device->address,
                                 LPS22HH_REG_CTRL_REG1, ctrl_reg1, timeout);
}

/**
 * @brief 恢复先前保存的 CTRL_REG1，使气压计回到诊断前状态。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param device 目标气压计器件及其 I2C 地址。
 * @param ctrl_reg1 需要写回的 CTRL_REG1 原值。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
HAL_StatusTypeDef LPS22HH_RestoreCtrlReg1(SoftI2C_HandleTypeDef *handle,
                                          const LPS22HH_Device *device,
                                          uint8_t ctrl_reg1,
                                          uint32_t timeout)
{
    if (handle == NULL || device == NULL) { return HAL_ERROR; }
    return LPS22HH_WriteRegister(handle, device->address,
                                 LPS22HH_REG_CTRL_REG1, ctrl_reg1, timeout);
}

/**
 * @brief 读取指定气压计的压力和温度原始寄存器，转换为 hPa 与 °C。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param device 目标气压计器件及其 I2C 地址。
 * @param pressure_hpa 接收压力结果的输出指针，单位 hPa。
 * @param temperature_c 接收温度结果的输出指针，单位 °C。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
HAL_StatusTypeDef LPS22HH_ReadDevice(SoftI2C_HandleTypeDef *handle,
                                     const LPS22HH_Device *device,
                                     float *pressure_hpa, float *temperature_c,
                                     uint32_t timeout)
{
    uint8_t status_reg;
    uint8_t data[5];
    int32_t pressure_raw;
    int16_t temperature_raw;
    HAL_StatusTypeDef status;

    if ((handle == NULL) || (device == NULL) ||
        (pressure_hpa == NULL) || (temperature_c == NULL)) { return HAL_ERROR; }

    status = LPS22HH_ReadRegisters(handle, device->address, LPS22HH_REG_STATUS,
                                   &status_reg, 1U, timeout);
    if (status != HAL_OK) { return status; }
    /* 即使 P_DA 尚未置位也读取一次原始寄存器，便于上电后诊断；
       BDU 已开启，跨字节读取不会得到撕裂数据。 */
    status = LPS22HH_ReadRegisters(handle, device->address, LPS22HH_REG_PRESS_OUT_XL,
                                   data, sizeof(data), timeout);
    if (status != HAL_OK) { return status; }

    pressure_raw = (int32_t)(((uint32_t)data[2] << 16U) |
                             ((uint32_t)data[1] << 8U) | data[0]);
    if ((pressure_raw & 0x00800000L) != 0L) { pressure_raw |= (int32_t)0xFF000000L; }
    temperature_raw = (int16_t)((uint16_t)data[3] | ((uint16_t)data[4] << 8U));
    *pressure_hpa = (float)pressure_raw / 4096.0f;
    *temperature_c = (float)temperature_raw / 100.0f;
    return HAL_OK;
}

/**
 * @brief 读取 ID、控制、状态和原始测量寄存器，供串口诊断使用。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param device 目标气压计器件及其 I2C 地址。
 * @param diagnostic 接收诊断寄存器快照的输出结构。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
HAL_StatusTypeDef LPS22HH_ReadDiagnostic(SoftI2C_HandleTypeDef *handle,
                                         const LPS22HH_Device *device,
                                         LPS22HH_Diagnostic *diagnostic,
                                         uint32_t timeout)
{
    uint8_t pressure[3];
    uint8_t temperature[2];
    HAL_StatusTypeDef status;

    if ((handle == NULL) || (device == NULL) || (diagnostic == NULL))
    {
        return HAL_ERROR;
    }
    status = LPS22HH_ReadRegisters(handle, device->address, LPS22HH_REG_WHO_AM_I,
                                   &diagnostic->who_am_i, 1U, timeout);
    if (status != HAL_OK) { return status; }
    status = LPS22HH_ReadRegisters(handle, device->address, LPS22HH_REG_CTRL_REG1,
                                   &diagnostic->ctrl_reg1, 1U, timeout);
    if (status != HAL_OK) { return status; }
    status = LPS22HH_ReadRegisters(handle, device->address, LPS22HH_REG_CTRL_REG2,
                                   &diagnostic->ctrl_reg2, 1U, timeout);
    if (status != HAL_OK) { return status; }
    status = LPS22HH_ReadRegisters(handle, device->address, LPS22HH_REG_STATUS,
                                   &diagnostic->status, 1U, timeout);
    if (status != HAL_OK) { return status; }
    status = LPS22HH_ReadRegisters(handle, device->address, LPS22HH_REG_PRESS_OUT_XL,
                                   pressure, 3U, timeout);
    if (status != HAL_OK) { return status; }
    status = LPS22HH_ReadRegisters(handle, device->address, LPS22HH_REG_TEMP_OUT_L,
                                   temperature, 2U, timeout);
    if (status != HAL_OK) { return status; }

    diagnostic->pressure_xl = pressure[0];
    diagnostic->pressure_l = pressure[1];
    diagnostic->pressure_h = pressure[2];
    diagnostic->temperature_l = temperature[0];
    diagnostic->temperature_h = temperature[1];
    return HAL_OK;
}

/**
 * @brief 兼容单器件接口，读取默认气压计的压力和温度。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param pressure_hpa 接收压力结果的输出指针，单位 hPa。
 * @param temperature_c 接收温度结果的输出指针，单位 °C。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
HAL_StatusTypeDef LPS22HH_Read(SoftI2C_HandleTypeDef *handle,
                               float *pressure_hpa, float *temperature_c,
                               uint32_t timeout)
{
    return LPS22HH_ReadDevice(handle, &g_lps22hh_u7,
                              pressure_hpa, temperature_c, timeout);
}

