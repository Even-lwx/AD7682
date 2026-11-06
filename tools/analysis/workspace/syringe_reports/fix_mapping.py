from pathlib import Path
p=Path('tools/analysis/workspace/syringe_reports/_generated_nov5_6_ad7682.py')
s=p.read_text(encoding='utf8')
s=s.replace("label='IN0：传感器3−传感器1'", "label='IN0：传感器4−传感器1'")
s=s.replace("label='IN2：传感器4−传感器2'", "label='IN2：传感器3−传感器2'")
s=s.replace("parts.append(figure('两板模拟ADC同通道差值（不含传感器4号）', draw_adc_delta_no4, '仅保留IN0板间差值；全通道差值图仍保留。'))", "parts.append(figure('两板模拟ADC同通道差值（不含传感器4号）', draw_adc_delta_no4, '排除2号板IN0的传感器4号，仅保留IN2：传感器3−传感器2；全通道差值图仍保留。'))")
s=s.replace("parts.append(figure('两板模拟压力同通道差值（不含传感器4号）', draw_pressure_delta_no4, '仅保留IN0板间差值；全通道差值图仍保留。'))", "parts.append(figure('两板模拟压力同通道差值（不含传感器4号）', draw_pressure_delta_no4, '排除2号板IN0的传感器4号，仅保留IN2：传感器3−传感器2；全通道差值图仍保留。'))")
s=s.replace("ax.plot(paired.time, paired.I10_b2-paired.I10_b1, lw=.6, color=COLORS[0], label='IN0：传感器4−传感器1')\n    ax.axhline(0", "ax.plot(paired.time, paired.I12_b2-paired.I12_b1, lw=.6, color=COLORS[0], label='IN2：传感器3−传感器2')\n    ax.axhline(0",1)
# replace pressure delta expressions and labels
s=s.replace("paired.S3_hPa-paired.S1_hPa, lw=.6, color=COLORS[2], label='IN0：传感器4−传感器1'", "paired.S4_hPa-paired.S1_hPa, lw=.6, color=COLORS[2], label='IN0：传感器4−传感器1'")
s=s.replace("paired.S4_hPa-paired.S2_hPa, lw=.6, color=COLORS[3], label='IN2：传感器3−传感器2'", "paired.S3_hPa-paired.S2_hPa, lw=.6, color=COLORS[3], label='IN2：传感器3−传感器2'")
# no4 pressure expression: first occurrence after def draw_pressure_delta_no4
s=s.replace("def draw_pressure_delta_no4(fig, ax):\n    ax.plot(paired.time, paired.S4_hPa-paired.S1_hPa, lw=.6, color=COLORS[2], label='IN0：传感器4−传感器1')", "def draw_pressure_delta_no4(fig, ax):\n    ax.plot(paired.time, paired.S3_hPa-paired.S2_hPa, lw=.6, color=COLORS[2], label='IN2：传感器3−传感器2')")
s=s.replace("('IN0滤波ADC码：传感器3−1','I10_b1','I10_b2','码'),('IN2滤波ADC码：传感器4−2','I12_b1','I12_b2','码'),('IN0换算压力：传感器3−1','S1_hPa','S3_hPa','hPa'),('IN2换算压力：传感器4−2','S2_hPa','S4_hPa','hPa')", "('IN0滤波ADC码：传感器4−1','I10_b1','I10_b2','码'),('IN2滤波ADC码：传感器3−2','I12_b1','I12_b2','码'),('IN0换算压力：传感器4−1','S1_hPa','S4_hPa','hPa'),('IN2换算压力：传感器3−2','S2_hPa','S3_hPa','hPa')")
p.write_text(s,encoding='utf8')
