/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file         stm32g0xx_hal_msp.c
  * @brief        本文件提供 MSP 初始化和反初始化代码。
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

/* 头文件包含。 */
#include "main.h"
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* 私有类型定义。 */
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* 私有宏定义。 */
/* USER CODE BEGIN Define */

/* USER CODE END Define */

/* 私有宏。 */
/* USER CODE BEGIN Macro */

/* USER CODE END Macro */

/* 私有变量。 */
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* 私有函数声明。 */
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* 外部函数。 */
/* USER CODE BEGIN ExternalFunctions */

/* USER CODE END ExternalFunctions */

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */
/**
  * 初始化全局 MSP。
  */
/**
 * @brief 配置 HAL 公共底层资源及系统中断相关设置。
 * @return 无返回值。
 */
void HAL_MspInit(void)
{
  /* USER CODE BEGIN MspInit 0 */

  /* USER CODE END MspInit 0 */

  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();

  /* 系统中断初始化。 */

  /* USER CODE BEGIN MspInit 1 */

  /* USER CODE END MspInit 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
