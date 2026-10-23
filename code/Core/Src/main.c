/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : 主程序主体。
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
#include "adc.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* 私有头文件包含。 */
/* USER CODE BEGIN Includes */
#include "i2c.h"
#include "ad7682.h"
#include "battery_adc.h"
#include "sht41.h"
#include "pcf8563.h"
#include "w25q128.h"
#include "lps22hh.h"
#include "data_logger.h"
#include <stdio.h>
#include <string.h>

#define BOOT_INIT_RETRIES 3U       /* 外部器件上电探测次数。 */
#define BOOT_INIT_TIMEOUT 100U     /* 单次器件访问超时，单位 ms。 */
#define SENSOR_INVALID_FLOAT (-9999.0f)
#define SENSOR_INVALID_ADC   0xFFFFU
#define PRESSURE_AVG_WINDOW  50U
#define SHT_AVG_WINDOW       50U
#define ADC_DIAG_ROWS        96U
#define ADC_DIAG_FIRST_PHASE 29U
#define ADC_DIAG_PHASES      40U /* Exclusive end; this run uses phases 29..39. */
#define ADC_DIAG_BLOCKS      3U
#define ADC_DIAG_PERIOD_US   20000U
#define ADC_FILTER_WINDOW    100U

/* USER CODE END Includes */

/* 私有类型定义。 */
/* USER CODE BEGIN PTD */
typedef struct
{
  float values[PRESSURE_AVG_WINDOW];
  float sum;
  uint16_t count;
  uint16_t next;
} PressureAverage;

typedef struct
{
  uint32_t first_cnv_us;
  uint16_t offsets_us[4];
  uint16_t samples[5];
  uint16_t status;
} AdcDiagRow;

typedef struct
{
  uint16_t values[ADC_FILTER_WINDOW];
  uint16_t sorted[ADC_FILTER_WINDOW];
  uint32_t mean_values[25]; /* Twice the median, preserving half-code values. */
  uint32_t mean_sum;
  uint8_t count;
  uint8_t next;
  uint8_t mean_count;
  uint8_t mean_next;
} AdcTemporalFilter;

/* USER CODE END PTD */

/* 私有宏定义。 */
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* 私有宏。 */
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* 私有变量。 */

/* USER CODE BEGIN PV */
volatile uint8_t g_uart_rx_byte;
volatile uint8_t g_uart_line_ready;
volatile uint8_t g_uart_line_overflow;
volatile uint8_t g_uart_rx_restart;
volatile uint8_t g_rtc_setting_mode;
static char g_uart_line[64];
static uint8_t g_uart_line_length;
volatile uint32_t g_record_timer_seconds;
volatile uint16_t g_sensor_timer_milliseconds;
volatile uint16_t g_sensor_sample_elapsed_ms;
volatile uint8_t g_sensor_sample_request;
/* ADC 正常采样由 TIM3 每 10 ms 触发；诊断期间保持关闭。 */
volatile uint8_t g_adc_normal_enabled;
volatile float g_adc_filtered_codes[4];
volatile uint16_t g_adc_raw_codes[4];
volatile AD7682_Status g_adc_normal_status;
static uint32_t g_record_last_second;
static volatile uint8_t g_recording_enabled;
static uint32_t g_dump_remaining;
static uint32_t g_dump_sequence;
static uint8_t g_dump_newest_first;
static float g_sht_filtered_temperature;
static float g_sht_filtered_humidity;
static uint8_t g_sht_filter_valid;
static uint32_t g_sht_last_valid_tick;
static AD7682_Handle g_ad7682;
static AD7682_Status g_adc_status = AD7682_ERROR;
static uint8_t g_w25q_ready;
static PressureAverage g_u7_pressure_average;
static PressureAverage g_u11_pressure_average;
/* Diagnostic capture and normal filtering never run simultaneously. Reuse
 * their RAM so four 100-point filters fit in the STM32G031's 8 KiB SRAM. */
static union
{
  AdcDiagRow diagnostic[ADC_DIAG_ROWS];
  AdcTemporalFilter filters[4];
} g_adc_workspace;
#define g_adc_diag_rows (g_adc_workspace.diagnostic)
#define g_adc_temporal_filters (g_adc_workspace.filters)
static uint8_t g_adc_diag_active;
static uint8_t g_adc_diag_auto_channels;
static uint8_t g_adc_diag_channel;
static uint8_t g_adc_diag_phase;
static uint8_t g_adc_diag_block;
static uint8_t g_adc_diag_edge_status;
static uint16_t g_adc_diag_count;
static uint8_t g_adc_diag_phase_ready;
static uint32_t g_adc_diag_phase_start_us;
static uint32_t g_adc_diag_next_us;
static uint32_t g_adc_diag_systick_ctrl;
static uint32_t g_adc_diag_timer_dier;
static uint32_t g_adc_diag_record_seconds;
static uint16_t g_adc_diag_record_ms;
static volatile uint32_t g_adc_diag_cpu_probe = 1U;
static uint8_t g_adc_diag_clock_status;
static uint8_t g_adc_diag_lps_status;
static uint8_t g_adc_diag_lps_saved;
static uint8_t g_adc_diag_lps_ctrl[2];
static uint8_t g_adc_diag_lps_readback[2];
static DataLogger_Sample g_pending_record;
static volatile uint8_t g_record_due;

/* USER CODE END PV */

/* 私有函数声明。 */
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void Serial_Write(const char *text);
static void Devices_Initialize(void);
static void Samples_ProcessAndOutput(void);
static void Serial_DumpLogs(void);
static void RecordTimer_Init(void);
static uint8_t SensorSample_ConsumeRequest(void);
static void AdcDiag_ProcessRequest(void);
static void AdcDiag_DumpPhase(uint8_t phase);
static void AdcDiag_StartPhase(void);
static void AdcDiag_StopPhase(void);
static uint8_t AdcDiag_SetCoreClock(uint8_t mode);
static uint8_t AdcDiag_SetLpsMode(uint8_t power_down_mask);
static uint8_t AdcDiag_SetEdgeSpeed(uint8_t high_mask);
static uint8_t Serial_ParseUint32(const char *text, uint32_t *value);
static void Serial_ProcessCommand(void);
static void Serial_RecoverReceive(void);
static void BatteryLed_Update(float voltage);
static void ShtFilter_Update(HAL_StatusTypeDef status, float temperature, float humidity);
static float PressureAverage_Update(PressureAverage *filter, float pressure_hpa);
static uint8_t Pressure_IsValid(float pressure_hpa);
static void RecordCaptureDue(void);
static void RecordWritePending(void);
void Application_AdcSample100Hz(void);
static float AdcFilter_Insert(AdcTemporalFilter *filter, uint16_t sample);
static uint8_t Application_AdcGetSnapshot(float filtered[4], uint16_t raw[4]);

/* USER CODE END PFP */

/* 私有用户代码。 */
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  应用程序入口。
  * @retval int 返回状态。
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU 配置。 */

  /* 复位所有外设，并初始化 Flash 接口和系统滴答定时器。 */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* 配置系统时钟。 */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* 初始化所有已配置的外设。 */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  Devices_Initialize();
  (void)DataLogger_Init(&hspi2, BOOT_INIT_TIMEOUT);
  RecordTimer_Init();
  /* 启动阶段的串口输出完成后再开启命令接收。清理 RX 残留和错误状态，
     确保复位后的第一条命令从完整的空闲接收状态开始。 */
  __HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_OREF | UART_CLEAR_NEF |
                                  UART_CLEAR_PEF | UART_CLEAR_FEF);
  __HAL_UART_SEND_REQ(&huart1, UART_RXDATA_FLUSH_REQUEST);
  huart1.ErrorCode = HAL_UART_ERROR_NONE;
  (void)HAL_UART_Receive_IT(&huart1, (uint8_t *)&g_uart_rx_byte, 1U);
  /* 上电直接进入正常采样和日志流程，不再自动执行诊断阶段。 */
  g_adc_diag_channel = 0U;
  g_adc_diag_phase = 0U;
  g_adc_diag_count = 0U;
  g_adc_diag_phase_ready = 0U;
  g_adc_diag_auto_channels = 0U;
  g_adc_diag_active = 0U;
  g_adc_normal_enabled = 1U;
  g_adc_normal_status = AD7682_ERROR;
  memset(g_adc_temporal_filters, 0, sizeof(g_adc_temporal_filters));
  memset((void *)g_adc_filtered_codes, 0, sizeof(g_adc_filtered_codes));
  memset((void *)g_adc_raw_codes, 0, sizeof(g_adc_raw_codes));
  g_record_due = 0U;
  g_recording_enabled = DataLogger_GetRecordingEnabled();
  g_sensor_sample_elapsed_ms = 0U;
  g_sensor_sample_request = 0U;
  g_record_last_second = g_record_timer_seconds;
  g_ad7682.acquisition_delay_us = 100U;
  g_ad7682.configured_channel_valid = 0U;
  TIM3->CNT = 0U;
  TIM3->SR = 0U;
  TIM3->CR1 = TIM_CR1_CEN;

  /* USER CODE END 2 */

  /* 无限循环。 */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* 仅显式发送 adcdiag 命令后执行；上电不会进入此分支。 */
    if (g_adc_diag_active != 0U)
    {
      AdcDiag_ProcessRequest();
      continue;
    }
    RecordWritePending();
    Serial_ProcessCommand();
    if (SensorSample_ConsumeRequest() != 0U && g_adc_diag_active == 0U)
    {
      /* setting 模式只关闭实时串口输出，传感器采样和历史记录仍持续运行。 */
      Samples_ProcessAndOutput();
    }
    RecordWritePending();
    if (g_adc_diag_active == 0U) { Serial_DumpLogs(); }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief 配置系统时钟。
  * @retval 无
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** 配置内部稳压器的主输出电压。
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** 根据 RCC_OscInitTypeDef 结构体中的参数初始化 RCC 振荡器。
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** 初始化 CPU、AHB 和 APB 总线时钟。
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* 统一的阻塞式串口发送入口；所有输出均由主循环调用。 */
static void Serial_Write(const char *text)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), 100U);
}

/* TIM14 内部时钟，无外部引脚。APB 分频为 1 时 TIMCLK=PCLK，
 * 否则 TIMCLK=2*PCLK；分频至 10 kHz，每 10 个计数产生一次 1 ms
 * 中断。中断同时维护记录秒计数，并按 SENSOR_SAMPLE_PERIOD_MS 置位
 * 其他传感器请求。ADC 单独使用 TIM3，I2C/SPI2/串口留在主循环。 */
static void RecordTimer_Init(void)
{
  uint32_t clock = HAL_RCC_GetPCLK1Freq();
  if ((RCC->CFGR & RCC_CFGR_PPRE) != 0U) { clock *= 2U; }
  /* TIM3 是 ADC 的独立 100 Hz 触发源，避免被传感器、串口或 Flash
   * 处理节奏带偏。它只产生中断，实际四路 SPI 采样在对应 ISR 中完成。 */
  __HAL_RCC_TIM3_CLK_ENABLE();
  TIM3->CR1 = 0U;
  TIM3->PSC = clock / 1000000U - 1U;
  TIM3->ARR = 9999U;
  TIM3->EGR = TIM_EGR_UG;
  TIM3->SR = 0U;
  TIM3->DIER = TIM_DIER_UIE;
  HAL_NVIC_SetPriority(TIM3_IRQn, 2U, 0U);
  HAL_NVIC_ClearPendingIRQ(TIM3_IRQn);
  HAL_NVIC_EnableIRQ(TIM3_IRQn);
  /* Start only after diagnostics, so its IRQ does not contaminate tests. */

  __HAL_RCC_TIM14_CLK_ENABLE();
  TIM14->CR1 = 0U;
  TIM14->PSC = clock / 10000U - 1U;
  TIM14->ARR = 9U;
  TIM14->EGR = TIM_EGR_UG;
  TIM14->SR = 0U;
  g_record_timer_seconds = 0U;
  g_sensor_timer_milliseconds = 0U;
  g_sensor_sample_elapsed_ms = 0U;
  g_sensor_sample_request = 0U;
  g_record_last_second = 0U;
  g_record_due = 0U;
  TIM14->DIER = TIM_DIER_UIE;
  HAL_NVIC_SetPriority(TIM14_IRQn, 0U, 0U);
  HAL_NVIC_ClearPendingIRQ(TIM14_IRQn);
  HAL_NVIC_EnableIRQ(TIM14_IRQn);
  TIM14->CR1 = TIM_CR1_CEN;
  /* SysTick must be able to preempt the ~1.7 ms ADC scan; otherwise multiple
   * 1 ms ticks merge, slowing HAL timeouts and SHT41 conversion waits. */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0U, 0U);
}

/* 原子地取出一次定时器产生的采样请求。若一次采集超过周期，
 * 多个周期会合并为一个请求，避免采集完成后出现突发追赶。 */
static uint8_t SensorSample_ConsumeRequest(void)
{
  uint8_t request;
  uint32_t primask;

  primask = __get_PRIMASK();
  __disable_irq();
  request = g_sensor_sample_request;
  g_sensor_sample_request = 0U;
  __set_PRIMASK(primask);
  return request;
}

/* Only used between diagnostic captures, with TIM3 stopped.
 * Modes: 0=PLL 64 MHz, 1=PLL/AHB2 32 MHz, 2=HSI 16 MHz + PLL on,
 * 3=HSI 16 MHz + PLL off. HSI itself must remain on in every mode.
 * HAL recalibrates SysTick; explicitly recalibrate the three hardware timers
 * and UART. TIM2 resets only at phase boundaries, before any timestamps exist.
 * Status bits: RCC=1, UART init=2, RX arm=4, SPI2 init=8, PLL=16. */
static uint8_t AdcDiag_SetCoreClock(uint8_t mode)
{
  RCC_ClkInitTypeDef clocks = {0};
  RCC_OscInitTypeDef osc = {0};
  uint32_t target = mode >= 2U ? 16000000U : mode == 1U ? 32000000U : 64000000U;
  uint32_t timer_clock;
  uint32_t timer_enabled;
  uint32_t primask;
  uint8_t status = 0U;

  if (HAL_RCC_GetHCLKFreq() == target &&
      __HAL_RCC_GET_SYSCLK_SOURCE() == (mode >= 2U ? RCC_SYSCLKSOURCE_STATUS_HSI :
                                                                   RCC_SYSCLKSOURCE_STATUS_PLLCLK) &&
      ((RCC->CR & RCC_CR_PLLRDY) != 0U) == (mode != 3U)) { return 0U; }
  (void)HAL_UART_AbortReceive(&huart1);
  HAL_NVIC_DisableIRQ(USART1_IRQn);
  primask = __get_PRIMASK();
  __disable_irq();
  timer_enabled = TIM14->CR1 & TIM_CR1_CEN;
  TIM14->CR1 &= (uint32_t)~TIM_CR1_CEN;
  TIM2->CR1 = 0U;
  __set_PRIMASK(primask);
  /* Oscillator readiness waits must run with SysTick IRQ enabled, so HAL
   * timeouts remain bounded even if a clock does not become ready. PLL OFF
   * clears its source selection in this HAL: explicitly restore all boot
   * PLL parameters instead of reading the cleared configuration back. */
  osc.PLL.PLLState = RCC_PLL_ON;
  osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  osc.PLL.PLLM = RCC_PLLM_DIV1;
  osc.PLL.PLLN = 8U;
  osc.PLL.PLLP = RCC_PLLP_DIV2;
  osc.PLL.PLLQ = RCC_PLLQ_DIV2;
  osc.PLL.PLLR = RCC_PLLR_DIV2;
  if (mode != 3U && (RCC->CR & RCC_CR_PLLRDY) == 0U)
  {
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) { status |= 16U; }
  }
  clocks.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_SYSCLK;
  clocks.SYSCLKSource = mode >= 2U ? RCC_SYSCLKSOURCE_HSI : RCC_SYSCLKSOURCE_PLLCLK;
  clocks.AHBCLKDivider = mode == 1U ? RCC_SYSCLK_DIV2 : RCC_SYSCLK_DIV1;
  clocks.APB1CLKDivider = RCC_HCLK_DIV1;
  /* Keep two Flash wait states at all speeds. */
  if (HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_2) != HAL_OK) { status |= 1U; }
  if (HAL_RCC_GetHCLKFreq() != target) { status |= 1U; }
  if (mode == 3U)
  {
    /* Never stop PLL until HSI is confirmed as the CPU clock source. */
    if (__HAL_RCC_GET_SYSCLK_SOURCE() == RCC_SYSCLKSOURCE_STATUS_HSI)
    {
      osc.PLL.PLLState = RCC_PLL_OFF;
      if (HAL_RCC_OscConfig(&osc) != HAL_OK) { status |= 16U; }
    }
    else { status |= 16U; }
  }
  if (((RCC->CR & RCC_CR_PLLRDY) != 0U) != (mode != 3U)) { status |= 16U; }
  timer_clock = HAL_RCC_GetPCLK1Freq();
  if ((RCC->CFGR & RCC_CFGR_PPRE) != 0U) { timer_clock *= 2U; }
  __disable_irq();
  TIM2->PSC = timer_clock / 1000000U - 1U;
  TIM2->EGR = TIM_EGR_UG;
  TIM2->SR = 0U;
  TIM2->CNT = 0U;
  TIM2->CR1 = TIM_CR1_CEN;
  TIM3->PSC = timer_clock / 1000000U - 1U;
  TIM3->EGR = TIM_EGR_UG;
  TIM3->CNT = 0U;
  TIM3->SR = 0U;
  HAL_NVIC_ClearPendingIRQ(TIM3_IRQn);
  TIM14->PSC = timer_clock / 10000U - 1U;
  TIM14->EGR = TIM_EGR_UG;
  TIM14->CNT = 0U;
  TIM14->SR = 0U;
  HAL_NVIC_ClearPendingIRQ(TIM14_IRQn);
  TIM14->CR1 |= timer_enabled;
  __set_PRIMASK(primask);

  /* USART1 is clocked by PCLK1. Recompute BRR before sending metadata;
   * never transmit using the previous clock's baud divisor. */
  if (HAL_UART_Init(&huart1) != HAL_OK) { status |= 2U; }
  __HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_OREF | UART_CLEAR_NEF |
                                  UART_CLEAR_PEF | UART_CLEAR_FEF);
  __HAL_UART_SEND_REQ(&huart1, UART_RXDATA_FLUSH_REQUEST);
  HAL_NVIC_ClearPendingIRQ(USART1_IRQn);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
  if (HAL_UART_Receive_IT(&huart1, (uint8_t *)&g_uart_rx_byte, 1U) != HAL_OK)
  { status |= 4U; }
  hspi2.Init.BaudRatePrescaler = HAL_RCC_GetPCLK1Freq() == 16000000U ?
                               SPI_BAUDRATEPRESCALER_8 :
                               HAL_RCC_GetPCLK1Freq() == 32000000U ?
                               SPI_BAUDRATEPRESCALER_16 : SPI_BAUDRATEPRESCALER_32;
  if (HAL_SPI_Init(&hspi2) != HAL_OK) { status |= 8U; }
  return status;
}

/* LPS22HH datasheet Rev 2 p43: CTRL_REG1(10h) ODR[6:4]=000 stops
 * autonomous conversions, preserving other control bits and I2C access.
 * Save each original register once per diagnostic run, and verify every
 * write. Bit0/1 of the result identify U7/U11 setup or readback failure.
 * Always call on 64 MHz, since software I2C delays use CPU cycles. */
static uint8_t AdcDiag_SetLpsMode(uint8_t power_down_mask)
{
  const LPS22HH_Device *devices[2] = { &g_lps22hh_u7, &g_lps22hh_u11 };
  uint8_t i;
  uint8_t result = 0U;
  if (HAL_RCC_GetHCLKFreq() != 64000000U) { return 3U; }
  for (i = 0U; i < 2U; i++)
  {
    uint8_t bit = (uint8_t)(1U << i);
    uint8_t desired;
    g_adc_diag_lps_readback[i] = 0xFFU;
    if ((g_adc_diag_lps_saved & bit) == 0U)
    {
      if (SoftI2C_MemRead(&hi2c1, devices[i]->address, 0x10U,
                         &g_adc_diag_lps_ctrl[i], 1U, BOOT_INIT_TIMEOUT) != HAL_OK)
      { result |= bit; continue; }
      g_adc_diag_lps_saved |= bit;
    }
    desired = g_adc_diag_lps_ctrl[i];
    if ((power_down_mask & bit) != 0U) { desired &= (uint8_t)~0x70U; }
    if (LPS22HH_RestoreCtrlReg1(&hi2c1, devices[i], desired, BOOT_INIT_TIMEOUT) != HAL_OK ||
        SoftI2C_MemRead(&hi2c1, devices[i]->address, 0x10U,
                       &g_adc_diag_lps_readback[i], 1U, BOOT_INIT_TIMEOUT) != HAL_OK ||
        g_adc_diag_lps_readback[i] != desired)
    { result |= bit; }
  }
  return result;
}

/* Only change OSPEEDR fields for PA1=SCK, PA7=DIN and PA5=CNV.
 * Never reinitialize GPIO mode/AF or touch PA6 (ADC drives SDO/MISO).
 * STM32G0 HAL LOW=00, HIGH=10. No output level or clock rate changes here.
 * high_mask: bit0=SCK, bit1=DIN, bit2=CNV. Called outside capture only. */
static uint8_t AdcDiag_SetEdgeSpeed(uint8_t high_mask)
{
  const uint32_t mask = (3UL << 2U) | (3UL << 14U) | (3UL << 10U);
  uint32_t desired = ((high_mask & 1U) != 0U ? GPIO_SPEED_FREQ_HIGH << 2U : 0U) |
                     ((high_mask & 2U) != 0U ? GPIO_SPEED_FREQ_HIGH << 14U : 0U) |
                     ((high_mask & 4U) != 0U ? GPIO_SPEED_FREQ_HIGH << 10U : 0U);
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  MODIFY_REG(GPIOA->OSPEEDR, mask, desired);
  __DSB();
  __set_PRIMASK(primask);
  return (GPIOA->OSPEEDR & mask) == desired ? 0U : 1U;
}

static void AdcDiag_DumpPhase(uint8_t phase)
{
  char line[176];
  unsigned tag = (unsigned)g_adc_diag_channel * 100U + phase;
  uint16_t i;

  for (i = 0U; i < g_adc_diag_count; i++)
  {
    AdcDiagRow *row = &g_adc_diag_rows[i];
    (void)snprintf(line, sizeof(line),
                   "ADC:%u,%lu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\r\n", tag,
                   (unsigned long)row->first_cnv_us,
                   (unsigned)row->samples[0], (unsigned)row->samples[1],
                   (unsigned)row->samples[2], (unsigned)row->samples[3],
                   (unsigned)row->samples[4],
                   (unsigned)row->offsets_us[0], (unsigned)row->offsets_us[1],
                   (unsigned)row->offsets_us[2], (unsigned)row->offsets_us[3],
                   (unsigned)row->status);
    Serial_Write(line);
  }
}

/* Finite automatic comparisons on IN0 followed by IN1.
 * 0:基准  1:SysTick IRQ 关  2:TIM14 IRQ 关  3:两者 IRQ 关
 * 4:SPI 1 MHz  5:SPI 4 MHz  6:等待 100 us  7:等待 1000 us
 * 8:触发周期 17/23 ms 交替（均值 20 ms）  9:重复基准。
 * 10: 3000 us acquisition wait, exposing pulse duration across conversions.
 * 11: WFI between groups (SysTick remains enabled); ADC code is unchanged.
 * 12: integer arithmetic between groups, unchanged ADC/SPI timing.
 * 13: 10 ms group trigger, otherwise baseline 500 us/2 MHz.
 * 14: repeat baseline after new comparisons to detect slow baseline drift.
 * 15/16/17: 64/32/64 MHz core/bus, same 2 MHz SPI and 500 us wait.
 * 18:baseline, 19:U7 powerdown, 20:U11 powerdown, 21:both powerdown,
 * 22:baseline, 23:HSI16 PLLon, 24:HSI16 PLLoff, 25:HSI16 PLLon, 26:baseline,
 * 27:HSI16 PLLoff + both LPS powerdown, 28:baseline. */
static void AdcDiag_StartPhase(void)
{
  char line[176];
  uint16_t discarded;
  uint32_t primask;
  uint32_t spi_hz = 2000000U;
  uint32_t group_us = g_adc_diag_phase == 13U ? 10000U : ADC_DIAG_PERIOD_US;
  uint8_t systick_off = g_adc_diag_phase == 1U || g_adc_diag_phase == 3U;
  uint8_t tim14_off = g_adc_diag_phase == 2U || g_adc_diag_phase == 3U;
  uint8_t clock_mode = g_adc_diag_phase == 16U ? 1U :
                       (g_adc_diag_phase == 24U || g_adc_diag_phase == 27U) ? 3U :
                       (g_adc_diag_phase == 23U || g_adc_diag_phase == 25U) ? 2U : 0U;
  uint8_t lps_mask = g_adc_diag_phase == 19U ? 1U :
                     g_adc_diag_phase == 20U ? 2U :
                     (g_adc_diag_phase == 21U || g_adc_diag_phase == 27U) ? 3U : 0U;
  uint8_t edge_mask = g_adc_diag_phase == 30U ? 1U :
                      g_adc_diag_phase == 32U ? 2U :
                      g_adc_diag_phase == 34U ? 4U :
                      g_adc_diag_phase == 36U ? 3U :
                      g_adc_diag_phase == 38U ? 7U : 0U;
  HAL_StatusTypeDef spi_status;

  /* Apply sensor controls before slowing the CPU. All bus transactions occur
   * outside capture, and a settling delay excludes the switching transient. */
  g_adc_diag_lps_status = (g_adc_diag_phase >= 18U && g_adc_diag_phase < 29U) ?
                         AdcDiag_SetLpsMode(lps_mask) : 0U;
  g_adc_diag_clock_status = AdcDiag_SetCoreClock(clock_mode);
  hspi1.Init.BaudRatePrescaler = HAL_RCC_GetPCLK1Freq() == 16000000U ?
                               SPI_BAUDRATEPRESCALER_8 :
                               HAL_RCC_GetPCLK1Freq() == 32000000U ?
                               SPI_BAUDRATEPRESCALER_16 : SPI_BAUDRATEPRESCALER_32;
  if (g_adc_diag_phase == 4U)
  {
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
    spi_hz = 1000000U;
  }
  else if (g_adc_diag_phase == 5U)
  {
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    spi_hz = 4000000U;
  }
  spi_status = HAL_SPI_Init(&hspi1);
  /* Apply after SPI init, so even an MSP reinitialization cannot overwrite
   * the requested edge setting. Warm-up/config and capture use this setting. */
  g_adc_diag_edge_status = AdcDiag_SetEdgeSpeed(edge_mask);
  g_ad7682.acquisition_delay_us = AD7682_ACQUISITION_DELAY_US;
  if (g_adc_diag_phase == 6U) { g_ad7682.acquisition_delay_us = 100U; }
  if (g_adc_diag_phase == 7U) { g_ad7682.acquisition_delay_us = 1000U; }
  if (g_adc_diag_phase == 10U) { g_ad7682.acquisition_delay_us = 3000U; }
  g_ad7682.configured_channel_valid = 0U;
  g_adc_status = AD7682_ReadChannel(&g_ad7682, g_adc_diag_channel,
                                    &discarded, 10U);
  (void)snprintf(line, sizeof(line),
                 "ADC:991,%u,%lu,%lu,%u,%u,%lu,%u,%u,%u,%lu,%u\r\n",
                 (unsigned)g_adc_diag_channel * 100U + g_adc_diag_phase,
                 (unsigned long)g_ad7682.acquisition_delay_us,
                 (unsigned long)spi_hz, (unsigned)systick_off,
                 (unsigned)tim14_off, (unsigned long)group_us,
                 (unsigned)spi_status, (unsigned)g_adc_status,
                 (unsigned)(g_adc_diag_phase == 11U ? 1U :
                            g_adc_diag_phase == 12U ? 2U : 0U),
                 (unsigned long)HAL_RCC_GetHCLKFreq(),
                 (unsigned)g_adc_diag_clock_status);
  Serial_Write(line);
  if (g_adc_diag_phase >= 18U && g_adc_diag_phase < 29U)
  {
    (void)snprintf(line, sizeof(line), "ADC:993,%u,%u,%u,%u,%u,%u,%u,%u,%u,%lu,%u\r\n",
                   (unsigned)g_adc_diag_channel * 100U + g_adc_diag_phase,
                   (unsigned)lps_mask, (unsigned)g_adc_diag_lps_status,
                   (unsigned)g_adc_diag_lps_readback[0],
                   (unsigned)g_adc_diag_lps_readback[1],
                   (unsigned)g_adc_diag_lps_ctrl[0], (unsigned)g_adc_diag_lps_ctrl[1],
                   (unsigned)clock_mode, (unsigned)((RCC->CR & RCC_CR_PLLRDY) != 0U),
                   (unsigned long)HAL_RCC_GetHCLKFreq(),
                   (unsigned)(__HAL_RCC_GET_SYSCLK_SOURCE() == RCC_SYSCLKSOURCE_STATUS_HSI));
    Serial_Write(line);
  }
  if (g_adc_diag_phase >= ADC_DIAG_FIRST_PHASE)
  {
    (void)snprintf(line, sizeof(line), "ADC:995,%u,%u,%u,%u,%u,%u,%u,%lu,%lu,%lu,%u\r\n",
                   (unsigned)g_adc_diag_channel * 100U + g_adc_diag_phase,
                   (unsigned)g_adc_diag_block + 1U, (unsigned)ADC_DIAG_BLOCKS,
                   (unsigned)edge_mask,
                   (unsigned)((GPIOA->OSPEEDR >> 2U) & 3U),
                   (unsigned)((GPIOA->OSPEEDR >> 14U) & 3U),
                   (unsigned)((GPIOA->OSPEEDR >> 10U) & 3U),
                   (unsigned long)HAL_RCC_GetHCLKFreq(),
                   (unsigned long)spi_hz, (unsigned long)group_us,
                   (unsigned)g_adc_diag_edge_status);
    Serial_Write(line);
  }
  HAL_Delay(g_adc_diag_phase >= 18U ? 250U : 50U);

  primask = __get_PRIMASK();
  __disable_irq();
  g_adc_diag_systick_ctrl = SysTick->CTRL;
  g_adc_diag_timer_dier = TIM14->DIER;
  g_adc_diag_record_seconds = g_record_timer_seconds;
  g_adc_diag_record_ms = g_sensor_timer_milliseconds;
  g_adc_diag_phase_start_us = AD7682_TimeUs();
  if (systick_off != 0U)
  {
    SysTick->CTRL &= (uint32_t)~SysTick_CTRL_TICKINT_Msk;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
  }
  if (tim14_off != 0U)
  {
    TIM14->DIER &= (uint32_t)~TIM_DIER_UIE;
    HAL_NVIC_ClearPendingIRQ(TIM14_IRQn);
  }
  __set_PRIMASK(primask);
  g_adc_diag_next_us = g_adc_diag_phase_start_us + group_us;
  g_adc_diag_phase_ready = 1U;
}

/* 测试结束先恢复中断与时间基准，再输出缓冲数据。
 * 补偿只用于 HAL/记录时间（毫秒量级），分析用的 CNV 时间全部来自 TIM2。 */
static void AdcDiag_StopPhase(void)
{
  uint32_t primask = __get_PRIMASK();
  uint32_t elapsed_ms;
  uint32_t ms;
  __disable_irq();
  elapsed_ms = (uint32_t)(AD7682_TimeUs() - g_adc_diag_phase_start_us) / 1000U;
  if (g_adc_diag_phase == 1U || g_adc_diag_phase == 3U)
  {
    ms = elapsed_ms;
    while (ms-- != 0U) { HAL_IncTick(); }
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
    SysTick->CTRL = g_adc_diag_systick_ctrl;
  }
  if (g_adc_diag_phase == 2U || g_adc_diag_phase == 3U)
  {
    ms = (uint32_t)g_adc_diag_record_ms + elapsed_ms;
    g_record_timer_seconds = g_adc_diag_record_seconds + ms / 1000U;
    g_sensor_timer_milliseconds = (uint16_t)(ms % 1000U);
    TIM14->SR &= (uint32_t)~TIM_SR_UIF;
    HAL_NVIC_ClearPendingIRQ(TIM14_IRQn);
    TIM14->DIER = g_adc_diag_timer_dier;
  }
  g_sensor_sample_request = 0U;
  g_sensor_sample_elapsed_ms = 0U;
  __set_PRIMASK(primask);
  g_adc_diag_phase_ready = 0U;
  if (g_adc_diag_phase >= ADC_DIAG_FIRST_PHASE)
  {
    char line[112];
    uint8_t edge_restore = AdcDiag_SetEdgeSpeed(0U);
    uint16_t i;
    if (edge_restore != 0U)
    {
      for (i = 0U; i < g_adc_diag_count; i++)
      { g_adc_diag_rows[i].status |= 0x1000U; }
    }
    (void)snprintf(line, sizeof(line), "ADC:996,%u,%u,%u,%u,%u,%u,%lu,0,0,0,0\r\n",
                   (unsigned)g_adc_diag_channel * 100U + g_adc_diag_phase,
                   (unsigned)g_adc_diag_block + 1U, (unsigned)edge_restore,
                   (unsigned)((GPIOA->OSPEEDR >> 2U) & 3U),
                   (unsigned)((GPIOA->OSPEEDR >> 14U) & 3U),
                   (unsigned)((GPIOA->OSPEEDR >> 10U) & 3U),
                   (unsigned long)HAL_RCC_GetHCLKFreq());
    Serial_Write(line);
  }
  /* Restore before buffered output, and before the next phase/normal mode.
   * All capture timestamps have already been saved in the row buffer. */
  {
    uint8_t restore_status = 0U;
    uint8_t lps_restore = 0U;
    uint8_t retry;
    uint16_t i;
    for (retry = 0U; retry < BOOT_INIT_RETRIES; retry++)
    {
      uint8_t attempt = AdcDiag_SetCoreClock(0U);
      restore_status |= attempt;
      if (attempt == 0U && HAL_RCC_GetHCLKFreq() == 64000000U) { break; }
    }
    if (g_adc_diag_phase >= 18U && g_adc_diag_phase < 29U)
    {
      for (retry = 0U; retry < BOOT_INIT_RETRIES; retry++)
      {
        lps_restore = AdcDiag_SetLpsMode(0U);
        if (lps_restore == 0U) { break; }
      }
      {
        char line[96];
        (void)snprintf(line, sizeof(line), "ADC:994,%u,%u,%u,%u,%u,%lu,0,0,0,0,0\r\n",
                       (unsigned)g_adc_diag_channel * 100U + g_adc_diag_phase,
                       (unsigned)restore_status, (unsigned)lps_restore,
                       (unsigned)g_adc_diag_lps_readback[0], (unsigned)g_adc_diag_lps_readback[1],
                       (unsigned long)HAL_RCC_GetHCLKFreq());
        Serial_Write(line);
      }
    }
    if (lps_restore != 0U)
    {
      for (i = 0U; i < g_adc_diag_count; i++)
      { g_adc_diag_rows[i].status |= 0x800U; }
    }
    if (restore_status != 0U || HAL_RCC_GetHCLKFreq() != 64000000U)
    {
      for (i = 0U; i < g_adc_diag_count; i++)
      { g_adc_diag_rows[i].status |= 0x400U; }
    }
  }
}

static void AdcDiag_ProcessRequest(void)
{
  uint16_t samples[5];
  uint32_t times[5];
  uint32_t now;
  uint32_t next_period;
  uint8_t i;
  AdcDiagRow *row;

  if (g_adc_diag_phase_ready == 0U) { AdcDiag_StartPhase(); }
  now = AD7682_TimeUs();
  if ((int32_t)(now - g_adc_diag_next_us) < 0)
  {
    /* Only alter inter-group activity, never the ADC transfer itself.
     * WFI wakes at least every 1 ms; final 2 ms use polling for an accurate
     * trigger deadline. The active workload is short and similarly bounded. */
    if ((uint32_t)(g_adc_diag_next_us - now) > 2000U)
    {
      if (g_adc_diag_phase == 11U) { __DSB(); __WFI(); }
      else if (g_adc_diag_phase == 12U)
      {
        g_adc_diag_cpu_probe = g_adc_diag_cpu_probe * 1664525U + 1013904223U;
      }
    }
    return;
  }
  row = &g_adc_diag_rows[g_adc_diag_count];
  row->status = (uint32_t)(now - g_adc_diag_next_us) > 100U ? 0x100U : 0U;
  if (g_adc_diag_clock_status != 0U) { row->status |= 0x400U; }
  if (g_adc_diag_lps_status != 0U) { row->status |= 0x800U; }
  if (g_adc_diag_edge_status != 0U) { row->status |= 0x1000U; }
  g_adc_status = AD7682_ReadChannelTimedSamples(&g_ad7682, g_adc_diag_channel,
                                                samples, times, 10U);
  row->status |= (uint16_t)g_adc_status;
  row->first_cnv_us = (g_adc_status == AD7682_OK ? times[0] : now) -
                      g_adc_diag_phase_start_us;
  for (i = 0U; i < 5U; i++)
  {
    row->samples[i] = g_adc_status == AD7682_OK ? samples[i] : SENSOR_INVALID_ADC;
    if (i != 0U)
    {
      uint32_t offset = g_adc_status == AD7682_OK ? times[i] - times[0] : 0U;
      if (offset > 65535U) { row->status |= 0x200U; }
      row->offsets_us[i - 1U] = (uint16_t)(offset > 65535U ? 65535U : offset);
    }
  }
  g_adc_diag_count++;
  next_period = ADC_DIAG_PERIOD_US;
  if (g_adc_diag_phase == 13U) { next_period = 10000U; }
  if (g_adc_diag_phase == 8U)
  {
    next_period = (g_adc_diag_count & 1U) != 0U ? 17000U : 23000U;
  }
  g_adc_diag_next_us += next_period;
  /* 发生异常超时则放弃追赶，不把多个过期请求挤在一起。 */
  now = AD7682_TimeUs();
  if ((int32_t)(now - g_adc_diag_next_us) >= 0)
  {
    row->status |= 0x100U;
    g_adc_diag_next_us = now + next_period;
  }

  if (g_adc_diag_count >= ADC_DIAG_ROWS)
  {
    AdcDiag_StopPhase();
    AdcDiag_DumpPhase(g_adc_diag_phase);
    g_adc_diag_count = 0U;
    g_adc_diag_block++;
    /* Each block is an independent capture with its own timestamp origin.
     * Dump while sampling is stopped; do not let UART activity contaminate
     * the next block. The 995 event identifies repeated blocks of one tag. */
    if (g_adc_diag_block < ADC_DIAG_BLOCKS) { return; }
    g_adc_diag_block = 0U;
    g_adc_diag_phase++;
    if (g_adc_diag_phase >= ADC_DIAG_PHASES)
    {
      if (g_adc_diag_auto_channels != 0U && g_adc_diag_channel == 0U)
      {
        g_adc_diag_channel = 1U;
        g_adc_diag_phase = ADC_DIAG_FIRST_PHASE;
        Serial_Write("ADC:992,1,0,0,0,0,0,0,0,0,0,0\r\n");
      }
      else
      {
        hspi1.Init.BaudRatePrescaler = HAL_RCC_GetPCLK1Freq() == 16000000U ?
                                     SPI_BAUDRATEPRESCALER_8 :
                                     HAL_RCC_GetPCLK1Freq() == 32000000U ?
                                     SPI_BAUDRATEPRESCALER_16 : SPI_BAUDRATEPRESCALER_32;
        (void)HAL_SPI_Init(&hspi1);
        /* 正常模式每 10 ms 扫描一次四路；100 us 已远大于手册要求的
         * 1.8 us 最小采集时间，并把一次四路扫描控制在约 2 ms 内。 */
        g_ad7682.acquisition_delay_us = 100U;
        g_ad7682.configured_channel_valid = 0U;
        g_adc_diag_active = 0U;
        g_adc_diag_auto_channels = 0U;
        g_record_last_second = g_record_timer_seconds;
        {
          char done[96];
          (void)snprintf(done, sizeof(done), "ADC:999,%lu,0,0,0,0,0,0,0,0,0,0\r\n",
                         (unsigned long)HAL_RCC_GetHCLKFreq());
          Serial_Write(done);
        }
        memset(g_adc_temporal_filters, 0, sizeof(g_adc_temporal_filters));
        memset((void *)g_adc_filtered_codes, 0, sizeof(g_adc_filtered_codes));
        memset((void *)g_adc_raw_codes, 0, sizeof(g_adc_raw_codes));
        g_record_due = 0U;
        g_sensor_sample_elapsed_ms = 0U;
        g_sensor_sample_request = 0U;
        g_adc_normal_enabled = 1U;
        TIM3->CNT = 0U;
        TIM3->SR = 0U;
        TIM3->CR1 = TIM_CR1_CEN;
      }
    }
  }
}

/*
 * 上电设备探测和初始化。
 *
 * MX_* 外设初始化由 CubeMX 生成，失败时会进入 Error_Handler；这里对
 * 外部器件增加有限重试，避免器件电源/复位释放稍慢时第一次访问失败。
 * 重试只执行无副作用的探测/配置，不重复执行 Flash 擦除。
 */
static void Devices_Initialize(void)
{
  uint8_t retry;
  uint8_t id[3] = {0U};
  uint8_t voltage_low = 0U;
  float temperature = 0.0f;
  float humidity = 0.0f;
  PCF8563_DateTime datetime = {0};
  HAL_StatusTypeDef status;
  uint8_t sht_ok = 0U;
  uint8_t rtc_ok = 0U;
  uint8_t flash_ok = 0U;
  uint8_t lps_u7_ok = 0U;
  uint8_t lps_u11_ok = 0U;

  for (retry = 0U; retry < BOOT_INIT_RETRIES; retry++)
  {
    g_adc_status = AD7682_Init(&g_ad7682, &hspi1, 2.5f);
    if (g_adc_status == AD7682_OK) { break; }
    HAL_Delay(10U);
  }
  Serial_Write(g_adc_status == AD7682_OK ? "AD7682 INIT OK\r\n" :
                                        "AD7682 INIT ERROR\r\n");
  Serial_Write("AD7682: 4CH 100Hz, DUMMY2+1, TEMPORAL MEDIAN100+AVG25, FILTERED LOG\r\n");

  Serial_Write("SHT41 INIT...\r\n");
  status = HAL_ERROR;
  for (retry = 0U; retry < BOOT_INIT_RETRIES; retry++)
  {
    status = SHT41_Reset(&hi2c1, BOOT_INIT_TIMEOUT);
    if (status == HAL_OK)
    {
      HAL_Delay(2U);
      status = SHT41_Read(&hi2c1, &temperature, &humidity, BOOT_INIT_TIMEOUT);
    }
    if (status == HAL_OK)
    {
      sht_ok = 1U;
      break;
    }
    HAL_Delay(10U);
  }
  Serial_Write("PCF8563 INIT...\r\n");
  status = HAL_ERROR;
  for (retry = 0U; retry < BOOT_INIT_RETRIES; retry++)
  {
    status = PCF8563_GetVoltageLow(&hi2c1, &voltage_low, BOOT_INIT_TIMEOUT);
    if (status == HAL_OK)
    {
      status = PCF8563_ReadDateTime(&hi2c1, &datetime, BOOT_INIT_TIMEOUT);
    }
    if (status == HAL_OK)
    {
      rtc_ok = 1U;
      break;
    }
    HAL_Delay(10U);
  }
  Serial_Write("W25Q128 INIT...\r\n");
  status = HAL_ERROR;
  for (retry = 0U; retry < BOOT_INIT_RETRIES; retry++)
  {
    status = W25Q128_ReadJedec(&hspi2, id, BOOT_INIT_TIMEOUT);
    if (status == HAL_OK && id[0] == 0xEFU && id[1] == 0x40U && id[2] == 0x18U)
    {
      flash_ok = 1U;
      break;
    }
    status = HAL_ERROR;
    HAL_Delay(10U);
  }
  Serial_Write("LPS22HH U7 INIT...\r\n");
  status = HAL_ERROR;
  for (retry = 0U; retry < BOOT_INIT_RETRIES; retry++)
  {
    status = LPS22HH_InitDevice(&hi2c1, &g_lps22hh_u7, BOOT_INIT_TIMEOUT);
    if (status == HAL_OK)
    {
      lps_u7_ok = 1U;
      break;
    }
    HAL_Delay(10U);
  }
  Serial_Write("LPS22HH U11 INIT...\r\n");
  status = HAL_ERROR;
  for (retry = 0U; retry < BOOT_INIT_RETRIES; retry++)
  {
    status = LPS22HH_InitDevice(&hi2c1, &g_lps22hh_u11, BOOT_INIT_TIMEOUT);
    if (status == HAL_OK)
    {
      lps_u11_ok = 1U;
      break;
    }
    HAL_Delay(10U);
  }
  Serial_Write(sht_ok ? "SHT41 INIT OK\r\n" : "SHT41 INIT ERROR\r\n");
  Serial_Write(rtc_ok ? "PCF8563 INIT OK\r\n" : "PCF8563 INIT ERROR\r\n");
  Serial_Write(flash_ok ? "W25Q128 INIT OK\r\n" : "W25Q128 INIT ERROR\r\n");
  Serial_Write(lps_u7_ok ? "LPS22HH U7 INIT OK\r\n" : "LPS22HH U7 INIT ERROR\r\n");
  Serial_Write(lps_u11_ok ? "LPS22HH U11 INIT OK\r\n" : "LPS22HH U11 INIT ERROR\r\n");
  g_w25q_ready = flash_ok;
}

/* PA15/LED2 电量指示：低电平点亮，高电平熄灭。 */
static void BatteryLed_Update(float voltage)
{
  static uint32_t last_toggle_tick;
  static uint8_t blink_state;
  uint32_t now = HAL_GetTick();

  if (voltage >= 4.0f)
  {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
    blink_state = 0U;
    last_toggle_tick = now;
  }
  else if (voltage >= 3.7f)
  {
    if ((now - last_toggle_tick) >= 500U)
    {
      last_toggle_tick = now;
      blink_state = (uint8_t)!blink_state;
      HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15,
                        blink_state ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }
  }
  else
  {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
    blink_state = 0U;
    last_toggle_tick = now;
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart1)
  {
    if (g_uart_rx_byte == '\r' || g_uart_rx_byte == '\n')
    {
      if (g_uart_line_ready == 0U)
      {
        g_uart_line[g_uart_line_length] = 0;
        g_uart_line_ready = 1U;
      }
      g_uart_line_length = 0U;
    }
    else if (g_uart_line_ready == 0U)
    {
      if (g_uart_line_length < (sizeof(g_uart_line) - 1U))
      {
        g_uart_line[g_uart_line_length++] = (char)g_uart_rx_byte;
      }
      else
      {
        g_uart_line_overflow = 1U;
      }
    }
    (void)HAL_UART_Receive_IT(&huart1, (uint8_t *)&g_uart_rx_byte, 1U);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart1)
  {
    /* 只置位标志；HAL 状态恢复放到主循环，避免在 UART ISR 内重入 HAL。 */
    g_uart_rx_restart = 1U;
  }
}

static void Serial_RecoverReceive(void)
{
  uint32_t primask;

  if (g_uart_rx_restart == 0U)
  {
    return;
  }
  primask = __get_PRIMASK();
  __disable_irq();
  if (g_uart_rx_restart != 0U)
  {
    (void)HAL_UART_AbortReceive(&huart1);
    __HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_OREF | UART_CLEAR_NEF |
                                    UART_CLEAR_PEF | UART_CLEAR_FEF);
    __HAL_UART_SEND_REQ(&huart1, UART_RXDATA_FLUSH_REQUEST);
    huart1.ErrorCode = HAL_UART_ERROR_NONE;
    g_uart_line_length = 0U;
    g_uart_line_ready = 0U;
    g_uart_line_overflow = 0U;
    g_uart_rx_restart = 0U;
    (void)HAL_UART_Receive_IT(&huart1, (uint8_t *)&g_uart_rx_byte, 1U);
  }
  __set_PRIMASK(primask);
}

static uint8_t Serial_IsSpace(char c){return (uint8_t)(c==' '||c=='\t'||c=='\r'||c=='\n');}
static uint8_t Serial_ToLower(char c){return (uint8_t)((c>='A'&&c<='Z')?(c+('a'-'A')):c);}
static uint8_t Serial_Equals(const char *a,const char *b){while(*a&&*b&&Serial_ToLower(*a)==Serial_ToLower(*b)){a++;b++;}return (uint8_t)(*a==0&&*b==0);}
static char *Serial_Trim(char *s){char *e;while(Serial_IsSpace(*s))s++;e=s+strlen(s);while(e>s&&Serial_IsSpace(e[-1]))e--;*e=0;return s;}
static uint8_t Serial_Digits(const char *s,uint8_t n,uint8_t *value){uint8_t i,v=0;if(!s||!value)return 0;for(i=0;i<n;i++){if(s[i]<'0'||s[i]>'9')return 0;v=(uint8_t)(v*10U+(uint8_t)(s[i]-'0'));}*value=v;return 1;}
static uint8_t Serial_ParseUint32(const char *text,uint32_t *value){uint32_t v=0U;const char *p=text;if(!text||!value||*text==0)return 0U;while(*p){if(*p<'0'||*p>'9'||v>429496729U)return 0U;v=v*10U+(uint32_t)(*p-'0');p++;}*value=v;return 1U;}
static uint8_t Serial_IsLeap(uint16_t year){return (uint8_t)((year%4U==0U&&year%100U!=0U)||(year%400U==0U));}
static uint8_t Serial_DaysInMonth(uint16_t year,uint8_t month){static const uint8_t days[]={31,28,31,30,31,30,31,31,30,31,30,31};if(month==2U&&Serial_IsLeap(year))return 29U;return days[month-1U];}
static uint8_t Serial_Weekday(uint16_t year,uint8_t month,uint8_t day){uint32_t days=0;uint16_t y;uint8_t m;for(y=2000U;y<year;y++)days+=Serial_IsLeap(y)?366U:365U;for(m=1U;m<month;m++)days+=Serial_DaysInMonth(year,m);days+=(uint32_t)(day-1U);return (uint8_t)((6U+days)%7U);}

static uint8_t Serial_ParseSetTime(const char *s,PCF8563_DateTime *dt)
{
  uint8_t yy,mo,da,hh,mi,se;uint16_t year;size_t n;
  if(!s||!dt)return 0;n=strlen(s);
  if(n!=21U||Serial_ToLower(s[0])!='s'||Serial_ToLower(s[1])!='e'||Serial_ToLower(s[2])!='t'||s[3]!=' '||s[6]!='-'||s[9]!='-'||s[12]!=' '||s[15]!=':'||s[18]!=':')return 0;
  if(!Serial_Digits(&s[4],2,&yy)||!Serial_Digits(&s[7],2,&mo)||!Serial_Digits(&s[10],2,&da)||!Serial_Digits(&s[13],2,&hh)||!Serial_Digits(&s[16],2,&mi)||!Serial_Digits(&s[19],2,&se))return 0;
  year=(uint16_t)(2000U+yy);if(mo<1U||mo>12U||da<1U||da>Serial_DaysInMonth(year,mo)||hh>23U||mi>59U||se>59U)return 0;
  dt->year=yy;dt->month=mo;dt->day=da;dt->hours=hh;dt->minutes=mi;dt->seconds=se;dt->weekday=Serial_Weekday(year,mo,da);dt->century=0U;return 1;
}

/* setting 模式下只处理配置和日志命令；实时采样不会被暂停。 */
static void Serial_PrintSettingHelp(void){Serial_Write("SETTING MODE\r\nSET YY-MM-DD HH:MM:SS\r\nlog info\r\nlog start\r\nlog stop\r\nlog interval SECONDS\r\nlog order old\r\nlog order new\r\nlog dump\r\nlog clear\r\nexit\r\n");}
static void Serial_HandleSetTime(const char *command)
{
  PCF8563_DateTime requested={0},actual={0};uint8_t vl=0;HAL_StatusTypeDef s;char line[128];
  if(!Serial_ParseSetTime(command,&requested)){Serial_Write("RTC SET ERROR: use SET YY-MM-DD HH:MM:SS\r\n");return;}
  s=PCF8563_WriteDateTime(&hi2c1,&requested,100);if(s==HAL_OK)s=PCF8563_SetRunning(&hi2c1,1U,100);
  if(s!=HAL_OK){(void)snprintf(line,sizeof(line),"RTC SET ERROR: status=%d\r\n",(int)s);Serial_Write(line);return;}
  s=PCF8563_GetVoltageLow(&hi2c1,&vl,100);if(s==HAL_OK)s=PCF8563_ReadDateTime(&hi2c1,&actual,100);
  if(s!=HAL_OK){(void)snprintf(line,sizeof(line),"RTC SET VERIFY ERROR: status=%d\r\n",(int)s);Serial_Write(line);return;}
  (void)snprintf(line,sizeof(line),"RTC SET OK: 20%02u-%02u-%02u %02u:%02u:%02u VL=%u\r\n",actual.year,actual.month,actual.day,actual.hours,actual.minutes,actual.seconds,vl);Serial_Write(line);
}

static void Serial_ProcessCommand(void)
{
  char command[64];char line[64];char *trimmed;uint8_t overflow;
  if(!g_uart_line_ready)
  {
    Serial_RecoverReceive();
    if(!g_uart_line_ready)return;
  }
  __disable_irq();
  memcpy(command,g_uart_line,sizeof(command));
  overflow=g_uart_line_overflow;
  g_uart_line_ready=0U;
  __enable_irq();
  command[sizeof(command)-1U]=0;
  g_uart_line_overflow=0U;
  if(overflow){Serial_Write("Command error: line too long\r\n");return;}
  trimmed=Serial_Trim(command);if(*trimmed==0)return;
  if(strncmp(trimmed,"adcdiag ",8U)==0)
  {
    if(trimmed[8]<'0'||trimmed[8]>'3'||trimmed[9]!=0||
       g_adc_diag_active!=0U||g_dump_remaining!=0U||g_rtc_setting_mode!=0U)
    {
      Serial_Write("ADC DIAG ERROR: use adcdiag 0..3 outside setting/log dump\r\n");
      return;
    }
    g_adc_diag_channel=(uint8_t)(trimmed[8]-'0');
    TIM3->CR1 = 0U;
    g_adc_normal_enabled = 0U;
    TIM3->SR = 0U;
    HAL_NVIC_ClearPendingIRQ(TIM3_IRQn);
    g_adc_diag_auto_channels=0U;
    g_adc_diag_phase=ADC_DIAG_FIRST_PHASE;
    g_adc_diag_block=0U;
    g_adc_diag_count=0U;
    g_adc_diag_phase_ready=0U;
    g_adc_diag_active=1U;
    g_adc_diag_lps_saved=0U;
    (void)snprintf(line, sizeof(line), "ADC:990,8,11,96,20000,%u,0,29,3,0,0,0\r\n",
                   (unsigned)g_adc_diag_channel);
    Serial_Write(line);
    return;
  }
  if(Serial_Equals(trimmed,"setting")){g_rtc_setting_mode=1U;Serial_PrintSettingHelp();return;}
  if(Serial_Equals(trimmed,"exit")){g_dump_remaining=0U;if(g_rtc_setting_mode){g_rtc_setting_mode=0U;Serial_Write("EXIT SETTING MODE\r\n");}return;}
  if(g_rtc_setting_mode)
  {
    if(Serial_Equals(trimmed,"log start") || Serial_Equals(trimmed,"log stop"))
    {
      uint8_t enabled = Serial_Equals(trimmed,"log start") ? 1U : 0U;
      if(DataLogger_SetRecordingEnabled(enabled)==HAL_OK)
      {
        uint32_t primask = __get_PRIMASK();
        __disable_irq();
        if(g_recording_enabled != enabled)
        {
          g_recording_enabled = enabled;
          g_record_due = 0U;
          g_record_last_second = g_record_timer_seconds;
        }
        __set_PRIMASK(primask);
        Serial_Write(enabled ? "LOG RECORDING ON\r\n" : "LOG RECORDING OFF\r\n");
      }
      else { Serial_Write("LOG RECORDING CONFIG ERROR\r\n"); }
    }
    else if(Serial_Equals(trimmed,"log dump"))
    {
      g_dump_remaining = DataLogger_Count();
      g_dump_newest_first = DataLogger_GetDumpNewestFirst();
      g_dump_sequence = g_dump_newest_first ? DataLogger_LastSequence() :
                        DataLogger_LastSequence() - g_dump_remaining + 1U;
    }
    else if(Serial_Equals(trimmed,"log info"))
    {
      char line[128];
      (void)snprintf(line,sizeof(line),"LOG INFO: count=%lu capacity=%lu interval=%lus order=%s recording=%s\r\n",
                     (unsigned long)DataLogger_Count(),
                     (unsigned long)DataLogger_Capacity(),
                     (unsigned long)DataLogger_GetInterval(),
                     DataLogger_GetDumpNewestFirst() ? "newest-first" : "oldest-first",
                     g_recording_enabled ? "on" : "off");
      Serial_Write(line);
    }
    else if(strncmp(trimmed,"log interval ",13U)==0)
    {
      uint32_t seconds;
      if(Serial_ParseUint32(&trimmed[13],&seconds) &&
         DataLogger_SetInterval(seconds,3000U)==HAL_OK)
      {
        g_record_last_second = g_record_timer_seconds;
        Serial_Write("LOG INTERVAL OK\r\n");
      }
      else
      {
        Serial_Write("LOG INTERVAL ERROR: use log interval 1..86400\r\n");
      }
    }
    else if(Serial_Equals(trimmed,"log order old") || Serial_Equals(trimmed,"log order oldest"))
    {
      Serial_Write(DataLogger_SetDumpNewestFirst(0U,3000U)==HAL_OK ?
                  "LOG ORDER OK: oldest-first\r\n" : "LOG ORDER ERROR\r\n");
    }
    else if(Serial_Equals(trimmed,"log order new") || Serial_Equals(trimmed,"log order newest"))
    {
      Serial_Write(DataLogger_SetDumpNewestFirst(1U,3000U)==HAL_OK ?
                  "LOG ORDER OK: newest-first\r\n" : "LOG ORDER ERROR\r\n");
    }
    else if(Serial_Equals(trimmed,"log clear"))
    {
      g_dump_remaining = 0U;
      Serial_Write(DataLogger_Clear(3000U)==HAL_OK ?
                  "LOG CLEAR OK\r\n" : "LOG CLEAR ERROR\r\n");
    }
    else if(strlen(trimmed)>=3U && Serial_ToLower(trimmed[0])=='s' &&
            Serial_ToLower(trimmed[1])=='e' && Serial_ToLower(trimmed[2])=='t')
    {
      Serial_HandleSetTime(trimmed);
    }
    else
    {
      Serial_Write("Unknown setting command\r\n");
    }
  }
}

/* 每次只发送一条历史记录，避免导出过程长时间阻塞采样。 */
static void Serial_DumpLogs(void)
{
  uint16_t year;
  char line[180];
  DataLogger_Sample sample;

  /* 每次主循环只发送一条，不阻塞采样、记录和命令处理。
     固定序号快照：导出期间新增记录不混入本次导出。 */
  if (g_dump_remaining != 0U && g_rtc_setting_mode != 0U)
  {
    uint32_t sequence = g_dump_sequence;
    g_dump_sequence = g_dump_newest_first ? sequence - 1U : sequence + 1U;
    g_dump_remaining--;
    if (DataLogger_ReadSequence(sequence, &sample, 100U) != HAL_OK)
    {
      return;
    }
    year = (uint16_t)(sample.datetime.year +
                      ((sample.datetime.century != 0U) ? 1900U : 2000U));
    (void)snprintf(line, sizeof(line),
                   "samples: %u,%02u,%02u,%02u,%02u,%02u,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\r\n",
                   (unsigned)year, (unsigned)sample.datetime.month,
                   (unsigned)sample.datetime.day, (unsigned)sample.datetime.hours,
                   (unsigned)sample.datetime.minutes, (unsigned)sample.datetime.seconds,
                   (double)sample.temperature_c, (double)sample.humidity_rh,
                   (double)sample.pressure_u7_hpa, (double)sample.pressure_u11_hpa,
                   (double)sample.adc[0], (double)sample.adc[1],
                   (double)sample.adc[2], (double)sample.adc[3]);
    Serial_Write(line);
  }
}

/* 三点中值滤波：抑制 SHT41 偶发的孤立异常值。 */
static float Median3(float a, float b, float c)
{
  float swap;
  if (a > b) { swap = a; a = b; b = swap; }
  if (b > c) { swap = b; b = c; c = swap; }
  if (a > b) { swap = a; a = b; b = swap; }
  return b;
}

/* 三点中值后接50点滑动平均，温湿度独立运算并保留小数。
 * 只接受成功且范围有效的测量；断连超过5秒后重新建立窗口。
 * 串口和Flash使用同一结果；超过5秒未更新时均使用无效标记。 */
static void ShtFilter_Update(HAL_StatusTypeDef status, float temperature, float humidity)
{
  static float temperatures[3];
  static float humidities[3];
  static uint8_t index;
  static float mean_temperatures[SHT_AVG_WINDOW];
  static float mean_humidities[SHT_AVG_WINDOW];
  static float temperature_sum;
  static float humidity_sum;
  static uint8_t mean_count;
  static uint8_t mean_index;
  float median_temperature;
  float median_humidity;
  uint32_t now = HAL_GetTick();
  uint8_t i;
  if (status != HAL_OK || !(temperature >= -40.0f && temperature <= 125.0f) ||
      !(humidity >= 0.0f && humidity <= 100.0f)) { return; }
  if (!g_sht_filter_valid || (now - g_sht_last_valid_tick) > 5000U)
  {
    index = 0U;
    mean_count = 0U;
    mean_index = 0U;
    temperature_sum = 0.0f;
    humidity_sum = 0.0f;
    for (i = 0U; i < 3U; i++)
    {
      temperatures[i] = temperature;
      humidities[i] = humidity;
    }
    g_sht_filter_valid = 1U;
  }
  temperatures[index] = temperature;
  humidities[index] = humidity;
  index = (uint8_t)((index + 1U) % 3U);
  median_temperature = Median3(temperatures[0], temperatures[1], temperatures[2]);
  median_humidity = Median3(humidities[0], humidities[1], humidities[2]);
  if (mean_count == SHT_AVG_WINDOW)
  {
    temperature_sum -= mean_temperatures[mean_index];
    humidity_sum -= mean_humidities[mean_index];
  }
  else { mean_count++; }
  mean_temperatures[mean_index] = median_temperature;
  mean_humidities[mean_index] = median_humidity;
  temperature_sum += median_temperature;
  humidity_sum += median_humidity;
  mean_index = (uint8_t)((mean_index + 1U) % SHT_AVG_WINDOW);
  g_sht_filtered_temperature = temperature_sum / (float)mean_count;
  g_sht_filtered_humidity = humidity_sum / (float)mean_count;
  g_sht_last_valid_tick = now;
}

/* LPS22HH 的有效气压范围约为 260~1260 hPa；这里放宽到 0~2000 hPa，
 * 只过滤通信失败时使用的负无效值和明显异常值。无效采样不进入窗口。 */
static uint8_t Pressure_IsValid(float pressure_hpa)
{
  return (uint8_t)(pressure_hpa > 0.0f && pressure_hpa < 2000.0f);
}

/* Across-time median rejects up to 49 contaminated points per 100 scans.
 * Mean25 smooths only median outputs, never raw conversions. No invalid-code
 * substitution is used for spikes; persistent real steps remain trackable. */
static float AdcFilter_Insert(AdcTemporalFilter *filter, uint16_t sample)
{
  uint16_t temp;
  uint32_t median_twice;
  uint8_t i;
  uint8_t count;

  if (filter == NULL) { return 0U; }
  count = filter->count;
  if (count == ADC_FILTER_WINDOW)
  {
    /* Remove one occurrence of the outgoing raw code from the sorted set. */
    temp = filter->values[filter->next];
    for (i = 0U; i < count && filter->sorted[i] != temp; i++) { }
    for (; i + 1U < count; i++) { filter->sorted[i] = filter->sorted[i + 1U]; }
    count--;
  }
  /* Maintain the sorted window in O(window), avoiding a 100x100 re-sort in
   * the ISR. The ring stores time order; sorted[] stores the same multiset. */
  i = count;
  while (i > 0U && filter->sorted[i - 1U] > sample)
  {
    filter->sorted[i] = filter->sorted[i - 1U];
    i--;
  }
  filter->sorted[i] = sample;
  count++;
  filter->values[filter->next] = sample;
  filter->next = (uint8_t)((filter->next + 1U) % ADC_FILTER_WINDOW);
  filter->count = count;
  median_twice = (uint32_t)filter->sorted[count / 2U] * 2U;
  if ((count & 1U) == 0U)
  {
    median_twice = (uint32_t)filter->sorted[count / 2U] +
                     filter->sorted[count / 2U - 1U];
  }
  /* Start the smoothing mean only after one complete second of samples. */
  if (count < ADC_FILTER_WINDOW)
  {
    return (float)median_twice * 0.5f;
  }
  if (filter->mean_count == 25U)
  {
    filter->mean_sum -= filter->mean_values[filter->mean_next];
  }
  else { filter->mean_count++; }
  filter->mean_values[filter->mean_next] = median_twice;
  filter->mean_sum += median_twice;
  filter->mean_next = (uint8_t)((filter->mean_next + 1U) % 25U);
  return (float)filter->mean_sum / (2.0f * (float)filter->mean_count);
}

/* Snapshot filtered ADC codes at the configured deadline; RTC and flash I/O
 * remain in the main loop so they cannot stretch the 100 Hz timer period. */
static void RecordCaptureDue(void)
{
  uint32_t now = g_record_timer_seconds;
  uint32_t interval = DataLogger_GetInterval();
  uint32_t elapsed;
  uint8_t channel;
  uint32_t primask;
  if (g_recording_enabled == 0U || interval == 0U ||
      (now - g_record_last_second) < interval) { return; }
  /* Never create initial records from zero-initialized buffers. After the
   * first real window, SPI failures retain the last measured filtered code. */
  for (channel = 0U; channel < 4U; channel++)
  {
    if (g_adc_temporal_filters[channel].count < ADC_FILTER_WINDOW) { return; }
  }
  elapsed = now - g_record_last_second;
  g_record_last_second += (elapsed / interval) * interval;
  primask = __get_PRIMASK();
  __disable_irq();
  for (channel = 0U; channel < 4U; channel++)
  {
    g_pending_record.adc[channel] = g_adc_filtered_codes[channel];
  }
  __set_PRIMASK(primask);
  g_record_due = 1U;
}

/* Keep one pending record. If flash takes longer than an interval, the next
 * deadline replaces it with the newest filtered values rather than replaying
 * stale samples as if they were taken on time. */
static void RecordWritePending(void)
{
  DataLogger_Sample sample;
  uint32_t primask;
  uint8_t sht_fresh;
  if (g_recording_enabled == 0U) { g_record_due = 0U; return; }
  if (g_record_due == 0U || g_adc_diag_active != 0U) { return; }
  primask = __get_PRIMASK();
  __disable_irq();
  if (g_record_due == 0U)
  {
    __set_PRIMASK(primask);
    return;
  }
  sample = g_pending_record;
  g_record_due = 0U;
  __set_PRIMASK(primask);
  if (PCF8563_ReadDateTime(&hi2c1, &sample.datetime, 100U) != HAL_OK)
  {
    memset(&sample.datetime, 0, sizeof(sample.datetime));
  }
  sht_fresh = (uint8_t)(g_sht_filter_valid &&
                       (HAL_GetTick() - g_sht_last_valid_tick) <= 5000U);
  sample.temperature_c = sht_fresh ? g_sht_filtered_temperature :
                         SENSOR_INVALID_FLOAT;
  sample.humidity_rh = sht_fresh ? g_sht_filtered_humidity :
                       SENSOR_INVALID_FLOAT;
  sample.pressure_u7_hpa = g_u7_pressure_average.count != 0U ?
      g_u7_pressure_average.sum / (float)g_u7_pressure_average.count : SENSOR_INVALID_FLOAT;
  sample.pressure_u11_hpa = g_u11_pressure_average.count != 0U ?
      g_u11_pressure_average.sum / (float)g_u11_pressure_average.count : SENSOR_INVALID_FLOAT;
  if (g_w25q_ready != 0U && DataLogger_Append(&sample, 3000U) != HAL_OK &&
      g_rtc_setting_mode != 0U)
  {
    Serial_Write("LOG WRITE ERROR\r\n");
  }
}

/* 由 TIM3 ISR 调用。四路每次各执行一次有效转换；驱动在通道切换时
 * 自动丢弃两次流水线结果。正常模式 acquisition=100 us，四路扫描
 * 约 2 ms 以内，远小于 10 ms 周期。即使某次 SPI 失败，也保留上一次
 * 已滤波值，保证串口和 W25Q128 永远得到有效的稳定值。 */
void Application_AdcSample100Hz(void)
{
  uint16_t raw[4];
  uint8_t channel;
  uint8_t all_ok = 1U;

  for (channel = 0U; channel < 4U; channel++)
  {
    raw[channel] = 0U;
    if (AD7682_ReadChannel(&g_ad7682, channel, &raw[channel], 10U) == AD7682_OK)
    {
      g_adc_raw_codes[channel] = raw[channel];
      g_adc_filtered_codes[channel] = AdcFilter_Insert(&g_adc_temporal_filters[channel],
                                                        raw[channel]);
    }
    else
    {
      all_ok = 0U;
    }
  }
      g_adc_normal_status = all_ok != 0U ? AD7682_OK : AD7682_ERROR;
  RecordCaptureDue();
}

static uint8_t Application_AdcGetSnapshot(float filtered[4], uint16_t raw[4])
{
  uint8_t channel;
  uint8_t ready = 1U;
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  for (channel = 0U; channel < 4U; channel++)
  {
    filtered[channel] = g_adc_filtered_codes[channel];
    raw[channel] = g_adc_raw_codes[channel];
    if (g_adc_temporal_filters[channel].count < ADC_FILTER_WINDOW) { ready = 0U; }
  }
  __set_PRIMASK(primask);
  return ready;
}

/* 按最近 50 个有效气压样本计算滑动平均。窗口尚未填满时，按当前有效
 * 样本数计算；传感器暂时失败时保留最近有效平均值。 */
static float PressureAverage_Update(PressureAverage *filter, float pressure_hpa)
{
  if (filter == NULL) { return SENSOR_INVALID_FLOAT; }
  if (Pressure_IsValid(pressure_hpa))
  {
    if (filter->count < PRESSURE_AVG_WINDOW)
    {
      filter->values[filter->next] = pressure_hpa;
      filter->sum += pressure_hpa;
      filter->count++;
    }
    else
    {
      filter->sum -= filter->values[filter->next];
      filter->values[filter->next] = pressure_hpa;
      filter->sum += pressure_hpa;
    }
    filter->next = (uint16_t)((filter->next + 1U) % PRESSURE_AVG_WINDOW);
  }
  if (filter->count == 0U) { return SENSOR_INVALID_FLOAT; }
  return filter->sum / (float)filter->count;
}

/* FireWater 实时数据行：ch0 电池电压，ch1/2 SHT41 温湿度，
 * ch3/4 U7 压力/温度，ch5/6 U11 压力/温度，随后是 AD7682 四通道的
 * 中值结果，最后为 U7 50 点平均、U11 50 点平均、
 * 两路实时平均，末尾追加 IN0～IN3 最新有效原始码。W25Q128 中的压力字段
 * 保存 U7/U11 各自的 50 点平均值。 */
/* 读取一轮所有传感器，按记录周期写 Flash，并按实时格式输出。 */
static void Samples_ProcessAndOutput(void)
{
  uint32_t battery_raw;
  float adc[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  uint16_t adc_raw[4] = {0U, 0U, 0U, 0U};
  float battery_voltage;
  float sht_temperature = SENSOR_INVALID_FLOAT;
  float sht_humidity = SENSOR_INVALID_FLOAT;
  float u7_pressure = SENSOR_INVALID_FLOAT;
  float u7_temperature = SENSOR_INVALID_FLOAT;
  float u11_pressure = SENSOR_INVALID_FLOAT;
  float u11_temperature = SENSOR_INVALID_FLOAT;
  float u7_pressure_average;
  float u11_pressure_average;
  float pressure_average;
  char line[320];
  char adc_text[48];
  HAL_StatusTypeDef sht_status;

  /* ADC 已由 TIM3 独立以 100 Hz 更新；这里仅复制快照，绝不再发起
     会拖慢记录周期的 SPI 采样。 */
  (void)Application_AdcGetSnapshot(adc, adc_raw);

  battery_raw = BatteryAdc_ReadRaw(&hadc1, 10U);
  battery_voltage = BatteryAdc_CalibratedVoltage(battery_raw);
  BatteryLed_Update(battery_voltage);
  /* 即使上电初始化失败，也继续尝试读取；器件可能在上电后稍晚恢复。 */
  sht_status = SHT41_Read(&hi2c1, &sht_temperature, &sht_humidity, 100U);
  ShtFilter_Update(sht_status, sht_temperature, sht_humidity);
  if (g_sht_filter_valid &&
      (HAL_GetTick() - g_sht_last_valid_tick) <= 5000U)
  {
    sht_temperature = g_sht_filtered_temperature;
    sht_humidity = g_sht_filtered_humidity;
  }
  else
  {
    sht_temperature = SENSOR_INVALID_FLOAT;
    sht_humidity = SENSOR_INVALID_FLOAT;
  }
  /* 不用上电初始化结果永久锁死采样，允许后续读取恢复。 */
  if (LPS22HH_ReadDevice(&hi2c1, &g_lps22hh_u7,
                         &u7_pressure, &u7_temperature, 100U) != HAL_OK)
  {
    u7_pressure = SENSOR_INVALID_FLOAT;
    u7_temperature = SENSOR_INVALID_FLOAT;
  }
  if (LPS22HH_ReadDevice(&hi2c1, &g_lps22hh_u11,
                         &u11_pressure, &u11_temperature, 100U) != HAL_OK)
  {
    u11_pressure = SENSOR_INVALID_FLOAT;
    u11_temperature = SENSOR_INVALID_FLOAT;
  }
  u7_pressure_average = PressureAverage_Update(&g_u7_pressure_average,
                                                u7_pressure);
  u11_pressure_average = PressureAverage_Update(&g_u11_pressure_average,
                                                 u11_pressure);
  pressure_average = (Pressure_IsValid(u7_pressure) &&
                      Pressure_IsValid(u11_pressure)) ?
                     ((u7_pressure + u11_pressure) * 0.5f) :
                     SENSOR_INVALID_FLOAT;
  /* Serve the timer-latched record before formatting/transmitting telemetry. */
  RecordWritePending();
  /* Do not print power-up zeros or partially warmed filters as measurements. */
  if (Application_AdcGetSnapshot(adc, adc_raw) == 0U) { return; }
  (void)snprintf(adc_text, sizeof(adc_text),
                 "%.2f,%.2f,%.2f,%.2f", (double)adc[0], (double)adc[1],
                 (double)adc[2], (double)adc[3]);

  /* 采样或 Flash 写入期间到达的命令，在下一行实时输出前处理。 */
  Serial_ProcessCommand();
  if (g_rtc_setting_mode != 0U || g_adc_diag_active != 0U)
  {
    return;
  }
  (void)snprintf(line, sizeof(line), "samples: %.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%s,%.3f,%.3f,%.3f,%u,%u,%u,%u\r\n",
                 (double)battery_voltage, (double)sht_temperature,
                 (double)sht_humidity, (double)u7_pressure,
                 (double)u7_temperature, (double)u11_pressure,
                 (double)u11_temperature, adc_text,
                 (double)u7_pressure_average,
                 (double)u11_pressure_average,
                 (double)pressure_average,
                 (unsigned)adc_raw[0], (unsigned)adc_raw[1],
                 (unsigned)adc_raw[2], (unsigned)adc_raw[3]);
  Serial_Write(line);
}

/* USER CODE END 4 */

/**
  * @brief  发生错误时执行此函数。
  * @retval 无
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* 可在此添加用户代码，用于记录 HAL 错误返回状态。 */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  报告 assert_param 错误所在的源文件名和行号。
  * @param  file: 指向源文件名的指针。
  * @param  line: assert_param 错误所在的源代码行号。
  * @retval 无
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* 可在此添加用户代码，输出文件名和行号，例如：
     printf("参数值错误：文件 %s，第 %d 行\r\n", file, line)。 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
