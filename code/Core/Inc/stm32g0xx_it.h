/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32g0xx_it.h
  * @brief   本文件包含中断处理函数的声明。
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * 版权所有，保留所有权利。
  *
  * 本软件遵循工程根目录 LICENSE 文件中的条款授权。
  * 如果工程未附带 LICENSE 文件，则按“现状”提供。
  *
 ******************************************************************************
  */
/* USER CODE END Header */

/* 防止头文件被重复包含。 */
#ifndef __STM32G0xx_IT_H
#define __STM32G0xx_IT_H

#ifdef __cplusplus
 extern "C" {
#endif

/* 私有头文件包含。 */
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* 对外提供的类型。 */
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* 对外提供的常量。 */
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* 对外提供的宏。 */
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* 对外提供的函数声明。 */
void NMI_Handler(void);
void HardFault_Handler(void);
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);
/* USER CODE BEGIN EFP */
void TIM14_IRQHandler(void);
void TIM3_IRQHandler(void);

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __STM32G0xx_IT_H */
