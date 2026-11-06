from pathlib import Path
base=Path(__file__).parent
for fn in ['build_long_static.py','compare_weekend.py']:
 p=base/fn;s=p.read_text(encoding='utf8')
 # AD7682 has 65536 quantization levels; 65535 remains the largest valid code.
 s=s.replace('/65535','/65536')
 if fn=='build_long_static.py':
  s=s.replace('未做零点补偿或拟合校正。MCU', '表中0.5V对应35kPa、2.5V对应85kPa、4.5V对应135kPa，故斜率25kPa/V、截距22.5kPa。这里把压力单位按kPa解释。该表没有注明传感器供电条件，本次若传感器供电已变化，不能未经确认直接沿用它的绝对压力标定。按此表，1023hPa需要约3.192V，超过2.5V REF的正常输入量程；目前硬件条件、表格适用条件与压力换算之间存在不一致。图表中的模拟hPa是按此表计算的条件值，不代表已确认真实压力。16位ADC采用65536量化级，改用65535只影响约0.01hPa量级，无法解释约250hPa偏差。未做零点补偿或拟合校正。MCU')
  s=s.replace('用户确认本次AD7682外部REF为2.5V。按原厂家公式换算', '用户确认本次AD7682外部REF为2.5V；厂家电压表的传感器供电适用条件尚待核实。按原厂家公式条件换算')
 else:
  s=s.replace('压力公式P(hPa)=225+250×REF×ADC/65536。', '条件换算公式P(hPa)=225+250×REF×ADC/65536。厂家表对应P(kPa)=22.5+25×V，但表上未注明供电适用条件。在1023hPa下该表要求约3.192V，超过本次REF=2.5V的输入量程，因此本次绝对压力及hPa波动仅作为沿用表格的条件计算值，不视作有效标定。电压与ADC码的波动仍单独列出。')
 p.write_text(s,encoding='utf8')
