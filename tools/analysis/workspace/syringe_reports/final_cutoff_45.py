from pathlib import Path
p=Path('tools/analysis/workspace/syringe_reports/_generated_nov5_6_ad7682.py')
s=p.read_text(encoding='utf8')
s=s.replace("END = pd.Timestamp('2026-11-06 09:00:00')", "END = pd.Timestamp('2026-11-05 16:28:50')")
# Replace temperature analysis cutoff logic with the global report cutoff.
s=s.replace("    _ge40=full_d[full_d['I6']>=40]\n    temp_end=_ge40.index.min() if len(_ge40) else full_d.index.max()\n    d=full_d.loc[:temp_end]", "    temp_end=END\n    d=full_d.loc[:END]")
s=s.replace("从 2026-11-05 15:22:00 开始，到该板温度首次达到40°C为止的区间。1号板截止 '+str(temp_ranges['1号板'][1])+'，2号板截止 '+str(temp_ranges['2号板'][1])+'。区间外的温度影响数据不纳入本节。", "从 2026-11-05 15:22:00 开始，到共同截止时间 2026-11-05 16:28:50（温度达到约45°C）为止的区间。区间外数据不纳入本报告。")
s=s.replace("只分析各板从 2026-11-05 15:22:00 开始，到该板温度首次达到40°C为止的区间", "只分析从 2026-11-05 15:22:00 开始，到 2026-11-05 16:28:50（温度达到约45°C）为止的区间")
s=s.replace("2026-11-05 15:22:00至2026-11-06 09:00:00", "2026-11-05 15:22:00至2026-11-05 16:28:50")
s=s.replace("用户指定区间为2026-11-05 15:22:00至2026-11-06 09:00:00", "用户指定区间为2026-11-05 15:22:00至2026-11-05 16:28:50")
s=s.replace("1号板、2号板各 {5365:,} 条区间内记录", "1号板、2号板各 {len(boards['1号板']['data']):,} 条区间内记录")
p.write_text(s,encoding='utf8')
