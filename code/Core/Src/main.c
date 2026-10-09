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
#include "i2c.h"
#include "stdio.h"
#include "string.h"
#include "ad7682.h"
#include "battery_adc.h"
#include "sht41.h"
#include "pcf8563.h"
#include "w25q128.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void Debug_Print(const char *text);
static void Debug_RunTests(void);
/* USER CODE BEGIN PFP */

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
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  Debug_Print("\r\nAD7682 board driver test start\r\n");
  {
    char clock_line[128];
    (void)snprintf(clock_line, sizeof(clock_line),
                   "Clock: SYS=%lu HCLK=%lu HSE_READY=%u RCC_CR=0x%08lX\r\n",
                   (unsigned long)HAL_RCC_GetSysClockFreq(),
                   (unsigned long)HAL_RCC_GetHCLKFreq(),
                   (unsigned)__HAL_RCC_GET_FLAG(RCC_FLAG_HSERDY),
                   (unsigned long)RCC->CR);
    Debug_Print(clock_line);
  }
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    Debug_RunTests();
    HAL_Delay(1000);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

static void Debug_Print(const char *text)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), 100);
}

static void Debug_RunTests(void)
{
  char line[160];
  uint16_t ad_code = 0;
  uint32_t battery_code;
  uint8_t jedec[3] = {0};
  float temperature = 0.0f, humidity = 0.0f;
  uint8_t voltage_low = 0;
  PCF8563_DateTime rtc = {0};
  AD7682_Handle ad = {0};
  AD7682_Status ad_status;
  HAL_StatusTypeDef status;

  AD7682_Init(&ad, &hspi1, 3.3f); /* Used only for handle setup; raw code is printed. */
  ad_status = AD7682_ReadRaw(&ad, &ad_code, 10);
  (void)snprintf(line, sizeof(line), "AD7682 raw: status=%d code=%u\r\n",
                 (int)ad_status, (unsigned)ad_code);
  Debug_Print(line);

  battery_code = BatteryAdc_ReadRaw(&hadc1, 10);
  (void)snprintf(line, sizeof(line), "Battery ADC raw: %lu\r\n",
                 (unsigned long)battery_code);
  Debug_Print(line);

  status = SHT41_Read(&hi2c1, &temperature, &humidity, 100);
  (void)snprintf(line, sizeof(line), "SHT41: status=%d T=%.2f C RH=%.2f %%\r\n",
                 (int)status, (double)temperature, (double)humidity);
  Debug_Print(line);

  (void)PCF8563_GetVoltageLow(&hi2c1, &voltage_low, 100);
  status = PCF8563_ReadDateTime(&hi2c1, &rtc, 100);
  (void)snprintf(line, sizeof(line),
                 "PCF8563: status=%d 20%02u-%02u-%02u %02u:%02u:%02u VL=%u\r\n",
                 (int)status, (unsigned)rtc.year, (unsigned)rtc.month,
                 (unsigned)rtc.day, (unsigned)rtc.hours, (unsigned)rtc.minutes,
                 (unsigned)rtc.seconds, (unsigned)voltage_low);
  Debug_Print(line);
  status = W25Q128_ReadJedec(&hspi2, jedec, 100);
  (void)snprintf(line, sizeof(line), "W25Q128 JEDEC: status=%d ID=%02X %02X %02X\r\n",
                 (int)status, jedec[0], jedec[1], jedec[2]);
  Debug_Print(line);
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
  /* Board oscillator: 8 MHz crystal on PF0/PF1, as shown in Sheet1.SchDoc. */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
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
