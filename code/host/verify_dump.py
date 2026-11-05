from pathlib import Path
import re, subprocess
from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_MEM_INVALID
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_PC
root=Path.cwd()
out=root/'tmp/log_dump_range_test'
out.mkdir(exist_ok=True)
source=(root/'code/Core/Src/main.c').read_text(encoding='utf-8-sig')
def function(name):
    """从 MCU 源文件提取指定函数定义，供 Cortex-M0 验证程序使用。

    参数：
    name：任务名或待提取函数名。

    返回：提取或格式化后的文本。"""
    m=re.search(r'static [^\n]*\b'+name+r'\([^;]*?\)\s*\{',source)
    assert m,name
    end=source.index('{',m.start())+1
    depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}')
        end+=1
    return source[m.start():end]
def struct(path,ending):
    """从头文件提取结构声明，供导出函数仿真编译使用。

    参数：
    path：采集临时文件或源文件路径。
    ending：待提取结构的结尾标记。

    返回：提取或格式化后的文本。"""
    s=(root/path).read_text(encoding='utf-8-sig')
    return s[s.index('typedef struct'):s.index(ending)+len(ending)]
prefix=r'''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#define SENSOR_INVALID_FLOAT (-9999.0f)
#define AD7682_REFERENCE_VOLTS 5.0f
typedef int HAL_StatusTypeDef;
#define HAL_OK 0
#define HAL_ERROR 1
'''+struct('code/code/pcf8563/pcf8563.h','} PCF8563_DateTime;')+struct('code/code/data_logger/data_logger.h','} DataLogger_Sample;')+r'''
static float printed_adc[4];
 /* 捕获传给格式化函数的实际 ADC 参数。 */
static int test_snprintf(char *dest,size_t size,const char *format,...){
  size_t n=strlen(format);unsigned i;va_list args;
  if(strncmp(format,"samples:",8)==0){
    va_start(args,format);
    for(i=0;i<6;i++)(void)va_arg(args,unsigned);
    for(i=0;i<4;i++)(void)va_arg(args,double);
    for(i=0;i<4;i++)printed_adc[i]=(float)va_arg(args,double);
    va_end(args);
  }
  if(n>=size)n=size-1;memcpy(dest,format,n);dest[n]=0;return (int)n;
}
#define snprintf test_snprintf
static uint32_t g_dump_remaining,g_dump_sequence,g_dump_start_time,g_dump_end_time;
static uint32_t g_dump_matched,g_dump_read_errors;
static uint8_t g_dump_newest_first,g_dump_time_filtered,g_dump_hpa,g_rtc_setting_mode=1;
static uint32_t count,last,reads,lines,done,errors,failed,seen[8];
static uint8_t newest;
static uint8_t g_dump_report_pending;
static void Serial_DumpPauseSampling(void){}
static uint32_t g_window_dump_sequence,g_window_dump_remaining,g_window_dump_matched,g_window_dump_errors;
static uint16_t g_window_scanned;
static unsigned window_mode,window_open,window_replay;
static unsigned window_size=16;
static unsigned UartCrc_WindowSize(void){return window_size;}
static int UartCrc_WindowEnabled(void){return window_mode;}
static int UartCrc_WindowOpen(void){return window_open;}
static int UartCrc_TakeReplay(void){int r=window_replay;window_replay=0;return r;}
static void UartCrc_WindowBegin(void){window_open=1;}
static void UartCrc_WindowEnd(void);

static unsigned crc_enabled,crc_busy,crc_failed,crc_sends;
static int UartCrc_IsEnabled(void){return crc_enabled;}
static int UartCrc_IsBusy(void){return crc_busy;}
static int UartCrc_IsFailed(void){return crc_failed;}
static void UartCrc_Send(const char *s){(void)s;crc_sends++;crc_busy=1;}
static void UartCrc_WindowEnd(void){window_open=0;crc_busy=1;}

static DataLogger_Sample rows[8];
static uint32_t DataLogger_Count(void){return count;}
static uint32_t DataLogger_LastSequence(void){return last;}
static uint8_t DataLogger_GetDumpNewestFirst(void){return newest;}
static HAL_StatusTypeDef DataLogger_ReadSequence(uint32_t seq,DataLogger_Sample *s,uint32_t timeout){
  (void)timeout;if(reads<8)seen[reads]=seq;reads++;
  if(seq==failed||seq<10||seq>17)return HAL_ERROR;
  *s=rows[seq-10];return HAL_OK;
}
static void Serial_Write(const char *s){
  if(strncmp(s,"samples:",8)==0){lines++;if(crc_enabled&&!window_open)crc_busy=1;}
  if(strncmp(s,"LOG DUMP DONE:",14)==0)done++;
  if(strncmp(s,"LOG DUMP ERROR:",15)==0)errors++;
}
'''
names=['Serial_ToLower','Serial_Equals','Serial_Digits','Serial_IsLeap','Serial_DaysInMonth','Serial_Weekday','Serial_ParseSetTime','Serial_DateTimeSeconds','Serial_ParseDumpTime','Serial_PrintDumpResult','Serial_StartLogDump','Serial_AdcPressureHpa','Serial_DumpLogs']
tests=r'''
#define CHECK(c) do{if(!(c))return __LINE__;}while(0)
static void reset(void){
  unsigned i;window_size=16;window_mode=window_open=window_replay=0;crc_enabled=crc_busy=crc_failed=crc_sends=0;g_dump_report_pending=0;count=5;last=14;reads=lines=done=errors=failed=newest=0;
  g_dump_remaining=0;g_rtc_setting_mode=1;memset(rows,0,sizeof(rows));
  for(i=0;i<8;i++){rows[i].datetime.year=26;rows[i].datetime.month=11;
    rows[i].datetime.day=2;rows[i].datetime.hours=9;rows[i].datetime.seconds=(uint8_t)i;}
}
static int drain(void){unsigned i=0;while(g_dump_remaining&&i++<10)Serial_DumpLogs();return !g_dump_remaining;}
int test_main(void){
  uint32_t seconds;PCF8563_DateTime dt={0};
  CHECK(Serial_ParseDumpTime("2000-01-01 00:00:00",19,&seconds)&&seconds==0);
  CHECK(Serial_ParseDumpTime("2099-12-31 23:59:59",19,&seconds)&&seconds==3155759999UL);
  CHECK(Serial_ParseDumpTime("24-02-29 23:59:59",17,&seconds));
  CHECK(!Serial_ParseDumpTime("2026-02-29 00:00:00",19,&seconds));
  CHECK(!Serial_ParseDumpTime("2026-04-31 00:00:00",19,&seconds));
  CHECK(!Serial_ParseDumpTime("2026-00-01 00:00:00",19,&seconds));
  CHECK(!Serial_ParseDumpTime("2026-13-01 00:00:00",19,&seconds));
  CHECK(!Serial_ParseDumpTime("2026-11-02 24:00:00",19,&seconds));
  CHECK(!Serial_ParseDumpTime("2026-11-02 09:60:00",19,&seconds));
  CHECK(!Serial_ParseDumpTime("2026-11-02 09:00:60",19,&seconds));
  CHECK(!Serial_ParseDumpTime("2100-11-02 09:00:00",19,&seconds));
  CHECK(!Serial_ParseDumpTime("2026/11/02 09:00:00",19,&seconds));
  CHECK(!Serial_ParseDumpTime("",0,&seconds));
  CHECK(Serial_DateTimeSeconds(&dt)==0xFFFFFFFFUL);
  reset();Serial_StartLogDump("log dump 2026-11-02 09:00:01 2026-11-02 09:00:03");
  CHECK(drain()&&reads==5&&lines==3&&done==1&&g_dump_matched==3);
  CHECK(seen[0]==10&&seen[4]==14);
  reset();newest=1;Serial_StartLogDump("log dump 26-11-02 09:00:01 26-11-02 09:00:03");
  CHECK(drain()&&lines==3&&seen[0]==14&&seen[4]==10);
  reset();rows[2].datetime.seconds=1;
  Serial_StartLogDump("log dump 2026-11-02 09:00:01 26-11-02 09:00:01");
  CHECK(drain()&&lines==2&&done==1);
  reset();Serial_StartLogDump("log dump 2026-11-01 23:59:59 2026-11-02 09:00:01");
  CHECK(drain()&&lines==2);
  reset();Serial_StartLogDump("log dump 2026-11-03 00:00:00 2026-11-03 23:59:59");
  CHECK(drain()&&lines==0&&done==1&&!g_dump_read_errors);
  reset();count=0;Serial_StartLogDump("log dump 26-11-02 09:00:00 26-11-02 09:00:01");
  CHECK(done==1&&!reads);
  reset();Serial_StartLogDump("log dump");
  Serial_StartLogDump("log dump 26-11-02 09:00:02 26-11-02 09:00:01");
  CHECK(errors==1&&g_dump_remaining==5&&!g_dump_time_filtered);
  Serial_StartLogDump("log dump nonsense");Serial_StartLogDump("log dump invalid");
  Serial_StartLogDump("log dump 2026-11-02 09:00:00");
  Serial_StartLogDump("log dump 26-11-02 09:00:00 26-11-02 09:00:01 junk");
  CHECK(errors==5&&drain()&&lines==5&&done==0);
  reset();failed=14;Serial_StartLogDump("log dump 26-11-02 09:00:00 26-11-02 09:00:04");
  CHECK(drain()&&lines==4&&done==1&&g_dump_read_errors==1);
  reset();rows[0].datetime.hours=10;rows[4].datetime.hours=8;
  Serial_StartLogDump("log dump 26-11-02 09:00:00 26-11-02 09:00:04");count=6;last=15;
  CHECK(drain()&&reads==5&&lines==3);
  reset();rows[2].datetime.month=15;
  Serial_StartLogDump("log dump 26-11-02 09:00:00 26-11-02 09:00:04");CHECK(drain()&&lines==4);
  reset();Serial_StartLogDump("log dump 26-11-02 09:00:00 26-11-02 09:00:04");
  Serial_DumpLogs();g_dump_remaining=0;Serial_DumpLogs();CHECK(lines==1&&reads==1&&!done);
  Serial_StartLogDump("log dump");CHECK(drain()&&!g_dump_time_filtered&&lines==6&&!done);

  CHECK(Serial_AdcPressureHpa(0.0f)==225.0f);
  CHECK(Serial_AdcPressureHpa(6553.6f)>349.99f&&Serial_AdcPressureHpa(6553.6f)<350.01f);
  CHECK(Serial_AdcPressureHpa(19660.8f)>599.99f&&Serial_AdcPressureHpa(19660.8f)<600.01f);
  CHECK(Serial_AdcPressureHpa(39321.6f)>974.99f&&Serial_AdcPressureHpa(39321.6f)<975.01f);
  CHECK(Serial_AdcPressureHpa(58982.4f)>1349.99f&&Serial_AdcPressureHpa(58982.4f)<1350.01f);
  CHECK(Serial_AdcPressureHpa(65534.9f)>1474.9f);
  CHECK(Serial_AdcPressureHpa(65535.0f)==-9999.0f);
  CHECK(Serial_AdcPressureHpa(-9999.0f)==-9999.0f);
  CHECK(Serial_AdcPressureHpa(100000.0f)==-9999.0f);
  reset();count=1;last=10;
  rows[0].adc[0]=6553.6f;rows[0].adc[1]=19660.8f;
  rows[0].adc[2]=39321.6f;rows[0].adc[3]=58982.4f;
  Serial_StartLogDump("log dump hpa");
  CHECK(g_dump_hpa&&!g_dump_time_filtered&&drain()&&lines==1&&done==1);
  CHECK(printed_adc[0]>349.99f&&printed_adc[0]<350.01f);
  CHECK(printed_adc[1]>599.99f&&printed_adc[1]<600.01f);
  CHECK(printed_adc[2]>974.99f&&printed_adc[2]<975.01f);
  CHECK(printed_adc[3]>1349.99f&&printed_adc[3]<1350.01f);
  CHECK(rows[0].adc[0]==6553.6f);
  Serial_StartLogDump("log dump raw");CHECK(!g_dump_hpa&&drain()&&printed_adc[0]==6553.6f);
  Serial_StartLogDump("log dump hpa");Serial_StartLogDump("log dump");CHECK(!g_dump_hpa&&drain());
  reset();newest=1;
  Serial_StartLogDump("log dump hpa 2026-11-02 09:00:01 2026-11-02 09:00:03");
  CHECK(g_dump_hpa&&g_dump_time_filtered&&drain()&&lines==3&&done==1&&seen[0]==14);
  reset();Serial_StartLogDump("log dump raw 26-11-02 09:00:01 26-11-02 09:00:03");
  CHECK(!g_dump_hpa&&g_dump_time_filtered&&drain()&&lines==3&&done==1);
  reset();count=0;Serial_StartLogDump("log dump hpa");CHECK(done==1&&!reads);
  reset();Serial_StartLogDump("log dump hpa");
  Serial_StartLogDump("log dump invalid");CHECK(errors==1&&g_dump_hpa&&g_dump_remaining==5);
  Serial_StartLogDump("log dump hpax");CHECK(errors==2&&g_dump_hpa);
  Serial_StartLogDump("log dump hpa 2026-11-02 09:00:00");CHECK(errors==3&&g_dump_hpa);
  reset();crc_enabled=1;Serial_StartLogDump("log dump raw");
  CHECK(g_dump_remaining==5);Serial_DumpLogs();CHECK(lines==1&&crc_busy&&g_dump_remaining==4);
  Serial_DumpLogs();CHECK(lines==1&&g_dump_remaining==4);
  while(g_dump_remaining){crc_busy=0;Serial_DumpLogs();}
  CHECK(lines==5&&g_dump_report_pending&&crc_sends==0);
 Serial_DumpLogs();CHECK(crc_sends==0); /* 最后一行仍在等待确认。 */
  crc_busy=0;Serial_DumpLogs();CHECK(crc_sends==1&&crc_busy&&!g_dump_report_pending);
  reset();crc_enabled=1;count=0;Serial_StartLogDump("log dump raw");
  CHECK(g_dump_report_pending&&crc_sends==0);Serial_DumpLogs();CHECK(crc_sends==1);
  reset();crc_enabled=1;Serial_StartLogDump("log dump raw");crc_failed=1;
  Serial_DumpLogs();CHECK(!reads&&g_dump_remaining==5);
  reset();crc_enabled=window_mode=1;Serial_StartLogDump("log dump raw");
  CHECK(drain()&&lines==5&&g_dump_matched==5&&crc_busy&&g_dump_report_pending);
  window_replay=1;crc_busy=0;Serial_DumpLogs();
  CHECK(lines==6&&g_dump_matched==1&&g_dump_remaining==4&&!g_dump_report_pending);
  CHECK(drain()&&lines==10&&g_dump_matched==5&&g_dump_read_errors==0);
  crc_busy=0;Serial_DumpLogs();CHECK(crc_sends==1);
  reset();crc_enabled=window_mode=1;newest=1;
  Serial_StartLogDump("log dump raw 2026-11-02 09:00:01 2026-11-02 09:00:03");
  failed=12;CHECK(drain()&&g_dump_matched==2&&g_dump_read_errors==1);
  window_replay=1;crc_busy=0;Serial_DumpLogs();CHECK(drain());
  CHECK(g_dump_matched==2&&g_dump_read_errors==1&&seen[0]==14);
  reset();crc_enabled=window_mode=1;window_size=1000;count=1001;last=1010;
  Serial_StartLogDump("log dump raw");
  {unsigned j;for(j=0;j<999;j++)Serial_DumpLogs();}
  CHECK(g_window_scanned==999&&!crc_busy&&g_dump_remaining==2);
  Serial_DumpLogs();CHECK(crc_busy&&g_window_scanned==1000&&g_dump_remaining==1);
  crc_busy=0;Serial_DumpLogs();CHECK(crc_busy&&g_dump_remaining==0&&g_dump_report_pending);
  CHECK(g_dump_read_errors==993&&g_dump_matched==8);
  return 0;
}
'''
(out/'harness.c').write_text(prefix+'\n'.join(function(n) for n in names)+tests,encoding='utf-8')
bin_dir=Path('C:/Keil_v5/ARM/ARMCC/bin')
subprocess.run([str(bin_dir/'armcc.exe'),'--cpu=Cortex-M0','--c99','-O0','-g','-c',str(out/'harness.c'),'-o',str(out/'harness.o')],check=True)
subprocess.run([str(bin_dir/'armlink.exe'),'--cpu=Cortex-M0','--entry=test_main','--ro_base=0x10000','--rw_base=0x20000000','--libpath=C:/Keil_v5/ARM/ARMCC/lib',str(out/'harness.o'),'-o',str(out/'harness.axf')],check=True)
uc=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
uc.mem_map(0x10000,0x100000);uc.mem_map(0x20000000,0x100000)
with (out/'harness.axf').open('rb') as f:
    elf=ELFFile(f)
    for seg in elf.iter_segments():
        if seg['p_type']=='PT_LOAD' and seg['p_filesz']:uc.mem_write(seg['p_vaddr'],seg.data())
    entry=elf.header['e_entry']
uc.reg_write(UC_ARM_REG_SP,0x200F0000);uc.reg_write(UC_ARM_REG_LR,0xF0001)
uc.hook_add(UC_HOOK_MEM_INVALID, lambda uc, access, addr, size, value, _: print('Invalid access',hex(addr),'PC',hex(uc.reg_read(UC_ARM_REG_PC))))
uc.emu_start(entry|1,0xF0000,count=10000000)
result=uc.reg_read(UC_ARM_REG_R0)
assert result==0,f'Firmware assertion failed at harness.c:{result}'
print('PASS: actual Cortex-M0 pressure conversion, formatting arguments, mode selection, and dump functions: valid/invalid dates, inclusive endpoints, both orders, duplicate seconds, cross-day, empty/no-match, malformed input, preserving active export, read failures, RTC jumps, snapshot, invalid stored date, cancellation, full export.')
