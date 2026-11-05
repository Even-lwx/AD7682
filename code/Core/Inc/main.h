/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : main.c 的头文件，包含应用程序的公共定义。
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* 头文件包含。 */
#include "stm32g0xx_hal.h"

/* 其他传感器和遥测的请求周期；ADC 使用独立的 100 Hz TIM3 中断。 */
#define SENSOR_SAMPLE_PERIOD_MS 20U

/* 1 ms 定时按键扫描事件，中断只负责消抖并锁存这些
 * 事件位；菜单操作仍在主循环中执行。 */
#define UI_KEY_UP_SHORT       0x01U
#define UI_KEY_UP_LONG        0x02U
#define UI_KEY_DOWN_SHORT     0x04U
#define UI_KEY_DOWN_LONG      0x08U

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
void Error_Handler(void);
void Application_KeyScan1ms(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* 私有定义。 */

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
