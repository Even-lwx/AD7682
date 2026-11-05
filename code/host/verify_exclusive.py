"""Run actual pause/resume functions on Cortex-M0 with mocked peripherals."""
from pathlib import Path
import re
import subprocess
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0

root = Path(__file__).resolve().parents[2]
out = root / "tmp/exclusive_test"
out.mkdir(parents=True, exist_ok=True)
source = (root/"code/Core/Src/main.c").read_text(encoding="utf-8-sig")

def function(name):
    """从 MCU 源文件提取指定函数定义，供 Cortex-M0 验证程序使用。

    参数：
    name：任务名或待提取函数名。

    返回：提取或格式化后的文本。"""
    match = re.search(r"static void " + name + r"\(void\)\s*\{", source)
    start = source.index("{", match.start())+1
    end, depth = start, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]

prefix = r'''
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
'''
tests = r'''
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
  /* 保留原先已停止的定时器状态和滴答计数回绕。 */
 TIM3->CR1=0;TIM14->CR1=1;tick=0xFFFFFFF0U;
 Serial_DumpPauseSampling();tick=0x20;Serial_DumpResumeSampling();
 CHECK(TIM3->CR1==0&&TIM14->CR1==1&&g_record_timer_milliseconds==5198);
 return 0;
}
'''
(out/"harness.c").write_text(prefix+function("Serial_DumpPauseSampling")+function("Serial_DumpResumeSampling")+tests, encoding="utf-8")
compiler = Path("C:/Keil_v5/ARM/ARMCC/bin")
subprocess.run([str(compiler/"armcc.exe"), "--cpu=Cortex-M0", "--c99", "-O0", "-g", "-c", str(out/"harness.c"), "-o", str(out/"harness.o")], check=True)
subprocess.run([str(compiler/"armlink.exe"), "--cpu=Cortex-M0", "--entry=test_main", "--ro_base=0x10000", "--rw_base=0x20000000", "--libpath=C:/Keil_v5/ARM/ARMCC/lib", str(out/"harness.o"), "-o", str(out/"harness.axf")], check=True)
u = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
u.mem_map(0x10000, 0x100000)
u.mem_map(0x20000000, 0x100000)
with (out/"harness.axf").open("rb") as stream:
    elf = ELFFile(stream)
    for seg in elf.iter_segments():
        if seg["p_type"] == "PT_LOAD" and seg["p_filesz"]:
            u.mem_write(seg["p_vaddr"], seg.data())
    entry = elf.header["e_entry"]
u.reg_write(UC_ARM_REG_SP, 0x200F0000)
u.reg_write(UC_ARM_REG_LR, 0xF0001)
u.emu_start(entry | 1, 0xF0000, count=1000000)
result = u.reg_read(UC_ARM_REG_R0)
assert result == 0, f"Assertion at harness.c:{result}"
print("PASS: actual pause/resume timer stop/restore, final write, pending clear, time compensation, fresh filters, idempotence and tick rollover.")
