#include "sht41.h"
static uint8_t crc8(const uint8_t *p, uint8_t n) { uint8_t c=0xFF; while(n--){c^=*p++; for(uint8_t i=0;i<8;i++) c=(c&0x80)?(uint8_t)((c<<1)^0x31):(uint8_t)(c<<1);} return c; }
HAL_StatusTypeDef SHT41_Reset(SoftI2C_HandleTypeDef *h, uint32_t t) { uint8_t c=0x94; return SoftI2C_MasterTransmit(h,SHT41_ADDR,&c,1,t); }
HAL_StatusTypeDef SHT41_Read(SoftI2C_HandleTypeDef *h, float *tc, float *rh, uint32_t t)
{
  uint8_t cmd=0xFD, b[6]; HAL_StatusTypeDef s=SoftI2C_MasterTransmit(h,SHT41_ADDR,&cmd,1,t); if(s!=HAL_OK)return s;
  HAL_Delay(10); s=SoftI2C_MasterReceive(h,SHT41_ADDR,b,6,t); if(s!=HAL_OK)return s;
  if(crc8(b,2)!=b[2] || crc8(&b[3],2)!=b[5]) return HAL_ERROR;
  *tc=-45.0f+175.0f*((uint16_t)b[0]*256U+b[1])/65535.0f;
  *rh=-6.0f+125.0f*((uint16_t)b[3]*256U+b[4])/65535.0f; if(*rh<0)*rh=0; if(*rh>100)*rh=100; return HAL_OK;
}
