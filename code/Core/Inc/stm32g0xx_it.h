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
/**
 * @brief 处理不可屏蔽异常，当前实现停留在异常循环等待调试。
 * @return 无返回值。
 */
void NMI_Handler(void);
/**
 * @brief 处理处理器硬件错误，停留在异常循环保留故障现场。
 * @return 无返回值。
 */
void HardFault_Handler(void);
/**
 * @brief 系统服务调用异常入口，当前工程未添加业务处理。
 * @return 无返回值。
 */
void SVC_Handler(void);
/**
 * @brief 可挂起系统服务异常入口，当前工程未添加业务处理。
 * @return 无返回值。
 */
void PendSV_Handler(void);
/**
 * @brief 维护 HAL 毫秒计数，为超时和主循环调度提供时间基准。
 * @return 无返回值。
 */
void SysTick_Handler(void);
/* USER CODE BEGIN EFP */
/**
 * @brief 维护毫秒节拍、扫描按键并设置慢速采样请求；不在此读传感器或写 Flash。
 * @return 无返回值。
 */
void TIM14_IRQHandler(void);
/**
 * @brief 清除 TIM3 更新标志，在正常采样启用时执行 100 Hz ADC 采样。
 * @return 无返回值。
 */
void TIM3_IRQHandler(void);
/**
 * @brief 转交 HAL 处理 USART1 RX DMA 通道中断。
 * @return 无返回值。
 */
void DMA1_Channel1_IRQHandler(void);

/* USER CODE END EFP */

#ifdef __cplusplus
}
#endif

#endif /* __STM32G0xx_IT_H */
