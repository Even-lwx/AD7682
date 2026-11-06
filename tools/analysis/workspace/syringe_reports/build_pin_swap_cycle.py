from pathlib import Path
import re, runpy

source = Path(__file__).with_name('build_reports.py').read_text(encoding='utf-8')
source = source.replace("sensors=[3,4]", "sensors=[4,3]")
source = source.replace("IN0＝传感器3号；IN2＝待验证的传感器4号", "IN0＝传感器4号；IN2＝传感器3号")
source = source.replace("本次接线标注假设", "本次接线标注")
source = source.replace("{cfg['board']}号板：IN0＝传感器{sid[0]}；IN2＝传感器{sid[1]}。", "2号板：IN0＝传感器4号；IN2＝传感器3号。")
source = source.replace("DATE='2026-11-03' if NEW else '2026-10-30'", "DATE='2026-11-05'")
source = source.replace("OUT=ROOT/('output/reports/针筒实验报告_'+args.session)", "OUT=ROOT/'output/reports/针脚更换压力循环_20261105'")
start = source.index('CONFIGS=[')
end = source.index("COLORS=", start)
configs = """CONFIGS=[dict(board=2,file='换了2号板通道2的针脚3次压力循环-2号板5VBOOST.csv',start='09:32:45',end='09:36:59',finish='09:36:59',sensors=[3,4],events=[('09:32:45','实验开始'),('09:36:59','三次高低压循环结束／实验截止')])]
"""
configs = configs.replace("sensors=[3,4]", "sensors=[4,3]")
source = source[:start] + configs + source[end:]
source = source.replace("DATE+' '+t", "DATE+' '+t")
source = source.replace("reporttitle=f'{cfg[\"board\"]}号板 针筒压力循环实验报告'", "reporttitle='2号板 通道2针脚更换后三次压力循环实验报告'")
source = source.replace("<b>{cfg['board']}号板：IN0＝传感器{sid[0]}；IN2＝传感器{sid[1]}。</b>", "<b>本次接线标注假设：2号板IN0＝传感器3号；IN2＝待验证的传感器4号。</b>")
source = source.replace("<p>CSV 的 I10/IN0、I12/IN2视为固件输出的滤波码值；本报告不额外滤波。", "<p>本报告按本次测试命名将IN2标为传感器4号；如果现场实际仍是其他接线，请以硬件接线为准重新解释标签。CSV 的 I10/IN0、I12/IN2视为固件输出的滤波码值；本报告不额外滤波。")
source = source.replace("dest=OUT/f'{cfg[\"board\"]}号板_针筒压力循环实验报告.html'", "dest=OUT/'2号板_通道2针脚更换后三次压力循环实验报告.html'")
source = source.replace("args=argparse.ArgumentParser();parser.add_argument('--session',default='20261030',choices=['20261030','20261103']);args=parser.parse_args()", "args=argparse.ArgumentParser();parser.add_argument('--session',default='20261030',choices=['20261030','20261103']);args=parser.parse_args()")
generated = Path(__file__).with_name('_generated_pin_swap_cycle.py')
generated.write_text(source, encoding='utf-8')
runpy.run_path(str(generated), run_name='__main__')
