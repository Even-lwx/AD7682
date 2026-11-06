import csv,base64,html,json
from pathlib import Path
from datetime import datetime
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.dates as md
from matplotlib.font_manager import FontProperties
root=Path('D:/lwx/lwx-project/AD7682')
out=root/'output/reports/5V板数据分析_20261028-29'
out.mkdir(parents=True,exist_ok=True)
rows=list(csv.reader((root/'code/log/1028-29_5V板子.csv').open(encoding='utf-8-sig')))[1:]
rows.sort(key=lambda r:tuple(float(x) for x in r[:6]))
t=[datetime(*[int(float(x)) for x in r[:6]]) for r in rows]
v=np.array([[float(x) for x in r[6:]] for r in rows]);v[v==-9999]=np.nan
names=['温度','湿度','1号气压计','2号气压计','IN0','IN1','IN2','IN3','电压记录I14']
units=['℃','%RH','hPa','hPa','codes','codes','codes','codes','V']
d=np.diff(v,axis=0)
plt.rcParams.update({'font.family':FontProperties(fname='C:/Windows/Fonts/msyh.ttc').get_name(),'axes.unicode_minus':False,'font.size':10})
def timeaxis(ax):
 ax.grid(alpha=.2);ax.xaxis.set_major_locator(md.HourLocator(interval=2));ax.xaxis.set_major_formatter(md.DateFormatter('%m-%d\n%H:%M'));ax.tick_params(labelbottom=True)
def save(fig,name):
 fig.tight_layout();fig.savefig(out/name,dpi=155,bbox_inches='tight');plt.close(fig)
def image(name):return '<img src="data:image/png;base64,'+base64.b64encode((out/name).read_bytes()).decode()+'">'
def table(headers,rr):
 return '<table><tr>'+''.join('<th>'+html.escape(str(x))+'</th>' for x in headers)+'</tr>'+''.join('<tr>'+''.join('<td>'+html.escape(str(x))+'</td>' for x in row)+'</tr>' for row in rr)+'</table>'
fig,axes=plt.subplots(7,1,figsize=(15,19),sharex=True)
for c,ax in [(8,axes[0]),(0,axes[1]),(1,axes[2])]:
 ax.scatter(t,v[:,c],s=8,linewidths=0);ax.set_ylabel(names[c]+' ('+units[c]+')')
for c in [2,3]:axes[3].scatter(t,v[:,c],s=8,label=names[c],linewidths=0)
axes[3].set_ylabel('气压 (hPa)');axes[3].legend()
for c in range(4,8):
 axes[4].scatter(t,v[:,c]-np.nanmedian(v[:10,c]),s=7,label=names[c],linewidths=0)
axes[4].set_ylabel('ADC相对前10条中值\n(codes)');axes[4].legend(ncol=4)
axes[5].scatter(t,v[:,3]-v[:,2],s=8,linewidths=0);axes[5].set_ylabel('2号−1号气压 (hPa)')
for c in range(4,8):axes[6].scatter(t[1:],d[:,c],s=7,label=names[c],linewidths=0)
axes[6].set_ylabel('ADC相邻记录变化\n(codes)');axes[6].legend(ncol=4)
for ax in axes:timeaxis(ax)
axes[0].set_title('5V供电板：电压、温湿度、气压、ADC共用时间轴')
save(fig,'01_全程散点.png')
fig,axes=plt.subplots(4,1,figsize=(15,12),sharex=True)
for ax,c in zip(axes,range(4,8)):
 ax.scatter(t,v[:,c],s=9,linewidths=0);ax.set_ylabel(names[c]+' (codes)');timeaxis(ax)
axes[0].set_title('四路ADC独立纵坐标：显示各通道细节')
save(fig,'02_ADC四通道.png')
eventrows=[];counts={}
for c,floor in [(0,.5),(1,.5),(4,100),(5,100),(6,100),(7,100),(8,.03)]:
 x=d[:,c];median=np.nanmedian(x);mad=np.nanmedian(abs(x-median));threshold=max(floor,6*1.4826*mad)
 ids=np.flatnonzero(abs(x-median)>=threshold)
 counts[names[c]]=len(ids)
 for i in ids:
  eventrows.append([t[i+1].isoformat(' '),names[c],f'{v[i,c]:.3f}',f'{v[i+1,c]:.3f}',f'{x[i]:+.3f}',f'{d[i,8]:+.2f}',f'{threshold:.3f}'])
eventrows.sort()
summary=[]
for c,n in enumerate(names):
 x=v[:,c];summary.append([n,units[c],np.isfinite(x).sum(),f'{np.nanmin(x):.3f}',f'{np.nanmax(x):.3f}',f'{np.nanstd(x):.3f}',f'{np.nanmedian(x[-10:])-np.nanmedian(x[:10]):+.3f}',f'{np.nanpercentile(abs(d[:,c]),95):.3f}'])
def corr(a,b):
 ok=np.isfinite(a)&np.isfinite(b)
 return float(np.corrcoef(a[ok],b[ok])[0,1]) if ok.sum()>3 and np.std(a[ok])>0 and np.std(b[ok])>0 else np.nan
correlations=[[names[c],f'{corr(v[:,c],v[:,8]):.3f}',f'{corr(d[:,c],d[:,8]):.3f}'] for c in [0,1,4,5,6,7]]
print('time',t[0],t[-1],'rows',len(t),'events',counts,'correlations',correlations)
print('ADC large events',[r for r in eventrows if r[1].startswith('IN')])
report='''<!doctype html><meta charset="utf-8"><title>5V板数据分析 10月28日至29日</title>
<style>body{max-width:1350px;margin:30px auto;padding:0 22px;color:#26364b;font:16px/1.75 "Microsoft YaHei"}img{width:100%}table{border-collapse:collapse;width:100%;font-size:13px}td,th{border:1px solid #ced8e1;padding:8px;text-align:right}th{background:#edf3f8}h2{margin-top:36px}.note{padding:15px;background:#edf3f8}</style>
<h1>5V供电板数据分析</h1>
<p class="note">878条记录，时间按指定的2026-10-28 18:30:00起点重建，每条1分钟，截止2026-10-29 09:07:00。
时间是人工重建时间，不能用于精确外部事件对齐。所有图均为散点图，时间轴对齐。原始传感器数值未改。</p>
<h2>主要发现</h2><p>I14记录在1.94～2.02 V之间，最大相邻变化0.01 V，没有出现上次日志中约4.10→4.20 V的周期上跳。
这只是I14测量结果，不能把它当作已经验证的实际5V供电电压；该通道的接线与标定需要确认。</p>
<p>温湿度在起始阶段有明显恢复过程，末尾09:05也出现温升、湿度下降。中间没有重现上次约128分钟一次的明显温湿度事件，
但本次只有14小时37分钟，且供电、传感器工作点等条件改变，不能证明干扰已永久消失。</p>
<p>四路ADC仍有相邻记录大跳变，IN1～IN3还有明显慢趋势。不能把全程标准差当作纯噪声，也不能因为无电压跳变就说ADC完全正常。
本次各通道工作点与此前板子差别较大，未确认输入接线和参考条件，不能直接比较codes标准差评判供电方案优劣。</p>
<h2>统一时间轴</h2>'''
report+=image('01_全程散点.png')+'<h2>四路ADC细节</h2>'+image('02_ADC四通道.png')
report+='<h2>统计汇总</h2>'+table(['字段','单位','有效条数','最小','最大','全程标准差','末10条−首10条中值','相邻变化绝对值P95'],summary)
report+='<h2>与电压的相关性</h2>'+table(['字段','数值Pearson r','相邻差分Pearson r'],correlations)
report+='<p>数值相关性可能由共同随时间漂移造成。差分用于观察相邻变化是否同步，仍不能证明因果；I14只有0.01V分辨率、每分钟一条且可能经过滤波，不能排除快速电源扰动。</p>'
report+='<h2>突变记录时间</h2><p>筛选阈值：相邻差分偏离其中位数至少6×1.4826×MAD，且温湿度至少0.5℃/%RH、ADC至少100 codes、电压至少0.03V。阈值筛出的是大变化，不自动等同故障。时间为变化后的记录时间。</p>'
report+=table(['时间','字段','前值','后值','变化量','同条电压变化 (V)','筛选阈值'],eventrows)
pr=corr(v[:,2],v[:,3]);pdiff=v[:,3]-v[:,2]
report+=f'<h2>两个气压计一致性</h2><p>1号气压计对应U7，2号气压计对应U11。气压趋势相关系数r={pr:.4f}，2号减1号平均差{np.mean(pdiff):.3f} hPa，差值标准差{np.std(pdiff):.3f} hPa。共同趋势一致不代表经过标准源校准的绝对精度。</p>'
report+='<p>来源：1028-29_5V板子.csv。当前15列依次为6列时间、温度、湿度、两路气压、IN0～IN3、I14电压。本报告未更改固件或CSV。</p>'
(out/'5V板数据分析.html').write_text(report,encoding='utf-8')
print(out/'5V板数据分析.html')
