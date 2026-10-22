/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.h
  * @brief   本文件包含 gpio.c 中的所有函数声明
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
#ifndef __GPIO_H__
#define __GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

/* 头文件包含。 */
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_GPIO_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif
#endif /*__ GPIO_H__ */

