#ifndef __I2C_H
#define __I2C_H
#include "stm32g0xx_hal.h"
typedef struct { GPIO_TypeDef *scl_port; uint16_t scl_pin; GPIO_TypeDef *sda_port; uint16_t sda_pin; } SoftI2C_HandleTypeDef;
typedef SoftI2C_HandleTypeDef I2C_HandleTypeDef;
#define I2C_MEMADD_SIZE_8BIT 1U
extern SoftI2C_HandleTypeDef hi2c1;
void MX_I2C1_Init(void);
HAL_StatusTypeDef SoftI2C_MasterTransmit(SoftI2C_HandleTypeDef *, uint16_t, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef SoftI2C_MasterReceive(SoftI2C_HandleTypeDef *, uint16_t, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef SoftI2C_MemRead(SoftI2C_HandleTypeDef *, uint16_t, uint8_t, uint8_t *, uint16_t, uint32_t);
HAL_StatusTypeDef SoftI2C_MemWrite(SoftI2C_HandleTypeDef *, uint16_t, uint8_t, uint8_t *, uint16_t, uint32_t);
#define HAL_I2C_Master_Transmit SoftI2C_MasterTransmit
#define HAL_I2C_Master_Receive SoftI2C_MasterReceive
#define HAL_I2C_Mem_Read(h,a,r,s,p,n,t) SoftI2C_MemRead((h),(a),(r),(p),(n),(t))
#define HAL_I2C_Mem_Write(h,a,r,s,p,n,t) SoftI2C_MemWrite((h),(a),(r),(p),(n),(t))
#endif
