/* LPS22HH 气压/温度传感器驱动，使用 PB11/PB12 软件 I2C。 */
#ifndef LPS22HH_H
#define LPS22HH_H

#include "i2c.h"

#define LPS22HH_I2C_ADDR_LOW   (0x5CU << 1) /* SA0=0，7 位地址 0x5C。 */
#define LPS22HH_I2C_ADDR_HIGH  (0x5DU << 1) /* SA0=1，7 位地址 0x5D。 */
#define LPS22HH_WHO_AM_I_VALUE 0xB3U

typedef struct
{
    uint16_t address; /* 软件 I2C 使用的 8 位基址：0xB8 或 0xBA。 */
} LPS22HH_Device;

typedef struct
{
    uint8_t who_am_i;
    uint8_t ctrl_reg1;
    uint8_t ctrl_reg2;
    uint8_t status;
    uint8_t pressure_xl;
    uint8_t pressure_l;
    uint8_t pressure_h;
    uint8_t temperature_l;
    uint8_t temperature_h;
} LPS22HH_Diagnostic;

extern LPS22HH_Device g_lps22hh_u7;
extern LPS22HH_Device g_lps22hh_u11;

/* 探测传感器、复位并配置 1 Hz 连续采样。 */
/**
 * @brief 初始化默认全局气压计器件。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef LPS22HH_Init(SoftI2C_HandleTypeDef *handle, uint32_t timeout);
/**
 * @brief 检查指定气压计 ID，执行复位并配置 1 Hz 连续采样。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param device 目标气压计器件及其 I2C 地址。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef LPS22HH_InitDevice(SoftI2C_HandleTypeDef *handle,
                                     LPS22HH_Device *device, uint32_t timeout);
/* 诊断控制：停止 ODR 转换，同时保留 CTRL_REG1 以便恢复。 */
/**
 * @brief 保存 CTRL_REG1 后关闭 ODR 转换，供 ADC 干扰诊断使用。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param device 目标气压计器件及其 I2C 地址。
 * @param saved_ctrl_reg1 接收暂停前 CTRL_REG1 的输出指针，恢复时使用。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef LPS22HH_SetPowerDown(SoftI2C_HandleTypeDef *handle,
                                       const LPS22HH_Device *device,
                                       uint8_t *saved_ctrl_reg1,
                                       uint32_t timeout);
/**
 * @brief 恢复先前保存的 CTRL_REG1，使气压计回到诊断前状态。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param device 目标气压计器件及其 I2C 地址。
 * @param ctrl_reg1 需要写回的 CTRL_REG1 原值。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef LPS22HH_RestoreCtrlReg1(SoftI2C_HandleTypeDef *handle,
                                          const LPS22HH_Device *device,
                                          uint8_t ctrl_reg1,
                                          uint32_t timeout);

/* 读取最新气压（hPa）和温度（°C）。 */
/**
 * @brief 兼容单器件接口，读取默认气压计的压力和温度。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param pressure_hpa 接收压力结果的输出指针，单位 hPa。
 * @param temperature_c 接收温度结果的输出指针，单位 °C。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef LPS22HH_Read(SoftI2C_HandleTypeDef *handle,
                               float *pressure_hpa, float *temperature_c,
                               uint32_t timeout);
/**
 * @brief 读取指定气压计的压力和温度原始寄存器，转换为 hPa 与 °C。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param device 目标气压计器件及其 I2C 地址。
 * @param pressure_hpa 接收压力结果的输出指针，单位 hPa。
 * @param temperature_c 接收温度结果的输出指针，单位 °C。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef LPS22HH_ReadDevice(SoftI2C_HandleTypeDef *handle,
                                     const LPS22HH_Device *device,
                                     float *pressure_hpa,
                                     float *temperature_c,
                                     uint32_t timeout);
/**
 * @brief 读取 ID、控制、状态和原始测量寄存器，供串口诊断使用。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param device 目标气压计器件及其 I2C 地址。
 * @param diagnostic 接收诊断寄存器快照的输出结构。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef LPS22HH_ReadDiagnostic(SoftI2C_HandleTypeDef *handle,
                                         const LPS22HH_Device *device,
                                         LPS22HH_Diagnostic *diagnostic,
                                         uint32_t timeout);

/* 读取 WHO_AM_I，便于串口诊断和产线检查。 */
/**
 * @brief 兼容单器件接口，读取默认低地址气压计的 ID。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param device_id 接收 WHO_AM_I 的输出指针。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef LPS22HH_ReadId(SoftI2C_HandleTypeDef *handle,
                                 uint8_t *device_id, uint32_t timeout);
/**
 * @brief 提供指定地址的器件 ID 读取接口，便于诊断接线和地址。
 * @param handle 外设或软件总线句柄，须由调用者正确初始化。
 * @param address 目标地址；Flash 为字节地址，软件 I2C 为左移一位的器件地址。
 * @param device_id 接收 WHO_AM_I 的输出指针。
 * @param timeout 保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 */
HAL_StatusTypeDef LPS22HH_ReadIdAtAddress(SoftI2C_HandleTypeDef *handle,
                                           uint16_t address,
                                           uint8_t *device_id,
                                           uint32_t timeout);

#endif

