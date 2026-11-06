from pathlib import Path
import runpy, re
base=Path('tools/analysis/workspace/syringe_reports/build_nov3_4_boost.py').read_text(encoding='utf-8')
repls=[
("OUT = ROOT / 'output/reports/长期测试_1103-1104_BOOST'", "OUT = ROOT / 'output/reports/长期测试_1105-1106_AD7682'"),
("START = pd.Timestamp('2026-11-03 18:00:00')", "START = pd.Timestamp('2026-11-05 15:22:00')"),
("END = pd.Timestamp('2026-11-04 08:55:00')", "END = pd.Timestamp('2026-11-06 09:00:00')"),
("FILES = [('1号板', '1103-1104-1号板5VBOOST.csv', [1, 2]),\n         ('2号板', '1103-1104-2号板5VBOOST.csv', [3, 4])]", "FILES = [('1号板', '1105-06-1号板-AD7682-history.csv', [1, 2]),\n         ('2号板', '1105-06-2号板-AD7682-history.csv', [4, 3]) ]"),
("2026年11月3日—4日 · 5V BOOST长期静态数据", "2026年11月5日—6日 · AD7682长期测试"),
("两块板5VBOOST长期测试报告_1103-1104.html", "两块板AD7682长期测试报告_1105-1106.html"),
("2026-11-03 18:00:00至2026-11-04 08:55:00", "2026-11-05 15:22:00至2026-11-06 09:00:00"),
]
for old,new in repls:
 if old not in base: raise RuntimeError('missing '+old[:80])
 base=base.replace(old,new)
# source builder has a hardcoded path under code/log; files are there so no change
base=base.replace('本报告只使用两份指定日志中11月3日18:00至11月4日08:55（含端点）的记录','本报告只使用两份指定日志中11月5日15:22至11月6日09:00（含端点）的记录')
base=base.replace('对应传感器1/2号（1号板）及3/4号（2号板）','对应传感器1号/2号（1号板IN0/IN2）及传感器4号/3号（2号板IN0/IN2）')
base=base.replace("body += '<section id=\"summary\"><h2>主要结果</h2><p>本报告只使用", "body += '<section id=\"summary\"><h2>主要结果</h2><p class=\"notice\"><b>本报告的传感器映射：</b>1号板IN0=传感器1号，IN2=传感器2号；2号板IN0=传感器4号，IN2=传感器3号。图例、表格和统计均按此映射生成。</p><p>本报告只使用")
base=base.replace("<p>字段映射：I0～I5为设备RTC年月日时分秒；I6温度；I7湿度；I8/I9为本板两个数字气压计；I10～I13为IN0～IN3的Flash滤波ADC码；I14为电池电压。报告关注IN0与IN2，对应传感器1号/2号（1号板IN0/IN2）及传感器4号/3号（2号板IN0/IN2）。", "<p><b>字段映射：</b>I0～I5为设备RTC年月日时分秒；I6温度；I7湿度；I8/I9为本板两个数字气压计；I10～I13为IN0～IN3的Flash滤波ADC码；I14为电池电压。1号板：IN0=传感器1号、IN2=传感器2号；2号板：IN0=传感器4号、IN2=传感器3号。</p><p>")
# replace output/reports/report names if still embedded
base=base.replace("report=OUT/'两块板5VBOOST长期测试报告_1103-1104.html'", "report=OUT/'两块板AD7682长期测试报告_1105-1106.html'")
base=base.replace('生成于2026-11-04','生成于2026-11-06')
# remove stale body replacement block if any
stale=base.find('body=body.replace(')
if stale>=0:
 end=base.find("body += f'<section id=",stale)
 if end>stale: base=base[:stale]+base[end:]
Path('tools/analysis/workspace/syringe_reports/_generated_nov5_6_ad7682.py').write_text(base,encoding='utf-8')
runpy.run_path('tools/analysis/workspace/syringe_reports/_generated_nov5_6_ad7682.py',run_name='__main__')
