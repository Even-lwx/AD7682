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
HAL_StatusTypeDef LPS22HH_Init(SoftI2C_HandleTypeDef *handle, uint32_t timeout);
HAL_StatusTypeDef LPS22HH_InitDevice(SoftI2C_HandleTypeDef *handle,
                                     LPS22HH_Device *device, uint32_t timeout);
/* Diagnostic controls: stop ODR conversions while preserving CTRL_REG1 for restore. */
HAL_StatusTypeDef LPS22HH_SetPowerDown(SoftI2C_HandleTypeDef *handle,
                                       const LPS22HH_Device *device,
                                       uint8_t *saved_ctrl_reg1,
                                       uint32_t timeout);
HAL_StatusTypeDef LPS22HH_RestoreCtrlReg1(SoftI2C_HandleTypeDef *handle,
                                          const LPS22HH_Device *device,
                                          uint8_t ctrl_reg1,
                                          uint32_t timeout);

/* 读取最新气压（hPa）和温度（°C）。 */
HAL_StatusTypeDef LPS22HH_Read(SoftI2C_HandleTypeDef *handle,
                               float *pressure_hpa, float *temperature_c,
                               uint32_t timeout);
HAL_StatusTypeDef LPS22HH_ReadDevice(SoftI2C_HandleTypeDef *handle,
                                     const LPS22HH_Device *device,
                                     float *pressure_hpa,
                                     float *temperature_c,
                                     uint32_t timeout);
HAL_StatusTypeDef LPS22HH_ReadDiagnostic(SoftI2C_HandleTypeDef *handle,
                                         const LPS22HH_Device *device,
                                         LPS22HH_Diagnostic *diagnostic,
                                         uint32_t timeout);

/* 读取 WHO_AM_I，便于串口诊断和产线检查。 */
HAL_StatusTypeDef LPS22HH_ReadId(SoftI2C_HandleTypeDef *handle,
                                 uint8_t *device_id, uint32_t timeout);
HAL_StatusTypeDef LPS22HH_ReadIdAtAddress(SoftI2C_HandleTypeDef *handle,
                                           uint16_t address,
                                           uint8_t *device_id,
                                           uint32_t timeout);

#endif
