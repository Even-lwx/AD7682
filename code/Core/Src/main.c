/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
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

#define BOOT_INIT_RETRIES 3U
#define BOOT_INIT_TIMEOUT 100U
#define SAMPLE_PERIOD_MS 20U

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile uint8_t g_uart_rx_byte;
volatile uint8_t g_uart_line_ready;
volatile uint8_t g_uart_line_overflow;
volatile uint8_t g_rtc_setting_mode;
static char g_uart_line[64];
static uint8_t g_uart_line_length;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void Debug_Print(const char *text);
static void Debug_BootInitializeDevices(void);
static void Debug_SendSamples(void);
static void SimulatedAdc_Read(uint16_t adc[4]);
static void Serial_DumpLogs(void);
static uint8_t Serial_ParseUint32(const char *text, uint32_t *value);
static void Serial_ProcessCommand(void);
static void Serial_ServiceDelay(uint32_t delay_ms);
static void BatteryLed_Update(float voltage);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  HAL_UART_Receive_IT(&huart1, (uint8_t *)&g_uart_rx_byte, 1);
  Debug_BootInitializeDevices();
  (void)DataLogger_Init(&hspi2, BOOT_INIT_TIMEOUT);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    Serial_ProcessCommand();
    /* setting 模式只关闭实时串口输出，传感器采样和历史记录仍持续运行。 */
    Debug_SendSamples();
    Serial_ServiceDelay(SAMPLE_PERIOD_MS);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
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

  /** Initializes the CPU, AHB and APB buses clocks
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

static void Debug_Print(const char *text){HAL_UART_Transmit(&huart1,(uint8_t *)text,(uint16_t)strlen(text),100);}

/*
 * 上电设备探测和初始化。
 *
 * MX_* 外设初始化由 CubeMX 生成，失败时会进入 Error_Handler；这里对
 * 外部器件增加有限重试，避免器件电源/复位释放稍慢时第一次访问失败。
 * 重试只执行无副作用的探测/配置，不在这里重复 Flash 擦除测试。
 */
static void Debug_BootInitializeDevices(void)
{
  uint8_t retry;
  uint8_t id[3] = {0U};
  uint8_t voltage_low = 0U;
  float temperature = 0.0f;
  float humidity = 0.0f;
  PCF8563_DateTime datetime = {0};
  HAL_StatusTypeDef status;

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
      break;
    }
    HAL_Delay(10U);
  }
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
      break;
    }
    HAL_Delay(10U);
  }
  status = HAL_ERROR;
  for (retry = 0U; retry < BOOT_INIT_RETRIES; retry++)
  {
    status = W25Q128_ReadJedec(&hspi2, id, BOOT_INIT_TIMEOUT);
    if (status == HAL_OK && id[0] == 0xEFU && id[1] == 0x40U && id[2] == 0x18U)
    {
      break;
    }
    status = HAL_ERROR;
    HAL_Delay(10U);
  }
  status = HAL_ERROR;
  for (retry = 0U; retry < BOOT_INIT_RETRIES; retry++)
  {
    status = LPS22HH_InitDevice(&hi2c1, &g_lps22hh_u7, BOOT_INIT_TIMEOUT);
    if (status == HAL_OK)
    {
      break;
    }
    HAL_Delay(10U);
  }
  status = HAL_ERROR;
  for (retry = 0U; retry < BOOT_INIT_RETRIES; retry++)
  {
    status = LPS22HH_InitDevice(&hi2c1, &g_lps22hh_u11, BOOT_INIT_TIMEOUT);
    if (status == HAL_OK)
    {
      break;
    }
    HAL_Delay(10U);
  }
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

static void Serial_PrintSettingHelp(void){Debug_Print("SETTING MODE\r\nSET YY-MM-DD HH:MM:SS\r\nlog info\r\nlog interval SECONDS\r\nlog order old\r\nlog order new\r\nlog dump\r\nlog clear\r\nexit\r\n");}
static void Serial_HandleSetTime(const char *command)
{
  PCF8563_DateTime requested={0},actual={0};uint8_t vl=0;HAL_StatusTypeDef s;char line[128];
  if(!Serial_ParseSetTime(command,&requested)){Debug_Print("RTC SET ERROR: use SET YY-MM-DD HH:MM:SS\r\n");return;}
  s=PCF8563_WriteDateTime(&hi2c1,&requested,100);if(s==HAL_OK)s=PCF8563_SetRunning(&hi2c1,1U,100);
  if(s!=HAL_OK){(void)snprintf(line,sizeof(line),"RTC SET ERROR: status=%d\r\n",(int)s);Debug_Print(line);return;}
  s=PCF8563_GetVoltageLow(&hi2c1,&vl,100);if(s==HAL_OK)s=PCF8563_ReadDateTime(&hi2c1,&actual,100);
  if(s!=HAL_OK){(void)snprintf(line,sizeof(line),"RTC SET VERIFY ERROR: status=%d\r\n",(int)s);Debug_Print(line);return;}
  (void)snprintf(line,sizeof(line),"RTC SET OK: 20%02u-%02u-%02u %02u:%02u:%02u VL=%u\r\n",actual.year,actual.month,actual.day,actual.hours,actual.minutes,actual.seconds,vl);Debug_Print(line);
}

static void Serial_ProcessCommand(void)
{
  char command[64];char *trimmed;uint8_t overflow;
  if(!g_uart_line_ready)return;
  __disable_irq();memcpy(command,g_uart_line,sizeof(command));overflow=g_uart_line_overflow;g_uart_line_ready=0;__enable_irq();command[sizeof(command)-1U]=0;
  g_uart_line_overflow=0U;
  if(overflow){Debug_Print("Command error: line too long\r\n");return;}
  trimmed=Serial_Trim(command);if(*trimmed==0)return;
  if(Serial_Equals(trimmed,"setting")){g_rtc_setting_mode=1U;Serial_PrintSettingHelp();return;}
  if(Serial_Equals(trimmed,"exit")){if(g_rtc_setting_mode){g_rtc_setting_mode=0U;Debug_Print("EXIT SETTING MODE\r\n");}return;}
  if(g_rtc_setting_mode)
  {
    if(Serial_Equals(trimmed,"log dump"))
    {
      Serial_DumpLogs();
    }
    else if(Serial_Equals(trimmed,"log info"))
    {
      char line[128];
      (void)snprintf(line,sizeof(line),"LOG INFO: count=%lu capacity=%lu interval=%lus order=%s\r\n",
                     (unsigned long)DataLogger_Count(),
                     (unsigned long)DataLogger_Capacity(),
                     (unsigned long)DataLogger_GetInterval(),
                     DataLogger_GetDumpNewestFirst() ? "newest-first" : "oldest-first");
      Debug_Print(line);
    }
    else if(strncmp(trimmed,"log interval ",13U)==0)
    {
      uint32_t seconds;
      if(Serial_ParseUint32(&trimmed[13],&seconds) &&
         DataLogger_SetInterval(seconds,3000U)==HAL_OK)
      {
        Debug_Print("LOG INTERVAL OK\r\n");
      }
      else
      {
        Debug_Print("LOG INTERVAL ERROR: use log interval 1..86400\r\n");
      }
    }
    else if(Serial_Equals(trimmed,"log order old") || Serial_Equals(trimmed,"log order oldest"))
    {
      Debug_Print(DataLogger_SetDumpNewestFirst(0U,3000U)==HAL_OK ?
                  "LOG ORDER OK: oldest-first\r\n" : "LOG ORDER ERROR\r\n");
    }
    else if(Serial_Equals(trimmed,"log order new") || Serial_Equals(trimmed,"log order newest"))
    {
      Debug_Print(DataLogger_SetDumpNewestFirst(1U,3000U)==HAL_OK ?
                  "LOG ORDER OK: newest-first\r\n" : "LOG ORDER ERROR\r\n");
    }
    else if(Serial_Equals(trimmed,"log clear"))
    {
      Debug_Print(DataLogger_Clear(3000U)==HAL_OK ?
                  "LOG CLEAR OK\r\n" : "LOG CLEAR ERROR\r\n");
    }
    else if(strlen(trimmed)>=3U && Serial_ToLower(trimmed[0])=='s' &&
            Serial_ToLower(trimmed[1])=='e' && Serial_ToLower(trimmed[2])=='t')
    {
      Serial_HandleSetTime(trimmed);
    }
    else
    {
      Debug_Print("Unknown setting command\r\n");
    }
  }
}

static void Serial_ServiceDelay(uint32_t delay_ms){uint32_t elapsed=0;while(elapsed<delay_ms){Serial_ProcessCommand();HAL_Delay(10);elapsed+=10U;}}

static void Serial_DumpLogs(void)
{
  uint32_t i;
  uint32_t count = DataLogger_Count();
  uint16_t year;
  char line[180];
  DataLogger_Sample sample;

  for (i = 0U; i < count; i++)
  {
    uint32_t ordinal = DataLogger_GetDumpNewestFirst() ? (count - 1U - i) : i;
    /* 大量历史数据导出也必须给采样和自动记录调度机会。
       dump 仅在 setting 内调用，因此不会混入实时输出。 */
    Debug_SendSamples();
    if (DataLogger_Read(ordinal, &sample, 100U) != HAL_OK)
    {
      continue;
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
    Debug_Print(line);
  }
}

static void SimulatedAdc_Read(uint16_t adc[4])
{
  adc[0] = 1000U;
  adc[1] = 2000U;
  adc[2] = 3000U;
  adc[3] = 4000U;
}

/* FireWater 数据行：ch0 电池电压，ch1/2 SHT41 温湿度，
 * ch3/4 U7 压力/温度，ch5/6 U11 压力/温度，ch7..10 模拟 ADC。 */
static void Debug_SendSamples(void)
{
  static uint32_t last_log_tick;
  uint32_t battery_raw;
  uint32_t now;
  uint16_t adc[4];
  float battery_voltage;
  float sht_temperature = 0.0f;
  float sht_humidity = 0.0f;
  float u7_pressure = 0.0f;
  float u7_temperature = 0.0f;
  float u11_pressure = 0.0f;
  float u11_temperature = 0.0f;
  char line[128];
  DataLogger_Sample log_sample;

  battery_raw = BatteryAdc_ReadRaw(&hadc1, 10U);
  battery_voltage = BatteryAdc_CalibratedVoltage(battery_raw);
  BatteryLed_Update(battery_voltage);
  if (SHT41_Read(&hi2c1, &sht_temperature, &sht_humidity, 100U) != HAL_OK)
  {
    sht_temperature = 0.0f;
    sht_humidity = 0.0f;
  }
  if (LPS22HH_ReadDevice(&hi2c1, &g_lps22hh_u7,
                         &u7_pressure, &u7_temperature, 100U) != HAL_OK)
  {
    u7_pressure = 0.0f;
    u7_temperature = 0.0f;
  }
  if (LPS22HH_ReadDevice(&hi2c1, &g_lps22hh_u11,
                         &u11_pressure, &u11_temperature, 100U) != HAL_OK)
  {
    u11_pressure = 0.0f;
    u11_temperature = 0.0f;
  }
  SimulatedAdc_Read(adc);

  now = HAL_GetTick();
  if ((now - last_log_tick) >= (DataLogger_GetInterval() * 1000UL))
  {
    if (PCF8563_ReadDateTime(&hi2c1, &log_sample.datetime, 100U) == HAL_OK)
    {
      log_sample.temperature_c = sht_temperature;
      log_sample.humidity_rh = sht_humidity;
      log_sample.pressure_u7_hpa = u7_pressure;
      log_sample.pressure_u11_hpa = u11_pressure;
      log_sample.adc[0] = adc[0];
      log_sample.adc[1] = adc[1];
      log_sample.adc[2] = adc[2];
      log_sample.adc[3] = adc[3];
      (void)DataLogger_Append(&log_sample, 3000U);
    }
    last_log_tick = now;
  }

  if (g_rtc_setting_mode != 0U)
  {
    return;
  }
  (void)snprintf(line, sizeof(line), "samples: %.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u,%u,%u,%u\r\n",
                 (double)battery_voltage, (double)sht_temperature,
                 (double)sht_humidity, (double)u7_pressure,
                 (double)u7_temperature, (double)u11_pressure,
                 (double)u11_temperature, (unsigned)adc[0],
                 (unsigned)adc[1], (unsigned)adc[2], (unsigned)adc[3]);
  Debug_Print(line);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
