from pathlib import Path
import json, hashlib, html
import pandas as pd, numpy as np
ROOT=Path(r'D:/lwx/lwx-project/AD7682'); OUT=ROOT/'output/reports/长期测试_1030-1102'; OUT.mkdir(parents=True,exist_ok=True)
PLOTLY=(ROOT/'tools/analysis/workspace/syringe_reports/plotly.min.js').read_text(encoding='utf8')
START=pd.Timestamp('2026-10-30 18:00:00')
FILES=[('1号板','1030-1102-1号板合并.csv',[1,2]),('2号板','1030-1102-2号板.csv',[3,4])]
COLORS=['#14689d','#ce812b','#158178','#a453a4','#788899']
MAP={'电池电压':'I14','温度':'I6','湿度':'I7','气压计1':'I8','气压计2':'I9','IN0':'I10','IN2':'I12'}
def esc(x): return html.escape(str(x))
def f(x,n=3): return 'N/A' if not np.isfinite(x) else f'{float(x):.{n}f}'
def tbl(rows,heads): return '<div class="tablewrap"><table><thead><tr>'+''.join('<th>'+esc(x)+'</th>' for x in heads)+'</tr></thead><tbody>'+''.join('<tr>'+''.join('<td>'+esc(v)+'</td>' for v in r)+'</tr>' for r in rows)+'</tbody></table></div>'
def vals(x): return [None if pd.isna(v) else round(float(v),5) for v in x]
def trace(x,y,name,color,mode='lines'):
 if len(x)>2000 and len(x)>0 and isinstance(x[0],str) and '-' in x[0]:
  z=pd.DataFrame({'x':pd.to_datetime(x),'y':list(y)}).dropna().set_index('x').resample('1min').mean().dropna()
  x=z.index.strftime('%Y-%m-%d %H:%M:%S').tolist(); y=z.y
 return {'type':'scatter','mode':mode,'x':list(x),'y':vals(y),'name':name,'connectgaps':False,'line':{'color':color,'width':1.35},'marker':{'color':color,'size':3,'opacity':.55}}
boards={}
for board,fn,sensors in FILES:
 p=ROOT/'code/log'/fn; digest=hashlib.sha256(p.read_bytes()).hexdigest(); raw=pd.read_csv(p); assert raw.columns.tolist()==['I'+str(i) for i in range(15)]
 ts=pd.to_datetime(dict(year=raw.I0.astype(int),month=raw.I1.astype(int),day=raw.I2.astype(int),hour=raw.I3.astype(int),minute=raw.I4.astype(int),second=raw.I5.astype(int)))
 raw['time']=ts; d=raw.loc[ts>=START].copy().sort_values('time',kind='stable').set_index('time')
 bad={}
 for c in ['I6','I7','I8','I9','I10','I12','I14']:
  m=~np.isfinite(d[c])|(d[c]==-9999)
  if c in ('I10','I12'):m|=(d[c]<0)|(d[c]>65535)
  if c in ('I8','I9'):m|=(d[c]<=0)
  bad[c]=int(m.sum());d.loc[m,c]=np.nan
 for ch in [0,2]:d[f'P{ch}']=225+1250*d[f'I{10+ch}']/65535
 sec=d.groupby(level=0).mean(numeric_only=True); gaps=sec.index.to_series().diff().dt.total_seconds().dropna()
 boards[board]=dict(raw=d,sec=sec,file=fn,sensors=sensors,digest=digest,bad=bad,gaps=gaps)
print('ranges',[(k,len(v['raw']),len(v['sec']),str(v['sec'].index.min()),str(v['sec'].index.max()),v['gaps'].max()) for k,v in boards.items()])
# Combined summaries use exact second timestamps in the overlap.
a,b=[boards[x]['sec'] for x in boards]
paired=a.join(b,lsuffix='_1',rsuffix='_2',how='inner')
# paired per sensor: board 1 IN0/IN2 to board 2 IN0/IN2
pairs=[('气压计1','I8','I8'),('气压计2','I9','I9'),('电池电压','I14','I14'),('温度','I6','I6'),('湿度','I7','I7'),('传感器1 vs 3 (IN0)','P0','P0'),('传感器2 vs 4 (IN2)','P2','P2'),('IN0 ADC码','I10','I10'),('IN2 ADC码','I12','I12')]
metrics=[]
for label,c1,c2 in pairs:
 q=paired[[c1+'_1',c2+'_2']].dropna();x=q[c1+'_1'].to_numpy();y=q[c2+'_2'].to_numpy();r=np.corrcoef(x,y)[0,1] if len(q)>2 else np.nan
 slope,inter=np.polyfit(x,y,1) if len(q)>2 and np.ptp(x)>0 else (np.nan,np.nan)
 res=y-(slope*x+inter);delta=y-x
 metrics.append(dict(label=label,c1=c1,c2=c2,n=len(q),r=r,slope=slope,inter=inter,bias=delta.mean(),mae=np.abs(delta).mean(),rmse=np.sqrt(np.mean(delta**2)),fitrmse=np.sqrt(np.mean(res**2))))
# html tables per board
stat_rows=[];gap_rows=[];range_end=max(x['sec'].index.max() for x in boards.values())
for board,v in boards.items():
 sec=v['sec'];gap=v['gaps'];
 for label,col,unit in [('电池电压','I14','V'),('温度','I6','°C'),('湿度','I7','%RH'),('气压计1','I8','hPa'),('气压计2','I9','hPa'),('传感器'+str(v['sensors'][0])+' IN0 ADC','I10','码'),('传感器'+str(v['sensors'][1])+' IN2 ADC','I12','码'),('传感器'+str(v['sensors'][0])+' IN0压力','P0','hPa'),('传感器'+str(v['sensors'][1])+' IN2压力','P2','hPa')]:
  z=sec[col].dropna();stat_rows.append([board,label,unit,len(z),f(z.min(),3),f(z.max(),3),f(z.mean(),3),f(z.std(),3),f(z.max()-z.min(),3)])
 gap_rows.append([board,len(v['raw']),len(sec),sec.index.min().strftime('%Y-%m-%d %H:%M:%S'),sec.index.max().strftime('%Y-%m-%d %H:%M:%S'),f((sec.index.max()-START).total_seconds()/3600,2),int((gap>2).sum()),f(gap.max(),0),f(gap[gap>2].sum(),0)])
# chart data and markup
specs=[];individual_cards=[];comparison_cards=[];scatter_cards=[]
def chart(title,traces,unit,note=''):
 i='plot'+str(len(specs));specs.append(dict(id=i,title=title,traces=traces,unit=unit));target=scatter_cards if '散点与线性拟合' in title else comparison_cards if title.startswith('两板') else individual_cards;target.append(f'<article class="card"><div class="chead"><h3>{title}</h3><div><select onchange="setMode(\'{i}\',this.value)"><option value="lines">曲线</option><option value="markers">散点</option><option value="lines+markers">曲线＋散点</option></select><button onclick="zoom(\'{i}\')">放大</button></div></div><p>{note}</p><div class="plot" id="{i}"></div></article>')
# Separate per-board sensor charts
for board,v in boards.items():
 d=v['raw'];n=v['sec'];colorbase=0 if board=='1号板' else 2;x=d.index.strftime('%Y-%m-%d %H:%M:%S').tolist()
 for key,label,unit in [('I14','电池电压','V'),('I6','温度','°C'),('I7','湿度','%RH')]:chart(f'{board} {label}',[trace(x,d[key],board+' '+label,COLORS[colorbase])],unit,'图中为每分钟均值；秒级源数据用于统计与拟合。数据空档不插值。')
 chart(f'{board} 数字气压计1和2',[trace(x,d.I8,'气压计1 U7',COLORS[colorbase]),trace(x,d.I9,'气压计2 U11',COLORS[colorbase+1])],'hPa','虚线为LPS22HH标称量程上限1260 hPa；超限样本只作探索性记录。')
 chart(f'{board} IN0（传感器{v["sensors"][0]}）ADC码',[trace(x,d.I10,f'传感器{v["sensors"][0]} IN0',COLORS[colorbase])],'ADC码','固件滤波后的码值，不在报告中二次滤波。')
 chart(f'{board} IN2（传感器{v["sensors"][1]}）ADC码',[trace(x,d.I12,f'传感器{v["sensors"][1]} IN2',COLORS[colorbase+1])],'ADC码')
 chart(f'{board} IN0与IN2 ADC码对比',[trace(x,d.I10,f'IN0 传感器{v["sensors"][0]}',COLORS[colorbase]),trace(x,d.I12,f'IN2 传感器{v["sensors"][1]}',COLORS[colorbase+1])],'ADC码')
 chart(f'{board} 模拟气压计压力（厂家换算）',[trace(x,d.P0,f'传感器{v["sensors"][0]} IN0',COLORS[colorbase]),trace(x,d.P2,f'传感器{v["sensors"][1]} IN2',COLORS[colorbase+1])],'hPa','厂家公式：P(hPa)=225+1250×ADC/65535，按AD7682外部REF=5 V。')
# long-term dual board comparisons, all x same exact common bounds
for label,c1,c2,unit in [('电池电压','I14','I14','V'),('温度','I6','I6','°C'),('湿度','I7','I7','%RH'),('气压计1 U7','I8','I8','hPa'),('气压计2 U11','I9','I9','hPa'),('模拟压力：IN0 传感器1/3','P0','P0','hPa'),('模拟压力：IN2 传感器2/4','P2','P2','hPa')]:
 chart(f'两板对比：{label}',[trace(a.index.strftime('%Y-%m-%d %H:%M:%S'),a[c1],f'1号板 {label}',COLORS[0]),trace(b.index.strftime('%Y-%m-%d %H:%M:%S'),b[c2],f'2号板 {label}',COLORS[1])],unit,'统一绝对时间轴，按秒聚合；未对曲线做平移、校准或插值。')
# two boards simulated pressures grouped overlays
chart('两板四个模拟压力传感器同图',[trace(boards['1号板']['sec'].index.strftime('%Y-%m-%d %H:%M:%S'),boards['1号板']['sec'].P0,'1号板 传感器1 IN0',COLORS[0]),trace(boards['1号板']['sec'].index.strftime('%Y-%m-%d %H:%M:%S'),boards['1号板']['sec'].P2,'1号板 传感器2 IN2',COLORS[2]),trace(boards['2号板']['sec'].index.strftime('%Y-%m-%d %H:%M:%S'),boards['2号板']['sec'].P0,'2号板 传感器3 IN0',COLORS[1]),trace(boards['2号板']['sec'].index.strftime('%Y-%m-%d %H:%M:%S'),boards['2号板']['sec'].P2,'2号板 传感器4 IN2',COLORS[3])],'hPa','四个厂家公式换算压力使用同一绝对时间轴。')
# trend comparison using 1-hour rolling mean, min_periods 30 seconds
for label,c1,c2,unit in [('气压计1','I8','I8','hPa'),('气压计2','I9','I9','hPa'),('IN0模拟压力','P0','P0','hPa'),('IN2模拟压力','P2','P2','hPa'),('温度','I6','I6','°C'),('湿度','I7','I7','%RH')]:
 traces=[]
 for board,df,color in [('1号板',a,COLORS[0]),('2号板',b,COLORS[1])]:
  roll=df[c1].rolling('1h',min_periods=30).mean();traces.append(trace(df.index.strftime('%Y-%m-%d %H:%M:%S'),roll,board+' 1小时移动均值',color))
 chart(f'两板趋势：{label}（1小时移动均值）',traces,unit,'按绝对时间对齐。移动平均只辅助观察慢变化，原始数据见上方曲线。')
# Difference chart exact matched timestamps
for label,c1,c2,unit in [('气压计1','I8','I8','hPa'),('气压计2','I9','I9','hPa'),('IN0模拟压力','P0','P0','hPa'),('IN2模拟压力','P2','P2','hPa'),('温度','I6','I6','°C'),('湿度','I7','I7','%RH'),('电池电压','I14','I14','V')]:
 q=paired[[c1+'_1',c2+'_2']].dropna();delta=q[c2+'_2']-q[c1+'_1'];chart(f'两板差值：{label}（2号−1号）',[trace(q.index.strftime('%Y-%m-%d %H:%M:%S'),delta,f'2号板−1号板',COLORS[4])],unit,'同一秒时间戳配对；不跨缺测时段插值。正值表示2号板读数高于1号板。')
# Scatter comparison per pair
scatter_metrics=[]
for m in metrics:
 c1,c2,label=m['c1'],m['c2'],m['label'];q=paired[[c1+'_1',c2+'_2']].dropna();x=q[c1+'_1'].to_numpy();y=q[c2+'_2'].to_numpy(); sl,it=np.polyfit(x,y,1);ends=[x.min(),x.max()]
 plot_q=q.resample('1min').mean().dropna(); px=plot_q[c1+'_1'].to_numpy();py=plot_q[c2+'_2'].to_numpy()
 chart(f'两板散点与线性拟合：{label}',[trace(px,py,'每分钟均值配对',COLORS[2],mode='markers'),trace(ends,sl*np.array(ends)+it,f'秒级OLS y={sl:.5g}x{it:+.4g}',COLORS[0]),trace(ends,ends,'一致线 y=x',COLORS[4])],label+' ('+('hPa' if '压力' in label or '气压计' in label else '同原始单位')+')','散点按分钟均值显示；拟合参数使用全部同秒数据。拟合用于观察板间关系，不代表校准或准确度。')
# Metrics tables
mt=[]
for m in metrics:mt.append([m['label'],m['n'],f(m['r'],5),f(m['slope'],5),f(m['inter'],4),f(m['bias'],4),f(m['mae'],4),f(m['rmse'],4),f(m['fitrmse'],4)])
# overall overall report end/start and coverage
end=max(x['sec'].index.max() for x in boards.values());duration=end-START
start_iso=START.strftime('%Y-%m-%d %H:%M:%S');end_iso=end.strftime('%Y-%m-%d %H:%M:%S')
# Summary statements for all paired items
head=f'<div class="metrics"><div><small>共同分析起点</small><b>{start_iso}</b></div><div><small>共同分析终点</small><b>{end_iso}</b></div><div><small>长期测试时长</small><b>{duration.days}天 {duration.seconds//3600}小时{duration.seconds%3600//60}分</b></div><div><small>同秒配对点</small><b>{len(paired):,}</b></div></div>'
summary=[]
for m in metrics:
 summary.append(f'<li>{m["label"]}：同秒相关系数 r={m["r"]:.4f}，板间平均差（2号−1号）={m["bias"]:+.4f}，RMSE={m["rmse"]:.4f}。</li>')
summary.append('<li>需要重点复核 IN2 传感器对比：传感器4（2号板）的厂家公式换算值与传感器2（1号板）存在明显均值差，且同秒相关性较低；当前报告不通过回归校正消除该差异。</li>')
# chart event markers: start only, no cycling assumptions
specjson=json.dumps(specs,ensure_ascii=False,allow_nan=False)
css='''*{box-sizing:border-box}body{margin:0;background:#f2f6f9;color:#263749;font:15px/1.7 "Microsoft YaHei",Arial,sans-serif}header,nav,section,footer{max-width:1440px;margin:auto}header{background:linear-gradient(120deg,#18384c,#245b62);color:white;padding:42px;border-radius:0 0 20px 20px}h1{font-size:32px;margin:8px 0}h2{font-size:24px;color:#17445d;margin:0 0 18px}h3{font-size:18px;margin:8px 0}.eyebrow{font-size:12px;color:#c3dce0;letter-spacing:1px}.metrics{display:grid;grid-template-columns:repeat(4,1fr);gap:14px;margin-top:24px}.metrics div{background:#ffffff15;padding:12px 16px;border-radius:8px}.metrics small{display:block;color:#c7dbdf}.metrics b{font-size:18px}nav{padding:18px 24px;display:flex;gap:22px;flex-wrap:wrap}a{color:#17618c;text-decoration:none}section{background:white;border:1px solid #e1e8ee;border-radius:12px;padding:28px 32px;margin-bottom:22px}.notice{background:#fff7e5;border-left:4px solid #d49a37;padding:14px}.formula{font-size:17px;background:#eef7f7;border-radius:8px;padding:14px 18px}.controls{position:sticky;top:0;background:#edf4f7;padding:10px;border-radius:7px;z-index:5;display:flex;gap:16px;align-items:center;flex-wrap:wrap}.card{border:1px solid #dfe7ed;border-radius:10px;padding:10px 16px;margin:20px 0}.chead{display:flex;justify-content:space-between;align-items:center}.card p{font-size:13px;color:#61768a;margin:4px 0}.plot{height:410px}.tablewrap{overflow:auto;margin:15px 0}table{border-collapse:collapse;width:100%;font-size:13px;font-variant-numeric:tabular-nums}th,td{padding:9px;border-bottom:1px solid #e2eaf0;text-align:right;white-space:nowrap}th{background:#edf3f7;color:#244b62}td:first-child,th:first-child{text-align:left}tr:nth-child(even){background:#f8fafb}button,select{border:1px solid #cad8e1;background:#fff;border-radius:5px;padding:6px 10px;color:#234e67;cursor:pointer}.card:fullscreen{padding:24px;background:white}.card:fullscreen .plot{height:85vh}footer{padding:20px;color:#62788a}@media(max-width:760px){header{padding:22px}.metrics{grid-template-columns:repeat(2,1fr)}section{padding:16px}h1{font-size:24px}.plot{height:340px}.chead{align-items:flex-start;gap:6px;flex-direction:column}}@media print{nav,.controls,button,select{display:none!important}header{print-color-adjust:exact}.card,section{break-inside:avoid}.plot{height:350px}}'''
script='''const specs=__SPECS__,start=__START__,end=__END__,graphs=[];let syncing=false;const ids=specs.map(s=>s.id);
function setMode(id,mode){Plotly.restyle(id,{mode})}function zoom(id){const c=document.getElementById(id).closest('.card');document.fullscreenElement?document.exitFullscreen():c.requestFullscreen()}
document.addEventListener('fullscreenchange',()=>graphs.forEach(g=>Plotly.Plots.resize(g)));
function resetAll(){syncing=true;Promise.all(ids.map(id=>Plotly.relayout(id,{'xaxis.range':[start,end],'yaxis.autorange':true}))).finally(()=>syncing=false)}
window.reportReady=Promise.all(specs.map(s=>Plotly.newPlot(s.id,s.traces,{font:{family:'Microsoft YaHei,Arial',color:'#30495d'},margin:{l:82,r:28,t:35,b:72},paper_bgcolor:'white',plot_bgcolor:'white',xaxis:{type:'date',range:[start,end],title:'绝对时间',tickformat:'%m-%d %H:%M',gridcolor:'#e5edf2'},yaxis:{title:s.unit,automargin:true,gridcolor:'#e5edf2',tickformat:s.unit==='V'?'.3f':'.2f'},hovermode:'x unified',legend:{orientation:'h',y:-.23},dragmode:'zoom'},{responsive:true,scrollZoom:true,displaylogo:false,toImageButtonOptions:{format:'png',filename:document.title+'_'+s.title,scale:2}}).then(g=>{graphs.push(g);g.on('plotly_relayout',ev=>{if(syncing||!document.getElementById('sync').checked)return;let u;if(ev['xaxis.range'])u={'xaxis.range':ev['xaxis.range']};else if(ev['xaxis.range[0]']!==undefined)u={'xaxis.range':[ev['xaxis.range[0]'],ev['xaxis.range[1]']]};else if(ev['xaxis.autorange'])u={'xaxis.range':[start,end]};if(u){syncing=true;Promise.all(ids.filter(x=>x!==s.id).map(id=>Plotly.relayout(id,u))).finally(()=>syncing=false)}});return s.id})));'''.replace('__SPECS__',specjson).replace('__START__',json.dumps(start_iso)).replace('__END__',json.dumps(end_iso))
# 页面正文使用格式化字符串，图表和统计仍由前面的计算结果生成。
body=f'''<header><div class="eyebrow">AD7682 长期测试</div><h1>两块板长期传感器数据对比</h1>{head}</header>
<section id="summary"><h2>结果概览</h2><ul>{''.join(summary)}</ul><p class="notice">相关高说明长期共同变化，不等于绝对准确。板间差值同时包含传感器个体差异、安装位置、模拟前端/ADC差异、参考电压、滤波与时间戳误差；没有独立可溯源标准时，不能仅靠两板互相比对判定哪一块准确。</p></section>
<section id="method"><h2>数据来源、时间轴和传感器编号</h2><p>两份日志均从 <b>2026-10-30 18:00:00</b> 开始纳入分析。统一时区按设备记录时间解释为本地时间。横轴统一显示至两份日志中较晚记录的时间 <b>{end_iso}</b>；该时刻之后没有1号板样本。每幅时序图共享相同绝对时间边界，图表缩放默认联动。报告不包含起点以前的记录。</p>{tbl([[b,v['file'],v['sensors'][0],v['sensors'][1],v['raw'].index.min().strftime('%F %T'),v['raw'].index.max().strftime('%F %T'),len(v['raw']),len(v['sec'])] for b,v in boards.items()],['板号','CSV','IN0传感器号','IN2传感器号','本板首条','本板末条','原始行数','不同秒'])}<p>1号板：IN0＝传感器1号，IN2＝传感器2号。2号板：IN0＝传感器3号，IN2＝传感器4号。两块板上的U7称为气压计1，U11称为气压计2。每条原始记录保留在单板趋势图中；板间配对、统计、拟合采用同一整秒的多条记录均值。只使用精确相同的秒时间戳配对，不对缺测时间插值或外推。</p><div class="formula">ADC压力换算：V = ADC × 5.000 / 65535<br>P(kPa) = 22.5 + 25 × V<br><b>P(hPa) = 225 + 1250 × ADC / 65535</b></div><p>公式来自用户提供的 IPNZAA135KP-4428-A1 表格，按名义外部参考电压5 V换算；未对传感器做零点或增益拟合校正。电池采样的MCU 3.3 V参考与该AD7682换算无关。数字气压计标称范围约260～1260 hPa，超出范围部分作为观测数据展示但不作为量程内性能证据。</p><p>CSV中的IN0/IN2视为固件滤波后的ADC码；图表使用每分钟均值，统计、板间拟合使用秒级均值，不做二次滤波。长时间序列不进行跨空档插值。单板日志中的重复秒在原始数据聚合时按时间取平均。</p></section>
<section id="comparison"><h2>两板横向对比</h2><div class="controls"><label><input id="sync" type="checkbox" checked> 所有图联动时间缩放</label><button onclick="resetAll()">恢复共同时间范围</button><button onclick="print()">打印／保存PDF</button></div><p>拖动框选缩放、滚轮缩放、双击复位；悬停看值和时间；图例点击隐藏、双击突出；每图可切换曲线/散点、全屏和下载PNG。</p>{''.join(comparison_cards)}</section>
<section id="individual"><h2>单板传感器长期趋势</h2>{''.join(individual_cards)}</section>
<section id="fit"><h2>两板相关性与线性关系</h2><p>回归为同秒数据的普通最小二乘：y=2号板、x=1号板，含截距；r为Pearson相关，平均差、MAE、RMSE直接以2号−1号计算。RMSE还包括恒定偏移，拟合RMSE衡量去掉线性映射后的残差，二者含义不同。时间序列相邻点相关，因此样本数不是独立重复实验次数，不据此计算虚假的置信度或显著性。</p>{tbl(mt,['变量','配对秒数','r','斜率','截距','平均差','MAE','RMSE','拟合RMSE'])}<p>如果一项环境量（温湿度、电池电压）在两板中同步变化，相关系数只描述共变，不说明因果关系。压力/温湿度慢变化还可能受环境位置、封装、通风和传感器热惯性影响。</p>{''.join(scatter_cards)}</section>
<section id="stats"><h2>统计与数据连续性</h2>{tbl(stat_rows,['板号','测量项','单位','有效秒数','最小','最大','均值','标准差','峰峰值'])}{tbl(gap_rows,['板号','有效原始行','不同秒','起点','终点','时长 h','大于2秒空档数','最大空档 s','空档累计 s'])}<p>标准差与峰峰值覆盖整个长期过程，包含真实环境变化，不能直接视为传感器噪声。若两板记录不同秒数，统计样本数只计有效秒。</p><p>数据无效标记-9999、非有限值及ADC超范围值按变量排除，不做补值。ADC码65535保留为测量码，不能仅凭满量程值判断是通信异常还是模拟输入饱和。</p><p>源CSV未修改。文件及SHA-256：</p>{tbl([[b,v['file'],v['digest']] for b,v in boards.items()],['板号','文件','SHA-256'])}</section><footer>长期测试报告 · 基于设备记录时间；压力换算及传感器编号按用户提供信息。</footer>'''
html_doc='<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>两块板长期传感器数据对比</title><style>'+css+'</style><script>'+PLOTLY+'</script></head><body>'+body+'<script>'+script+'</script></body></html>'
out=OUT/'两块板长期数据对比报告.html';out.write_text(html_doc,encoding='utf8')
print('output',out,'plots',len(specs),'paired seconds',len(paired),'size',out.stat().st_size)
for m in metrics:print(m['label'],m['n'],m['r'],m['bias'],m['rmse'])
