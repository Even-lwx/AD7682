from pathlib import Path
import runpy
import re

# Reuse the validated long-test report builder and change only this run's
# source files, interval, output folder, and the swapped sensor mapping.
source = Path(__file__).with_name('build_nov3_4_boost.py').read_text(encoding='utf-8')
replacements = [
    ("OUT = ROOT / 'output/reports/长期测试_1103-1104_BOOST'", "OUT = ROOT / 'output/reports/长期测试_1104-1105_BOOST'"),
    ("START = pd.Timestamp('2026-11-03 18:00:00')", "START = pd.Timestamp('2026-11-04 13:00:00')"),
    ("END = pd.Timestamp('2026-11-04 08:55:00')", "END = pd.Timestamp('2026-11-05 08:25:00')"),
    ("FILES = [('1号板', '1103-1104-1号板5VBOOST.csv', [1, 2]),\n         ('2号板', '1103-1104-2号板5VBOOST.csv', [3, 4])]", "FILES = [('1号板', r'C:/Users/嵌入式软件实习生电脑02/Desktop/1104-05-1号板子.csv', [1, 2]),\n         ('2号板', r'C:/Users/嵌入式软件实习生电脑02/Desktop/1104-05-2号板子.csv', [4, 3])]"),
    ("path = ROOT / 'code/log' / filename", "path = Path(filename) if Path(filename).is_absolute() else ROOT / 'code/log' / filename"),
    ("2026年11月3日—4日 · 5V BOOST长期静态数据", "2026年11月4日—5日 · 5V BOOST长期静态数据"),
    ("两块板5VBOOST长期测试报告_1103-1104.html", "两块板5VBOOST长期测试报告_1104-1105.html"),
    ("2026-11-03 18:00:00至2026-11-04 08:55:00", "2026-11-04 13:00:00至2026-11-05 08:25:00"),
]
for old, new in replacements:
    if old not in source:
        raise RuntimeError(f'missing expected builder text: {old[:80]}')
    source = source.replace(old, new)
# The previous report builder contains a run-specific narrative. Remove it
# before generating this run so no old date range or old metrics leak in.
source = re.sub(r"body=body\.replace\(.*?\)\r?\nbody \+= f'<section id=\\\"method\\\"',", "body += f'<section id=\\\"method\\\"',", source, count=1, flags=re.S)
source = source.replace('1号板、2号板各 {5365:,} 条区间内记录', '1号板、2号板各 {len(boards["1号板"]["data"]):,} 条区间内记录')
source = source.replace('本报告只使用两份指定日志中11月3日18:00至11月4日08:55（含端点）的记录', '本报告只使用两份指定日志中11月4日13:00至11月5日08:25（含端点）的记录')
source = source.replace('对应传感器1/2号（1号板）及3/4号（2号板）', '对应传感器1号/2号（1号板IN0/IN2）及传感器4号/3号（2号板IN0/IN2）')
source = source.replace("body += '<section id=\"summary\"><h2>主要结果</h2><p>本报告只使用", "body += '<section id=\"summary\"><h2>主要结果</h2><p class=\"notice\"><b>本报告的传感器映射：</b>1号板IN0=传感器1号，IN2=传感器2号；2号板IN0=传感器4号，IN2=传感器3号。图例、表格和统计均按此映射生成。</p><p>本报告只使用")
source = source.replace("<p>字段映射：I0～I5为设备RTC年月日时分秒；I6温度；I7湿度；I8/I9为本板两个数字气压计；I10～I13为IN0～IN3的Flash滤波ADC码；I14为电池电压。报告关注IN0与IN2，对应传感器1号/2号（1号板IN0/IN2）及传感器4号/3号（2号板IN0/IN2）。", "<p><b>字段映射：</b>I0～I5为设备RTC年月日时分秒；I6温度；I7湿度；I8/I9为本板两个数字气压计；I10～I13为IN0～IN3的Flash滤波ADC码；I14为电池电压。1号板：IN0=传感器1号、IN2=传感器2号；2号板：IN0=传感器4号、IN2=传感器3号。</p><p>")
source = source.replace("summary_rows.append([board,f'传感器{sid}号 IN{ch}',f(d[analog].mean()),f(digital.mean()),f(r,4),f((d[analog]-digital).mean())])", "summary_rows.append([board,f'传感器{sid}号 IN{ch}',f(d[analog].mean()),f(digital.mean()),f(r,4),f((d[analog]-digital).mean()),f(d[analog].max()-d[analog].min()),f(d[analog].std())])")
source = source.replace("table(summary_rows,['板号','模拟通道','模拟换算均值hPa','数字气压均值hPa','相关系数r','模拟−数字均值hPa'])", "table(summary_rows,['板号','模拟通道','模拟换算均值hPa','数字气压均值hPa','相关系数r','模拟−数字均值hPa','模拟峰峰值hPa','模拟标准差hPa'])")
source = source.replace("模拟压力采用厂家线性表格换算，结果依赖传感器供电/校准条件；相关性表示趋势一致程度，不等同于绝对准确度。短时波动见下方独立统计。", "相关系数只表示模拟压力是否跟随数字压力的整体趋势，不能单独判断噪声大小。传感器3号相关系数最低，是因为它的曲线较平、没有跟随这段数字压力变化；传感器4号蓝线相关系数较高，但峰峰值和短时波动更大，异常应通过峰峰值、标准差和下方局部残差指标判断。模拟压力采用厂家线性表格换算，结果依赖传感器供电/校准条件；相关性不等同于绝对准确度。")
source = source.replace('用户指定区间为2026-11-03 18:00:00至2026-11-04 08:55:00', '用户指定区间为2026-11-04 13:00:00至2026-11-05 08:25:00')
source = source.replace("report=OUT/'两块板5VBOOST长期测试报告_1103-1104.html'", "report=OUT/'两块板5VBOOST长期测试报告_1104-1105.html'")
source = source.replace('生成于2026-11-04', '生成于2026-11-05')
stale_start = source.find('body=body.replace(')
stale_end = source.find("body += f'<section id=", stale_start)
if stale_start >= 0 and stale_end > stale_start:
    source = source[:stale_start] + source[stale_end:]
generated = Path(__file__).with_name('_generated_nov4_5_boost.py')
generated.write_text(source, encoding='utf-8')
runpy.run_path(str(generated), run_name='__main__')
