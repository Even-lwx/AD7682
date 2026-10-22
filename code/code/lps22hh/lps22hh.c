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

static HAL_StatusTypeDef LPS22HH_ReadRegisters(SoftI2C_HandleTypeDef *handle,
                                               uint16_t address, uint8_t reg,
                                               uint8_t *data, uint16_t length,
                                               uint32_t timeout)
{
    return SoftI2C_MemRead(handle, address, reg, data, length, timeout);
}

static HAL_StatusTypeDef LPS22HH_WriteRegister(SoftI2C_HandleTypeDef *handle,
                                               uint16_t address, uint8_t reg,
                                               uint8_t value, uint32_t timeout)
{
    return SoftI2C_MemWrite(handle, address, reg, &value, 1U, timeout);
}

static HAL_StatusTypeDef LPS22HH_ReadIdAt(SoftI2C_HandleTypeDef *handle,
                                          uint16_t address, uint8_t *device_id,
                                          uint32_t timeout)
{
    if ((handle == NULL) || (device_id == NULL)) { return HAL_ERROR; }
    return LPS22HH_ReadRegisters(handle, address, LPS22HH_REG_WHO_AM_I,
                                 device_id, 1U, timeout);
}

HAL_StatusTypeDef LPS22HH_ReadIdAtAddress(SoftI2C_HandleTypeDef *handle,
                                          uint16_t address, uint8_t *device_id,
                                          uint32_t timeout)
{
    return LPS22HH_ReadIdAt(handle, address, device_id, timeout);
}

HAL_StatusTypeDef LPS22HH_ReadId(SoftI2C_HandleTypeDef *handle,
                                 uint8_t *device_id, uint32_t timeout)
{
    return LPS22HH_ReadIdAt(handle, g_lps22hh_u7.address, device_id, timeout);
}

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

HAL_StatusTypeDef LPS22HH_Init(SoftI2C_HandleTypeDef *handle, uint32_t timeout)
{
    HAL_StatusTypeDef status = LPS22HH_InitDevice(handle, &g_lps22hh_u7, timeout);
    if (status != HAL_OK) { return status; }
    return LPS22HH_InitDevice(handle, &g_lps22hh_u11, timeout);
}

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
    /* ODR[2:0]=000 powers down pressure/temperature conversions. */
    ctrl_reg1 &= (uint8_t)~0x70U;
    return LPS22HH_WriteRegister(handle, device->address,
                                 LPS22HH_REG_CTRL_REG1, ctrl_reg1, timeout);
}

HAL_StatusTypeDef LPS22HH_RestoreCtrlReg1(SoftI2C_HandleTypeDef *handle,
                                          const LPS22HH_Device *device,
                                          uint8_t ctrl_reg1,
                                          uint32_t timeout)
{
    if (handle == NULL || device == NULL) { return HAL_ERROR; }
    return LPS22HH_WriteRegister(handle, device->address,
                                 LPS22HH_REG_CTRL_REG1, ctrl_reg1, timeout);
}

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

HAL_StatusTypeDef LPS22HH_Read(SoftI2C_HandleTypeDef *handle,
                               float *pressure_hpa, float *temperature_c,
                               uint32_t timeout)
{
    return LPS22HH_ReadDevice(handle, &g_lps22hh_u7,
                              pressure_hpa, temperature_c, timeout);
}
