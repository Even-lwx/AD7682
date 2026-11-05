"""Execute the real uart_crc.c as Cortex-M0 code with mocked UART/ticks.

Developer check requires Keil ARMCC, pyelftools and Unicorn. No board required.
"""
import subprocess
import sys
from pathlib import Path

from elftools.elf.elffile import ELFFile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0
from protocol import data_frame, control_frame

root = Path(__file__).resolve().parents[2]
out = root / "tmp" / "crc_firmware_test"
out.mkdir(parents=True, exist_ok=True)
stub = '''#ifndef TEST_HAL_H
#define TEST_HAL_H
#include <stdint.h>
typedef int HAL_StatusTypeDef;
#define HAL_OK 0
#define HAL_ERROR 1
#define HAL_BUSY 2
typedef int UART_HandleTypeDef;
extern UART_HandleTypeDef huart1;
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef*,uint8_t*,uint16_t,uint32_t);
uint32_t HAL_GetTick(void);
#endif
'''
(out / "stm32g0xx_hal.h").write_text(stub)
(out / "usart.h").write_text('#include "stm32g0xx_hal.h"\n')

def c_literal(data):
    return '"' + ''.join(f"\\x{b:02x}" for b in data) + '"'

prefix = r'''
#include <string.h>
#include <stdio.h>
#include "stm32g0xx_hal.h"
UART_HandleTypeDef huart1;
static uint32_t ticks,tx_count;
static char sent[400];
static uint16_t sent_len;
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h,uint8_t *p,uint16_t n,uint32_t timeout){
 (void)h;(void)timeout;memcpy(sent,p,n);sent[n]=0;sent_len=n;tx_count++;return HAL_OK;
}
uint32_t HAL_GetTick(void){return ticks;}
'''
module = (root / "code/code/uart_crc/uart_crc.c").read_text()
tests = r'''
#define CHECK(c) do{if(!(c))return __LINE__;}while(0)
int test_main(void){
 uint32_t hex;
 CHECK(Link_Crc16("123456789",9)==0x4B37);
 CHECK(UartCrc_ParseHex8("1234abcd",&hex)&&hex==0x1234ABCD);
 CHECK(!UartCrc_ParseHex8("short",&hex));
 CHECK(!UartCrc_ParseHex8("1234abcx",&hex));
 UartCrc_Enable(0x1234);ticks=0;tx_count=0;
 CHECK(UartCrc_Send("samples: 1,2\r\n")==HAL_OK);
 CHECK(UartCrc_IsBusy()&&tx_count==1);
 CHECK(strcmp(sent,EXPECTED)==0&&sent_len==strlen(EXPECTED));
 CHECK(UartCrc_Send("samples: 3")==HAL_BUSY);
 UartCrc_HandleControl(BAD_ACK);CHECK(UartCrc_IsBusy());
 UartCrc_HandleControl(STALE_ACK);CHECK(UartCrc_IsBusy());
 UartCrc_HandleControl(ACK1);CHECK(!UartCrc_IsBusy());
 CHECK(UartCrc_Send("samples: 3")==HAL_OK&&g_sequence==2);
 UartCrc_HandleControl(ACK1);CHECK(UartCrc_IsBusy());
 ticks=499;UartCrc_Process();CHECK(tx_count==2);
 ticks=500;UartCrc_Process();CHECK(tx_count==3&&g_retried);
 ticks=1000;UartCrc_Process();CHECK(tx_count==4&&UartCrc_IsFailed());
 CHECK(UartCrc_Send("samples: 3")==HAL_ERROR);
 UartCrc_Enable(0x1234);tx_count=0;CHECK(UartCrc_Send("samples: 1,2")==HAL_OK);
 UartCrc_HandleControl(NACK1);CHECK(tx_count==2&&!UartCrc_IsFailed());
 CHECK(strcmp(sent,EXPECTED)==0);
 UartCrc_HandleControl(NACK1);CHECK(tx_count==3&&UartCrc_IsFailed());
 UartCrc_Enable(0x1234);CHECK(UartCrc_Send("samples: 1,2")==HAL_OK);
 UartCrc_HandleControl(NACK1);UartCrc_HandleControl(ACK1);CHECK(!UartCrc_IsBusy());
 UartCrc_Disable();CHECK(!UartCrc_IsEnabled());
 CHECK(UartCrc_Send("samples: 3")==HAL_ERROR);
 UartCrc_Enable(1);g_sequence=0xFFFFFFFF;CHECK(UartCrc_Send("test")==HAL_OK);
 UartCrc_HandleControl(WRAP_ACK);CHECK(g_sequence==0&&!UartCrc_IsBusy());
 UartCrc_Enable(1);memset(sent,'X',300);sent[300]=0;
 CHECK(UartCrc_Send(sent)==HAL_ERROR&&UartCrc_IsFailed());
 return 0;
}
'''
macros = {
    "EXPECTED": data_frame(0x1234, 1, b"samples: 1,2"),
    "ACK1": control_frame("A", 0x1234, 1).strip(),
    "BAD_ACK": b"@A,00001234,00000001*0000",
    "STALE_ACK": control_frame("A", 99, 1).strip(),
    "NACK1": control_frame("N", 0x1234, 1).strip(),
    "WRAP_ACK": control_frame("A", 1, 0xFFFFFFFF).strip(),
}
defines = '\n'.join(f"#define {key} {c_literal(value)}" for key,value in macros.items())
(out / "harness.c").write_text(prefix + module + defines + tests)
bin_dir = Path("C:/Keil_v5/ARM/ARMCC/bin")
subprocess.run([str(bin_dir/"armcc.exe"), "--cpu=Cortex-M0", "--c99", "-O0", "-g", "-I"+str(out),
                "-I"+str(root/"code/code/uart_crc"), "-c", str(out/"harness.c"), "-o", str(out/"harness.o")], check=True)
subprocess.run([str(bin_dir/"armlink.exe"), "--cpu=Cortex-M0", "--entry=test_main", "--ro_base=0x10000",
                "--rw_base=0x20000000", "--libpath=C:/Keil_v5/ARM/ARMCC/lib", str(out/"harness.o"), "-o", str(out/"harness.axf")], check=True)
u = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
u.mem_map(0x10000, 0x100000)
u.mem_map(0x20000000, 0x100000)
with (out/"harness.axf").open("rb") as stream:
    elf = ELFFile(stream)
    for segment in elf.iter_segments():
        if segment["p_type"] == "PT_LOAD" and segment["p_filesz"]:
            u.mem_write(segment["p_vaddr"], segment.data())
    entry = elf.header["e_entry"]
u.reg_write(UC_ARM_REG_SP, 0x200F0000)
u.reg_write(UC_ARM_REG_LR, 0xF0001)
u.emu_start(entry | 1, 0xF0000, count=10000000)
result = u.reg_read(UC_ARM_REG_R0)
assert result == 0, f"Firmware assertion failed at harness.c:{result}"
print("PASS: real Cortex-M0 CRC frames match Python byte-for-byte; ACK/NACK CRC, stale ACK, timeout, single retry, failure latch, session reset, sequence wrap, oversize guard.")
