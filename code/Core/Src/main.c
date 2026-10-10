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
#include <stdio.h>
#include <string.h>

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
static void Debug_RunTests(void);
static void Debug_FlashReadWriteTest(void);
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
  /* Run once per boot, not every second, to limit flash erase wear. */
  Debug_FlashReadWriteTest();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    Serial_ProcessCommand();
    if (!g_rtc_setting_mode)
    {
      Debug_RunTests();
    }
    Serial_ServiceDelay(1000);
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

/* PA15/LED2 电量指示：高电平点亮，低电平熄灭。 */
static void BatteryLed_Update(float voltage)
{
  static uint32_t last_toggle_tick;
  static uint8_t blink_state;
  uint32_t now = HAL_GetTick();

  if (voltage >= 4.0f)
  {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
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
                        blink_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
  }
  else
  {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
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

static void Serial_PrintSettingHelp(void){Debug_Print("SETTING MODE\r\nSET YY-MM-DD HH:MM:SS\r\nexit\r\n");}
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
  if(g_rtc_setting_mode){if(strlen(trimmed)>=3U&&(Serial_ToLower(trimmed[0])=='s')&&(Serial_ToLower(trimmed[1])=='e')&&(Serial_ToLower(trimmed[2])=='t'))Serial_HandleSetTime(trimmed);else Debug_Print("Unknown setting command\r\n");}
}

static void Serial_ServiceDelay(uint32_t delay_ms){uint32_t elapsed=0;while(elapsed<delay_ms){Serial_ProcessCommand();HAL_Delay(10);elapsed+=10U;}}

static void Debug_FlashReadWriteTest(void){uint8_t tx[16],rx[16],id[3];uint32_t i;HAL_StatusTypeDef s;char l[96];s=W25Q128_ReadJedec(&hspi2,id,100);if(s!=HAL_OK||id[0]!=0xEF||id[1]!=0x40||id[2]!=0x18){Debug_Print("Flash test: SKIP\r\n");return;}s=W25Q128_EraseSector(&hspi2,0xFFF000,3000);if(s!=HAL_OK){Debug_Print("Flash erase: FAIL\r\n");return;}for(i=0;i<sizeof(tx);i++)tx[i]=(uint8_t)(i^0xA5);s=W25Q128_Write(&hspi2,0xFFF000,tx,sizeof(tx),100);if(s==HAL_OK)s=W25Q128_Read(&hspi2,0xFFF000,rx,sizeof(rx),100);for(i=0;i<sizeof(tx)&&s==HAL_OK;i++)if(tx[i]!=rx[i])break;(void)snprintf(l,sizeof(l),"Flash RW test: %s\r\n",(s==HAL_OK&&i==sizeof(tx))?"PASS":"FAIL");Debug_Print(l);(void)W25Q128_EraseSector(&hspi2,0xFFF000,3000);}
static void Debug_RunTests(void){char l[160];uint32_t b;float bv,t=0,r=0;uint8_t id[3]={0},vl=0;PCF8563_DateTime dt={0};HAL_StatusTypeDef s;uint8_t adbuf=0;(void)adbuf;b=BatteryAdc_ReadRaw(&hadc1,10);bv=BatteryAdc_CalibratedVoltage(b);BatteryLed_Update(bv);(void)snprintf(l,sizeof(l),"Battery ADC raw: %lu voltage: %.3f V\r\n",(unsigned long)b,(double)bv);Debug_Print(l);s=SHT41_Read(&hi2c1,&t,&r,100);(void)snprintf(l,sizeof(l),"SHT41: status=%d T=%.2f C RH=%.2f %%\r\n",(int)s,(double)t,(double)r);Debug_Print(l);(void)PCF8563_GetVoltageLow(&hi2c1,&vl,100);s=PCF8563_ReadDateTime(&hi2c1,&dt,100);(void)snprintf(l,sizeof(l),"PCF8563: status=%d 20%02u-%02u-%02u %02u:%02u:%02u VL=%u\r\n",(int)s,dt.year,dt.month,dt.day,dt.hours,dt.minutes,dt.seconds,vl);Debug_Print(l);s=W25Q128_ReadJedec(&hspi2,id,100);(void)snprintf(l,sizeof(l),"W25Q128 JEDEC: status=%d ID=%02X %02X %02X\r\n",(int)s,id[0],id[1],id[2]);Debug_Print(l);}

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
