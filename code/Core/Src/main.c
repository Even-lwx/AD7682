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
static uint32_t g_record_last_second;
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
static uint8_t Serial_ParseUint32(const char *text, uint32_t *value);
static void Serial_ProcessCommand(void);
static void Serial_RecoverReceive(void);
static void BatteryLed_Update(float voltage);
static void ShtFilter_Update(HAL_StatusTypeDef status, float temperature, float humidity);
static float PressureAverage_Update(PressureAverage *filter, float pressure_hpa);
static uint8_t Pressure_IsValid(float pressure_hpa);

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

  /* USER CODE END 2 */

  /* 无限循环。 */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    Serial_ProcessCommand();
    /* 由 TIM14 周期中断触发采样；耗时的 I2C/SPI 事务不在中断中执行。 */
    if (SensorSample_ConsumeRequest() != 0U)
    {
      /* setting 模式只关闭实时串口输出，传感器采样和历史记录仍持续运行。 */
      Samples_ProcessAndOutput();
    }
    Serial_DumpLogs();
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
 * 采样请求。I2C/SPI 和串口发送全部留在主循环，避免在 ISR 中阻塞。 */
static void RecordTimer_Init(void)
{
  uint32_t clock = HAL_RCC_GetPCLK1Freq();
  if ((RCC->CFGR & RCC_CFGR_PPRE) != 0U) { clock *= 2U; }
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
  TIM14->DIER = TIM_DIER_UIE;
  HAL_NVIC_SetPriority(TIM14_IRQn, 2U, 0U);
  HAL_NVIC_ClearPendingIRQ(TIM14_IRQn);
  HAL_NVIC_EnableIRQ(TIM14_IRQn);
  TIM14->CR1 = TIM_CR1_CEN;
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
  Serial_Write("AD7682 MULTI: 4CH, DUMMY2+5 VALID, MEDIAN5, ACQ=500us, RB=1, CFG-ONCE, SPI-QUIET\r\n");

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
static void Serial_PrintSettingHelp(void){Serial_Write("SETTING MODE\r\nSET YY-MM-DD HH:MM:SS\r\nlog info\r\nlog interval SECONDS\r\nlog order old\r\nlog order new\r\nlog dump\r\nlog clear\r\nexit\r\n");}
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
  char command[64];char *trimmed;uint8_t overflow;
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
  if(Serial_Equals(trimmed,"setting")){g_rtc_setting_mode=1U;Serial_PrintSettingHelp();return;}
  if(Serial_Equals(trimmed,"exit")){g_dump_remaining=0U;if(g_rtc_setting_mode){g_rtc_setting_mode=0U;Serial_Write("EXIT SETTING MODE\r\n");}return;}
  if(g_rtc_setting_mode)
  {
    if(Serial_Equals(trimmed,"log dump"))
    {
      g_dump_remaining = DataLogger_Count();
      g_dump_newest_first = DataLogger_GetDumpNewestFirst();
      g_dump_sequence = g_dump_newest_first ? DataLogger_LastSequence() :
                        DataLogger_LastSequence() - g_dump_remaining + 1U;
    }
    else if(Serial_Equals(trimmed,"log info"))
    {
      char line[128];
      (void)snprintf(line,sizeof(line),"LOG INFO: count=%lu capacity=%lu interval=%lus order=%s\r\n",
                     (unsigned long)DataLogger_Count(),
                     (unsigned long)DataLogger_Capacity(),
                     (unsigned long)DataLogger_GetInterval(),
                     DataLogger_GetDumpNewestFirst() ? "newest-first" : "oldest-first");
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
                   "samples: %u,%02u,%02u,%02u,%02u,%02u,%.2f,%.2f,%.2f,%.2f,%u,%u,%u,%u\r\n",
                   (unsigned)year, (unsigned)sample.datetime.month,
                   (unsigned)sample.datetime.day, (unsigned)sample.datetime.hours,
                   (unsigned)sample.datetime.minutes, (unsigned)sample.datetime.seconds,
                   (double)sample.temperature_c, (double)sample.humidity_rh,
                   (double)sample.pressure_u7_hpa, (double)sample.pressure_u11_hpa,
                   (unsigned)sample.adc[0], (unsigned)sample.adc[1],
                   (unsigned)sample.adc[2], (unsigned)sample.adc[3]);
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

/* 只接受成功且范围有效的测量；失败不把人为补零放进窗口。
 * 初次成功填满窗口，后续三点中值去除孤立尖峰，不禁止真实零值。
 * 串口允许短期保持上次结果，但超过 5 秒未成功更新不写入历史。 */
static void ShtFilter_Update(HAL_StatusTypeDef status, float temperature, float humidity)
{
  static float temperatures[3];
  static float humidities[3];
  static uint8_t index;
  uint8_t i;
  if (status != HAL_OK || !(temperature >= -40.0f && temperature <= 125.0f) ||
      !(humidity >= 0.0f && humidity <= 100.0f)) { return; }
  if (!g_sht_filter_valid)
  {
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
  g_sht_filtered_temperature = Median3(temperatures[0], temperatures[1], temperatures[2]);
  g_sht_filtered_humidity = Median3(humidities[0], humidities[1], humidities[2]);
  g_sht_last_valid_tick = HAL_GetTick();
}

/* LPS22HH 的有效气压范围约为 260~1260 hPa；这里放宽到 0~2000 hPa，
 * 只过滤通信失败时使用的负无效值和明显异常值。无效采样不进入窗口。 */
static uint8_t Pressure_IsValid(float pressure_hpa)
{
  return (uint8_t)(pressure_hpa > 0.0f && pressure_hpa < 2000.0f);
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
 * 两路实时平均。W25Q128 中的压力字段保存 U7/U11 各自的 50 点平均值。 */
/* 读取一轮所有传感器，按记录周期写 Flash，并按实时格式输出。 */
static void Samples_ProcessAndOutput(void)
{
  uint32_t battery_raw;
  uint32_t now;
  uint32_t interval;
  uint16_t adc[4] = {SENSOR_INVALID_ADC, SENSOR_INVALID_ADC,
                     SENSOR_INVALID_ADC, SENSOR_INVALID_ADC};
  uint16_t adc_samples[4][5] = {
      {SENSOR_INVALID_ADC, SENSOR_INVALID_ADC, SENSOR_INVALID_ADC,
       SENSOR_INVALID_ADC, SENSOR_INVALID_ADC},
      {SENSOR_INVALID_ADC, SENSOR_INVALID_ADC, SENSOR_INVALID_ADC,
       SENSOR_INVALID_ADC, SENSOR_INVALID_ADC},
      {SENSOR_INVALID_ADC, SENSOR_INVALID_ADC, SENSOR_INVALID_ADC,
       SENSOR_INVALID_ADC, SENSOR_INVALID_ADC},
      {SENSOR_INVALID_ADC, SENSOR_INVALID_ADC, SENSOR_INVALID_ADC,
       SENSOR_INVALID_ADC, SENSOR_INVALID_ADC}};
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
  char line[384];
  char adc_text[128];
  char adc_raw_text[160];
  DataLogger_Sample log_sample;
  HAL_StatusTypeDef sht_status;

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
  /* 多通道保守采样：切换后丢弃两次，再每通道取5次有效转换，
     由驱动返回5个样本的中位数。 */
  g_adc_status = AD7682_ReadFourChannelsSamples(&g_ad7682, adc_samples, 10U);
  if (g_adc_status == AD7682_OK)
  {
    adc[0] = AD7682_Median5(adc_samples[0]);
    adc[1] = AD7682_Median5(adc_samples[1]);
    adc[2] = AD7682_Median5(adc_samples[2]);
    adc[3] = AD7682_Median5(adc_samples[3]);
    (void)snprintf(adc_text, sizeof(adc_text),
                   "%u,%u,%u,%u", (unsigned)adc[0], (unsigned)adc[1],
                   (unsigned)adc[2], (unsigned)adc[3]);
    (void)snprintf(adc_raw_text, sizeof(adc_raw_text),
                   "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
                   (unsigned)adc_samples[0][0], (unsigned)adc_samples[0][1],
                   (unsigned)adc_samples[0][2], (unsigned)adc_samples[0][3],
                   (unsigned)adc_samples[0][4], (unsigned)adc_samples[1][0],
                   (unsigned)adc_samples[1][1], (unsigned)adc_samples[1][2],
                   (unsigned)adc_samples[1][3], (unsigned)adc_samples[1][4],
                   (unsigned)adc_samples[2][0], (unsigned)adc_samples[2][1],
                   (unsigned)adc_samples[2][2], (unsigned)adc_samples[2][3],
                   (unsigned)adc_samples[2][4], (unsigned)adc_samples[3][0],
                   (unsigned)adc_samples[3][1], (unsigned)adc_samples[3][2],
                   (unsigned)adc_samples[3][3], (unsigned)adc_samples[3][4]);
  }
  else
  {
    (void)snprintf(adc_text, sizeof(adc_text),
                   "%u,%u,%u,%u",
                   (unsigned)SENSOR_INVALID_ADC, (unsigned)SENSOR_INVALID_ADC,
                   (unsigned)SENSOR_INVALID_ADC, (unsigned)SENSOR_INVALID_ADC);
    (void)snprintf(adc_raw_text, sizeof(adc_raw_text),
                   "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
                   (unsigned)SENSOR_INVALID_ADC, (unsigned)SENSOR_INVALID_ADC,
                   (unsigned)SENSOR_INVALID_ADC, (unsigned)SENSOR_INVALID_ADC,
                   (unsigned)SENSOR_INVALID_ADC, (unsigned)SENSOR_INVALID_ADC,
                   (unsigned)SENSOR_INVALID_ADC, (unsigned)SENSOR_INVALID_ADC,
                   (unsigned)SENSOR_INVALID_ADC, (unsigned)SENSOR_INVALID_ADC,
                   (unsigned)SENSOR_INVALID_ADC, (unsigned)SENSOR_INVALID_ADC,
                   (unsigned)SENSOR_INVALID_ADC, (unsigned)SENSOR_INVALID_ADC,
                   (unsigned)SENSOR_INVALID_ADC, (unsigned)SENSOR_INVALID_ADC,
                   (unsigned)SENSOR_INVALID_ADC, (unsigned)SENSOR_INVALID_ADC,
                   (unsigned)SENSOR_INVALID_ADC, (unsigned)SENSOR_INVALID_ADC);
  }

  now = g_record_timer_seconds;
  interval = DataLogger_GetInterval();
  if ((now - g_record_last_second) >= interval)
  {
    (void)memset(&log_sample, 0, sizeof(log_sample));
    (void)PCF8563_ReadDateTime(&hi2c1, &log_sample.datetime, 100U);
    log_sample.temperature_c = sht_temperature;
    log_sample.humidity_rh = sht_humidity;
    /* 历史记录保存各气压计的滑动平均值，而不是单次原始读数。
       窗口未满时由 PressureAverage_Update() 使用已有有效样本数计算。 */
    log_sample.pressure_u7_hpa = u7_pressure_average;
    log_sample.pressure_u11_hpa = u11_pressure_average;
    /* 历史记录结构每个通道只保留一个 ADC 字段，使用本轮中值结果。 */
    log_sample.adc[0] = adc[0];
    log_sample.adc[1] = adc[1];
    log_sample.adc[2] = adc[2];
    log_sample.adc[3] = adc[3];
    if ((g_w25q_ready != 0U) &&
        DataLogger_Append(&log_sample, 3000U) != HAL_OK)
    {
      if (g_rtc_setting_mode) { Serial_Write("LOG WRITE ERROR\r\n"); }
    }
    /* 按原定时基准推进，不把采样/写入耗时累计进记录周期。
       超时后不伪造缺失时刻的历史样本。 */
    g_record_last_second += ((now - g_record_last_second) / interval) * interval;
  }

  /* 采样或 Flash 写入期间到达的命令，在下一行实时输出前处理。 */
  Serial_ProcessCommand();
  if (g_rtc_setting_mode != 0U)
  {
    return;
  }
  (void)snprintf(line, sizeof(line), "samples: %.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%s,%.3f,%.3f,%.3f,%s\r\n",
                 (double)battery_voltage, (double)sht_temperature,
                 (double)sht_humidity, (double)u7_pressure,
                 (double)u7_temperature, (double)u11_pressure,
                 (double)u11_temperature, adc_text,
                 (double)u7_pressure_average,
                 (double)u11_pressure_average,
                 (double)pressure_average, adc_raw_text);
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
