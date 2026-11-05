/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32g0xx_it.c
  * @brief   中断服务函数。
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
#include "stm32g0xx_it.h"
/* 私有头文件包含。 */
/* USER CODE BEGIN Includes */
#include "usart.h"
/* USER CODE END Includes */

/* 私有类型定义。 */
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* 私有宏定义。 */
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* 私有宏。 */
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* 私有变量。 */
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* 私有函数声明。 */
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* 私有用户代码。 */
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* 外部变量。 */

/* USER CODE BEGIN EV */
extern UART_HandleTypeDef huart1;

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M0+ 处理器中断和异常处理函数                         */
/******************************************************************************/
/**
  * @brief 处理不可屏蔽中断。
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
  while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief 处理硬件错误中断。
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief 处理通过 SWI 指令触发的系统服务调用。
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVC_IRQn 0 */

  /* USER CODE END SVC_IRQn 0 */
  /* USER CODE BEGIN SVC_IRQn 1 */

  /* USER CODE END SVC_IRQn 1 */
}

/**
  * @brief 处理可挂起的系统服务请求。
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief 处理系统滴答定时器中断。
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32G0xx 外设中断处理函数。                                              */
/* 在此添加工程使用外设的中断处理函数。                                      */
/* 可用的中断函数名称请参考 startup_stm32g0xx.s。                             */
/******************************************************************************/

/* USER CODE BEGIN USART1_IRQn 0 */
void USART1_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart1);
}

extern DMA_HandleTypeDef hdma_usart1_rx;
void DMA1_Channel1_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_usart1_rx);
}
/* USER CODE END USART1_IRQn 0 */

/* USER CODE BEGIN 1 */
extern volatile uint32_t g_record_timer_seconds;
extern volatile uint32_t g_record_timer_milliseconds;
extern volatile uint16_t g_sensor_timer_milliseconds;
extern volatile uint16_t g_sensor_sample_elapsed_ms;
extern volatile uint8_t g_sensor_sample_request;
extern volatile uint8_t g_adc_normal_enabled;
void Application_AdcSample100Hz(void);

void TIM3_IRQHandler(void)
{
  if ((TIM3->SR & TIM_SR_UIF) != 0U)
  {
    TIM3->SR &= (uint32_t)~TIM_SR_UIF;
    if (g_adc_normal_enabled != 0U)
    {
      Application_AdcSample100Hz();
    }
  }
}

void TIM14_IRQHandler(void)
{
  if ((TIM14->SR & TIM_SR_UIF) != 0U)
  {
    TIM14->SR &= (uint32_t)~TIM_SR_UIF;
    /* TIM14 每 1 ms 进入一次；中断只更新时间基准和置位请求，
       禁止在这里执行 I2C/SPI、打印或 HAL_Delay。 */
    Application_KeyScan1ms();
    g_sensor_timer_milliseconds++;
    g_record_timer_milliseconds++;
    if (g_sensor_timer_milliseconds >= 1000U)
    {
      g_sensor_timer_milliseconds = 0U;
      g_record_timer_seconds++;
    }
    g_sensor_sample_elapsed_ms++;
    if (g_sensor_sample_elapsed_ms >= SENSOR_SAMPLE_PERIOD_MS)
    {
      g_sensor_sample_elapsed_ms = 0U;
      g_sensor_sample_request = 1U;
    }
  }
}

/* USER CODE END 1 */
