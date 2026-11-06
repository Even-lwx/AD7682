import csv, base64, html
from datetime import datetime
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from matplotlib.font_manager import FontProperties

root = Path('D:/lwx/lwx-project/AD7682')
out = root/'output/reports/电池与突变关联_20261027-28'
out.mkdir(parents=True, exist_ok=True)
plt.rcParams.update({'font.family': FontProperties(fname='C:/Windows/Fonts/msyh.ttc').get_name(),
                     'axes.unicode_minus':False,'font.size':10})
boards = []
for number in (1,2):
    filename = root/'code/log'/f'1027-28-{number}板子.csv'
    rows = list(csv.reader(filename.open(encoding='utf-8-sig',newline='')))
    parsed = []
    rejected = 0
    for line in rows[1:]:
        try:
            stamp = datetime(*[int(float(x)) for x in line[:6]])
            values = [float(x) for x in line[6:]]
            if len(values)!=9: raise ValueError()
            parsed.append((stamp, values))
        except (ValueError, TypeError): rejected += 1
    parsed.sort()
    times = np.array([r[0] for r in parsed])
    v = np.array([r[1] for r in parsed])
    invalid = (v == -9999)
    v[invalid] = np.nan
    dt = np.array([(times[i+1]-times[i]).total_seconds() for i in range(len(times)-1)])
    d = np.diff(v,axis=0)
    # Consecutive minute records only; startup, missing intervals and ADC invalid
    # records are never treated as physical ADC steps.
    rises = np.flatnonzero((d[:,8] >= .049) & (dt>0) & (dt<90))+1
    boards.append((times,v,rises))
    print(number,'valid timestamp rows',len(v),'rejected rows',rejected,'ADC invalid',invalid[:,5].sum(),invalid[:,7].sum())
    for i in rises:
        print(number,str(times[i]),'delta',np.round(d[i-1,[0,1,5,7,8]],3))

fig, axes = plt.subplots(4,2,figsize=(16,13),sharex=True)
for bi,(times,v,rises) in enumerate(boards):
    for c, ax, unit in [(8,axes[0,bi],'电池电压 (V)'),(0,axes[1,bi],'温度 (℃)'),(1,axes[2,bi],'相对湿度 (%RH)')]:
        ax.scatter(times,v[:,c],s=6,alpha=.65,linewidths=0)
        ax.set_ylabel(unit)
    for c,label,color in [(5,'IN1','#24764d'),(7,'IN3','#8250ab')]:
        valid = np.flatnonzero(np.isfinite(v[:,c]))
        baseline = np.nanmedian(v[valid[:10],c])
        axes[3,bi].scatter(times,v[:,c]-baseline,s=6,label=label,alpha=.65,c=color,linewidths=0)
    axes[3,bi].set_ylabel('ADC相对前10个有效值中值 (codes)')
    axes[3,bi].legend()
    axes[0,bi].set_title(f'{bi+1}号板')
    for ax in axes[:,bi]:
        for i in rises: ax.axvline(times[i],color='#d95136',alpha=.4,lw=.8)
        ax.grid(alpha=.2)
        ax.xaxis.set_major_locator(mdates.HourLocator(interval=4))
        ax.xaxis.set_major_formatter(mdates.DateFormatter('%m-%d\n%H:%M'))
        ax.tick_params(labelbottom=True)
fig.suptitle('电池电压与温湿度、ADC变化：红色竖线为电池电压相邻记录上升≥0.05 V',fontsize=14)
fig.tight_layout(rect=(0,0,1,.97))
fig.savefig(out/'电池与突变时间对照.png',dpi=150)
plt.close(fig)

table_rows=[]
for bi,(times,v,rises) in enumerate(boards):
    for i in rises:
        if times[i]<datetime(2026,10,27,20) and not (bi==0 and times[i]>=datetime(2026,10,27,17)):
            continue
        # 5-record median reference, followed by 15-minute observation.
        base=np.median(v[max(0,i-5):i,:2],axis=0)
        use=np.array([0 <= (t-times[i]).total_seconds() < 900 for t in times])
        rise=float(np.max(v[use,0])-base[0])
        drop=float(np.min(v[use,1])-base[1])
        delta=v[i]-v[i-1]
        fm=lambda x:'缺测' if not np.isfinite(x) else f'{x:+.3f}'
        table_rows.append([f'{bi+1}号',str(times[i]),f'{v[i-1,8]:.2f} → {v[i,8]:.2f}',
                           fm(delta[5]),fm(delta[7]),f'{rise:+.2f}',f'{drop:+.2f}'])
headers=['板子','电压上跳记录时间','电压 (V)','IN1同步变化 (codes)','IN3同步变化 (codes)',
         '随后15分钟最大温升 (℃)','随后15分钟最大湿度变化 (%RH)']
table='<table><tr>'+''.join('<th>'+x+'</th>' for x in headers)+'</tr>'
for row in table_rows:table+='<tr>'+''.join('<td>'+html.escape(x)+'</td>' for x in row)+'</tr>'
table+='</table>'
png=base64.b64encode((out/'电池与突变时间对照.png').read_bytes()).decode()
report='''<!doctype html><meta charset="utf-8"><title>电池电压与突变关联</title>
<style>body{max-width:1450px;margin:30px auto;font:16px/1.7 "Microsoft YaHei";padding:20px;color:#243047}
table{border-collapse:collapse;font-size:13px}td,th{padding:9px;border:1px solid #ccd5df}th{background:#edf2f7}
img{width:100%}.note{padding:16px;background:#edf2f7}</style>
<h1>10月27日至28日电池电压与突变关联</h1>
<p class="note">本次存在明确的事件时间关联。2号板夜间6次电池电压上跳均伴随IN1/IN3同步上升，随后温度上升、湿度下降。
后4个间隔约128分钟。1号板02:54也出现同一顺序。日志支持优先排查供电/充电状态切换，但不能仅凭记录证明电池是根因。</p>
<p>时间取CSV前6列，I14为电池电压；温湿度为I6/I7，IN1/IN3为I11/I13。1号板1366条有效时间记录，排除1行全零。
2号板1381条。两板IN1/IN3各有333条-9999，作为缺测保留断点，不算尖峰，不补值。
2号板1号气压计全程无效，本次未用于关联分析。原始CSV未修改。两列图采用同一时间范围，各纵轴独立显示细节。</p>
<h2>完整时序散点图</h2>'''
report+=f'<img src="data:image/png;base64,{png}"><h2>主要事件</h2>'+table
report+='''<p>温湿度变化以事件前5条记录中值作基线，取随后15分钟最大温度和最低湿度。属于描述性事件对照，不能代替对照实验。
电压变化来自ADC测得且保留0.01V精度的日志；如果采用滤波固件，短暂跌落还可能被滤波削弱。不能据此排除毫秒级电源扰动。</p>
<h2>解释与下一步</h2><p>约4.10V恢复到4.20～4.21V后，电压维持一段时间再下降，符合充电/供电状态周期切换的候选特征。
温升、相对湿度下降可能来自同一次切换后的板上发热，ADC可同时受到模拟电源、基准或地的影响。
电池测量本身也依赖MCU ADC参考，记录上跳尚未证明电池端真实上跳。建议核对充电芯片型号、供电结构和状态引脚，并同时记录
电池端电压、充电状态、ADC参考/模拟供电。温湿度和ADC不是单点瞬态：1号板02:54事件的恢复过程可延续约20～30分钟。</p>
<p>来源：1027-28-1板子.csv、1027-28-2板子.csv。文件日期按板载RTC解释。</p>'''
(out/'电池与突变关联分析.html').write_text(report,encoding='utf-8')
print('Report',out/'电池与突变关联分析.html')
