
#include <stdint.h>
#include <string.h>
#define UART_RX_DMA_SIZE 128U
#define RESET 0
#define DMA_FLAG_TC1 1
#define HAL_UART_ERROR_PE 1
#define HAL_UART_ERROR_FE 2
#define HAL_UART_ERROR_NE 4
#define HAL_UART_ERROR_ORE 8
typedef int HAL_StatusTypeDef;
typedef struct{uint32_t ErrorCode;} UART_HandleTypeDef;
UART_HandleTypeDef huart1;
int hdma_usart1_rx;
static volatile uint8_t g_uart_dma_buffer[128];
static uint16_t g_uart_dma_read;
static volatile uint32_t g_uart_dma_wraps;
static uint32_t g_uart_dma_consumed,g_uart_rx_count;
static volatile uint32_t g_uart_parity_errors,g_uart_frame_errors,g_uart_noise_errors,g_uart_overrun_errors;
static uint8_t g_uart_line_length,g_uart_line_overflow,g_uart_line_ready,g_uart_rx_restart;
static char g_uart_line[64];
static uint32_t counter=128,tc;
static uint32_t __get_PRIMASK(void){return 0;}
#define __disable_irq() ((void)0)
static void __set_PRIMASK(uint32_t v){(void)v;}
#define __HAL_DMA_GET_COUNTER(h) counter
#define __HAL_DMA_GET_FLAG(h,f) tc
static int HAL_UART_Receive_DMA(UART_HandleTypeDef *h,uint8_t *b,uint16_t n){(void)h;(void)b;counter=n;tc=0;return 0;}
static HAL_StatusTypeDef Serial_StartReceive(void)
{
  g_uart_dma_read = 0U;
  g_uart_dma_wraps = 0U;
  g_uart_dma_consumed = 0U;
  return HAL_UART_Receive_DMA(&huart1, (uint8_t *)g_uart_dma_buffer, UART_RX_DMA_SIZE);
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart1) { g_uart_dma_wraps++; }
}
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
#define CHECK(c) do{if(!(c))return __LINE__;}while(0)
static void reset(void){
 g_uart_rx_restart=g_uart_line_ready=g_uart_line_length=g_uart_line_overflow=0;
 g_uart_parity_errors=g_uart_frame_errors=g_uart_noise_errors=g_uart_overrun_errors=0;
 g_uart_rx_count=0;huart1.ErrorCode=0;Serial_StartReceive();
}
static void feed(const char *s){while(*s){
 g_uart_dma_buffer[128-counter]=(uint8_t)*s++;
 if(--counter==0){counter=128;HAL_UART_RxCpltCallback(&huart1);}
}}
int test_main(void){
 unsigned i;char longline[90];reset();
 feed("setting\r\n");Serial_PollReceive();CHECK(g_uart_line_ready&&!strcmp(g_uart_line,"setting"));
 CHECK(g_uart_rx_count==8);g_uart_line_ready=0;Serial_PollReceive();CHECK(!g_uart_line_ready&&g_uart_rx_count==9);
 reset();feed("setting\r\nlog info\r\nexit\r\n");
 Serial_PollReceive();CHECK(g_uart_line_ready&&!strcmp(g_uart_line,"setting"));g_uart_line_ready=0;
 Serial_PollReceive();CHECK(g_uart_line_ready&&!strcmp(g_uart_line,"log info"));g_uart_line_ready=0;
 Serial_PollReceive();CHECK(g_uart_line_ready&&!strcmp(g_uart_line,"exit"));g_uart_line_ready=0;
 Serial_PollReceive();CHECK(!g_uart_line_ready);
 reset();for(i=0;i<40;i++){
 feed("setting\r\n");Serial_PollReceive();CHECK(g_uart_line_ready&&!strcmp(g_uart_line,"setting"));
 g_uart_line_ready=0;Serial_PollReceive();CHECK(!g_uart_rx_restart);
 }CHECK(g_uart_dma_wraps==2&&g_uart_rx_count==360);
 reset();for(i=0;i<80;i++)longline[i]='x';longline[80]='\r';longline[81]=0;
 feed(longline);Serial_PollReceive();CHECK(g_uart_line_ready&&g_uart_line_overflow&&strlen(g_uart_line)==63);
 reset();feed("log dump hpa 2026-11-02 09:00:00 2026-11-02 10:30:00\r\n");Serial_PollReceive();
 CHECK(g_uart_line_ready&&!g_uart_line_overflow&&!strcmp(g_uart_line,"log dump hpa 2026-11-02 09:00:00 2026-11-02 10:30:00"));
 reset();feed("set");Serial_PollReceive();CHECK(!g_uart_line_ready);feed("ting\n");Serial_PollReceive();CHECK(g_uart_line_ready&&!strcmp(g_uart_line,"setting"));
 reset();feed("setting\r\n");g_uart_rx_restart=1;Serial_PollReceive();CHECK(!g_uart_line_ready);
 reset();huart1.ErrorCode=HAL_UART_ERROR_PE|HAL_UART_ERROR_FE|HAL_UART_ERROR_ORE|HAL_UART_ERROR_NE;
 HAL_UART_ErrorCallback(&huart1);CHECK(g_uart_rx_restart&&g_uart_parity_errors==1&&g_uart_frame_errors==1&&g_uart_overrun_errors==1&&g_uart_noise_errors==1);
 reset();g_uart_dma_wraps=2;Serial_PollReceive();CHECK(g_uart_rx_restart&&g_uart_overrun_errors==1);
 /* A TC interrupt pending before the snapshot must be counted once. */
 reset();g_uart_dma_consumed=120;g_uart_dma_read=120;counter=124;tc=1;
 for(i=0;i<12;i++)g_uart_dma_buffer[(120+i)%128]='a';Serial_PollReceive();CHECK(g_uart_dma_consumed==132&&!g_uart_rx_restart);
 g_uart_dma_wraps=1;tc=0;Serial_PollReceive();CHECK(g_uart_dma_consumed==132&&g_uart_line_length==12);
 reset();g_uart_dma_consumed=128;g_uart_dma_read=0;counter=0;tc=1;Serial_PollReceive();CHECK(g_uart_dma_consumed==128&&!g_uart_rx_restart);
 return 0;
}
