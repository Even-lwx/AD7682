from pathlib import Path
import json, hashlib, html, argparse
import pandas as pd, numpy as np
parser=argparse.ArgumentParser()
parser.add_argument('--session',choices=['1030-1102','1102-1103'],default='1030-1102')
parser.add_argument('--compare-weekend',action='store_true')
args=parser.parse_args()
TODAY=args.session=='1102-1103'
ROOT=Path(r'D:/lwx/lwx-project/AD7682'); OUT=ROOT/('output/reports/长期测试_'+args.session); OUT.mkdir(parents=True,exist_ok=True)
VREF=2.5 if TODAY else 5.0
GENERATED='2026-11-03' if TODAY else '2026-11-02'
FILES=[('1号板','1102-1103-1号板.csv',[1,2]),('2号板','1102-1103-2号板.csv',[3,4])] if TODAY else [('1号板','1030-1102-1号板合并.csv',[1,2]),('2号板','1030-1102-2号板.csv',[3,4])]
START=pd.Timestamp('2026-11-02 12:00:00') if TODAY else pd.Timestamp('2026-10-30 18:00:00')
LIMIT_END=pd.Timestamp('2026-11-03 09:00:00') if TODAY else None
COLORS=['#14689d','#ce812b','#158178','#a453a4','#788899','#d64d53','#6c63ac','#8e5d2e']
MAP={'电池电压':'I14','温度':'I6','湿度':'I7','气压计1':'I8','气压计2':'I9','IN0':'I10','IN2':'I12'}
def esc(x): return html.escape(str(x))

def analogs(board,v):
 return [('I10','P0',v['sensors'][0])] + ([] if board=='2号板' and not TODAY else [('I12','P2',v['sensors'][1])])

def digital(board,n): return f'{board}气压计{n}号'
def f(x,n=3): return 'N/A' if not np.isfinite(x) else f'{float(x):.{n}f}'
def tbl(rows,heads): return '<div class="tablewrap"><table><thead><tr>'+''.join('<th>'+esc(x)+'</th>' for x in heads)+'</tr></thead><tbody>'+''.join('<tr>'+''.join('<td>'+esc(v)+'</td>' for v in r)+'</tr>' for r in rows)+'</tbody></table></div>'
boards={}
for board,fn,sensors in FILES:
 p=ROOT/'code/log'/fn; digest=hashlib.sha256(p.read_bytes()).hexdigest(); raw=pd.read_csv(p); assert raw.columns.tolist()==['I'+str(i) for i in range(15)]
 ts=pd.to_datetime(dict(year=raw.I0.astype(int),month=raw.I1.astype(int),day=raw.I2.astype(int),hour=raw.I3.astype(int),minute=raw.I4.astype(int),second=raw.I5.astype(int)))
 raw['time']=ts; mask=ts>=START
 if LIMIT_END is not None:mask &= ts<=LIMIT_END
 d=raw.loc[mask].copy().sort_values('time',kind='stable').set_index('time')
 bad={}
 for c in ['I6','I7','I8','I9','I10','I12','I14']:
  m=~np.isfinite(d[c])|(d[c]==-9999)
  if c in ('I10','I12'):m|=(d[c]<0)|(d[c]>65535)
  if c in ('I8','I9'):m|=(d[c]<=0)
  bad[c]=int(m.sum());d.loc[m,c]=np.nan
 for ch in [0,2]:d[f'P{ch}']=225+250*VREF*d[f'I{10+ch}']/65536
 sec=d.groupby(level=0).mean(numeric_only=True); gaps=sec.index.to_series().diff().dt.total_seconds().dropna()
 boards[board]=dict(raw=d,sec=sec,file=fn,sensors=sensors,digest=digest,bad=bad,gaps=gaps)
print('ranges',[(k,len(v['raw']),len(v['sec']),str(v['sec'].index.min()),str(v['sec'].index.max()),v['gaps'].max()) for k,v in boards.items()])
# Combined summaries use exact second timestamps in the overlap.
a,b=[boards[x]['sec'] for x in boards]
paired=a.join(b,lsuffix='_1',rsuffix='_2',how='inner')
# paired per sensor: board 1 IN0/IN2 to board 2 IN0/IN2
pairs=[('电池电压','I14','I14'),('温度','I6','I6'),('湿度','I7','I7')]
metrics=[]
for label,c1,c2 in pairs:
 q=paired[[c1+'_1',c2+'_2']].dropna();x=q[c1+'_1'].to_numpy();y=q[c2+'_2'].to_numpy();r=np.corrcoef(x,y)[0,1] if len(q)>2 and np.ptp(x)>0 and np.ptp(y)>0 else np.nan
 slope,inter=np.polyfit(x,y,1) if len(q)>2 and np.ptp(x)>0 else (np.nan,np.nan)
 res=y-(slope*x+inter);delta=y-x
 metrics.append(dict(label=label,c1=c1,c2=c2,n=len(q),r=r,slope=slope,inter=inter,bias=delta.mean(),mae=np.abs(delta).mean(),rmse=np.sqrt(np.mean(delta**2)),fitrmse=np.sqrt(np.mean(res**2))))
# html tables per board
stat_rows=[];gap_rows=[];range_end=max(x['sec'].index.max() for x in boards.values())
for board,v in boards.items():
 sec=v['sec'];gap=v['gaps'];
 for label,col,unit in [('电池电压','I14','V'),('温度','I6','°C'),('湿度','I7','%RH'),('气压计1','I8','hPa'),('气压计2','I9','hPa'),('传感器'+str(v['sensors'][0])+' IN0 ADC','I10','码'),('传感器'+str(v['sensors'][1])+' IN2 ADC','I12','码'),('传感器'+str(v['sensors'][0])+' IN0压力','P0','hPa'),('传感器'+str(v['sensors'][1])+' IN2压力','P2','hPa')]:
  if board=='2号板' and col in ('I12','P2') and not TODAY:continue
  label=label.replace('气压计1',board+'气压计1号').replace('气压计2',board+'气压计2号')
  for sid in v['sensors']:label=label.replace('传感器'+str(sid)+' ', '传感器'+str(sid)+'号 ')
  z=sec[col].dropna();stat_rows.append([board,label,unit,len(z),f(z.min(),3),f(z.max(),3),f(z.mean(),3),f(z.std(),3),f(z.max()-z.min(),3)])
 gap_rows.append([board,len(v['raw']),len(sec),sec.index.min().strftime('%Y-%m-%d %H:%M:%S'),sec.index.max().strftime('%Y-%m-%d %H:%M:%S'),f((sec.index.max()-START).total_seconds()/3600,2),int((gap>2).sum()),f(gap.max(),0),f(gap[gap>2].sum(),0)])
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt, matplotlib.dates as mdates
from matplotlib.ticker import ScalarFormatter
import base64,io
plt.rcParams.update({'font.sans-serif':['Microsoft YaHei'],'axes.unicode_minus':False,'path.simplify':False,'agg.path.chunksize':10000,'font.size':10})
END=LIMIT_END if LIMIT_END is not None else max(v['raw'].index.max() for v in boards.values()); figures=[]; sections={'board':[],'cross':[],'fit':[]}
def figplot(title,series,unit,kind='board',scatter=False,note=''):
 fig,ax=plt.subplots(figsize=(14,4.5));fig.patch.set_facecolor('white');ax.set_facecolor('#fcfdff')
 for i,(x,y,label) in enumerate(series):
  color=COLORS[i%len(COLORS)];y=np.asarray(y,dtype=float)
  if scatter:
   ax.scatter(x,y,s=2,alpha=.17,color=color,label=label,rasterized=True)
  else:
   x=pd.DatetimeIndex(x); yy=y.copy(); gaps=np.r_[False,np.diff(x.asi8)>2000000000];
   # Insert NaN separators without discarding either endpoint around missing intervals.
   xx=x.to_numpy(); at=np.flatnonzero(gaps)
   if len(at):xx=np.insert(xx,at,xx[at]);yy=np.insert(yy,at,np.nan)
   ax.plot(xx,yy,lw=.7,color=color,label=label)
 if not scatter:
  ax.set_xlim(START,END);ax.xaxis.set_major_locator(mdates.HourLocator(interval=3 if TODAY else 12));ax.xaxis.set_major_formatter(mdates.DateFormatter('%m-%d\n%H:%M'));ax.set_xlabel('设备记录时间（统一绝对时间轴）')
 else:ax.set_xlabel('参照压力 / 1号板读数（与图例和表格单位一致）')
 ax.set_ylabel(unit);ax.grid(color='#dbe5ed',lw=.6,alpha=.8);ax.spines[['top','right']].set_visible(False);ax.legend(loc='upper left',bbox_to_anchor=(0,1.18),ncol=4,frameon=False,fontsize=9)
 ax.ticklabel_format(axis='y',style='plain',useOffset=False);fig.subplots_adjust(left=.085,right=.98,bottom=.18,top=.84)
 buf=io.BytesIO();fig.savefig(buf,format='png',dpi=150);plt.close(fig)
 image=base64.b64encode(buf.getvalue()).decode(); idx=len(figures);figures.append(title)
 sections[kind].append(f'<article><h3>{esc(title)}</h3><p>{note}</p><a href="data:image/png;base64,{image}" target="_blank"><img src="data:image/png;base64,{image}" alt="{esc(title)}" loading="lazy"></a></article>')
 print('figure',idx+1,flush=True)
# Single-board charts: every valid observation, excluding contact-fault sensor 4.
for board,v in boards.items():
 d=v['raw']; aa=analogs(board,v)
 for title,c,u in [('电池电压','I14','V'),('温度','I6','°C'),('湿度','I7','%RH')]:
  figplot(board+' '+title,[(d.index,d[c],board+' '+title)],u)
 for adc,pressure,sid in aa:
  figplot(f'{board} 传感器{sid}号 ADC码',[(d.index,d[adc],f'传感器{sid}号')],'ADC码')
  figplot(f'{board} 传感器{sid}号换算压力',[(d.index,d[pressure],f'传感器{sid}号')],'hPa')
 ds=[(d.index,d.I8,digital(board,1)),(d.index,d.I9,digital(board,2))]
 ps=[(d.index,d[pc],f'传感器{sid}号') for ac,pc,sid in aa]
 figplot(board+' 两个数字气压计',ds,'hPa')
 if len(aa)>1:
  figplot(board+' 模拟传感器 ADC码对比',[(d.index,d[ac],f'传感器{sid}号') for ac,pc,sid in aa],'ADC码')
  figplot(board+' 模拟传感器换算压力对比',ps,'hPa')
 figplot(board+' 模拟与数字压力对比',ds+ps,'hPa')
 ref=d[['I8','I9']].mean(axis=1,skipna=False)
 figplot(board+' 模拟压力相对数字均值的差值',[(d.index,d[pc]-ref,f'传感器{sid}号−{board}数字均值') for ac,pc,sid in aa],'hPa')
# Cross-board charts exclude comparisons against the faulty sensor.
for label,c,u in [('电池电压','I14','V'),('温度','I6','°C'),('湿度','I7','%RH')]:
 series=[]
 for board,v in boards.items():
  name=board+label
  series.append((v['raw'].index,v['raw'][c],name))
 figplot('两板对比 '+label,series,u,'cross')
 q=paired[[c+'_1',c+'_2']].dropna()
 delta_label='2号板'+label+'−1号板'+label
 figplot('两板差值 '+label,[(q.index,q[c+'_2']-q[c+'_1'],delta_label)],u,'cross',note='所有同秒配对值参与计算，重复秒先取均值；不插值。')
figplot('两块板四个数字气压计同图对比',[(v['raw'].index,v['raw'][col],digital(board,n)) for board,v in boards.items() for n,col in [(1,'I8'),(2,'I9')]],'hPa','cross')
figplot(('四个' if TODAY else '三个')+'模拟传感器 ADC码同图对比',[(v['raw'].index,v['raw'][ac],f'传感器{sid}号') for board,v in boards.items() for ac,pc,sid in analogs(board,v)],'ADC码','cross')
figplot(('四个' if TODAY else '三个')+'模拟传感器厂家换算压力同图对比',[(v['raw'].index,v['raw'][pc],f'传感器{sid}号') for board,v in boards.items() for ac,pc,sid in analogs(board,v)],'hPa','cross')
fitrows=[];conclusions=[]
for board,v in boards.items():
 sec=v['sec']
 for adc,ac,sid in analogs(board,v):
  for dc,n in [('I8',1),('I9',2)]:
   refname=digital(board,n)
   q=sec[[dc,ac]].dropna();x=q[dc].to_numpy();y=q[ac].to_numpy();sl,it=np.polyfit(x,y,1);pr=sl*x+it;e=y-x;res=y-pr;r=np.corrcoef(x,y)[0,1];r2=1-np.sum(res**2)/np.sum((y-y.mean())**2)
   fitrows.append([board,f'传感器{sid}号',refname,len(q),f(r,5),f(r2,5),f(sl,5),f(it,3),f(e.mean()),f(np.sqrt(np.mean(e**2))),f(np.sqrt(np.mean(res**2))),'观察性比较'])
   if dc=='I8':
    conclusions.append(f'<li>传感器{sid}号 vs {refname}：平均差 {e.mean():+.2f} hPa，r={r:.4f}，斜率={sl:.4f}。</li>')
    figplot(f'传感器{sid}号与{refname}散点',[(x,y,f'传感器{sid}号 vs {refname}')],'模拟压力 (hPa)','fit',True,'未抽点。点密集处会重叠；线性参数列于表格。')
# Daily table in long format avoids displaying faulty sensor columns.
daily=[]
for board,v in boards.items():
 for date,q in v['sec'].groupby(v['sec'].index.date):
  measures=[('电池电压','I14','V'),('温度','I6','°C'),('湿度','I7','%RH'),(digital(board,1),'I8','hPa'),(digital(board,2),'I9','hPa')]
  measures += [(f'传感器{sid}号',pc,'hPa') for ac,pc,sid in analogs(board,v)]
  for label,col,unit in measures:
   z=q[col].dropna()
   daily.append([board,str(date),label,unit,len(z),f(z.mean()),f(z.min()),f(z.max()),f(z.std())])
# actual gap inventory and coverage count
gaplist=[];quality=[]
for board,v in boards.items():
 sec=v['sec'];diff=v['gaps'];expected=int((sec.index.max()-sec.index.min()).total_seconds())+1
 quality.append([board,len(v['raw']),len(sec),len(v['raw'])-len(sec),expected-len(sec),f(len(sec)/expected*100,3),str(sec.index.max()),sum(v['bad'].values())])
 for t,dt in diff[diff>2].items():gaplist.append([board,str(t-pd.Timedelta(seconds=dt)),str(t),int(dt),int(dt-1)])
metricrows=[]
for m in metrics:
 unit='ADC码' if 'ADC' in m['label'] else 'hPa' if '气压计' in m['label'] or '传感器' in m['label'] else 'V' if '电池' in m['label'] else '°C' if '温度' in m['label'] else '%RH'
 metricrows.append([m['label'],unit,m['n'],f(m['r'],5),f(m['slope'],5),f(m['inter']),f(m['bias']),f(m['rmse']),f(m['fitrmse']),'观察性比较'])
noise_rows=[]
if TODAY:
 residual_series=[];relative_series=[]
 for board,v in boards.items():
  d=v['sec']; measures=[('I8',digital(board,1)),('I9',digital(board,2))]+[(pc,f'传感器{sid}号') for ac,pc,sid in analogs(board,v)]
  for col,label in measures:
   x=d[col].dropna();base=x.rolling('5min',center=True,min_periods=30).median();res=x-base
   delta=x.diff().where(x.index.to_series().diff().dt.total_seconds()==1)
   minute=x.resample('1min').agg(['min','max','count']);span=(minute['max']-minute['min']).where(minute['count']>=30).dropna()
   noise_rows.append([label,f(span.median()),f(span.quantile(.95)),f(np.sqrt(np.nanmean(res**2))),f(res.quantile(.975)-res.quantile(.025)),f(delta.abs().quantile(.95)),f(delta.abs().max())])
   residual_series.append((x.index,res,label));relative_series.append((x.index,x-x.iloc[0],label))
 figplot('各路有效压力相对各自首值的变化',relative_series,'相对首值变化 / hPa','cross',note='仅减去各通道首个有效值，用于比较变化方向；不代表绝对压力校准，不用于修正原数据。')
 figplot('各路有效压力扣除5分钟局部基线后的波动',residual_series,'偏离局部基线 / hPa','cross',note='局部基线为居中5分钟中位数，仅作离线波动诊断；包含所有有效点，不是新增固件滤波结果。')
 noise_html='<section id="noise"><h2>模拟与数字压力的短时波动幅值</h2><p>同秒重复值先取均值，表中所有幅值单位均为hPa。分钟峰峰值仅统计有至少30个有效秒的分钟；相邻变化仅比较严格相邻的1秒。居中5分钟中位数作为局部基线，残差RMS和中央95%宽度用于区分短时波动与长期变化，不能直接等同于传感器本底噪声。</p>'+tbl(noise_rows,['测量项','分钟峰峰值中位数','分钟峰峰值P95','基线残差RMS','残差中央95%宽度','相邻秒绝对变化P95','最大相邻秒绝对变化'])+'<p>数字气压日志仅有0.1hPa小数分辨率，分钟峰峰值为0表示记录值未跨过量化刻度，不代表没有噪声。模拟压力波动会随假定参考电压同比例缩放。低通滤波可以减轻快速抖动，但会带来响应延迟，不能修复持续漂移、通道接触异常或绝对压力换算偏差。</p></section>'
else:noise_html=''
# Verify full data boundary and source hashes.
for board,v in boards.items():
 assert v['raw'].index.min()>=START
 if LIMIT_END is not None:assert v['raw'].index.max()<=LIMIT_END
 assert hashlib.sha256((ROOT/'code/log'/v['file']).read_bytes()).hexdigest()==v['digest']
css='''*{box-sizing:border-box}body{margin:0;background:#f2f6fa;color:#24384c;font:15px/1.8 "Microsoft YaHei",Arial}header,section,nav,footer{max-width:1400px;margin:auto}header{background:linear-gradient(120deg,#183b50,#24636b);padding:40px;color:white;border-radius:0 0 20px 20px}h1{font-size:30px}h2{color:#20536b;font-size:24px}h3{font-size:18px}section{padding:26px 32px;background:white;border-radius:12px;margin-bottom:22px;border:1px solid #e0e8ef}nav{padding:18px;display:flex;gap:24px;flex-wrap:wrap}a{color:#146d97}article{border:1px solid #e0e7ee;border-radius:9px;padding:14px;margin:18px 0;break-inside:avoid}article p{font-size:13px;color:#647b8e}img{width:100%;height:auto}table{border-collapse:collapse;width:100%;font-size:13px}th,td{padding:9px;border-bottom:1px solid #e1e8ef;white-space:nowrap;text-align:right}th{background:#edf4f7}th:first-child,td:first-child{text-align:left}.tablewrap{overflow-x:auto;margin:16px 0}.notice{background:#fff3dc;border-left:4px solid #d1a033;padding:16px}.formula{background:#edf7f5;padding:18px;font-size:18px}footer{padding:22px;color:#718391}@media print{nav{display:none}article{break-inside:avoid}section{border:0}header{print-color-adjust:exact}}'''
header=f'<header><p>{START:%Y.%m.%d}—{END:%Y.%m.%d} · 全数据静态图版</p><h1>两块板长期传感器数据分析报告</h1><p>分析区间：{START} 至 {END}（北京时间，按设备RTC记录）</p><p>1号板 {len(a):,} 个不同秒；2号板 {len(b):,} 个不同秒；两板精确同秒配对 {len(paired):,} 个。</p></header>'
notice=('本次仅分析2026-11-02 12:00:00至2026-11-03 09:00:00，含两端点。四个模拟传感器均纳入曲线、统计、相关性与拟合分析。' if TODAY else '传感器4号（2号板IN2）此前确认接触不良，无明确恢复时间，因此排除于正常性能分析。')

body=header+'<nav><a href="#overview">概览</a><a href="#method">分析口径</a><a href="#cross">两板对比</a><a href="#board">单板曲线</a><a href="#fit">相关与线性</a><a href="#quality">统计与数据质量</a></nav>'
summary='四个数字气压计以同一时间轴集中展示；'+('传感器1号、2号、3号、4号同图比较。' if TODAY else '传感器1号、2号、3号同图比较。')
summary+=' '.join(f"两板{m['label']}平均差（2号−1号）为{m['bias']:+.3f} {'V' if m['label']=='电池电压' else '°C' if m['label']=='温度' else '%RH'}，"+(f"r={m['r']:.4f}。" if np.isfinite(m['r']) else '因读数恒定，相关系数无定义。') for m in metrics)
if TODAY:
 summary+='用户确认本次传感器供电=2.5V，AD7682外部REF=2.5V。厂家表包含最高4.5V输出，但未注明适用供电，不能直接作为本次供电下的完整标定。按原厂家公式条件换算，四路模拟压力约为757～780 hPa，而数字压力约为1020～1024 hPa；这些模拟hPa仅为算术条件值，绝对压力与压力波动幅值的物理适用性尚未确认，不应据此评定精度。应确认厂家2.5V供电标定及器件最低工作电压，不擅自按供电比例缩放。举例：按原表1023hPa对应3.192V；若另假设它是5V标定下严格比例输出，2.5V供电应为1.596V，而本次传感器1号约为2.18V，仍不吻合，故该比例假设也未得到支持。本次1号板电池字段为8.21～8.22V，2号板为恒定8.21V；不能计算有意义的两板电压相关系数，也不能用2号板恒定的电压记录解释压力与温湿度的波动。'
body+=f'<section id="overview"><h2>主要发现</h2><p class="notice">{notice}</p><ul>'+''.join(conclusions)+f'</ul><p>{summary}</p></section>'
body+=f'''<section id="method"><h2>实验范围与计算口径</h2><p>仅纳入{START}至{END}内的记录，含两端点，范围外数据不参与绘图、统计和拟合。所有时序图横轴起止完全一致；1号板止于{a.index.max()}，2号板止于{b.index.max()}，缺测部分不补齐。没有三次循环、阈值平台或主动加压阶段的假设。</p><p><b>全部原始有效记录参与时序绘图，不降采样、不按分钟聚合、不平滑，也关闭Matplotlib路径简化。</b>固定图片有有限像素，因此密集数据会重叠；不表示数据被删减。单板时序保留重复秒的全部值；统计、回归及板间差值先将同秒多值取均值，然后仅配对相同的RTC秒时间。记录缺口插入断线，不跨空档连线或插值。RTC未提供同步误差，跨板对齐只能精确到记录的秒。</p><p>1号板IN0＝传感器1号，IN2＝传感器2号；2号板IN0＝传感器3号，IN2＝传感器4号（本次按上方说明纳入或排除）。每块板的U7称为该板气压计1号，U11称为该板气压计2号；图例均注明板号。故障通道对应关系见上方说明。CSV I6温度、I7湿度、I8/I9数字压力、I10/IN0、I12/IN2、I14电池电压。ADC值按固件已经滤波的输出分析。</p><div class="formula">V＝ADC×{VREF:.3f}/65536<br>P(kPa)＝22.5＋25×V<br><b>P(hPa)＝225＋{250*VREF:.0f}×ADC/65536</b></div><p>厂家换算来自用户提供的IPNZAA135KP-4428-A1表格；压力单位按kPa，AD7682 REF按本次{VREF:.3f} V计算，参考电压来自用户确认。表中0.5V对应35kPa、2.5V对应85kPa、4.5V对应135kPa，故斜率25kPa/V、截距22.5kPa。这里把压力单位按kPa解释。该表没有注明传感器供电条件，本次若传感器供电已变化，不能未经确认直接沿用它的绝对压力标定。按此表，1023hPa需要约3.192V，超过2.5V REF的正常输入量程；目前硬件条件、表格适用条件与压力换算之间存在不一致。图表中的模拟hPa是按此表计算的条件值，不代表已确认真实压力。16位ADC采用65536量化级，改用65535只影响约0.01hPa量级，无法解释约250hPa偏差。未做零点补偿或拟合校正。MCU电池ADC的3.3 V参考与此公式无关。绝对压力准确度仍取决于实际REF、器件校准和压力标准。LPS22HH标称压力量程260～1260 hPa；本次数字压力最小{min(v["sec"][["I8","I9"]].min().min() for v in boards.values()):.1f} hPa，最大{max(v["sec"][["I8","I9"]].max().max() for v in boards.values()):.1f} hPa。</p><p>整体标准差同时包含环境变化、漂移和测量误差，不能称作静态噪声；长期窄压力范围拟合不足以评价厂家全量程线性。相关度不代表准确度或因果关系；时序样本不是独立重复试验，因此不计算夸大的显著性。</p></section>'''
body+='<section id="cross"><h2>两块板的同类传感器横向对比</h2><p>全区间时序图使用同一时间边界，纵轴按测量项独立适配；异常局部放大图会明确注明独立时间范围。</p>'+''.join(sections['cross'])+'</section>'
body+='<section id="board"><h2>每块板的独立数据分析</h2>'+''.join(sections['board'])+'</section>'
body+='<section id="fit"><h2>相关度、线性关系与压力误差</h2><p>下表x为数字气压、y为厂家换算模拟压力，OLS拟合y=a·x+b。r为Pearson相关，R²为含截距拟合优度；直接RMSE＝sqrt(mean((y−x)²))，拟合RMSE＝sqrt(mean((y−ŷ)²))。拟合没有替换测量值。</p>'+tbl(fitrows,['板号','模拟传感器','数字参照','秒数','r','R²','斜率a','截距hPa','平均差hPa','直接RMSE hPa','拟合RMSE hPa','解释范围'])+'<h3>两板同类量的线性关系</h3><p>x为1号板、y为2号板，各行单位见表。本次传感器纳入范围见概览。拟合为描述性分析，不代表校准或厂家精度验证。</p>'+tbl(metricrows,['变量','单位','配对秒数','r','斜率','截距','2号−1号均差','直接RMSE','拟合RMSE','解释范围'])+''.join(sections['fit'])+'</section>'
body+=noise_html
comparison_images=0
if args.compare_weekend:
 from compare_weekend import build_comparison
 comparison_html,comparison_images=build_comparison(ROOT,OUT)
 body+=comparison_html
 body=body.replace('<a href="#overview">概览</a>','<a href="#overview">概览</a><a href="#weekend">周末5V历史对比</a>')
 body=body.replace('<h2>主要发现</h2>','<h2>主要发现</h2><p class="notice"><b>ADC趋势结论：</b>周末5V组与数字气压强相关，本次相关性明显下降；该变化不能用固定线性换算公式解释。滤波不保证修复慢漂移或错误趋势。<a href="#trend-conclusion">查看ADC相关系数及解释</a>。</p>')
body+='<section id="quality"><h2>全区间统计和每天变化</h2>'+tbl(stat_rows,['板号','测量项','单位','有效秒','最小','最大','均值','标准差','峰峰值'])+'<h3>每天统计（首尾两天为不完整自然日）</h3>'+tbl(daily,['板号','日期','测量项','单位','有效秒','均值','最小','最大','标准差'])+'<h3>记录连续性</h3>'+tbl(quality,['板号','原始行','不同秒','同秒额外行','缺测秒','秒覆盖率%','末条时间','排除无效字段数'])+'<h3>超过2秒的采样间隔</h3>'+tbl(gaplist,['板号','前一条时间','后一条时间','端点间隔s','中间缺测秒'])+'<p>缺测秒数根据逐秒理论网格计算。-9999、非有限值及ADC越界值按字段剔除，不修改CSV；ADC满量程65535保留为观测，不能单凭数值区分饱和与通信故障。日报不代表精度测试；未减去日均值、没有去漂移。</p><h3>来源与追溯</h3>'+tbl([[k,v['file'],v['digest']] for k,v in boards.items()],['板号','CSV源文件','SHA-256'])+'</section><footer>'+GENERATED+'生成 · 全数据静态图片 · 源CSV未修改</footer>'
content='<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>两块板长期数据对比报告</title><style>'+css+'</style></head><body>'+body+'</body></html>'
report=OUT/'两块板长期数据对比报告.html';report.write_text(content,encoding='utf8')
(OUT/'分析核验.json').write_text(json.dumps({'start':str(START),'end':str(END),'figures':len(figures)+comparison_images,'paired_seconds':len(paired),'board_rows':{k:len(v['raw']) for k,v in boards.items()},'downsampled':False,'reference_volts':VREF,'noise_rows':noise_rows},ensure_ascii=False,indent=2),encoding='utf8')
print('DONE',len(figures)+comparison_images,report.stat().st_size,flush=True)
