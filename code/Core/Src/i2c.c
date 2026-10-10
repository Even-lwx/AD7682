#include "i2c.h"
/* Board wiring: PB11=SCL, PB12=SDA. Both lines are open-drain. */
SoftI2C_HandleTypeDef hi2c1={GPIOB,GPIO_PIN_11,GPIOB,GPIO_PIN_12};
static void d(void){for(volatile uint32_t i=0;i<32;i++)__NOP();}
static void w(SoftI2C_HandleTypeDef*h,GPIO_TypeDef*p,uint16_t n,GPIO_PinState s){HAL_GPIO_WritePin(p,n,s);d();}
static GPIO_PinState r(SoftI2C_HandleTypeDef*h){return HAL_GPIO_ReadPin(h->sda_port,h->sda_pin);}
static void st(SoftI2C_HandleTypeDef*h){w(h,h->sda_port,h->sda_pin,1);w(h,h->scl_port,h->scl_pin,1);w(h,h->sda_port,h->sda_pin,0);w(h,h->scl_port,h->scl_pin,0);}
static void sp(SoftI2C_HandleTypeDef*h){w(h,h->sda_port,h->sda_pin,0);w(h,h->scl_port,h->scl_pin,1);w(h,h->sda_port,h->sda_pin,1);}
static uint8_t wb(SoftI2C_HandleTypeDef*h,uint8_t b){uint8_t i,a;for(i=0;i<8;i++){w(h,h->sda_port,h->sda_pin,(b&0x80)?1:0);w(h,h->scl_port,h->scl_pin,1);w(h,h->scl_port,h->scl_pin,0);b<<=1;}w(h,h->sda_port,h->sda_pin,1);w(h,h->scl_port,h->scl_pin,1);a=(r(h)==0);w(h,h->scl_port,h->scl_pin,0);return a;}
static uint8_t rb(SoftI2C_HandleTypeDef*h,uint8_t a){uint8_t i,b=0;w(h,h->sda_port,h->sda_pin,1);for(i=0;i<8;i++){b<<=1;w(h,h->scl_port,h->scl_pin,1);if(r(h))b|=1;w(h,h->scl_port,h->scl_pin,0);}w(h,h->sda_port,h->sda_pin,a?0:1);w(h,h->scl_port,h->scl_pin,1);w(h,h->scl_port,h->scl_pin,0);w(h,h->sda_port,h->sda_pin,1);return b;}
void MX_I2C1_Init(void){GPIO_InitTypeDef g={0};__HAL_RCC_GPIOB_CLK_ENABLE();g.Pin=GPIO_PIN_11|GPIO_PIN_12;g.Mode=GPIO_MODE_OUTPUT_OD;g.Pull=GPIO_NOPULL;g.Speed=GPIO_SPEED_FREQ_HIGH;HAL_GPIO_Init(GPIOB,&g);HAL_GPIO_WritePin(GPIOB,GPIO_PIN_11|GPIO_PIN_12,GPIO_PIN_SET);}
HAL_StatusTypeDef SoftI2C_MasterTransmit(SoftI2C_HandleTypeDef*h,uint16_t a,uint8_t*p,uint16_t n,uint32_t t){uint16_t i;(void)t;st(h);if(!wb(h,a&0xfe)){sp(h);return HAL_ERROR;}for(i=0;i<n;i++)if(!wb(h,p[i])){sp(h);return HAL_ERROR;}sp(h);return HAL_OK;}
HAL_StatusTypeDef SoftI2C_MasterReceive(SoftI2C_HandleTypeDef*h,uint16_t a,uint8_t*p,uint16_t n,uint32_t t){uint16_t i;(void)t;st(h);if(!wb(h,a|1)){sp(h);return HAL_ERROR;}for(i=0;i<n;i++)p[i]=rb(h,i+1<n);sp(h);return HAL_OK;}
HAL_StatusTypeDef SoftI2C_MemRead(SoftI2C_HandleTypeDef*h,uint16_t a,uint8_t q,uint8_t*p,uint16_t n,uint32_t t){(void)t;st(h);if(!wb(h,a&0xfe)||!wb(h,q)){sp(h);return HAL_ERROR;}st(h);if(!wb(h,a|1)){sp(h);return HAL_ERROR;}while(n--)*p++=rb(h,n!=0);sp(h);return HAL_OK;}
HAL_StatusTypeDef SoftI2C_MemWrite(SoftI2C_HandleTypeDef*h,uint16_t a,uint8_t q,uint8_t*p,uint16_t n,uint32_t t){uint16_t i;(void)t;st(h);if(!wb(h,a&0xfe)||!wb(h,q)){sp(h);return HAL_ERROR;}for(i=0;i<n;i++)if(!wb(h,p[i])){sp(h);return HAL_ERROR;}sp(h);return HAL_OK;}
