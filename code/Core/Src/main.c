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
static void Debug_FlashReadWriteTest(void);
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
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  /* Run once per boot, not every second, to limit flash erase wear. */
  Debug_FlashReadWriteTest();

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

static void Debug_Print(const char *text){HAL_UART_Transmit(&huart1,(uint8_t *)text,(uint16_t)strlen(text),100);}
static void Debug_FlashReadWriteTest(void){uint8_t tx[16],rx[16],id[3];uint32_t i;HAL_StatusTypeDef s;char l[96];s=W25Q128_ReadJedec(&hspi2,id,100);if(s!=HAL_OK||id[0]!=0xEF||id[1]!=0x40||id[2]!=0x18){Debug_Print("Flash test: SKIP\r\n");return;}s=W25Q128_EraseSector(&hspi2,0xFFF000,3000);if(s!=HAL_OK){Debug_Print("Flash erase: FAIL\r\n");return;}for(i=0;i<sizeof(tx);i++)tx[i]=(uint8_t)(i^0xA5);s=W25Q128_Write(&hspi2,0xFFF000,tx,sizeof(tx),100);if(s==HAL_OK)s=W25Q128_Read(&hspi2,0xFFF000,rx,sizeof(rx),100);for(i=0;i<sizeof(tx)&&s==HAL_OK;i++)if(tx[i]!=rx[i])break;(void)snprintf(l,sizeof(l),"Flash RW test: %s\r\n",(s==HAL_OK&&i==sizeof(tx))?"PASS":"FAIL");Debug_Print(l);(void)W25Q128_EraseSector(&hspi2,0xFFF000,3000);}
static void Debug_RunTests(void){char l[160];uint32_t b;float bv,t=0,r=0;uint8_t id[3]={0},vl=0;PCF8563_DateTime dt={0};HAL_StatusTypeDef s;uint8_t adbuf=0;(void)adbuf;b=BatteryAdc_ReadRaw(&hadc1,10);bv=BatteryAdc_CalibratedVoltage(b);(void)snprintf(l,sizeof(l),"Battery ADC raw: %lu voltage: %.3f V\r\n",(unsigned long)b,(double)bv);Debug_Print(l);s=SHT41_Read(&hi2c1,&t,&r,100);(void)snprintf(l,sizeof(l),"SHT41: status=%d T=%.2f C RH=%.2f %%\r\n",(int)s,(double)t,(double)r);Debug_Print(l);(void)PCF8563_GetVoltageLow(&hi2c1,&vl,100);s=PCF8563_ReadDateTime(&hi2c1,&dt,100);(void)snprintf(l,sizeof(l),"PCF8563: status=%d 20%02u-%02u-%02u %02u:%02u:%02u VL=%u\r\n",(int)s,dt.year,dt.month,dt.day,dt.hours,dt.minutes,dt.seconds,vl);Debug_Print(l);s=W25Q128_ReadJedec(&hspi2,id,100);(void)snprintf(l,sizeof(l),"W25Q128 JEDEC: status=%d ID=%02X %02X %02X\r\n",(int)s,id[0],id[1],id[2]);Debug_Print(l);}
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
