#include "pcf8563.h"
#define REG_CONTROL1 0x00U
#define REG_SECONDS 0x02U
static uint8_t b2i(uint8_t v){return (uint8_t)((v>>4)*10U+(v&15U));}
static uint8_t i2b(uint8_t v){return (uint8_t)(((v/10U)<<4)|(v%10U));}
static HAL_StatusTypeDef rd(SoftI2C_HandleTypeDef*h,uint8_t r,uint8_t*d,uint16_t n,uint32_t t){return SoftI2C_MemRead(h,PCF8563_I2C_ADDR,r,d,n,t);}
static HAL_StatusTypeDef wr(SoftI2C_HandleTypeDef*h,uint8_t r,uint8_t*d,uint16_t n,uint32_t t){return SoftI2C_MemWrite(h,PCF8563_I2C_ADDR,r,d,n,t);}
HAL_StatusTypeDef PCF8563_ReadDateTime(SoftI2C_HandleTypeDef*h,PCF8563_DateTime*dt,uint32_t t){uint8_t r[7];HAL_StatusTypeDef s;if(!h||!dt)return HAL_ERROR;s=rd(h,REG_SECONDS,r,7,t);if(s!=HAL_OK)return s;dt->seconds=b2i(r[0]&0x7f);dt->minutes=b2i(r[1]&0x7f);dt->hours=b2i(r[2]&0x3f);dt->day=b2i(r[3]&0x3f);dt->weekday=b2i(r[4]&7);dt->century=(uint8_t)(r[5]>>7);dt->month=b2i(r[5]&0x1f);dt->year=b2i(r[6]);return HAL_OK;}
HAL_StatusTypeDef PCF8563_WriteDateTime(SoftI2C_HandleTypeDef*h,const PCF8563_DateTime*dt,uint32_t t){uint8_t r[7];if(!h||!dt||dt->seconds>59||dt->minutes>59||dt->hours>23||dt->day<1||dt->day>31||dt->weekday>6||dt->month<1||dt->month>12)return HAL_ERROR;r[0]=i2b(dt->seconds);r[1]=i2b(dt->minutes);r[2]=i2b(dt->hours);r[3]=i2b(dt->day);r[4]=i2b(dt->weekday);r[5]=(uint8_t)(i2b(dt->month)|((dt->century&1U)<<7));r[6]=i2b(dt->year);return wr(h,REG_SECONDS,r,7,t);}
HAL_StatusTypeDef PCF8563_SetRunning(SoftI2C_HandleTypeDef*h,uint8_t run,uint32_t t){uint8_t c;HAL_StatusTypeDef s=rd(h,REG_CONTROL1,&c,1,t);if(s!=HAL_OK)return s;if(run)c&=(uint8_t)~0x20U;else c|=0x20U;return wr(h,REG_CONTROL1,&c,1,t);}
HAL_StatusTypeDef PCF8563_GetVoltageLow(SoftI2C_HandleTypeDef*h,uint8_t*low,uint32_t t){uint8_t s;HAL_StatusTypeDef x=rd(h,REG_SECONDS,&s,1,t);if(x==HAL_OK&&low)*low=(uint8_t)(s>>7);return x;}
