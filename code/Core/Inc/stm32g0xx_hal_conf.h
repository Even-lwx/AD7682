/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32g0xx_hal_conf.h
  * @author  MCD 应用团队
  * @brief   HAL 配置文件。
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2018 STMicroelectronics.
  * 版权所有，保留所有权利。
  *
  * 本软件遵循工程根目录 LICENSE 文件中的条款授权。
  * 如果工程未附带 LICENSE 文件，则按“现状”提供。
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef STM32G0xx_HAL_CONF_H
#define STM32G0xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* 对外提供的类型。 */
/* 对外提供的常量。 */

/* ########################## 模块选择 ##################################### */
/**
  * @brief HAL 驱动中启用的模块列表。
  */
#define HAL_MODULE_ENABLED
#define HAL_ADC_MODULE_ENABLED
/* #define HAL_CEC_MODULE_ENABLED   */
/* #define HAL_COMP_MODULE_ENABLED   */
/* #define HAL_CRC_MODULE_ENABLED   */
/* #define HAL_CRYP_MODULE_ENABLED   */
/* #define HAL_DAC_MODULE_ENABLED   */
/* #define HAL_EXTI_MODULE_ENABLED   */
/* #define HAL_FDCAN_MODULE_ENABLED   */
/* #define HAL_HCD_MODULE_ENABLED   */
#define HAL_I2C_MODULE_ENABLED
/* #define HAL_I2S_MODULE_ENABLED   */
/* #define HAL_IWDG_MODULE_ENABLED   */
/* #define HAL_IRDA_MODULE_ENABLED   */
/* #define HAL_LPTIM_MODULE_ENABLED   */
/* #define HAL_PCD_MODULE_ENABLED   */
/* #define HAL_RNG_MODULE_ENABLED   */
/* #define HAL_RTC_MODULE_ENABLED   */
/* #define HAL_SMARTCARD_MODULE_ENABLED   */
/* #define HAL_SMBUS_MODULE_ENABLED   */
#define HAL_SPI_MODULE_ENABLED
/* #define HAL_TIM_MODULE_ENABLED   */
#define HAL_UART_MODULE_ENABLED
/* #define HAL_USART_MODULE_ENABLED   */
/* #define HAL_WWDG_MODULE_ENABLED   */
#define HAL_GPIO_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED

/* ########################## 注册回调选择 ################################# */
/**
  * @brief 可使用注册回调的模块列表。
  */
#define USE_HAL_ADC_REGISTER_CALLBACKS    0u
#define USE_HAL_CEC_REGISTER_CALLBACKS    0u
#define USE_HAL_COMP_REGISTER_CALLBACKS   0u
#define USE_HAL_CRYP_REGISTER_CALLBACKS   0u
#define USE_HAL_DAC_REGISTER_CALLBACKS    0u
#define USE_HAL_FDCAN_REGISTER_CALLBACKS  0u
#define USE_HAL_HCD_REGISTER_CALLBACKS    0u
#define USE_HAL_I2C_REGISTER_CALLBACKS    0u
#define USE_HAL_I2S_REGISTER_CALLBACKS    0u
#define USE_HAL_IRDA_REGISTER_CALLBACKS   0u
#define USE_HAL_LPTIM_REGISTER_CALLBACKS  0u
#define USE_HAL_PCD_REGISTER_CALLBACKS    0u
#define USE_HAL_RNG_REGISTER_CALLBACKS    0u
#define USE_HAL_RTC_REGISTER_CALLBACKS    0u
#define USE_HAL_SMBUS_REGISTER_CALLBACKS  0u
#define USE_HAL_SPI_REGISTER_CALLBACKS    0u
#define USE_HAL_TIM_REGISTER_CALLBACKS    0u
#define USE_HAL_UART_REGISTER_CALLBACKS   0u
#define USE_HAL_USART_REGISTER_CALLBACKS  0u
#define USE_HAL_WWDG_REGISTER_CALLBACKS   0u

/* ########################## 振荡器参数 ################################### */
/**
  * @brief 设置应用中使用的外部高速振荡器（HSE）频率。
  *        RCC HAL 模块使用该值计算系统频率（HSE 直接或经 PLL 作为系统时钟源时）。
  */
#if !defined  (HSE_VALUE)
#define HSE_VALUE    (8000000UL)         /*!< 外部振荡器频率，单位 Hz。 */
#endif /* HSE_VALUE */

#if !defined  (HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT    (100UL)         /*!< HSE 启动超时时间，单位 ms。 */
#endif /* HSE_STARTUP_TIMEOUT */

/**
  * @brief 内部高速振荡器（HSI）频率。
  *        RCC HAL 模块使用该值计算系统频率（HSI 直接或经 PLL 作为系统时钟源时）。
  */
#if !defined  (HSI_VALUE)
#define HSI_VALUE    (16000000UL)            /*!< 内部振荡器频率，单位 Hz。 */
#endif /* HSI_VALUE */

#if defined(STM32G0C1xx) || defined(STM32G0B1xx) || defined(STM32G0B0xx)
/**
  * @brief USB FS、SDMMC 和 RNG 使用的内部高速振荡器（HSI48）频率。
  *        该振荡器主要通过时钟恢复系统（CRS）为 USB 外设提供高精度时钟。
  *        未使用 CRS 时，HSI48 RC 振荡器以默认频率运行，该频率会受制造工艺影响。
  */
#if !defined  (HSI48_VALUE)
  #define HSI48_VALUE   48000000U             /*!< USB FS/SDMMC/RNG 使用的内部高速振荡器频率，单位 Hz。
                                               实际值可能因制造工艺变化而不同。 */
#endif /* HSI48_VALUE */
#endif

/**
  * @brief 内部低速振荡器（LSI）频率。
  */
#if !defined  (LSI_VALUE)
#define LSI_VALUE  (32000UL)                /*!< LSI 典型频率，单位 Hz。 */
#endif /* LSI_VALUE */                      /*!< 内部低速振荡器频率，单位 Hz。
实际值可能随电压和温度变化而变化。 */
/**
  * @brief 外部低速振荡器（LSE）频率。
  *        UART、RTC HAL 模块使用该值计算系统频率。
  */
#if !defined  (LSE_VALUE)
#define LSE_VALUE    (32768UL)               /*!< 外部振荡器频率，单位 Hz。 */
#endif /* LSE_VALUE */

#if !defined  (LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT    (5000UL)      /*!< LSE 启动超时时间，单位 ms。 */
#endif /* LSE_STARTUP_TIMEOUT */

/**
  * @brief I2S1 外部时钟源频率。
  *        RCC HAL 模块使用该值计算 I2S1 时钟源频率。
  */
#if !defined  (EXTERNAL_I2S1_CLOCK_VALUE)
#define EXTERNAL_I2S1_CLOCK_VALUE    (12288000UL) /*!< I2S1 外部时钟源频率，单位 Hz。 */
#endif /* EXTERNAL_I2S1_CLOCK_VALUE */

#if defined(STM32G0C1xx) || defined(STM32G0B1xx) || defined(STM32G0B0xx)
/**
  * @brief I2S2 外部时钟源频率。
  *        RCC HAL 模块使用该值计算 I2S2 时钟源频率。
  */
#if !defined  (EXTERNAL_I2S2_CLOCK_VALUE)
  #define EXTERNAL_I2S2_CLOCK_VALUE    48000U /*!< I2S2 外部时钟源频率，单位 Hz。 */
#endif /* EXTERNAL_I2S2_CLOCK_VALUE */
#endif

/* 提示：如需使用不同的 HSE 频率，可在编译器预处理器中定义 HSE_VALUE，
   这样无需每次修改本文件。 */

/* ########################### 系统配置 ################################### */
/**
  * @brief HAL 系统配置区。
  */
#define  VDD_VALUE                    (3300UL)                                        /*!< VDD 电压，单位 mV。 */
#define  TICK_INT_PRIORITY            3U /*!< 系统滴答中断优先级。 */
#define  USE_RTOS                     0U
#define  PREFETCH_ENABLE              1U
#define  INSTRUCTION_CACHE_ENABLE     1U

/* ################## SPI 外设配置 ######################################## */

/* CRC 功能：用于启用 HAL SPI 驱动中的 CRC。
* 启用：驱动包含 CRC 代码。
* 禁用：驱动移除 CRC 代码。
*/

#define USE_SPI_CRC                     0U

/* ################## CRYP 外设配置 ####################################### */

#define USE_HAL_CRYP_SUSPEND_RESUME     1U

/* ########################## 断言选择 ##################################### */
/**
  * @brief 取消下面一行的注释，可在 HAL 驱动代码中展开 assert_param 宏。
  */
/* #define USE_FULL_ASSERT    1U */

/* 头文件包含。 */
/**
  * @brief 包含各模块头文件。
  */

#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32g0xx_hal_rcc.h"
#endif /* HAL_RCC_MODULE_ENABLED */

#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32g0xx_hal_gpio.h"
#endif /* HAL_GPIO_MODULE_ENABLED */

#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32g0xx_hal_dma.h"
#endif /* HAL_DMA_MODULE_ENABLED */

#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32g0xx_hal_cortex.h"
#endif /* HAL_CORTEX_MODULE_ENABLED */

#ifdef HAL_ADC_MODULE_ENABLED
#include "stm32g0xx_hal_adc.h"
#include "stm32g0xx_hal_adc_ex.h"
#endif /* HAL_ADC_MODULE_ENABLED */

#ifdef HAL_CEC_MODULE_ENABLED
#include "stm32g0xx_hal_cec.h"
#endif /* HAL_CEC_MODULE_ENABLED */

#ifdef HAL_COMP_MODULE_ENABLED
#include "stm32g0xx_hal_comp.h"
#endif /* HAL_COMP_MODULE_ENABLED */

#ifdef HAL_CRC_MODULE_ENABLED
#include "stm32g0xx_hal_crc.h"
#endif /* HAL_CRC_MODULE_ENABLED */

#ifdef HAL_CRYP_MODULE_ENABLED
#include "stm32g0xx_hal_cryp.h"
#endif /* HAL_CRYP_MODULE_ENABLED */

#ifdef HAL_DAC_MODULE_ENABLED
#include "stm32g0xx_hal_dac.h"
#endif /* HAL_DAC_MODULE_ENABLED */

#ifdef HAL_EXTI_MODULE_ENABLED
#include "stm32g0xx_hal_exti.h"
#endif /* HAL_EXTI_MODULE_ENABLED */

#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32g0xx_hal_flash.h"
#endif /* HAL_FLASH_MODULE_ENABLED */

#ifdef HAL_FDCAN_MODULE_ENABLED
#include "stm32g0xx_hal_fdcan.h"
#endif /* HAL_FDCAN_MODULE_ENABLED */

#ifdef HAL_HCD_MODULE_ENABLED
#include "stm32g0xx_hal_hcd.h"
#endif /* HAL_HCD_MODULE_ENABLED */

#ifdef HAL_I2C_MODULE_ENABLED
#include "stm32g0xx_hal_i2c.h"
#endif /* HAL_I2C_MODULE_ENABLED */

#ifdef HAL_I2S_MODULE_ENABLED
#include "stm32g0xx_hal_i2s.h"
#endif /* HAL_I2S_MODULE_ENABLED */

#ifdef HAL_IRDA_MODULE_ENABLED
#include "stm32g0xx_hal_irda.h"
#endif /* HAL_IRDA_MODULE_ENABLED */

#ifdef HAL_IWDG_MODULE_ENABLED
#include "stm32g0xx_hal_iwdg.h"
#endif /* HAL_IWDG_MODULE_ENABLED */

#ifdef HAL_LPTIM_MODULE_ENABLED
#include "stm32g0xx_hal_lptim.h"
#endif /* HAL_LPTIM_MODULE_ENABLED */

#ifdef HAL_PCD_MODULE_ENABLED
#include "stm32g0xx_hal_pcd.h"
#endif /* HAL_PCD_MODULE_ENABLED */

#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32g0xx_hal_pwr.h"
#endif /* HAL_PWR_MODULE_ENABLED */

#ifdef HAL_RNG_MODULE_ENABLED
#include "stm32g0xx_hal_rng.h"
#endif /* HAL_RNG_MODULE_ENABLED */

#ifdef HAL_RTC_MODULE_ENABLED
#include "stm32g0xx_hal_rtc.h"
#endif /* HAL_RTC_MODULE_ENABLED */

#ifdef HAL_SMARTCARD_MODULE_ENABLED
#include "stm32g0xx_hal_smartcard.h"
#endif /* HAL_SMARTCARD_MODULE_ENABLED */

#ifdef HAL_SMBUS_MODULE_ENABLED
#include "stm32g0xx_hal_smbus.h"
#endif /* HAL_SMBUS_MODULE_ENABLED */

#ifdef HAL_SPI_MODULE_ENABLED
#include "stm32g0xx_hal_spi.h"
#endif /* HAL_SPI_MODULE_ENABLED */

#ifdef HAL_TIM_MODULE_ENABLED
#include "stm32g0xx_hal_tim.h"
#endif /* HAL_TIM_MODULE_ENABLED */

#ifdef HAL_UART_MODULE_ENABLED
#include "stm32g0xx_hal_uart.h"
#endif /* HAL_UART_MODULE_ENABLED */

#ifdef HAL_USART_MODULE_ENABLED
#include "stm32g0xx_hal_usart.h"
#endif /* HAL_USART_MODULE_ENABLED */

#ifdef HAL_WWDG_MODULE_ENABLED
#include "stm32g0xx_hal_wwdg.h"
#endif /* HAL_WWDG_MODULE_ENABLED */

/* 对外提供的宏。 */
#ifdef  USE_FULL_ASSERT
/**
  * @brief  assert_param 宏用于检查函数参数。
  * @param  expr 为假时调用 assert_failed，报告出错的源文件名和行号；为真时不执行任何操作。
  * @retval 无
  */
#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
/* 对外提供的函数。 */
/**
 * @brief 接收 HAL 参数断言失败的位置；当前实现预留给调试，不自动恢复。
 * @param file 触发断言的源文件名。
 * @param line 触发断言的源文件行号。
 * @return 无返回值。
 */
void assert_failed(uint8_t *file, uint32_t line);
#else
#define assert_param(expr) ((void)0U)
#endif /* USE_FULL_ASSERT */

#ifdef __cplusplus
}
#endif

#endif /* STM32G0xx_HAL_CONF_H */
