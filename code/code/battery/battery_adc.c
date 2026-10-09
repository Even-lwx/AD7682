#include "battery_adc.h"
uint32_t BatteryAdc_ReadRaw(ADC_HandleTypeDef *h,uint32_t t){uint32_t v=0;if(HAL_ADC_Start(h)!=HAL_OK)return 0;if(HAL_ADC_PollForConversion(h,t)!=HAL_OK)return 0;v=HAL_ADC_GetValue(h);HAL_ADC_Stop(h);return v;}
float BatteryAdc_ReadVoltage(ADC_HandleTypeDef*h,float vr,float ratio,uint32_t t){return (BatteryAdc_ReadRaw(h,t)*vr/4095.0f)*ratio;}
