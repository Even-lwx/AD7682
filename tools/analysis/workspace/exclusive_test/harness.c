
#include <stdint.h>
#include <string.h>
#define TIM_CR1_CEN 1U
#define TIM3_IRQn 1
#define TIM14_IRQn 2
typedef struct { uint32_t CR1,CNT,SR; } Timer;
static Timer adc_timer,scheduler;
#define TIM3 (&adc_timer)
#define TIM14 (&scheduler)
static uint8_t g_dump_exclusive,g_dump_adc_was_running,g_dump_scheduler_was_running;
static uint32_t g_dump_pause_tick,g_record_timer_milliseconds,g_record_timer_seconds;
static uint32_t g_record_last_millisecond,g_record_due;
static uint16_t g_sensor_timer_milliseconds,g_sensor_sample_elapsed_ms;
static uint8_t g_sensor_sample_request,g_sht_filter_valid;
static uint32_t g_adc_temporal_filters[8],g_u7_pressure_average,g_u11_pressure_average;
static uint32_t tick,primask,commits,paused,resumed,clears;
static uint32_t HAL_GetTick(void){return tick;}
static uint32_t __get_PRIMASK(void){return primask;}
static void __disable_irq(void){primask=1;}
static void __set_PRIMASK(uint32_t p){primask=p;}
static void HAL_NVIC_ClearPendingIRQ(int irq){(void)irq;clears++;}
static void RecordWritePending(void){commits++;g_record_due=0;}
static void Serial_Write(const char *text){if(strstr(text,"PAUSED"))paused++;if(strstr(text,"RESUMED"))resumed++;}
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
}static void Serial_DumpResumeSampling(void)
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
#define CHECK(c) do{if(!(c))return __LINE__;}while(0)
int test_main(void){
 TIM3->CR1=TIM14->CR1=1;TIM3->CNT=20;TIM14->CNT=4;
 g_record_timer_milliseconds=1900;g_record_timer_seconds=1;g_sensor_timer_milliseconds=900;
 g_record_due=1;g_sensor_sample_request=1;g_sht_filter_valid=1;
 memset(g_adc_temporal_filters,1,sizeof(g_adc_temporal_filters));
 g_u7_pressure_average=g_u11_pressure_average=99;tick=100;
 Serial_DumpPauseSampling();
 CHECK(g_dump_exclusive&&TIM3->CR1==0&&TIM14->CR1==0);
 CHECK(!g_record_due&&!g_sensor_sample_request&&commits==1&&paused==1&&!primask);
 Serial_DumpPauseSampling();CHECK(commits==1&&paused==1);
 tick=3350;Serial_DumpResumeSampling();
 CHECK(!g_dump_exclusive&&TIM3->CR1==1&&TIM14->CR1==1);
 CHECK(g_record_timer_milliseconds==5150&&g_record_timer_seconds==5&&g_sensor_timer_milliseconds==150);
 CHECK(g_record_last_millisecond==5150&&!g_record_due&&g_sensor_sample_request==1);
 CHECK(!g_sht_filter_valid&&!g_adc_temporal_filters[0]&&!g_u7_pressure_average&&!g_u11_pressure_average);
 CHECK(!TIM3->CNT&&!TIM14->CNT&&resumed==1&&!primask);
 Serial_DumpResumeSampling();CHECK(resumed==1&&g_record_timer_milliseconds==5150);
 /* Preserve originally stopped timers and tick rollover. */
 TIM3->CR1=0;TIM14->CR1=1;tick=0xFFFFFFF0U;
 Serial_DumpPauseSampling();tick=0x20;Serial_DumpResumeSampling();
 CHECK(TIM3->CR1==0&&TIM14->CR1==1&&g_record_timer_milliseconds==5198);
 return 0;
}
