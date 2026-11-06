from pathlib import Path
import re,subprocess
from elftools.elf.elffile import ELFFile
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_THUMB,UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_R0
root=Path.cwd();out=root/'tools/analysis/workspace/uart_dma_test';out.mkdir(exist_ok=True)
s=(root/'code/Core/Src/main.c').read_text(encoding='utf-8-sig')
def fn(name):
 m=re.search(r'^(?:static )?(?:void|HAL_StatusTypeDef) '+name+r'\([^;{}]*\)\s*\{',s,re.M);assert m,name
 a=s.index('{',m.start())+1;i=a;depth=1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[m.start():i]
prefix=r'''
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
'''
code='\n'.join(fn(n) for n in ['Serial_StartReceive','HAL_UART_RxCpltCallback','Serial_PollReceive','HAL_UART_ErrorCallback'])
tests=r'''
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
'''
(out/'harness.c').write_text(prefix+code+tests)
b=Path('C:/Keil_v5/ARM/ARMCC/bin')
subprocess.run([str(b/'armcc.exe'),'--cpu=Cortex-M0','--c99','-O0','-g','-c',str(out/'harness.c'),'-o',str(out/'harness.o')],check=True)
subprocess.run([str(b/'armlink.exe'),'--cpu=Cortex-M0','--entry=test_main','--ro_base=0x10000','--rw_base=0x20000000','--libpath=C:/Keil_v5/ARM/ARMCC/lib',str(out/'harness.o'),'-o',str(out/'harness.axf')],check=True)
u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);u.mem_map(0x10000,0x100000);u.mem_map(0x20000000,0x100000)
with (out/'harness.axf').open('rb') as f:
 e=ELFFile(f)
 for seg in e.iter_segments():
  if seg['p_type']=='PT_LOAD' and seg['p_filesz']:u.mem_write(seg['p_vaddr'],seg.data())
 entry=e.header['e_entry']
u.reg_write(UC_ARM_REG_SP,0x200F0000);u.reg_write(UC_ARM_REG_LR,0xF0001)
u.emu_start(entry|1,0xF0000,count=1000000)
r=u.reg_read(UC_ARM_REG_R0);assert r==0,f'Assertion at harness.c:{r}'
print('PASS: firmware DMA parser: CRLF, queued commands, long and split commands, repeated ring wrap, overflow, errors, pending TC and counter reload.')
