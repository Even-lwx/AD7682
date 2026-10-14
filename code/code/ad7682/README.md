# AD7682 driver

This driver targets the AD7682 connection in the project schematic:

- SPI1 SCK: PA1 (`SCK`)
- SPI1 SDO: PA6 (`SDO`)
- SPI1 DIN: PA7 (`DIN`)
- Conversion start (`CNV`): PA5

Board source: `pcb/AD7682-Ver1.0/Sheet1.SchDoc`, U5. CNV/DIN/SCK/SDO
are pins 11/12/13/14. IN0/1/2/3 are pins 16/18/7/9; COM is grounded.
VDD/VIO use 3V. REF uses external `2V5_SENSOR` (REF3025) through 0 ohms;
the firmware therefore disables the internal reference and reference buffer.

## Configuration and protocol

Source: `doc/C398823_模数转换芯片ADC_AD7682BCPZ_规格书_WJ214604.PDF`,
Rev. B, pp.7,19,21,23-25,28. The PDF's extracted Chinese text has damaged
font mappings; bit fields and timing labels remain readable.

The chip has a 14-bit CFG word (not an I2C-style addressed register map).
Base CFG is `0x3C70`, plus `channel << 7`:

| Field | Setting | Meaning |
| --- | --- | --- |
| CFG[13] | 1 | Update configuration |
| INCC[12:10] | 111 | Unipolar single-ended input relative to GND |
| INx[9:7] | 000..011 | IN0..IN3 |
| BW[6] | 1 | Full bandwidth |
| REF[5:3] | 110 | External REF, internal reference/buffer disabled |
| SEQ[2:1] | 00 | Sequencer disabled, explicit channel selection |
| RB[0] | 0 | Configuration readback enabled |

SPI1 uses mode 0, MSB first, 8-bit transfers, 2 MHz (64 MHz / 32).
Each transaction raises CNV, waits conservatively beyond the 3.2 us maximum
conversion time, then lowers CNV and exchanges four bytes. CFG is sent
MSB-aligned (`CFG << 2`). Receive data is 16 conversion bits followed by
14 CFG bits; the two trailing high-Z bits are ignored. CNV stays low
between transactions to guarantee a rising edge for the next conversion.
The implementation waits 10 us with CNV high and 5 us after CNV falls,
providing margin for both conversion and acquisition.

Every channel read discards two conversions to flush the CFG latency and
returns the third. CFG readback is clocked for diagnostics but is not used
to reject an otherwise successful SPI conversion. A full four-channel
read performs 12 conversions, updating the caller's array only on complete
success. `AD7682_ReadRaw` / `AD7682_ReadVoltage` retain IN0 compatibility.
`timeout_ms` applies to each HAL SPI call, not the entire scan. These
blocking functions must not be called concurrently or from an ISR.

## Application behavior

- Startup initialization retries up to three times and reports `AD7682 INIT OK/ERROR`.
- Each normal sampling cycle reads real IN0..IN3. Real-time ch7..ch10 and
  the four history ADC fields retain unsigned 16-bit raw codes (0..65535).
- Communication failure
  prints `nan,nan,nan,nan` and skips that cycle's scheduled history record.
  Other sampling, command handling and dump continue. Later successful
  scans recover automatically. With the ADC missing, new history is not saved.
- Sampling and scheduled logging continue in `setting` mode.
- Existing history and its binary layout are unchanged.
- Conversion to volts: `code * measured_REF / 65536`; nominal REF is 2.5 V.

The existing main loop still waits 20 ms **after** sampling/output, so its
actual output rate is below 50 Hz and depends on peripheral/serial delays.
This driver does not change that scheduling policy.

## Hardware acceptance (not yet performed)

1. Verify U5 VDD/VIO and REF (nominal 3 V/2.5 V), and common ground.
2. Boot and check the `AD7682 INIT OK` startup status.
3. Connect each input to GND, then a known voltage below REF. At 1.25 V
   and REF=2.5 V expect approximately 32768; drive one input at a time
   to check ordering and absence of stale-channel results. Do not exceed REF.
4. Use `log interval 1` and `log dump` to compare the four stored codes;
   check restart persistence without clearing existing logs.
5. Disconnect SDO or remove U5 power safely, verify error/NaN and no new
   invalid history. Restore wiring and verify automatic recovery.
6. Check CNV/SCK/DIN/SDO on a logic analyzer: 32 clocks per conversion,
   correct configuration, 12 conversions per scan, mode 0.

CFG readback detects many digital-link errors (including constant zero/one
SDO), but is not a chip ID or an analog self-test. It cannot establish REF
accuracy, analog power integrity, input wiring, or resistance to noise.
