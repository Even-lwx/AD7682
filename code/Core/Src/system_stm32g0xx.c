/**
  ******************************************************************************
  * @file    system_stm32g0xx.c
  * @author  MCD 应用团队
  * @brief   CMSIS Cortex-M0+ 器件外设访问层系统源文件。
  *
  *   本文件为用户应用提供两个函数和一个全局变量：
  *      - SystemInit()：复位后、跳转到主程序前由 startup_stm32g0xx.s 调用。
  *
  *      - SystemCoreClock：保存内核时钟（HCLK），应用可用它配置 SysTick
  *                         定时器或其他参数。
  *
  *      - SystemCoreClockUpdate()：更新 SystemCoreClock；程序运行期间修改内核时钟后必须调用。
  *
  *   每次器件复位后先使用 HSI（8 MHz 后为 16 MHz）作为系统时钟源。
  *   随后由 startup_stm32g0xx.s 调用 SystemInit()，配置系统时钟后再跳转到主程序。
  *
  *   本文件将系统时钟配置如下：
  *=============================================================================
  *-----------------------------------------------------------------------------
  *        系统时钟源                              | HSI
  *-----------------------------------------------------------------------------
  *        SYSCLK(Hz)                             | 16000000
  *-----------------------------------------------------------------------------
  *        HCLK(Hz)                               | 16000000
  *-----------------------------------------------------------------------------
  *        AHB Prescaler                          | 1
  *-----------------------------------------------------------------------------
  *        APB Prescaler                          | 1
  *-----------------------------------------------------------------------------
  *        HSI Division factor                    | 1
  *-----------------------------------------------------------------------------
  *        PLL_M                                  | 1
  *-----------------------------------------------------------------------------
  *        PLL_N                                  | 8
  *-----------------------------------------------------------------------------
  *        PLL_P                                  | 7
  *-----------------------------------------------------------------------------
  *        PLL_Q                                  | 2
  *-----------------------------------------------------------------------------
  *        PLL_R                                  | 2
  *-----------------------------------------------------------------------------
  *        RNG 所需 48MHz                         | 未启用
  *-----------------------------------------------------------------------------
  *=============================================================================
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2018-2021 STMicroelectronics.
  * 版权所有，保留所有权利。
  *
  * 本软件遵循工程根目录 LICENSE 文件中的条款授权。
  * 如果工程未附带 LICENSE 文件，则按“现状”提供。
  *
  ******************************************************************************
  */
/** @addtogroup CMSIS
  * @{
  */

/** @addtogroup stm32g0xx_system
  * @{
  */

/** @addtogroup STM32G0xx_System_Private_Includes
  * @{
  */

#include "stm32g0xx.h"

#if !defined  (HSE_VALUE)
#define HSE_VALUE    (8000000UL)    /*!< 外部振荡器频率，单位 Hz。 */
#endif /* HSE_VALUE */

#if !defined  (HSI_VALUE)
  #define HSI_VALUE  (16000000UL)   /*!< 内部振荡器频率，单位 Hz。 */
#endif /* HSI_VALUE */

#if !defined  (LSI_VALUE)
 #define LSI_VALUE   (32000UL)     /*!< LSI 频率，单位 Hz。 */
#endif /* LSI_VALUE */

#if !defined  (LSE_VALUE)
  #define LSE_VALUE  (32768UL)      /*!< LSE 频率，单位 Hz。 */
#endif /* LSE_VALUE */

/**
  * @}
  */

/** @addtogroup STM32G0xx_System_Private_TypesDefinitions
  * @{
  */

/**
  * @}
  */

/** @addtogroup STM32G0xx_System_Private_Defines
  * @{
  */

/************************* 其他配置 *****************************************/
/* 注意：下面的向量表地址必须与链接器配置保持一致。 */
/*!< 如需将向量表重定位到 Flash 或 SRAM 的其他位置，请取消下面一行注释；
     否则向量表保留在启动地址自动重映射的位置。 */
/* #define USER_VECT_TAB_ADDRESS */

#if defined(USER_VECT_TAB_ADDRESS)
/*!< 如需将向量表重定位到 SRAM，请取消下面一行注释；否则重映射在 Flash 中进行。 */
/* #define VECT_TAB_SRAM */
#if defined(VECT_TAB_SRAM)
#define VECT_TAB_BASE_ADDRESS   SRAM_BASE       /*!< 向量表基地址；必须是 0x200 的整数倍。 */
#else
#define VECT_TAB_BASE_ADDRESS   FLASH_BASE      /*!< 向量表基地址；必须是 0x200 的整数倍。 */
#endif /* VECT_TAB_SRAM */

#if !defined(VECT_TAB_OFFSET)
#define VECT_TAB_OFFSET         0x00000000U     /*!< 向量表偏移；必须是 0x200 的整数倍。 */
#endif /* VECT_TAB_OFFSET */

#endif /* USER_VECT_TAB_ADDRESS */
/******************************************************************************/
/**
  * @}
  */

/** @addtogroup STM32G0xx_System_Private_Macros
  * @{
  */

/**
  * @}
  */

/** @addtogroup STM32G0xx_System_Private_Variables
  * @{
  */
  /* SystemCoreClock 变量有三种更新方式：
      1) 调用 CMSIS 函数 SystemCoreClockUpdate()；
      2) 调用 HAL API 函数 HAL_RCC_GetHCLKFreq()；
      3) 每次调用 HAL_RCC_ClockConfig() 配置系统时钟时更新。
         注意：如果使用该函数配置系统时钟，则无需再调用前两项函数，
         因为 SystemCoreClock 会自动更新。
  */
  uint32_t SystemCoreClock = 16000000UL;

  const uint32_t AHBPrescTable[16UL] = {0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 0UL, 1UL, 2UL, 3UL, 4UL, 6UL, 7UL, 8UL, 9UL};
  const uint32_t APBPrescTable[8UL] =  {0UL, 0UL, 0UL, 0UL, 1UL, 2UL, 3UL, 4UL};

/**
  * @}
  */

/** @addtogroup STM32G0xx_System_Private_FunctionPrototypes
  * @{
  */

/**
  * @}
  */

/** @addtogroup STM32G0xx_System_Private_Functions
  * @{
  */

/**
  * @brief  配置微控制器系统。
  * @param  无
  * @retval 无
  */
/**
 * @brief 在 C 运行环境建立前初始化芯片系统寄存器，供启动汇编调用。
 * @return 无返回值。
 */
void SystemInit(void)
{
  /* 配置向量表位置。 */
#if defined(USER_VECT_TAB_ADDRESS)
  SCB->VTOR = VECT_TAB_BASE_ADDRESS | VECT_TAB_OFFSET; /* 重定位向量表。 */
#endif /* USER_VECT_TAB_ADDRESS */
}

/**
  * @brief  根据时钟寄存器值更新 SystemCoreClock 变量。
  *         该变量保存内核时钟（HCLK），应用可用它配置 SysTick 定时器或其他参数。
  *
  * @note   每次修改内核时钟（HCLK）后都必须调用本函数，否则基于该变量的配置可能错误。
  *
  * @note   本函数计算的是基于预定义常量和时钟源的理论频率，不一定等于芯片实际频率。
  *
  *           使用 HSI 时，SystemCoreClock 为 HSI_VALUE 除以 HSI 分频系数。
  *
  *           使用 HSE 时，SystemCoreClock 为 HSE_VALUE。
  *
  *           使用 LSI 时，SystemCoreClock 为 LSI_VALUE。
  *
  *           使用 LSE 时，SystemCoreClock 为 LSE_VALUE。
  *
  *           使用 PLL 时，根据 PLL 参数对 HSE_VALUE 或 HSI_VALUE 进行倍频/分频。
  *
  *         HSI_VALUE 定义在 stm32g0xx_hal_conf.h 中（默认 16 MHz），实际值可能随电压和温度变化。
  *
  *         HSE_VALUE 定义在 stm32g0xx_hal_conf.h 中（默认 8 MHz），用户必须确保它与实际晶体频率一致。
  *
  *         使用分数频率 HSE 晶体时，本函数结果可能不准确。
  *
  * @param  无
  * @retval 无
  */
/**
 * @brief 根据 RCC 当前配置重新计算 SystemCoreClock，不修改实际时钟配置。
 * @return 无返回值。
 */
void SystemCoreClockUpdate(void)
{
  uint32_t tmp;
  uint32_t pllvco;
  uint32_t pllr;
  uint32_t pllsource;
  uint32_t pllm;
  uint32_t hsidiv;

  /* 获取 SYSCLK 时钟源。 */
  switch (RCC->CFGR & RCC_CFGR_SWS)
  {
    case RCC_CFGR_SWS_0:                /* 使用 HSE 作为系统时钟。 */
      SystemCoreClock = HSE_VALUE;
      break;

    case (RCC_CFGR_SWS_1 | RCC_CFGR_SWS_0):  /* 使用 LSI 作为系统时钟。 */
      SystemCoreClock = LSI_VALUE;
      break;

    case RCC_CFGR_SWS_2:                /* 使用 LSE 作为系统时钟。 */
      SystemCoreClock = LSE_VALUE;
      break;

    case RCC_CFGR_SWS_1:  /* 使用 PLL 作为系统时钟。 */
      /* PLL_VCO =（HSE_VALUE 或 HSI_VALUE / PLLM）* PLLN，SYSCLK = PLL_VCO / PLLR。 */
      pllsource = (RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC);
      pllm = ((RCC->PLLCFGR & RCC_PLLCFGR_PLLM) >> RCC_PLLCFGR_PLLM_Pos) + 1UL;

      if(pllsource == 0x03UL)           /* 使用 HSE 作为 PLL 时钟源。 */
      {
        pllvco = (HSE_VALUE / pllm);
      }
      else                              /* 使用 HSI 作为 PLL 时钟源。 */
      {
          pllvco = (HSI_VALUE / pllm);
      }
      pllvco = pllvco * ((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> RCC_PLLCFGR_PLLN_Pos);
      pllr = (((RCC->PLLCFGR & RCC_PLLCFGR_PLLR) >> RCC_PLLCFGR_PLLR_Pos) + 1UL);

      SystemCoreClock = pllvco/pllr;
      break;
      
    case 0x00000000U:                   /* 使用 HSI 作为系统时钟。 */
    default:                            /* 使用 HSI 作为系统时钟。 */
      hsidiv = (1UL << ((READ_BIT(RCC->CR, RCC_CR_HSIDIV))>> RCC_CR_HSIDIV_Pos));
      SystemCoreClock = (HSI_VALUE/hsidiv);
      break;
  }
  /* 计算 HCLK 时钟频率。 */
  /* 获取 HCLK 分频系数。 */
  tmp = AHBPrescTable[((RCC->CFGR & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos)];
  /* HCLK 时钟频率。 */
  SystemCoreClock >>= tmp;
}


/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */
