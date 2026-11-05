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
#include "ssd1306.h"
#include "uart_crc.h"
#include <stdio.h>
#include <string.h>

#define BOOT_INIT_RETRIES 3U       /* 外部器件上电探测次数。 */
#define BOOT_INIT_TIMEOUT 100U     /* 单次器件访问超时，单位 ms。 */
#define SENSOR_INVALID_FLOAT (-9999.0f)
#define SENSOR_INVALID_ADC   0xFFFFU
#define AD7682_REFERENCE_VOLTS 5.0f
#define UART_RX_DMA_SIZE 128U
#define PRESSURE_AVG_WINDOW  50U
#define SHT_AVG_WINDOW       50U
#define BATTERY_AVG_WINDOW   50U
#define ADC_DIAG_ROWS        96U
#define ADC_DIAG_FIRST_PHASE 29U
#define ADC_DIAG_PHASES      40U /* 结束值不包含在范围内；本轮使用阶段 29～39。 */
#define ADC_DIAG_BLOCKS      3U
#define ADC_DIAG_PERIOD_US   20000U
#define ADC_FILTER_WINDOW    100U
/* 100 Hz 时 alpha = 1-exp(-0.01/0.2)，预先计算以避免在中断中调用 exp()。 */
#define ADC_LOWPASS_ALPHA    0.0487705755f
#define ADC_PROBE_POINTS     288U
#define ADC_PROBE_PHASES     6U
#define ADC_PROBE_BLOCKS     3U

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
  uint32_t cnv_us;
  uint16_t raw;
  uint16_t status;
} AdcProbePoint;

typedef struct
{
  uint16_t values[ADC_FILTER_WINDOW];
  uint16_t sorted[ADC_FILTER_WINDOW];
  float lowpass1;
  float lowpass2;
  uint8_t count;
  uint8_t next;
  uint8_t lowpass_ready;
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
volatile uint8_t g_uart_line_ready;
volatile uint8_t g_uart_line_overflow;
volatile uint8_t g_uart_rx_restart;
volatile uint8_t g_rtc_setting_mode;
static char g_uart_line[64];
static uint8_t g_uart_line_length;
static volatile uint8_t g_uart_dma_buffer[UART_RX_DMA_SIZE];
static uint16_t g_uart_dma_read;
static volatile uint32_t g_uart_dma_wraps;
static uint32_t g_uart_dma_consumed;
static uint32_t g_uart_rx_count;
static volatile uint32_t g_uart_parity_errors;
static volatile uint32_t g_uart_frame_errors;
static volatile uint32_t g_uart_overrun_errors;
static volatile uint32_t g_uart_noise_errors;
volatile uint32_t g_record_timer_seconds;
volatile uint32_t g_record_timer_milliseconds;
volatile uint16_t g_sensor_timer_milliseconds;
volatile uint16_t g_sensor_sample_elapsed_ms;
volatile uint8_t g_sensor_sample_request;
/* ADC 正常采样由 TIM3 每 10 ms 触发；诊断期间保持关闭。 */
volatile uint8_t g_adc_normal_enabled;
volatile float g_adc_filtered_codes[4];
volatile uint16_t g_adc_raw_codes[4];
volatile AD7682_Status g_adc_normal_status;
static uint32_t g_record_last_millisecond;
static volatile uint8_t g_recording_enabled;
static uint32_t g_dump_remaining;
static uint32_t g_dump_sequence;
static uint8_t g_dump_newest_first;
static uint8_t g_dump_time_filtered;
static uint8_t g_dump_hpa;
static uint32_t g_dump_start_time;
static uint32_t g_dump_end_time;
static uint32_t g_dump_matched;
static uint32_t g_dump_read_errors;
static uint8_t g_dump_report_pending;
static uint8_t g_dump_exclusive;
static uint8_t g_dump_adc_was_running, g_dump_scheduler_was_running;
static uint32_t g_dump_pause_tick;
/* Flash checkpoints allow replay without caching sixteen text rows in RAM. */
static uint32_t g_window_dump_sequence, g_window_dump_remaining;
static uint32_t g_window_dump_matched, g_window_dump_errors;
static uint16_t g_window_scanned;
static float g_sht_filtered_temperature;
static float g_sht_filtered_humidity;
static uint8_t g_sht_filter_valid;
static uint32_t g_sht_last_valid_tick;
static AD7682_Handle g_ad7682;
static AD7682_Status g_adc_status = AD7682_ERROR;
static uint8_t g_w25q_ready;
static PressureAverage g_u7_pressure_average;
static PressureAverage g_u11_pressure_average;
/* 诊断采集和正常滤波不会同时运行；复用
 * 二者的内存，使四路 100 点滤波器能装入 STM32G031 的 8 KiB SRAM。 */
static union
{
  AdcDiagRow diagnostic[ADC_DIAG_ROWS];
  AdcProbePoint probe[ADC_PROBE_POINTS];
  AdcTemporalFilter filters[4];
} g_adc_workspace;
#define g_adc_diag_rows (g_adc_workspace.diagnostic)
#define g_adc_temporal_filters (g_adc_workspace.filters)
static uint8_t g_adc_diag_active;
static uint8_t g_adc_probe_active;
static uint32_t g_adc_probe_random;
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
static float g_battery_nominal;
static uint32_t g_battery_valid_tick;
static uint8_t g_battery_valid;
static uint8_t g_ui_active;
static uint8_t g_ui_selected;
static uint8_t g_ui_edit_interval;
static uint8_t g_ui_clear_confirm;
static uint8_t g_ui_display;
static uint8_t g_ui_recording;
static uint8_t g_ui_order;
static uint32_t g_ui_interval;
static uint8_t g_ui_dirty;
static const char *g_ui_message;
volatile uint8_t g_ui_key_events;

/* USER CODE END PV */

/* 私有函数声明。 */
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void Serial_Write(const char *text);
static void Devices_Initialize(void);
static void Samples_ProcessAndOutput(void);
static void Serial_DumpLogs(void);
static void RecordTimer_Init(void);
static void Serial_DumpPauseSampling(void);
static void Serial_DumpResumeSampling(void);
static uint8_t SensorSample_ConsumeRequest(void);
static void AdcDiag_ProcessRequest(void);
static void AdcProbe_ProcessRequest(void);
static void AdcDiag_DumpPhase(uint8_t phase);
static void AdcDiag_StartPhase(void);
static void AdcDiag_StopPhase(void);
static uint8_t AdcDiag_SetCoreClock(uint8_t mode);
static uint8_t AdcDiag_SetLpsMode(uint8_t power_down_mask);
static uint8_t AdcDiag_SetEdgeSpeed(uint8_t high_mask);
static void Serial_ProcessCommand(void);
static void Serial_RecoverReceive(void);
static HAL_StatusTypeDef Serial_StartReceive(void);
static void Serial_PollReceive(void);
static void BatteryLed_Update(float voltage);
static void Ui_Process(void);
static void Recording_ApplySavedConfig(void);
static float BatteryFilter_Update(HAL_StatusTypeDef status, uint32_t raw);
static void ShtFilter_Update(HAL_StatusTypeDef status, float temperature, float humidity);
static float PressureAverage_Update(PressureAverage *filter, float pressure_hpa);
static uint8_t Pressure_IsValid(float pressure_hpa);
static void RecordCaptureDue(void);
static void RecordWritePending(void);
void Application_AdcSample1000Hz(void);
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
/**
 * @brief 初始化时钟和外设，运行采样、记录、界面与串口命令调度；导出期间进入独占处理分支。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
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
  /* OLED 与 Flash 共用 SPI2；GPIO 初始化已将两路片选置为无效。
   * 这个只写 SPI 接口没有显示屏应答信号。 */
  {
    HAL_StatusTypeDef oled_status;
    uint8_t attempt;
    for (attempt = 0U; attempt < 3U; attempt++)
    {
      oled_status = SSD1306_Init(&hspi2);
      if (oled_status == HAL_OK) { break; }
    }
    if (oled_status == HAL_OK)
    {
      /* 启动 ADC 中断或等待滤波器之前，先检查
       * 显示通路；SPI 为只写接口，发送成功不代表 OLED 已应答。 */
      (void)SSD1306_BeginFrame(HAL_GetTick());
      SSD1306_SetLine(0U, "SSD1306 READY");
      SSD1306_SetLine(2U, "WAITING FOR SENSORS");
      for (attempt = 0U; attempt < 8U; attempt++)
      {
        HAL_Delay(5U);
        SSD1306_Process(HAL_GetTick());
      }
      Serial_Write("OLED SPI TX OK (NO ACK)\r\n");
    }
    else { Serial_Write("OLED SPI INIT TX ERROR\r\n"); }
  }
  RecordTimer_Init();
  /* 启动阶段的串口输出完成后再开启命令接收。清理 RX 残留和错误状态，
     确保复位后的第一条命令从完整的空闲接收状态开始。 */
  __HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_OREF | UART_CLEAR_NEF |
                                  UART_CLEAR_PEF | UART_CLEAR_FEF);
  __HAL_UART_SEND_REQ(&huart1, UART_RXDATA_FLUSH_REQUEST);
  huart1.ErrorCode = HAL_UART_ERROR_NONE;
  if (Serial_StartReceive() != HAL_OK) { Serial_Write("UART RX START ERROR\r\n"); }
  Serial_Write("UART RX DMA128 READY: 921600 8E1 (2026-11-04)\r\n");
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
  g_record_last_millisecond = g_record_timer_milliseconds;
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
      if (g_adc_probe_active != 0U) { AdcProbe_ProcessRequest(); }
      else { AdcDiag_ProcessRequest(); }
      continue;
    }
    RecordWritePending();
    Serial_ProcessCommand();
    UartCrc_Process();
    if (UartCrc_IsFailed()) { g_dump_remaining = 0U; g_dump_report_pending = 0U; }
    if (g_dump_exclusive)
    {
      /* Dedicated export loop: no ADC/sensor/UI/recording work. Keep RX DMA,
       * UART commands and SysTick timeout processing alive on every row. */
      Serial_DumpLogs();
      if (UartCrc_IsFailed() || (g_dump_remaining == 0U && !g_dump_report_pending &&
          (!UartCrc_IsEnabled() || !UartCrc_IsBusy())))
      { Serial_DumpResumeSampling(); }
      continue;
    }
    if (g_adc_diag_active == 0U)
    {
      uint8_t burst;
      uint32_t burst_start = HAL_GetTick();
      /* Give export a bounded burst rather than one row per sensor loop.
       * ISR sampling stays enabled; drain records/commands between rows. */
      for (burst = 0U; burst < 16U; burst++)
      {
        Serial_DumpLogs();
        RecordWritePending();
        Serial_ProcessCommand();
        UartCrc_Process();
        if (!UartCrc_WindowEnabled() || UartCrc_IsBusy() || UartCrc_IsFailed() ||
            g_dump_remaining == 0U || HAL_GetTick() - burst_start >= 12U) { break; }
      }
    }
    if (g_adc_diag_active == 0U) { Ui_Process(); }
    /* 在阻塞式传感器读取前后各发送一个页面，防止
     * 连续的 20 ms 传感器请求使一帧绘制延长到八次循环。 */
    if (g_adc_diag_active == 0U && g_record_due == 0U && g_dump_remaining == 0U)
    {
      SSD1306_Process(HAL_GetTick());
    }
    if (SensorSample_ConsumeRequest() != 0U && g_adc_diag_active == 0U)
    {
      /* setting 模式只关闭实时串口输出，传感器采样和历史记录仍持续运行。 */
      Samples_ProcessAndOutput();
    }
    RecordWritePending();
    /* 日志和命令优先，诊断采集期间必须保持安静。 */
    if (g_adc_diag_active == 0U && g_record_due == 0U &&
        g_dump_remaining == 0U)
    {
      SSD1306_Process(HAL_GetTick());
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief 配置系统时钟。
  * @retval 无
  */
/**
 * @brief 配置系统时钟及总线分频；初始化失败时进入 Error_Handler。
 * @return 无返回值。
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

/* 统一的阻塞式串口发送入口；所有输出均由主循环调用。
 * UART 硬件报告发送失败时最多重发一次；奇偶校验只能由接收端检测。 */
/**
 * @brief 按当前链路模式发送文本；采样行与完成信息走 CRC 协议，其余文本直接发送。
 * @param text 以零结尾的文本或命令字符串。
 * @return 无返回值。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static void Serial_Write(const char *text)
{
  HAL_StatusTypeDef status;
  uint16_t length = (uint16_t)strlen(text);
  if (UartCrc_IsEnabled() && strncmp(text, "samples:", 8U) == 0)
  {
    (void)UartCrc_Send(text);
    return;
  }
  status = HAL_UART_Transmit(&huart1, (uint8_t *)text, length, 100U);
  if (status != HAL_OK)
  {
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)text, length, 100U);
  }
}

/* TIM14 内部时钟，无外部引脚。APB 分频为 1 时 TIMCLK=PCLK，
 * 否则 TIMCLK=2*PCLK；分频至 10 kHz，每 10 个计数产生一次 1 ms
 * 中断。中断同时维护记录秒计数，并按 SENSOR_SAMPLE_PERIOD_MS 置位
 * 其他传感器请求。ADC 单独使用 TIM3，I2C/SPI2/串口留在主循环。 */
/**
 * @brief 配置 TIM3 的 ADC 采样节拍与 TIM14 的毫秒调度节拍并启用中断。
 * @return 无返回值。
 */
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
  /* 诊断结束后才启动，避免其中断干扰测试。 */

  __HAL_RCC_TIM14_CLK_ENABLE();
  TIM14->CR1 = 0U;
  TIM14->PSC = clock / 10000U - 1U;
  TIM14->ARR = 9U;
  TIM14->EGR = TIM_EGR_UG;
  TIM14->SR = 0U;
  g_record_timer_seconds = 0U;
  g_record_timer_milliseconds = 0U;
  g_sensor_timer_milliseconds = 0U;
  g_sensor_sample_elapsed_ms = 0U;
  g_sensor_sample_request = 0U;
  g_record_last_millisecond = 0U;
  g_record_due = 0U;
  TIM14->DIER = TIM_DIER_UIE;
  HAL_NVIC_SetPriority(TIM14_IRQn, 0U, 0U);
  HAL_NVIC_ClearPendingIRQ(TIM14_IRQn);
  HAL_NVIC_EnableIRQ(TIM14_IRQn);
  TIM14->CR1 = TIM_CR1_CEN;
  /* SysTick 必须能抢占约 1.7 ms 的 ADC 扫描，否则多个
   * 1 ms 滴答会合并，使 HAL 超时和 SHT41 转换等待变慢。 */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0U, 0U);
}

/* 原子地取出一次定时器产生的采样请求。若一次采集超过周期，
 * 多个周期会合并为一个请求，避免采集完成后出现突发追赶。 */
/**
 * @brief 在临界区取走并清除慢速传感器采样请求，避免主循环与中断竞争。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 */
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

/**
 * @brief 保存运行状态，停止 TIM3/TIM14 采样调度并暂停普通任务，进入独占导出。
 * @return 无返回值。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static void Serial_DumpPauseSampling(void)
{
  uint32_t primask;
  if (g_dump_exclusive) { return; }
  /* Commit the final sample captured before the pause, before the snapshot. */
  RecordWritePending();
  primask = __get_PRIMASK();
  __disable_irq();
  g_dump_adc_was_running = (uint8_t)((TIM3->CR1 & TIM_CR1_CEN) != 0U);
  g_dump_scheduler_was_running = (uint8_t)((TIM14->CR1 & TIM_CR1_CEN) != 0U);
  TIM3->CR1 &= (uint32_t)~TIM_CR1_CEN;
  TIM14->CR1 &= (uint32_t)~TIM_CR1_CEN;
  TIM3->SR = TIM14->SR = 0U;
  HAL_NVIC_ClearPendingIRQ(TIM3_IRQn);
  HAL_NVIC_ClearPendingIRQ(TIM14_IRQn);
  g_record_due = g_sensor_sample_request = 0U;
  g_dump_pause_tick = HAL_GetTick();
  g_dump_exclusive = 1U;
  __set_PRIMASK(primask);
  Serial_Write("LOG DUMP SAMPLING: PAUSED\r\n");
}

/**
 * @brief 退出独占导出，恢复调度、重建滤波窗口并请求新数据，不补采导出期间的数据。
 * @return 无返回值。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static void Serial_DumpResumeSampling(void)
{
  uint32_t elapsed, subsecond, primask;
  if (!g_dump_exclusive) { return; }
  primask = __get_PRIMASK();
  __disable_irq();
  elapsed = HAL_GetTick() - g_dump_pause_tick;
  subsecond = g_sensor_timer_milliseconds + elapsed;
  g_record_timer_milliseconds += elapsed;
  g_record_timer_seconds += subsecond / 1000U;
  g_sensor_timer_milliseconds = (uint16_t)(subsecond % 1000U);
  /* Do not backfill the pause with stale values. Rebuild ADC and pressure
   * filters and obtain fresh slow sensors before the next logged interval. */
  g_record_last_millisecond = g_record_timer_milliseconds;
  g_record_due = 0U;
  g_sensor_sample_elapsed_ms = 0U;
  g_sensor_sample_request = 1U;
  memset(g_adc_temporal_filters, 0, sizeof(g_adc_temporal_filters));
  memset(&g_u7_pressure_average, 0, sizeof(g_u7_pressure_average));
  memset(&g_u11_pressure_average, 0, sizeof(g_u11_pressure_average));
  g_sht_filter_valid = 0U;
  TIM3->CNT = TIM14->CNT = 0U;
  TIM3->SR = TIM14->SR = 0U;
  HAL_NVIC_ClearPendingIRQ(TIM3_IRQn);
  HAL_NVIC_ClearPendingIRQ(TIM14_IRQn);
  if (g_dump_adc_was_running) { TIM3->CR1 |= TIM_CR1_CEN; }
  if (g_dump_scheduler_was_running) { TIM14->CR1 |= TIM_CR1_CEN; }
  g_dump_exclusive = 0U;
  __set_PRIMASK(primask);
  Serial_Write("LOG DUMP SAMPLING: RESUMED\r\n");
}

/* 仅在两次诊断采集之间、TIM3 停止时使用。
 * 模式：0=PLL 64 MHz，1=PLL/AHB2 32 MHz，2=HSI 16 MHz 且 PLL 开启，
 * 3=HSI 16 MHz 且 PLL 关闭；所有模式下 HSI 自身都必须保持开启。
 * HAL 会重新校准 SysTick，还需显式重新校准三个硬件定时器
 * 和串口。TIM2 仅在阶段边界、产生任何时间戳之前复位。
 * 状态位：RCC=1、串口初始化=2、接收启动=4、SPI2 初始化=8、PLL=16。 */
/**
 * @brief 诊断时切换 MCU 时钟方案，并更新受影响的外设计时配置。
 * @param mode 硬件诊断模式编号。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 */
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
  /* 等待振荡器就绪时必须开启 SysTick 中断，确保即使时钟未就绪，HAL
   * 仍能有限超时。此 HAL 中关闭 PLL
   * 会清除源选择，因此必须显式恢复所有启动时的
   * PLL 参数，不能读回已清除的配置。 */
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
  /* 所有频率下都保持两个 Flash 等待周期。 */
  if (HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_2) != HAL_OK) { status |= 1U; }
  if (HAL_RCC_GetHCLKFreq() != target) { status |= 1U; }
  if (mode == 3U)
  {
    /* 确认 HSI 已成为 CPU 时钟源之前，不得停止 PLL。 */
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

  /* USART1 使用 PCLK1 时钟，发送元数据之前重新计算 BRR；
   * 不能使用切换时钟前的波特率分频值发送。 */
  if (HAL_UART_Init(&huart1) != HAL_OK) { status |= 2U; }
  __HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_OREF | UART_CLEAR_NEF |
                                  UART_CLEAR_PEF | UART_CLEAR_FEF);
  __HAL_UART_SEND_REQ(&huart1, UART_RXDATA_FLUSH_REQUEST);
  HAL_NVIC_ClearPendingIRQ(USART1_IRQn);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
  if (Serial_StartReceive() != HAL_OK)
  { status |= 4U; }
  hspi2.Init.BaudRatePrescaler = HAL_RCC_GetPCLK1Freq() == 16000000U ?
                               SPI_BAUDRATEPRESCALER_8 :
                               HAL_RCC_GetPCLK1Freq() == 32000000U ?
                               SPI_BAUDRATEPRESCALER_16 : SPI_BAUDRATEPRESCALER_32;
  if (HAL_SPI_Init(&hspi2) != HAL_OK) { status |= 8U; }
  return status;
}

/* 依据 LPS22HH 数据手册第 2 版第 43 页，CTRL_REG1(10h) 的 ODR[6:4]=000 会停止
 * 自主转换，同时保留其他控制位和 I2C 访问能力。
 * 每轮诊断保存一次各寄存器原值，并校验每次
 * 写入；结果的 bit0/1 分别表示 U7/U11 设置或读回失败。
 * 由于软件 I2C 延时依赖 CPU 周期，必须在 64 MHz 下调用。 */
/**
 * @brief 按掩码暂停两颗 LPS22HH，保存或恢复控制寄存器用于诊断对比。
 * @param power_down_mask 气压计停机选择掩码。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 */
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

/* 只修改 PA1=SCK、PA7=DIN、PA5=CNV 的 OSPEEDR 字段。
 * 不要重新初始化 GPIO 模式或复用功能，也不要修改由 ADC 驱动 SDO/MISO 的 PA6。
 * STM32G0 HAL 中 LOW=00、HIGH=10；这里不改变输出电平或时钟频率。
 * high_mask 中 bit0=SCK、bit1=DIN、bit2=CNV；仅在采集之外调用。 */
/**
 * @brief 按掩码调整诊断所用 GPIO 的输出边沿速度。
 * @param high_mask GPIO 高速边沿选择掩码。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 */
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

/**
 * @brief 将指定诊断阶段的采样缓存和统计数据发送至串口。
 * @param phase 诊断阶段编号。
 * @return 无返回值。
 */
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
/**
 * @brief 设置当前诊断阶段的硬件条件并启动该阶段采集。
 * @return 无返回值。
 */
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

  /* 降低 CPU 频率之前先应用传感器控制，所有总线事务都在
   * 采集之外执行，并通过稳定延时排除切换瞬态。 */
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
  /* 在 SPI 初始化后应用，确保即使 MSP 重新初始化也不会覆盖
   * 所需的边沿设置；预热、配置和采集均使用该设置。 */
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
/**
 * @brief 停止诊断阶段采集，输出结果并恢复阶段修改的硬件状态。
 * @return 无返回值。
 */
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
    g_record_timer_milliseconds = g_record_timer_seconds * 1000U + ms % 1000U;
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
  /* 在输出缓冲数据以及进入下一阶段或正常模式之前恢复。
   * 所有采集时间戳均已保存在行缓冲区中。 */
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

/* 仅应用层探测：复用现有单通道原始采集驱动和
 * 工作区，不改变参考源、GPIO 速度、SPI 时钟或转换时序。
 * 采集时不进行串口发送、I2C 或 Flash 访问，SysTick、TIM14 和接收保持开启。 */
/**
 * @brief 计算单通道探测下一次转换的间隔，支持固定、伪随机及突发节奏。
 * @return 下一次转换的调度间隔，单位 µs。
 */
static uint32_t AdcProbe_NextPeriod(void)
{
  if (g_adc_diag_phase == 1U)
  {
    g_adc_probe_random = g_adc_probe_random * 1664525U + 1013904223U;
    return 5000U + ((g_adc_probe_random >> 8) % 10001U);
  }
  if (g_adc_diag_phase == 2U) { return 1000U; }
  if (g_adc_diag_phase == 3U) { return 4000U; }
  if (g_adc_diag_phase == 4U)
  {
    return (g_adc_diag_count % 5U) == 0U ? 46000U : 1000U;
  }
  return 10000U;
}

/**
 * @brief 结束单通道探测，恢复正常采样状态并输出结束标记。
 * @return 无返回值。
 */
static void AdcProbe_Finish(void)
{
  /* 原始采集占用了滤波器内存，重新开启中断前先清空；
   * 正常输出和记录与上电时一样，等待新的 100 点窗口建立。 */
  memset(&g_adc_workspace, 0, sizeof(g_adc_workspace));
  memset((void *)g_adc_filtered_codes, 0, sizeof(g_adc_filtered_codes));
  memset((void *)g_adc_raw_codes, 0, sizeof(g_adc_raw_codes));
  g_ad7682.acquisition_delay_us = 100U;
  g_ad7682.configured_channel_valid = 0U;
  g_adc_normal_status = AD7682_ERROR;
  g_record_due = 0U;
  g_record_last_millisecond = g_record_timer_milliseconds;
  g_sensor_sample_request = 0U;
  g_sensor_sample_elapsed_ms = 0U;
  g_adc_probe_active = 0U;
  g_adc_diag_active = 0U;
  Serial_Write("ADC:999,9,0,0,0,0,0,0,0,0,0,0\r\n");
  g_adc_normal_enabled = 1U;
  TIM3->CNT = 0U;
  TIM3->SR = 0U;
  HAL_NVIC_ClearPendingIRQ(TIM3_IRQn);
  TIM3->CR1 = TIM_CR1_CEN;
}

/**
 * @brief 在主循环处理单通道探测请求，分块采集原始 ADC 与 CNV 时间戳后输出。
 * @return 无返回值。
 */
static void AdcProbe_ProcessRequest(void)
{
  AdcProbePoint *point;
  uint32_t now;
  uint32_t period;
  uint16_t raw = SENSOR_INVALID_ADC;
  uint16_t i;
  AD7682_Status status;
  char line[128];

  if (g_adc_diag_phase_ready == 0U)
  {
    g_ad7682.acquisition_delay_us = 100U;
    /* 定时采集前先建立所选通道。 */
    g_ad7682.configured_channel_valid = 0U;
    status = AD7682_ReadChannel(&g_ad7682, g_adc_diag_channel, &raw, 10U);
    (void)snprintf(line, sizeof(line),
      "ADC:991,%u,%u,%u,%u,100,%u,%lu,%lu,0,0,0\r\n",
      (unsigned)(g_adc_diag_channel * 100U + 40U + g_adc_diag_phase),
      (unsigned)g_adc_diag_block + 1U, (unsigned)ADC_PROBE_POINTS,
      (unsigned)g_adc_diag_phase, (unsigned)status,
      (unsigned long)HAL_RCC_GetHCLKFreq(),
      (unsigned long)(HAL_RCC_GetPCLK1Freq() /
                     (2UL << ((SPI1->CR1 & SPI_CR1_BR) >> 3U))));
    Serial_Write(line);
    /* 进入安静保护时间之前发送已完成，外设供电保持不变。 */
    HAL_Delay(250U);
    g_adc_diag_count = 0U;
    g_adc_diag_phase_start_us = AD7682_TimeUs();
    g_adc_diag_next_us = g_adc_diag_phase_start_us + 1000U;
    g_adc_diag_phase_ready = 1U;
  }
  now = AD7682_TimeUs();
  if ((int32_t)(now - g_adc_diag_next_us) < 0) { return; }
  point = &g_adc_workspace.probe[g_adc_diag_count];
  point->status = (uint32_t)(now - g_adc_diag_next_us) > 100U ? 256U : 0U;
  status = AD7682_ReadChannel(&g_ad7682, g_adc_diag_channel, &raw, 10U);
  point->status |= (uint16_t)status;
  point->raw = status == AD7682_OK ? raw : SENSOR_INVALID_ADC;
  point->cnv_us = (status == AD7682_OK ? g_ad7682.last_cnv_us : now) -
                    g_adc_diag_phase_start_us;
  g_adc_diag_count++;
  period = AdcProbe_NextPeriod();
  g_adc_diag_next_us += period;
  now = AD7682_TimeUs();
  if ((int32_t)(now - g_adc_diag_next_us) >= 0)
  {
    point->status |= 256U;
    g_adc_diag_next_us = now + period;
  }
  if (g_adc_diag_count < ADC_PROBE_POINTS) { return; }

  /* 整块连续数据采集完成后才导出；每个数据块都有
   * 独立时间起点，不得跨串口导出或安静间隔推算时间。 */
  for (i = 0U; i < ADC_PROBE_POINTS; i++)
  {
    point = &g_adc_workspace.probe[i];
    (void)snprintf(line, sizeof(line),
      "ADC:%u,%lu,%u,%u,%u,%u,%u,%u,0,0,0,0\r\n",
      (unsigned)(g_adc_diag_channel * 100U + 40U + g_adc_diag_phase),
      (unsigned long)point->cnv_us, (unsigned)point->raw,
      (unsigned)point->status, (unsigned)i, (unsigned)g_adc_diag_channel,
      (unsigned)g_adc_diag_phase, (unsigned)g_adc_diag_block + 1U);
    Serial_Write(line);
  }
  g_adc_diag_phase_ready = 0U;
  g_adc_diag_block++;
  if (g_adc_diag_block >= ADC_PROBE_BLOCKS)
  {
    g_adc_diag_block = 0U;
    g_adc_diag_phase++;
    if (g_adc_diag_phase >= ADC_PROBE_PHASES) { AdcProbe_Finish(); }
  }
}

/**
 * @brief 在主循环推进多阶段 ADC 诊断状态机，执行阶段切换与恢复。
 * @return 无返回值。
 */
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
    /* 仅改变组间活动，不改变 ADC 传输本身。
     * WFI 至少每 1 ms 唤醒一次，最后 2 ms 使用轮询以精确
     * 控制触发时刻；主动运行的工作量也保持短小且有界。 */
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
    /* 每个数据块独立采集，具有自己的时间戳起点。
     * 停止采样后再导出，避免串口活动干扰
     * 下一个数据块；995 事件用于标识同一标签下的重复数据块。 */
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
        g_record_last_millisecond = g_record_timer_milliseconds;
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
/**
 * @brief 初始化各传感器、RTC、Flash 日志和 OLED，并报告设备状态。
 * @return 无返回值。
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
    /* AD7682 外部参考电压为 5 V，与 MCU 的 VDDA（3.3 V）独立。 */
    g_adc_status = AD7682_Init(&g_ad7682, &hspi1, AD7682_REFERENCE_VOLTS);
    if (g_adc_status == AD7682_OK) { break; }
    HAL_Delay(10U);
  }
  Serial_Write(g_adc_status == AD7682_OK ? "AD7682 INIT OK\r\n" :
                                        "AD7682 INIT ERROR\r\n");
  Serial_Write("AD7682: 4CH 100Hz, DUMMY2+1, TEMPORAL MEDIAN100+LOWPASS, FILTERED LOG\r\n");

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

/* PA15/LED2：首次有效电压按5.5V分界识别1S/2S并锁定至复位。
 * 5.5V位于正常1S最高4.2V与正常2S放电电压之间。
 * 仅凭电压无法可靠识别深度过放的2S电池。
 * 1S >=4.0V常亮、3.7~4.0V闪烁；2S阈值对应8.0/7.4V。 */
/**
 * @brief 根据电池电压更新状态指示灯及对应闪烁状态。
 * @param voltage 电压值，单位 V；指针形式用于接收输出。
 * @return 无返回值。
 */
static void BatteryLed_Update(float voltage)
{
  static uint32_t last_toggle_tick;
  static uint8_t blink_state;
  static uint8_t cell_count;
  uint32_t now = HAL_GetTick();
  float high_threshold;
  float low_threshold;

  /* 等待启动读取成功且数值合理后再锁存。 */
  if (!(voltage >= 0.0f)) { return; }
  if (cell_count == 0U)
  {
    if (voltage < 2.5f || voltage > 8.6f) { return; }
    cell_count = voltage >= 5.5f ? 2U : 1U;
  }
  high_threshold = 4.0f * (float)cell_count;
  low_threshold = 3.7f * (float)cell_count;
  if (voltage >= high_threshold)
  {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
    blink_state = 0U;
    last_toggle_tick = now;
  }
  else if (voltage >= low_threshold)
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

/* ADC 屏蔽 CPU 中断时 DMA 仍会读取 RDR；半传输和全传输回调
 * 不解析命令，由主循环消费循环缓冲区。 */
/**
 * @brief 启动 USART1 循环 DMA 接收，将输入存入接收环形缓冲区。
 * @return HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static HAL_StatusTypeDef Serial_StartReceive(void)
{
  g_uart_dma_read = 0U;
  g_uart_dma_wraps = 0U;
  g_uart_dma_consumed = 0U;
  return HAL_UART_Receive_DMA(&huart1, (uint8_t *)g_uart_dma_buffer, UART_RX_DMA_SIZE);
}

/**
 * @brief 处理 HAL 接收完成回调，核对串口实例并维护接收状态。
 * @param huart 触发回调的 UART 句柄。
 * @return 无返回值。
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart1) { g_uart_dma_wraps++; }
}

/**
 * @brief 在主循环读取 DMA 新到字节，按换行组装命令并检测接收覆盖。
 * @return 无返回值。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static void Serial_PollReceive(void)
{
  uint32_t primask, wraps, remaining, produced;
  if (g_uart_rx_restart != 0U) { return; }
  primask = __get_PRIMASK();
  __disable_irq();
  wraps = g_uart_dma_wraps;
  remaining = __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);
  /* 计入 DMA 中断尚未处理的已完成一圈；这个短暂快照期间 DMA 本身
   * 仍继续运行，因此检测到传输完成后再次读取 CNDTR。 */
  if (__HAL_DMA_GET_FLAG(&hdma_usart1_rx, DMA_FLAG_TC1) != RESET)
  {
    wraps++;
    remaining = __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);
    if (remaining == 0U) { remaining = UART_RX_DMA_SIZE; }
  }
  __set_PRIMASK(primask);
  produced = wraps * UART_RX_DMA_SIZE + UART_RX_DMA_SIZE - remaining;
  if (produced - g_uart_dma_consumed > UART_RX_DMA_SIZE)
  {
    g_uart_overrun_errors++;
    g_uart_rx_restart = 1U;
    return;
  }
  while (g_uart_dma_consumed != produced && g_uart_line_ready == 0U)
  {
    uint8_t byte;
    if (g_uart_rx_restart != 0U) { return; }
    byte = g_uart_dma_buffer[g_uart_dma_read];
    g_uart_dma_read = (uint16_t)((g_uart_dma_read + 1U) % UART_RX_DMA_SIZE);
    g_uart_dma_consumed++;
    g_uart_rx_count++;
    if (byte == '\r' || byte == '\n')
    {
      /* 忽略空的回车或换行，使 CRLF 和多行排队输入均可正常工作。 */
      if (g_uart_line_length != 0U || g_uart_line_overflow != 0U)
      {
        g_uart_line[g_uart_line_length] = 0;
        g_uart_line_ready = 1U;
        g_uart_line_length = 0U;
      }
    }
    else if (g_uart_line_length < sizeof(g_uart_line) - 1U)
    {
      g_uart_line[g_uart_line_length++] = (char)byte;
    }
    else { g_uart_line_overflow = 1U; }
  }
}

/**
 * @brief 在串口错误回调中记录错误和恢复请求，由主循环完成接收恢复。
 * @param huart 触发回调的 UART 句柄。
 * @return 无返回值。
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart1)
  {
    if (huart->ErrorCode & HAL_UART_ERROR_PE) { g_uart_parity_errors++; }
    if (huart->ErrorCode & HAL_UART_ERROR_FE) { g_uart_frame_errors++; }
    if (huart->ErrorCode & HAL_UART_ERROR_ORE) { g_uart_overrun_errors++; }
    if (huart->ErrorCode & HAL_UART_ERROR_NE) { g_uart_noise_errors++; }
    /* DMA 中止完成时也会调用这里；仅在主循环中恢复。 */
    g_uart_rx_restart = 1U;
  }
}

/**
 * @brief 在主循环清理 UART 错误、重启 DMA 接收并重置命令拼装状态。
 * @return 无返回值。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static void Serial_RecoverReceive(void)
{
  uint32_t primask;
  static uint32_t last_report;
  static uint8_t reported;

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
    if (Serial_StartReceive() != HAL_OK) { g_uart_rx_restart = 1U; }
  }
  __set_PRIMASK(primask);
  if (!reported || HAL_GetTick() - last_report >= 1000U)
  {
    char line[144];
    reported = 1U;
    last_report = HAL_GetTick();
    (void)snprintf(line, sizeof(line),
      "UART RX ERROR: parity=%lu frame=%lu overrun=%lu noise=%lu; resend command\r\n",
      (unsigned long)g_uart_parity_errors, (unsigned long)g_uart_frame_errors,
      (unsigned long)g_uart_overrun_errors, (unsigned long)g_uart_noise_errors);
    Serial_Write(line);
  }
}

/**
 * @brief 识别空格、制表符、回车和换行。
 * @param c 待判断或转换的 ASCII 字符。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static uint8_t Serial_IsSpace(char c){return (uint8_t)(c==' '||c=='\t'||c=='\r'||c=='\n');}
/**
 * @brief 将 ASCII 大写字母转换为小写，其他字符保持原值。
 * @param c 待判断或转换的 ASCII 字符。
 * @return 转换后的 ASCII 字符。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static uint8_t Serial_ToLower(char c){return (uint8_t)((c>='A'&&c<='Z')?(c+('a'-'A')):c);}
/**
 * @brief 忽略 ASCII 大小写比较两个以零结尾的字符串。
 * @param a 第一个输入值或字符串。
 * @param b 第二个输入值或字符串。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static uint8_t Serial_Equals(const char *a,const char *b){while(*a&&*b&&Serial_ToLower(*a)==Serial_ToLower(*b)){a++;b++;}return (uint8_t)(*a==0&&*b==0);}
/**
 * @brief 原地去掉首尾空白，返回有效子串起点；调用者必须传入可写字符串。
 * @param s 待处理字符串；原地修剪接口要求可写内存。
 * @return 修剪后的子串首地址，仍指向原缓冲区。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static char *Serial_Trim(char *s){char *e;while(Serial_IsSpace(*s))s++;e=s+strlen(s);while(e>s&&Serial_IsSpace(e[-1]))e--;*e=0;return s;}
/**
 * @brief 将指定数量的十进制字符解析到输出字节，遇非数字返回失败。
 * @param s 待处理字符串；原地修剪接口要求可写内存。
 * @param n 需要解析的十进制字符数。
 * @param value 待编码、待解析或待写入的值；指针形式用于接收解析输出。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static uint8_t Serial_Digits(const char *s,uint8_t n,uint8_t *value){uint8_t i,v=0;if(!s||!value)return 0;for(i=0;i<n;i++){if(s[i]<'0'||s[i]>'9')return 0;v=(uint8_t)(v*10U+(uint8_t)(s[i]-'0'));}*value=v;return 1;}
/**
 * @brief 按公历规则判断指定年份是否为闰年。
 * @param year 公历年份。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static uint8_t Serial_IsLeap(uint16_t year){return (uint8_t)((year%4U==0U&&year%100U!=0U)||(year%400U==0U));}
/**
 * @brief 计算月份天数并处理闰年二月；调用前须保证月份为 1～12。
 * @param year 公历年份。
 * @param month 月份，1～12。
 * @return 该月份的天数。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static uint8_t Serial_DaysInMonth(uint16_t year,uint8_t month){static const uint8_t days[]={31,28,31,30,31,30,31,31,30,31,30,31};if(month==2U&&Serial_IsLeap(year))return 29U;return days[month-1U];}
/**
 * @brief 计算 2000 年起的公历星期，结果 0 为周日、6 为周六。
 * @param year 公历年份。
 * @param month 月份，1～12。
 * @param day 日期，须已校验有效。
 * @return 星期编号，0 为周日、6 为周六。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static uint8_t Serial_Weekday(uint16_t year,uint8_t month,uint8_t day){uint32_t days=0;uint16_t y;uint8_t m;for(y=2000U;y<year;y++)days+=Serial_IsLeap(y)?366U:365U;for(m=1U;m<month;m++)days+=Serial_DaysInMonth(year,m);days+=(uint32_t)(day-1U);return (uint8_t)((6U+days)%7U);}

/**
 * @brief 解析并校验 SET 命令的日期时间，将合法结果写入 RTC 时间结构。
 * @param s 待处理字符串；原地修剪接口要求可写内存。
 * @param dt RTC 日期时间结构，按函数用途作为输入或解析输出。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static uint8_t Serial_ParseSetTime(const char *s,PCF8563_DateTime *dt)
{
  uint8_t yy,mo,da,hh,mi,se;uint16_t year;size_t n;
  if(!s||!dt)return 0;n=strlen(s);
  if(n!=21U||Serial_ToLower(s[0])!='s'||Serial_ToLower(s[1])!='e'||Serial_ToLower(s[2])!='t'||s[3]!=' '||s[6]!='-'||s[9]!='-'||s[12]!=' '||s[15]!=':'||s[18]!=':')return 0;
  if(!Serial_Digits(&s[4],2,&yy)||!Serial_Digits(&s[7],2,&mo)||!Serial_Digits(&s[10],2,&da)||!Serial_Digits(&s[13],2,&hh)||!Serial_Digits(&s[16],2,&mi)||!Serial_Digits(&s[19],2,&se))return 0;
  year=(uint16_t)(2000U+yy);if(mo<1U||mo>12U||da<1U||da>Serial_DaysInMonth(year,mo)||hh>23U||mi>59U||se>59U)return 0;
  dt->year=yy;dt->month=mo;dt->day=da;dt->hours=hh;dt->minutes=mi;dt->seconds=se;dt->weekday=Serial_Weekday(year,mo,da);dt->century=0U;return 1;
}

/* 自 2000-01-01 起的秒数；记录器存储 2000～2099 年。 */
/**
 * @brief 将 RTC 日期时间转换为相对 2000 年的秒数，供历史时间范围比较。
 * @param dt RTC 日期时间结构，按函数用途作为输入或解析输出。
 * @return 相对 2000 年的秒数；无效日期返回 0xFFFFFFFF。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static uint32_t Serial_DateTimeSeconds(const PCF8563_DateTime *dt)
{
  uint32_t days = 0U;
  uint16_t year;
  uint8_t month;
  if (dt->century != 0U || dt->year > 99U || dt->month < 1U || dt->month > 12U ||
      dt->day < 1U || dt->day > Serial_DaysInMonth((uint16_t)(2000U + dt->year), dt->month) ||
      dt->hours > 23U || dt->minutes > 59U || dt->seconds > 59U)
  {
    return 0xFFFFFFFFUL;
  }
  for (year = 2000U; year < 2000U + dt->year; year++)
  {
    days += Serial_IsLeap(year) ? 366U : 365U;
  }
  for (month = 1U; month < dt->month; month++)
  {
    days += Serial_DaysInMonth((uint16_t)(2000U + dt->year), month);
  }
  days += dt->day - 1U;
  return ((days * 24U + dt->hours) * 60U + dt->minutes) * 60U + dt->seconds;
}

/**
 * @brief 解析历史导出起止时间并转换为秒数，拒绝格式或日期不合法的输入。
 * @param text 以零结尾的文本或命令字符串。
 * @param length 连续处理的字节数，数组尺寸须与该长度匹配。
 * @param seconds 接收相对 2000 年秒数的输出指针。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static uint8_t Serial_ParseDumpTime(const char *text, size_t length, uint32_t *seconds)
{
  char command[22] = "SET ";
  PCF8563_DateTime dt;
  if (length == 19U && text[0] == '2' && text[1] == '0')
  {
    text += 2U;
    length -= 2U;
  }
  if (length != 17U) { return 0U; }
  memcpy(command + 4U, text, 17U);
  if (!Serial_ParseSetTime(command, &dt)) { return 0U; }
  *seconds = Serial_DateTimeSeconds(&dt);
  return 1U;
}

/**
 * @brief 输出本次历史导出的匹配条数和读取错误数，供上位机完成校验。
 * @return 无返回值。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static void Serial_PrintDumpResult(void)
{
  char line[80];
  (void)snprintf(line, sizeof(line), "LOG DUMP DONE: matched=%lu read_errors=%lu\r\n",
                 (unsigned long)g_dump_matched, (unsigned long)g_dump_read_errors);
  if (UartCrc_IsEnabled()) { (void)UartCrc_Send(line); }
  else { Serial_Write(line); }
}

/**
 * @brief 解析模式和可选时间范围，建立 Flash 序号快照并启动独占历史导出。
 * @param command 命令字或以零结尾的串口命令。
 * @return 无返回值。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static void Serial_StartLogDump(const char *command)
{
  uint32_t start = 0U, end = 0U;
  const char *text = command + 8U;
  uint8_t hpa = 0U, explicit_mode = 0U;
  uint8_t filtered;
  if (UartCrc_IsEnabled() && (UartCrc_IsBusy() || UartCrc_IsFailed() ||
                            g_dump_remaining != 0U || g_dump_report_pending))
  {
    Serial_Write("LOG DUMP ERROR: link busy or failed\r\n");
    return;
  }
  if (*text == ' ') { text++; }
  if (strncmp(text, "hpa", 3U) == 0 && (text[3] == 0 || text[3] == ' '))
  {
    hpa = 1U; explicit_mode = 1U; text += 3U;
    if (*text == ' ') { text++; }
  }
  else if (strncmp(text, "raw", 3U) == 0 && (text[3] == 0 || text[3] == ' '))
  {
    explicit_mode = 1U; text += 3U;
    if (*text == ' ') { text++; }
  }
  filtered = (uint8_t)(*text != 0);
  if (filtered)
  {
    const char *separator = strchr(text, ' ');
    /* 第一个空格分隔日期和时间，第二个空格分隔起止范围。 */
    if (separator != NULL) { separator = strchr(separator + 1U, ' '); }
    if (separator == NULL ||
        !Serial_ParseDumpTime(text, (size_t)(separator - text), &start) ||
        !Serial_ParseDumpTime(separator + 1U, strlen(separator + 1U), &end) ||
        start > end)
    {
      Serial_Write("LOG DUMP ERROR: use log dump [raw|hpa] [YYYY-MM-DD HH:MM:SS YYYY-MM-DD HH:MM:SS] (2000..2099, start <= end)\r\n");
      return;
    }
  }
  /* 先校验，再替换正在进行的导出，随后保存序号快照。 */
  Serial_DumpPauseSampling();
  g_dump_time_filtered = filtered;
  g_dump_hpa = hpa;
  g_dump_start_time = start;
  g_dump_end_time = end;
  g_dump_matched = 0U;
  g_dump_read_errors = 0U;
  g_dump_report_pending = 0U;
  g_dump_remaining = DataLogger_Count();
  g_dump_newest_first = DataLogger_GetDumpNewestFirst();
  g_dump_sequence = g_dump_newest_first ? DataLogger_LastSequence() :
                    DataLogger_LastSequence() - g_dump_remaining + 1U;
  if (explicit_mode)
  {
    Serial_Write(hpa ? "LOG DUMP ADC: hPa (0.5V=350hPa, 4.5V=1350hPa, VREF=5V)\r\n" :
                       "LOG DUMP ADC: codes (stored filtered ADC values)\r\n");
  }
  if (UartCrc_IsEnabled())
  {
    char begin[80];
    (void)snprintf(begin, sizeof(begin), "LOG DUMP BEGIN: scanned=%lu\r\n",
                   (unsigned long)g_dump_remaining);
    Serial_Write(begin);
    if (g_dump_remaining == 0U) { g_dump_report_pending = 1U; }
  }
  else if ((filtered || hpa) && g_dump_remaining == 0U) { Serial_PrintDumpResult(); }
}

/* 厂家表格给出 0.5 V=35 kPa、4.5 V=135 kPa。
 * 仅在导出时换算，保持存储码值和无效值语义不变。
 * 不进行钳位：表格还给出 0 V=22.5 kPa、5 V=147.5 kPa。 */
/**
 * @brief 按厂家压力表将 ADC 码换算为 hPa：码值 × 5 / 65536 × 250 + 225。
 * @param code ADC 码值；指针形式为输出，成功后写入。
 * @return 计算后的值，量纲与用途说明一致。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static float Serial_AdcPressureHpa(float code)
{
  if (!(code >= 0.0f && code < 65535.0f)) { return SENSOR_INVALID_FLOAT; }
  return code * (AD7682_REFERENCE_VOLTS * 250.0f / 65536.0f) + 225.0f;
}

/* 设置模式抑制实时串口输出；log dump 还会暂停传感器采样。 */
/**
 * @brief 将 Flash 保存的间隔和记录开关应用到运行时记录调度。
 * @return 无返回值。
 */
static void Recording_ApplySavedConfig(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  g_recording_enabled = DataLogger_GetRecordingEnabled();
  g_record_due = 0U;
  g_record_last_millisecond = g_record_timer_milliseconds;
  __set_PRIMASK(primask);
}

/* SW2 接 PB4、SW3 接 PB5，对应原理图 BUTTON1/BUTTON2，外部上拉，
 * 按下接地；中断消抖、主循环处理事件，不在中断中访问 SPI 或 Flash。 */
/* 仅由 TIM14 以 1 kHz 调用，不访问菜单、显示屏或
 * Flash，只对 PB4/PB5 消抖并锁存简短的事件位。 */
/**
 * @brief 在 TIM14 毫秒中断中扫描按键、消抖并登记事件，不访问总线或打印。
 * @return 无返回值。
 */
void Application_KeyScan1ms(void)
{
  static uint8_t raw_state[2];
  static uint8_t stable[2];
  static uint8_t held[2];
  static uint16_t stable_ms[2];
  static uint16_t pressed_ms[2];
  uint8_t key;
  for (key = 0U; key < 2U; key++)
  {
    uint8_t raw = (GPIOB->IDR & (key == 0U ? GPIO_PIN_4 : GPIO_PIN_5)) == 0U;
    if (raw != raw_state[key])
    {
      raw_state[key] = raw;
      stable_ms[key] = 0U;
    }
    else if (stable_ms[key] < 30U) { stable_ms[key]++; }

    if (stable_ms[key] >= 30U && raw != stable[key])
    {
      stable[key] = raw;
      if (raw)
      {
        pressed_ms[key] = 0U;
        held[key] = 0U;
      }
      else if (held[key] == 0U)
      {
        g_ui_key_events |= key == 0U ? UI_KEY_UP_SHORT : UI_KEY_DOWN_SHORT;
      }
    }
    if (stable[key] != 0U && raw != 0U)
    {
      if (pressed_ms[key] < 800U) { pressed_ms[key]++; }
      if (pressed_ms[key] >= 800U && held[key] == 0U)
      {
        held[key] = 1U;
        g_ui_key_events |= key == 0U ? UI_KEY_UP_LONG : UI_KEY_DOWN_LONG;
      }
    }
  }
}

/**
 * @brief 根据菜单页面和当前测量值构建 OLED 显示内容，由主循环调用。
 * @return 无返回值。
 */
static void Ui_Draw(void)
{
  char line[32];
  SSD1306_BeginUiFrame(HAL_GetTick());
  SSD1306_SetLine(0U,"SETTINGS");
  (void)snprintf(line,sizeof(line),"%s BOOT %s",g_ui_selected==0U?"+":" ",g_ui_display?"ADC ONLY":"ALL SENSORS");
  SSD1306_SetLine(1U,line);
  (void)snprintf(line,sizeof(line),"%s RECORD %s",g_ui_selected==1U?"+":" ",g_ui_recording?"START":"STOP");
  SSD1306_SetLine(2U,line);
  (void)snprintf(line,sizeof(line),"%s ORDER %s",g_ui_selected==2U?"+":" ",g_ui_order?"NEW FIRST":"OLD FIRST");
  SSD1306_SetLine(3U,line);
  (void)snprintf(line,sizeof(line),"%s INT %.1fS%s",g_ui_selected==3U?"+":" ",(double)g_ui_interval/1000.0,g_ui_edit_interval?" EDIT":"");
  SSD1306_SetLine(4U,line);
  SSD1306_SetLine(5U,g_ui_selected==4U?"+ CLEAR LOGS":"  CLEAR LOGS");
  SSD1306_SetLine(6U,g_ui_clear_confirm?"HOLD2 AGAIN TO CLEAR":g_ui_message);
  SSD1306_SetLine(7U,"HOLD2 OK HOLD3 SAVE");
  g_ui_dirty = 0U;
}

/**
 * @brief 在主循环消费按键事件、修改菜单设置并推进 OLED 分页刷新。
 * @return 无返回值。
 */
static void Ui_Process(void)
{
  uint32_t now=HAL_GetTick();
  uint8_t events;
  uint8_t up=0U, down=0U;
  __disable_irq();
  events = g_ui_key_events;
  g_ui_key_events = 0U;
  __enable_irq();
  if (events == 0U) { return; }
  if ((events & UI_KEY_UP_LONG) != 0U) { up = 2U; }
  else if ((events & UI_KEY_UP_SHORT) != 0U) { up = 1U; }
  if ((events & UI_KEY_DOWN_LONG) != 0U) { down = 2U; }
  else if ((events & UI_KEY_DOWN_SHORT) != 0U) { down = 1U; }
  /* 串口设置模式在退出前拥有配置控制权，丢弃界面中未保存的草稿。
   * 仍消费按键状态，避免退出时触发积压动作。 */
  if (g_rtc_setting_mode || g_dump_remaining)
  { g_ui_active=0U; return; }
  if (up && down) { return; }
  if (!g_ui_active)
  {
    if (!up && !down) { return; }
    g_ui_active=1U; g_ui_selected=0U; g_ui_edit_interval=0U;
    g_ui_clear_confirm=0U;
    g_ui_display=DataLogger_GetDisplayAdcOnly();
    g_ui_recording=DataLogger_GetRecordingEnabled();
    g_ui_order=DataLogger_GetDumpNewestFirst();
    g_ui_interval=DataLogger_GetIntervalMs();
    g_ui_message="UP2 DOWN3"; g_ui_dirty=1U;
  }
  else if (down==2U)
  {
    uint8_t timer_changed = (uint8_t)(g_ui_recording != DataLogger_GetRecordingEnabled() ||
                                     g_ui_interval != DataLogger_GetIntervalMs());
    /* 共用一条配置记录，不是每次按键都写入。 */
    if (DataLogger_SetMenuConfig(g_ui_display,g_ui_recording,g_ui_order,g_ui_interval)==HAL_OK)
    {
      if (timer_changed) { Recording_ApplySavedConfig(); }
      g_ui_active=0U;
      SSD1306_BeginUiFrame(now);
      SSD1306_SetLine(0U,"SETTINGS SAVED");
      return;
    }
    g_ui_message="SAVE ERROR RETRY"; g_ui_dirty=1U;
  }
  else if (up==1U || down==1U)
  {
    g_ui_clear_confirm=0U; g_ui_message="UP2 DOWN3";
    if (g_ui_edit_interval)
    {
      if (up==1U)
      {
        if (g_ui_interval > 500UL) { g_ui_interval -= 500UL; }
      }
      else
      {
        if (g_ui_interval < 86400000UL) { g_ui_interval += 500UL; }
      }
    }
    else { g_ui_selected=(uint8_t)((g_ui_selected+(up==1U?4U:1U))%5U); }
    g_ui_dirty=1U;
  }
  else if (up==2U)
  {
    switch (g_ui_selected)
    {
      case 0U:g_ui_display^=1U;break;
      case 1U:g_ui_recording^=1U;break;
      case 2U:g_ui_order^=1U;break;
      case 3U:g_ui_edit_interval^=1U;break;
      case 4U:
        if (!g_ui_clear_confirm) { g_ui_clear_confirm=1U; }
        else
        {
          /* 使用与串口相同的清除接口，保留校准值和菜单设置。 */
          g_ui_message=DataLogger_Clear(3000U)==HAL_OK?"LOGS CLEARED":"CLEAR ERROR RETRY";
          g_ui_clear_confirm=0U;
        }
        break;
      default:break;
    }
    g_ui_dirty=1U;
  }
  if (g_ui_dirty) { Ui_Draw(); }
}

/**
 * @brief 通过串口输出设置模式支持的命令和参数格式。
 * @return 无返回值。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static void Serial_PrintSettingHelp(void){Serial_Write("SETTING MODE\r\nSET YY-MM-DD HH:MM:SS\r\nlog info\r\nlog start\r\nlog stop\r\nlog interval SECONDS (0.5 step)\r\nlog order old\r\nlog order new\r\nlog dump [raw|hpa]\r\nlog dump [raw|hpa] YYYY-MM-DD HH:MM:SS YYYY-MM-DD HH:MM:SS\r\nlog clear\r\ndisplay all\r\ndisplay adc\r\nbattery info\r\nbattery cal VOLTS (example: battery cal 8.120)\r\nbattery reset\r\nadcprobe 0..3\r\nexit\r\n");}

/**
 * @brief 解析电池状态、单点校准及复位命令并返回执行结果。
 * @param command 命令字或以零结尾的串口命令。
 * @return 无返回值。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static void Serial_BatteryCommand(const char *command)
{
  char line[160];
  float actual = 0.0f, place = 0.1f, gain;
  uint8_t decimal = 0U, digits = 0U, fraction = 0U;
  const char *p;
  if (Serial_Equals(command, "battery info"))
  {
    (void)snprintf(line,sizeof(line),
      "BATTERY: MCU VREF=3.300V Rtop=100k Rbottom=47k gain=%.6f voltage=%.3fV fresh=%u\r\n",
      (double)DataLogger_GetBatteryGain(),
      (double)(g_battery_valid ? g_battery_nominal * DataLogger_GetBatteryGain() : SENSOR_INVALID_FLOAT),
      (unsigned)(g_battery_valid && (HAL_GetTick()-g_battery_valid_tick)<=2000U));
    Serial_Write(line); return;
  }
  if (Serial_Equals(command,"battery reset")) { gain = 1.0f; }
  else if (strncmp(command,"battery cal ",12U)==0)
  {
    for (p=command+12U; *p; p++)
    {
      if (*p=='.' && !decimal) { decimal=1U; continue; }
      if (*p<'0' || *p>'9' || digits>=6U || (decimal && fraction>=3U)) { break; }
      digits++;
      if (decimal) { actual += (float)(*p-'0')*place; place*=0.1f; fraction++; }
      else { actual=actual*10.0f+(float)(*p-'0'); }
    }
    if (*p || !digits || (decimal && !fraction) || !(actual>=2.5f && actual<=8.6f))
    { Serial_Write("BATTERY CAL ERROR: use battery cal 2.500..8.600 (actual battery volts)\r\n"); return; }
    if (!g_battery_valid || HAL_GetTick()-g_battery_valid_tick>2000U || g_battery_nominal<=0.0f)
    { Serial_Write("BATTERY CAL ERROR: no fresh ADC reading\r\n"); return; }
    /* 始终使用未经校准的滤波电压，重复校准不会叠加增益。 */
    gain=actual/g_battery_nominal;
    if (!(gain>=0.5f && gain<=1.5f))
    { Serial_Write("BATTERY CAL ERROR: gain outside 0.5..1.5; check divider/reference\r\n"); return; }
  }
  else { Serial_Write("Use battery info / battery cal VOLTS / battery reset\r\n"); return; }
  if (DataLogger_SetBatteryGain(gain)!=HAL_OK)
  { Serial_Write("BATTERY CAL SAVE ERROR\r\n"); return; }
  if (g_battery_valid)
  {
    g_pending_record.battery_voltage=g_battery_nominal*DataLogger_GetBatteryGain();
    BatteryLed_Update(g_pending_record.battery_voltage);
  }
  (void)snprintf(line,sizeof(line),"BATTERY CAL SAVED: gain=%.6f voltage=%.3fV (MCU FLASH)\r\n",
    (double)DataLogger_GetBatteryGain(),
    (double)(g_battery_valid ? g_pending_record.battery_voltage : SENSOR_INVALID_FLOAT));
  Serial_Write(line);
}
/* 以半秒分辨率精确解析秒数，避免 scanf 或浮点舍入。 */
/**
 * @brief 将秒数字符串解析为毫秒，校验 0.5 秒步进及允许范围。
 * @param text 以零结尾的文本或命令字符串。
 * @param result 接收记录间隔毫秒数的输出指针。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static uint8_t Serial_ParseIntervalMs(const char *text, uint32_t *result)
{
  uint32_t seconds=0U;
  uint16_t fraction=0U;
  uint16_t scale=100U;
  uint8_t digits=0U;
  const char *p=text;
  while (*p>='0' && *p<='9')
  {
    if (seconds>86400UL) { return 0U; }
    seconds=seconds*10UL+(uint32_t)(*p++-'0'); digits++;
  }
  if (!digits || seconds>86400UL) { return 0U; }
  if (*p=='.')
  {
    p++; digits=0U;
    while (*p>='0' && *p<='9')
    {
      if (digits>=3U) { return 0U; }
      fraction+=(uint16_t)(*p++-'0')*scale;scale/=10U;digits++;
    }
    if (!digits) { return 0U; }
  }
  *result=seconds*1000UL+fraction;
  return (uint8_t)(*p==0 && *result>=500UL && *result<=86400000UL && *result%500UL==0U);
}

/**
 * @brief 处理串口校时命令，写入 RTC 后回读并报告日期时间和 VL 状态。
 * @param command 命令字或以零结尾的串口命令。
 * @return 无返回值。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
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

/**
 * @brief 分发完整串口命令，处理握手、ACK/NACK、日志、校时与诊断操作。
 * @return 无返回值。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static void Serial_ProcessCommand(void)
{
  char command[64];char line[64];char *trimmed;uint8_t overflow;
  Serial_RecoverReceive();
  Serial_PollReceive();
  if (!g_uart_line_ready || g_uart_rx_restart != 0U) { return; }
  __disable_irq();
  memcpy(command,g_uart_line,sizeof(command));
  overflow=g_uart_line_overflow;
  g_uart_line_ready=0U;
  __enable_irq();
  command[sizeof(command)-1U]=0;
  g_uart_line_overflow=0U;
  if(overflow){Serial_Write("Command error: line too long\r\n");return;}
  trimmed=Serial_Trim(command);if(*trimmed==0)return;
  if (UartCrc_HandleControl(trimmed)) { return; }
  if (strncmp(trimmed, "link crc on ", 12U) == 0)
  {
    uint32_t session;
    if (!UartCrc_ParseHex8(trimmed + 12U, &session))
    { Serial_Write("LINK CRC ERROR: use link crc on SESSION8HEX\r\n"); return; }
    g_dump_remaining = 0U; g_dump_report_pending = 0U;
    Serial_DumpResumeSampling();
    UartCrc_Enable(session);
    (void)snprintf(line, sizeof(line), "LINK CRC ON %08lX\r\n", (unsigned long)session);
    Serial_Write(line);
    return;
  }
  if (Serial_Equals(trimmed, "link crc off"))
  {
    g_dump_remaining = 0U; g_dump_report_pending = 0U;
    Serial_DumpResumeSampling();
    UartCrc_Disable();
    Serial_Write("LINK CRC OFF\r\n"); return;
  }
  if (Serial_Equals(trimmed, "link window 16"))
  {
    if (!g_rtc_setting_mode || !UartCrc_IsEnabled() || UartCrc_IsBusy() ||
        g_dump_remaining || g_dump_report_pending)
    { Serial_Write("LINK WINDOW ERROR: enter idle setting mode\r\n"); return; }
    UartCrc_SetWindow(1U);
    Serial_Write("LINK WINDOW OK: 16\r\n"); return;
  }
  if (Serial_Equals(trimmed,"link batch 100") || Serial_Equals(trimmed,"link batch 1000"))
  {
    if (!g_rtc_setting_mode || !UartCrc_IsEnabled() || UartCrc_IsBusy() ||
        g_dump_remaining || g_dump_report_pending)
    { Serial_Write("LINK BATCH ERROR: enter idle setting mode\r\n"); return; }
    UartCrc_SetWindow(Serial_Equals(trimmed,"link batch 1000") ? 3U : 2U);
    Serial_Write(Serial_Equals(trimmed,"link batch 1000") ?
                 "LINK BATCH OK: 1000\r\n" : "LINK BATCH OK: 100\r\n");return;
  }
  if (Serial_Equals(trimmed, "uart info"))
  {
    char info[160];
    (void)snprintf(info, sizeof(info),
      "UART RX DMA128 921600 8E1: bytes=%lu parity=%lu frame=%lu overrun=%lu noise=%lu\r\n",
      (unsigned long)g_uart_rx_count, (unsigned long)g_uart_parity_errors,
      (unsigned long)g_uart_frame_errors, (unsigned long)g_uart_overrun_errors,
      (unsigned long)g_uart_noise_errors);
    Serial_Write(info);
    return;
  }
  if (g_dump_exclusive && !Serial_Equals(trimmed, "exit"))
  { Serial_Write("LOG DUMP BUSY: use exit to cancel\r\n"); return; }
  if(strncmp(trimmed,"adcprobe ",9U)==0)
  {
    if (UartCrc_IsEnabled()) { Serial_Write("ADC PROBE ERROR: disable CRC link first\r\n"); return; }
    if(trimmed[9]<'0'||trimmed[9]>'3'||trimmed[10]!=0||
       g_adc_diag_active!=0U||g_dump_remaining!=0U)
    {
      Serial_Write("ADC PROBE ERROR: use adcprobe 0..3 without log dump\r\n");
      return;
    }
    /* 复用滤波器存储之前先停止正常采样中断。 */
    TIM3->CR1 = 0U;
    g_adc_normal_enabled = 0U;
    TIM3->SR = 0U;
    HAL_NVIC_ClearPendingIRQ(TIM3_IRQn);
    g_adc_diag_channel = (uint8_t)(trimmed[9]-'0');
    g_adc_diag_phase = 0U;
    g_adc_diag_block = 0U;
    g_adc_diag_count = 0U;
    g_adc_diag_phase_ready = 0U;
    g_adc_probe_random = 0x76820001UL;
    g_record_due = 0U;
    g_adc_probe_active = 1U;
    g_adc_diag_active = 1U;
    (void)snprintf(line, sizeof(line),
      "ADC:990,9,6,288,3,%u,100,2000000,0,0,0,0\r\n",
      (unsigned)g_adc_diag_channel);
    Serial_Write(line);
    return;
  }
  if(strncmp(trimmed,"adcdiag ",8U)==0)
  {
    if (UartCrc_IsEnabled()) { Serial_Write("ADC DIAG ERROR: disable CRC link first\r\n"); return; }
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
  if(Serial_Equals(trimmed,"exit")){g_dump_remaining=0U;g_dump_report_pending=0U;Serial_DumpResumeSampling();UartCrc_SetWindow(0U);if(g_rtc_setting_mode){g_rtc_setting_mode=0U;Serial_Write("EXIT SETTING MODE\r\n");}return;}
  if(g_rtc_setting_mode)
  {
    if(strncmp(trimmed,"battery ",8U)==0)
    {
      Serial_BatteryCommand(trimmed);
    }
    else if(Serial_Equals(trimmed,"display all") || Serial_Equals(trimmed,"display adc"))
    {
      Serial_Write(DataLogger_SetMenuConfig(Serial_Equals(trimmed,"display adc"),
        DataLogger_GetRecordingEnabled(),DataLogger_GetDumpNewestFirst(),
        DataLogger_GetIntervalMs())==HAL_OK ? "DISPLAY SAVED\r\n" : "DISPLAY SAVE ERROR\r\n");
    }
    else if(Serial_Equals(trimmed,"log start") || Serial_Equals(trimmed,"log stop"))
    {
      uint8_t enabled = Serial_Equals(trimmed,"log start") ? 1U : 0U;
      if(DataLogger_SetRecordingEnabled(enabled)==HAL_OK)
      {
        if(g_recording_enabled != enabled)
        {
          Recording_ApplySavedConfig();
        }
        Serial_Write(enabled ? "LOG RECORDING ON\r\n" : "LOG RECORDING OFF\r\n");
      }
      else { Serial_Write("LOG RECORDING CONFIG ERROR\r\n"); }
    }
    else if(Serial_Equals(trimmed,"log dump") || strncmp(trimmed,"log dump ",9U)==0)
    {
      Serial_StartLogDump(trimmed);
    }
    else if(Serial_Equals(trimmed,"log info"))
    {
      char line[128];
      (void)snprintf(line,sizeof(line),"LOG INFO: count=%lu capacity=%lu interval=%.1fs order=%s recording=%s display=%s\r\n",
                     (unsigned long)DataLogger_Count(),
                     (unsigned long)DataLogger_Capacity(),
                     (double)DataLogger_GetIntervalMs()/1000.0,
                     DataLogger_GetDumpNewestFirst() ? "newest-first" : "oldest-first",
                     g_recording_enabled ? "on" : "off",
                     DataLogger_GetDisplayAdcOnly() ? "adc" : "all");
      Serial_Write(line);
    }
    else if(strncmp(trimmed,"log interval ",13U)==0)
    {
      uint32_t milliseconds;
      if(Serial_ParseIntervalMs(&trimmed[13],&milliseconds) &&
         DataLogger_SetIntervalMs(milliseconds,3000U)==HAL_OK)
      {
        Recording_ApplySavedConfig();
        Serial_Write("LOG INTERVAL OK\r\n");
      }
      else
      {
        Serial_Write("LOG INTERVAL ERROR: use log interval 0.5..86400 step 0.5 seconds\r\n");
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
      g_dump_report_pending = 0U;
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

/* 每次扫描一条历史记录；由暂停采样后的独占主循环连续调用。 */
/**
 * @brief 在独占导出循环扫描一条 Flash 记录，筛选、换算和发送，维护批次检查点与重放状态。
 * @return 无返回值。
 * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。
 */
static void Serial_DumpLogs(void)
{
  uint16_t year;
  char line[180];
  DataLogger_Sample sample;
  uint8_t include;
  if (UartCrc_IsEnabled())
  {
    if (UartCrc_TakeReplay())
    {
      g_dump_sequence = g_window_dump_sequence;
      g_dump_remaining = g_window_dump_remaining;
      g_dump_matched = g_window_dump_matched;
      g_dump_read_errors = g_window_dump_errors;
      g_dump_report_pending = 0U;
      g_window_scanned = 0U;
    }
    if (UartCrc_IsBusy() || UartCrc_IsFailed()) { return; }
    if (g_dump_report_pending)
    {
      g_dump_report_pending = 0U;
      Serial_PrintDumpResult();
      return;
    }
  }

  /* 每次调用只扫描一条；独占主循环持续调用，并在记录之间处理命令和 ACK。
     固定序号快照：导出期间新增记录不混入本次导出。 */
  if (g_dump_remaining != 0U && g_rtc_setting_mode != 0U)
  {
    uint32_t sequence = g_dump_sequence;
    if (UartCrc_WindowEnabled() && !UartCrc_WindowOpen())
    {
      g_window_dump_sequence = g_dump_sequence;
      g_window_dump_remaining = g_dump_remaining;
      g_window_dump_matched = g_dump_matched;
      g_window_dump_errors = g_dump_read_errors;
      g_window_scanned = 0U;
      UartCrc_WindowBegin();
    }
    g_dump_sequence = g_dump_newest_first ? sequence - 1U : sequence + 1U;
    g_dump_remaining--;
    if (DataLogger_ReadSequence(sequence, &sample, 100U) != HAL_OK)
    {
      g_dump_read_errors++;
    }
    else
    {
      include = 1U;
      if (g_dump_time_filtered)
      {
        uint32_t seconds = Serial_DateTimeSeconds(&sample.datetime);
        include = (uint8_t)(seconds >= g_dump_start_time && seconds <= g_dump_end_time);
      }
      if (include)
      {
        if (g_dump_hpa)
        {
          uint8_t channel;
          for (channel = 0U; channel < 4U; channel++)
          {
            sample.adc[channel] = Serial_AdcPressureHpa(sample.adc[channel]);
          }
        }
        year = (uint16_t)(sample.datetime.year +
                         ((sample.datetime.century != 0U) ? 1900U : 2000U));
        (void)snprintf(line, sizeof(line),
                   "samples: %u,%02u,%02u,%02u,%02u,%02u,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\r\n",
                   (unsigned)year, (unsigned)sample.datetime.month,
                   (unsigned)sample.datetime.day, (unsigned)sample.datetime.hours,
                   (unsigned)sample.datetime.minutes, (unsigned)sample.datetime.seconds,
                   (double)sample.temperature_c, (double)sample.humidity_rh,
                   (double)sample.pressure_u7_hpa, (double)sample.pressure_u11_hpa,
                   (double)sample.adc[0], (double)sample.adc[1],
                   (double)sample.adc[2], (double)sample.adc[3],
                   (double)sample.battery_voltage);
        Serial_Write(line);
        g_dump_matched++;
      }
    }
    if (UartCrc_WindowOpen() && (++g_window_scanned >= UartCrc_WindowSize() || g_dump_remaining == 0U))
    { UartCrc_WindowEnd(); }
    if (g_dump_remaining == 0U)
    {
      if (UartCrc_IsEnabled()) { g_dump_report_pending = 1U; }
      else if (g_dump_time_filtered || g_dump_hpa) { Serial_PrintDumpResult(); }
    }
    else if (UartCrc_IsEnabled() && (g_dump_remaining % 128U) == 0U)
    {
      (void)snprintf(line, sizeof(line), "LOG DUMP PROGRESS: remaining=%lu\r\n",
                     (unsigned long)g_dump_remaining);
      Serial_Write(line);
    }
  }
}

/* 三点中值滤波：抑制 SHT41 偶发的孤立异常值。 */
/**
 * @brief 返回三个浮点数的中位数，用于抑制孤立尖峰。
 * @param a 第一个输入值或字符串。
 * @param b 第二个输入值或字符串。
 * @param c 第三个待比较的浮点数。
 * @return 计算后的值，量纲与用途说明一致。
 */
static float Median3(float a, float b, float c)
{
  float swap;
  if (a > b) { swap = a; a = b; b = swap; }
  if (b > c) { swap = b; b = c; c = swap; }
  if (a > b) { swap = a; a = b; b = swap; }
  return b;
}

/* 三点中值去除孤立尖峰，再做50点滑动平均；只在有效转换时推进。
 * 初次使用实际读数建窗，不填零；读取失败保留上次结果，
 * 超过5秒无有效转换后，下一次成功读取重新建立窗口。 */
/**
 * @brief 依据 ADC 状态更新电池电压滤波与有效性，返回当前滤波结果。
 * @param status 本次传感器或 ADC 读取状态，用于判断读数是否有效。
 * @param raw ADC 原始码；指针形式用于接收输出。
 * @return 计算后的值，量纲与用途说明一致。
 */
static float BatteryFilter_Update(HAL_StatusTypeDef status, uint32_t raw)
{
  static float recent[3];
  static float means[BATTERY_AVG_WINDOW];
  static float sum;
  static float filtered = SENSOR_INVALID_FLOAT;
  static uint32_t last_tick;
  static uint8_t valid;
  static uint8_t recent_index;
  static uint8_t mean_index;
  static uint8_t count;
  uint32_t now = HAL_GetTick();
  float voltage;
  float median;
  uint8_t i;
  if (status != HAL_OK || raw > 4095U)
  { return valid ? filtered * DataLogger_GetBatteryGain() : SENSOR_INVALID_FLOAT; }
  voltage = BatteryAdc_CalibratedVoltage(raw);
  if (!valid || (now - last_tick) > 5000U)
  {
    for (i = 0U; i < 3U; i++) { recent[i] = voltage; }
    recent_index = 0U;
    mean_index = 0U;
    count = 0U;
    sum = 0.0f;
    valid = 1U;
  }
  recent[recent_index] = voltage;
  recent_index = (uint8_t)((recent_index + 1U) % 3U);
  median = Median3(recent[0], recent[1], recent[2]);
  if (count == BATTERY_AVG_WINDOW) { sum -= means[mean_index]; }
  else { count++; }
  means[mean_index] = median;
  sum += median;
  mean_index = (uint8_t)((mean_index + 1U) % BATTERY_AVG_WINDOW);
  filtered = sum / (float)count;
  last_tick = now;
  g_battery_nominal = filtered;
  g_battery_valid_tick = now;
  g_battery_valid = 1U;
  return filtered * DataLogger_GetBatteryGain();
}

/* 三点中值后接50点滑动平均，温湿度独立运算并保留小数。
 * 只接受成功且范围有效的测量；断连超过5秒后重新建立窗口。
 * 串口和Flash使用同一结果；超过5秒未更新时均使用无效标记。 */
/**
 * @brief 依据读取状态更新温湿度滤波及有效性，抑制孤立异常读数。
 * @param status 本次传感器或 ADC 读取状态，用于判断读数是否有效。
 * @param temperature 本次温度读数，单位 °C。
 * @param humidity 本次相对湿度读数，单位 %RH。
 * @return 无返回值。
 */
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
/**
 * @brief 检查气压值是否落在当前程序接受的有效范围内。
 * @param pressure_hpa 接收压力结果的输出指针，单位 hPa。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 */
static uint8_t Pressure_IsValid(float pressure_hpa)
{
  return (uint8_t)(pressure_hpa > 0.0f && pressure_hpa < 2000.0f);
}

/* 100 点时间中值滤波后串联两级一阶低通。
 * 在 100 Hz 下每级时间常数为 0.2 秒，中值滤波剔除短暂异常值；
 * 持续阶跃仍可跟踪，只产生平滑延迟，不进行硬限幅。 */
/**
 * @brief 将一个 ADC 样本插入时间滤波窗口，更新排序及累计值并返回滤波码值。
 * @param filter 由本函数更新的滤波状态。
 * @param sample 传感器快照；const 指针为待写入输入，其他指针为读取输出。
 * @return 计算后的值，量纲与用途说明一致。
 */
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
    /* 从有序集合中删除一个移出窗口的原始码值。 */
    temp = filter->values[filter->next];
    for (i = 0U; i < count && filter->sorted[i] != temp; i++) { }
    for (; i + 1U < count; i++) { filter->sorted[i] = filter->sorted[i + 1U]; }
    count--;
  }
  /* 以窗口长度的线性复杂度维护有序窗口，避免在中断中
   * 进行 100×100 的重新排序；环形缓冲区保留时间顺序，sorted[] 存储相同的多重集合。 */
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
  /* 使用实际中值预热，绝不用零填充低通级。 */
  if (count < ADC_FILTER_WINDOW)
  {
    return (float)median_twice * 0.5f;
  }
  if (filter->lowpass_ready == 0U)
  {
    filter->lowpass1 = (float)median_twice * 0.5f;
    filter->lowpass2 = filter->lowpass1;
    filter->lowpass_ready = 1U;
  }
  else
  {
    filter->lowpass1 += ADC_LOWPASS_ALPHA *
                         ((float)median_twice * 0.5f - filter->lowpass1);
    filter->lowpass2 += ADC_LOWPASS_ALPHA *
                         (filter->lowpass1 - filter->lowpass2);
  }
  return filter->lowpass2;
}

/* 在配置的记录时刻保存滤波 ADC 码值快照；RTC 和 Flash 读写
 * 仍在主循环中执行，避免拉长 100 Hz 定时周期。 */
/**
 * @brief 检查记录周期并保存待写快照；只在数据就绪时捕获，避免在中断中写 Flash。
 * @return 无返回值。
 */
static void RecordCaptureDue(void)
{
  uint32_t now = g_record_timer_milliseconds;
  uint32_t interval = DataLogger_GetIntervalMs();
  uint32_t elapsed;
  uint8_t channel;
  uint32_t primask;
  if (g_recording_enabled == 0U || interval == 0U ||
      (now - g_record_last_millisecond) < interval) { return; }
  /* 绝不使用初始化为零的缓冲区生成初始记录；建立
   * 首个真实窗口后，SPI 失败时保留上次实测滤波码值。 */
  for (channel = 0U; channel < 4U; channel++)
  {
    if (g_adc_temporal_filters[channel].count < ADC_FILTER_WINDOW) { return; }
  }
  elapsed = now - g_record_last_millisecond;
  g_record_last_millisecond += (elapsed / interval) * interval;
  primask = __get_PRIMASK();
  __disable_irq();
  for (channel = 0U; channel < 4U; channel++)
  {
    g_pending_record.adc[channel] = g_adc_filtered_codes[channel];
  }
  __set_PRIMASK(primask);
  g_record_due = 1U;
}

/* 保留一条待写记录；Flash 操作超过一个记录间隔时，下次
 * 记录时刻用最新滤波值替换，避免将过期样本重新写入
 * 并误认为它们是按时采集的数据。 */
/**
 * @brief 在主循环将待写快照追加到 Flash，处理写入状态和记录调度。
 * @return 无返回值。
 */
static void RecordWritePending(void)
{
  DataLogger_Sample sample;
  uint32_t primask;
  uint8_t sht_fresh;
  if (g_recording_enabled == 0U) { g_record_due = 0U; return; }
  if (g_dump_exclusive) { return; }
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
  sample.battery_voltage = g_pending_record.battery_voltage;
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
/**
 * @brief 由 TIM3 中断执行四路 ADC 扫描和滤波，发布主循环可读取的采样快照。
 * @return 无返回值。
 */
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

/**
 * @brief 在临界区复制滤波码和原始码，避免与 ADC 中断并发导致半更新数据。
 * @param filtered 接收四路滤波 ADC 码值的数组。
 * @param raw 接收四路 ADC 原始码的数组。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 */
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
/**
 * @brief 更新气压滑动平均窗口，返回当前平均压力。
 * @param filter 由本函数更新的滤波状态。
 * @param pressure_hpa 接收压力结果的输出指针，单位 hPa。
 * @return 计算后的值，量纲与用途说明一致。
 */
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
/**
 * @brief 在主循环读取慢速传感器、更新有效性及滤波，并输出实时采样数据。
 * @return 无返回值。
 */
static void Samples_ProcessAndOutput(void)
{
  uint32_t battery_raw = 0U;
  HAL_StatusTypeDef battery_status;
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

  battery_status = BatteryAdc_ReadRawChecked(&hadc1, &battery_raw, 10U);
  battery_voltage = BatteryFilter_Update(battery_status, battery_raw);
  g_pending_record.battery_voltage = battery_voltage;
  if (battery_voltage >= 0.0f) { BatteryLed_Update(battery_voltage); }
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
  /* 格式化或发送遥测之前，先处理定时器锁存的记录。 */
  RecordWritePending();
  /* 不要将上电初始零值或尚未完全预热的滤波结果作为测量值输出。 */
  if (Application_AdcGetSnapshot(adc, adc_raw) == 0U) { return; }
  if (!g_ui_active && SSD1306_BeginFrame(HAL_GetTick()))
  {
    static uint32_t rtc_tick;
    static uint8_t rtc_valid;
    static uint8_t rtc_attempted;
    static PCF8563_DateTime display_time;
    uint32_t now = HAL_GetTick();
    if (DataLogger_GetDisplayAdcOnly())
    {
    SSD1306_SetDashboard();
    /* 每秒最多读取一次 RTC，读取失败时不能一直沿用旧时间。 */
    if (!rtc_attempted || now-rtc_tick >= 1000U)
    {
      rtc_attempted=1U;
      rtc_tick=now;
      rtc_valid=PCF8563_ReadDateTime(&hi2c1,&display_time,10U)==HAL_OK ? 1U : 0U;
    }
    if (rtc_valid)
    {
      (void)snprintf(adc_text,sizeof(adc_text),"%02u:%02u:%02u",
        (unsigned)display_time.hours,(unsigned)display_time.minutes,(unsigned)display_time.seconds);
    }
    else { (void)snprintf(adc_text,sizeof(adc_text),"RTC READ ERROR"); }
    SSD1306_SetLine(0U,adc_text);
    (void)snprintf(adc_text,sizeof(adc_text),"P1:%.2f",(double)u7_pressure_average);
    SSD1306_SetLine(1U,adc_text);
    (void)snprintf(adc_text,sizeof(adc_text),"P2:%.2f",(double)u11_pressure_average);
    SSD1306_SetLine(2U,adc_text);
    (void)snprintf(adc_text,sizeof(adc_text),"0:%.2f",(double)adc[0]);
    SSD1306_SetLine(3U,adc_text);
    (void)snprintf(adc_text,sizeof(adc_text),"2:%.2f",(double)adc[2]);
    SSD1306_SetLine(4U,adc_text);
    }
    else
    {
      uint8_t channel;
      SSD1306_SetLargeText(0U);
      (void)snprintf(adc_text,sizeof(adc_text),"BAT %.3fV LOG %s",
        (double)battery_voltage,g_recording_enabled?"ON":"OFF");
      SSD1306_SetLine(0U,adc_text);
      (void)snprintf(adc_text,sizeof(adc_text),"T %.2fC H %.2f%%",
        (double)sht_temperature,(double)sht_humidity);
      SSD1306_SetLine(1U,adc_text);
      (void)snprintf(adc_text,sizeof(adc_text),"P1 %.2f HPA",(double)u7_pressure_average);
      SSD1306_SetLine(2U,adc_text);
      (void)snprintf(adc_text,sizeof(adc_text),"P2 %.2f HPA",(double)u11_pressure_average);
      SSD1306_SetLine(3U,adc_text);
      for(channel=0U;channel<4U;channel++)
      {
        (void)snprintf(adc_text,sizeof(adc_text),"IN%u %.2f",(unsigned)channel,(double)adc[channel]);
        SSD1306_SetLine((uint8_t)(4U+channel),adc_text);
      }
    }
  }
  (void)snprintf(adc_text, sizeof(adc_text),
                 "%.2f,%.2f,%.2f,%.2f", (double)adc[0], (double)adc[1],
                 (double)adc[2], (double)adc[3]);

  /* 采样或 Flash 写入期间到达的命令，在下一行实时输出前处理。 */
  Serial_ProcessCommand();
  if (g_rtc_setting_mode != 0U || g_adc_diag_active != 0U ||
      (UartCrc_IsEnabled() && (UartCrc_IsBusy() || UartCrc_IsFailed())))
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
/**
 * @brief 关闭中断并停留在错误循环，供无法继续运行的初始化失败调用。
 * @return 无返回值。
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
/**
 * @brief 接收 HAL 参数断言失败的位置；当前实现预留给调试，不自动恢复。
 * @param file 触发断言的源文件名。
 * @param line 触发断言的源文件行号。
 * @return 无返回值。
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* 可在此添加用户代码，输出文件名和行号，例如：
     printf("参数值错误：文件 %s，第 %d 行\r\n", file, line)。 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
