from pathlib import Path
import json, hashlib, base64, html, argparse
import pandas as pd
import numpy as np
ROOT=Path(r'D:/lwx/lwx-project/AD7682')
parser=argparse.ArgumentParser();parser.add_argument('--session',default='20261030',choices=['20261030','20261103']);args=parser.parse_args()
NEW=args.session=='20261103'; DATE='2026-11-05'; VREF=2.5 if NEW else 5.0
OUT=ROOT/'output/reports/针脚更换压力循环_20261105'
OUT.mkdir(exist_ok=True,parents=True)
JS=(ROOT/'tools/analysis/workspace/syringe_reports/plotly.min.js').read_text(encoding='utf-8')
CONFIGS=[dict(board=2,file='换了2号板通道2的针脚3次压力循环-2号板5VBOOST.csv',start='09:32:45',end='09:36:59',finish='09:36:59',sensors=[4,3],events=[('09:32:45','实验开始'),('09:36:59','三次高低压循环结束／实验截止')])]
COLORS=['#1769aa','#d48124','#168579','#a150a8']
if NEW:
 CONFIGS=[dict(board=1,file='2611031033_1号板2V5电压循环测试.csv',start='10:23:56',end='10:31:19',finish='10:30:25',sensors=[1,2],events=[('10:23:56','盖上活塞／实验开始'),('10:25:00','隔绝大气'),('10:30:25','三次高低压循环结束'),('10:30:51','连通大气'),('10:31:19','取出活塞／实验截止')]),dict(board=2,file='2611031047_2号板2V5电压循环测试.csv',start='10:39:05',end='10:44:39',finish='10:44:17',sensors=[4,3],events=[('10:39:05','隔绝大气／实验开始'),('10:44:17','连通大气'),('10:44:39','取出活塞／实验截止')])]

def stamp(t): return pd.Timestamp(DATE+' '+t)
def num(v,n=3): return f'{float(v):.{n}f}' if np.isfinite(v) else '不适用'
def table(rows,heads):
 return '<div class="tablewrap"><table><thead><tr>'+''.join('<th>'+html.escape(str(v))+'</th>' for v in heads)+'</tr></thead><tbody>'+''.join('<tr>'+''.join('<td>'+html.escape(str(v))+'</td>' for v in row)+'</tr>' for row in rows)+'</tbody></table></div>'
def arr(a): return [None if pd.isna(v) else round(float(v),6) for v in a]
def correlation(a,b):
 q=pd.concat([a,b],axis=1).dropna()
 return q.iloc[:,0].corr(q.iloc[:,1]) if len(q)>2 and q.iloc[:,0].nunique()>1 and q.iloc[:,1].nunique()>1 else np.nan
def xytrace(x,y,name,color,mode='lines',**kw): return dict(x=list(x),y=arr(y),name=name,type='scatter',mode=mode,line=dict(color=color,width=1.6),marker=dict(color=color,size=4,opacity=.65),connectgaps=False,**kw)

for cfg in CONFIGS:
 path=ROOT/'code/log'/cfg['file']; hash_before=hashlib.sha256(path.read_bytes()).hexdigest()
 raw=pd.read_csv(path); assert list(raw.columns)==['I'+str(i) for i in range(15)]
 ts=pd.to_datetime(dict(year=raw.I0.astype(int),month=raw.I1.astype(int),day=raw.I2.astype(int),hour=raw.I3.astype(int),minute=raw.I4.astype(int),second=raw.I5.astype(int)))
 raw['t']=ts
 d=raw[(ts>=stamp(cfg['start']))&(ts<=stamp(cfg['end']))].sort_values('t',kind='stable').copy().set_index('t')
 assert len(d)>0 and d.index.min()>=stamp(cfg['start']) and d.index.max()<=stamp(cfg['end'])
 # Invalid sentinels never become plausible pressures. The source is unchanged.
 invalid={}
 for c in ['I6','I7','I8','I9','I10','I12','I14']:
  bad=~np.isfinite(d[c]) | (d[c]==-9999)
  if c in ['I10','I12']: bad|=(d[c]<0)|(d[c]>65535)
  if c in ['I8','I9']: bad|=(d[c]<=0)
  invalid[c]=int(bad.sum()); d.loc[bad,c]=np.nan
 for ch in [0,2]:
  d[f'V{ch}']=d[f'I{10+ch}']*VREF/65536
  d[f'P{ch}']=225+250*d[f'V{ch}']
 d['Dmean']=d[['I8','I9']].mean(axis=1,skipna=False)
 d['Ddiff']=d.I8-d.I9
 s=d.groupby(level=0).mean(numeric_only=True)
 dt=s.index.to_series().diff().dt.total_seconds().dropna()
 sid=cfg['sensors']; labels=[f'{cfg["board"]}号板气压计1号',f'{cfg["board"]}号板气压计2号',f'传感器{sid[0]}号',f'传感器{sid[1]}号']; keys=['I8','I9','P0','P2']
 t=d.index.strftime('%Y-%m-%d %H:%M:%S').tolist(); st=s.index.strftime('%Y-%m-%d %H:%M:%S').tolist()
 specs=[]; sections=[]
 def chart(title, traces,unit,time=True,note='',scatter=False):
  id='chart'+str(len(specs)); specs.append(dict(id=id,title=title,traces=traces,unit=unit,time=time,scatter=scatter))
  sections.append(f'<article class="chart-card"><div class="chart-head"><h3>{title}</h3><div><select aria-label="图表类型" onchange="changeMode(\'{id}\',this.value)"><option value="lines">曲线图</option><option value="markers" '+('selected' if scatter else '')+f'>散点图</option><option value="lines+markers">曲线＋散点</option></select> <button onclick="full(\'{id}\')">放大</button></div></div><p class="chart-note">{note}</p><div id="{id}" class="plot"></div></article>')
 def timechart(title,cs,ns,unit,note=''):
  chart(title,[xytrace(t,d[c],n,COLORS[i]) for i,(c,n) in enumerate(zip(cs,ns))],unit,note=note)
 timechart('01 电池电压',['I14'],['电池电压'],'V','观察供电变化；日志记录的是电池电压，不能直接代表 ADC REF 引脚的纹波。')
 timechart('02 温度',['I6'],['SHT41温度'],'°C')
 timechart('03 相对湿度',['I7'],['SHT41相对湿度'],'%RH')
 timechart('04 两个数字气压计',['I8','I9'],labels[:2],'hPa','两路使用同一纵坐标。灰色虚线为 LPS22HH 标称量程上限 1260 hPa；超出部分仅作探索性对比。')
 timechart('05 IN0通道 ADC码值',['I10'],[labels[2]+' ADC码值'],'ADC码','CSV 中的 ADC 字段已经过固件滤波；本报告没有再次滤波。')
 timechart('06 IN2通道 ADC码值',['I12'],[labels[3]+' ADC码值'],'ADC码')
 timechart('07 IN0与IN2 ADC码值对比',['I10','I12'],labels[2:],'ADC码')
 timechart('08 IN0厂家公式换算压力',['P0'],[labels[2]],'hPa',f'条件换算：P(hPa)=225+{250*VREF:.0f}×ADC/65536；REF={VREF:.3f}V。表格供电适用条件未确认。' if NEW else '按此前厂家表格换算。')
 timechart('09 IN2厂家公式换算压力',['P2'],[labels[3]],'hPa')
 timechart('10 两个模拟压力传感器',['P0','P2'],labels[2:],'hPa')
 timechart('11 模拟与数字四路压力横向对比',keys,labels,'hPa','四路同一单位、同一时间轴，不进行零点平移或拟合校正。')
 if NEW:
  timechart('11a 模拟传感器输出电压',['V0','V2'],labels[2:],'V','ADC电压=日志码值×2.5/65536。输入接近2.5V时，须考虑量程顶端压缩或饱和。')
  for ac,lab in zip(['I10','I12'],labels[2:]):
   q=s[['Dmean',ac]].dropna()
   chart('ADC码直接响应：'+lab,[xytrace(q.Dmean,q[ac],lab+' 日志ADC码',COLORS[2 if ac=='I10' else 3],mode='markers')],'ADC码',False,'横轴为同板数字气压均值。绕开电压—压力公式直接观察响应；上端平台或弯曲不能用零点偏置解释。',True)
 # Fit using one-second means; full and nominal valid range.
 fits=[]; fit_records=[]
 for ac,lab in zip(['P0','P2'],labels[2:]):
  for dc,dlab in zip(['I8','I9'],labels[:2]):
   for scope,mask in [('全实验区间',pd.Series(True,index=s.index)),('数字量程内',s[dc].between(260,1260))]:
    q=s.loc[mask,[dc,ac]].dropna(); x=q[dc].to_numpy(); y=q[ac].to_numpy(); a,b=np.polyfit(x,y,1); pred=a*x+b; resid=y-pred; e=y-x; r=np.corrcoef(x,y)[0,1]; span=np.ptp(x)
    record=dict(analog=lab,digital=dlab,scope=scope,n=len(q),slope=a,intercept=b,r=r,r2=1-np.sum(resid**2)/np.sum((y-y.mean())**2),bias=e.mean(),mae=np.abs(e).mean(),rmse=np.sqrt(np.mean(e**2)),fitrmse=np.sqrt(np.mean(resid**2)),maxresid=np.max(np.abs(resid)),span=span)
    fit_records.append(record)
    fits.append([lab,dlab,scope,len(q),num(r,6),num(record['r2'],6),num(a,5),num(b,2),num(e.mean(),2),num(record['rmse'],2),num(record['fitrmse'],2)])
  # analog vs digital mean fit diagram
  q=s[['Dmean',ac]].dropna(); x=q.Dmean.to_numpy(); y=q[ac].to_numpy(); a,b=np.polyfit(x,y,1); xx=[float(x.min()),float(x.max())]
  chart('线性关系：'+lab+' vs 数字气压均值',[
   xytrace(x,y,'每秒均值',COLORS[2 if ac=='P0' else 3],mode='markers'),
   xytrace(xx,a*np.array(xx)+b,f'OLS拟合 y={a:.5f}x{b:+.2f}',COLORS[0]),
   xytrace(xx,xx,'理想一致线 y=x','#8a96a6')], '模拟压力 (hPa)',False,'横坐标为两个数字气压计的同秒均值 (hPa)。拟合图不代表传感器已被校准。',True)
 for ac in ['P0','P2']: d['E'+ac]=d[ac]-d.Dmean
 timechart('12 模拟压力相对数字均值的误差',['EP0','EP2'],labels[2:],'hPa','误差=厂家公式换算压力−数字气压均值。保留零点偏差和动态滤波差异。')
 timechart('13 两个数字气压计的差值',['Ddiff'],['气压计1−气压计2'],'hPa')
 # Bland Altman, quantitative agreement beyond r.
 ba=[]
 for ac,lab,col in zip(['P0','P2'],labels[2:],COLORS[2:]):
  q=s[[ac,'Dmean']].dropna(); e=q[ac]-q.Dmean; bias=e.mean(); sd=e.std(ddof=1); ba.append([lab,num(bias,2),num(sd,2),num(bias-1.96*sd,2),num(bias+1.96*sd,2)])
  xx=(q[ac]+q.Dmean)/2; ends=[float(xx.min()),float(xx.max())]
  chart('一致性分析：'+lab,[xytrace(xx,e,'每秒差值',col,mode='markers'),xytrace(ends,[bias]*2,'平均差',COLORS[0]),xytrace(ends,[bias-1.96*sd]*2,'平均差−1.96SD','#8995a6'),xytrace(ends,[bias+1.96*sd]*2,'平均差＋1.96SD','#8995a6')],'模拟−数字均值 (hPa)',False,'横坐标为模拟压力和数字均值的平均值 (hPa)。一致性界限是本次动态试验的描述量，含滤波时延，不是厂家精度。',True)
 # Episodes >1200; final three before cycle finish.
 work=s.loc[:stamp(cfg['finish'])]; high=work.I8>1200; blocks=(high!=high.shift()).cumsum(); episodes=[]
 for _,q in work[high].groupby(blocks[high]):
  if len(q)>=2: episodes.append((q.index.min(),q.index.max()))
 formal=episodes[-3:]
 if len(formal)!=3:raise ValueError(f'板{cfg["board"]}识别高压段{len(formal)}个，需核对阈值与时间')
 eventrows=[]
 for i,(a,b) in enumerate(episodes):
  q=s.loc[a:b]; eventrows.append(['正式第'+str(formal.index((a,b))+1)+'次' if (a,b) in formal else '前置压力操作',a.strftime('%H:%M:%S'),b.strftime('%H:%M:%S'),num((b-a).total_seconds(),0),num(q.I8.max(),2)])
 cyclerows=[]
 for i,(a,b) in enumerate(formal):
  nextstart=formal[i+1][0] if i<2 else stamp(cfg['finish'])
  after=s[(s.index>b)&(s.index<nextstart)]
  q=s.loc[a:b]
  for k,lab in zip(keys,labels):
   valid=q[k].dropna(); peak_t=valid.idxmax(); trough=after[k].dropna()
   cyclerows.append([i+1,lab,a.strftime('%H:%M:%S')+'–'+b.strftime('%H:%M:%S'),num(valid.mean(),2),num(valid.max(),2),peak_t.strftime('%H:%M:%S'),num(trough.min(),2) if len(trough) else '无回落样本',trough.idxmin().strftime('%H:%M:%S') if len(trough) else '—'])
 # Stages by supplied timestamps, half-open except final.
 stage_rows=[]
 events=[(stamp(tm),txt) for tm,txt in cfg['events']]
 for i in range(len(events)-1):
  a,lab=events[i]; b,_=events[i+1]; q=s[(s.index>=a)&((s.index<=b) if i==len(events)-2 else (s.index<b))]
  stage_rows.append([lab,a.strftime('%H:%M:%S')+'–'+b.strftime('%H:%M:%S'),len(q),*[num(q[k].mean(),2) for k in keys],num(q.I6.mean(),2),num(q.I7.mean(),2),num(q.I14.mean(),3)])
 stats=[]
 for k,lab,unit in [('I14','电池','V'),('I6','温度','°C'),('I7','湿度','%RH'),*[(k,l,'hPa') for k,l in zip(keys,labels)]]:
  q=s[k].dropna(); stats.append([lab,unit,len(q),num(q.min()),num(q.max()),num(q.mean()),num(q.std()),num(q.max()-q.min())])
 nominal=[r for r in fit_records if r['scope']=='数字量程内' and r['digital']==labels[0]]
 summary=''.join(f'<li>{r["analog"]}与气压计1的量程内比较：r={r["r"]:.5f}，斜率={r["slope"]:.4f}，平均差={r["bias"]:+.2f} hPa，直接比较RMSE={r["rmse"]:.2f} hPa。</li>' for r in nominal)
 if NEW:
  response_rows=[]
  for col,lab in zip(['I10','I12'],labels[2:]):
   q=s[[col,'Dmean']].dropna(); r=q[col].corr(q.Dmean)
   top=q[col]>=.99*65536
   response_rows.append([lab,len(q),num(r,5),num(q[col].min(),2),num(q[col].max(),2),int(top.sum()),num(top.mean()*100,2)])
   summary+=f'<li>{lab}日志ADC码与同板数字均值全程相关系数r={r:.5f}；ADC≥99%满量程的记录秒数为{int(top.sum())}。相关系数不证明高压段保持线性。</li>'
 over=int((s.I8>1260).sum()); dif=s.Ddiff.dropna(); duration=(stamp(cfg['end'])-stamp(cfg['start'])).total_seconds()
 # descriptive associations
 assoc=[]
 for ac,lab in zip(['P0','P2'],labels[2:]):
  e=s[ac]-s.Dmean
  for kc,kl in [('I14','电池电压'),('I6','温度'),('I7','湿度')]:
   assoc.append([lab,kl,num(correlation(e,s[kc]),4),num(correlation(e.diff(),s[kc].diff()),4)])
 meta=dict(board=cfg['board'],source=cfg['file'],source_sha256=hash_before,start=str(stamp(cfg['start'])),end=str(stamp(cfg['end'])),rows=len(d),seconds=len(s),over1260_seconds=over,fits=fit_records,formal_high_segments=[(str(a),str(b)) for a,b in formal])
 (OUT/f'板子{cfg["board"]}_统计.json').write_text(json.dumps(meta,ensure_ascii=False,indent=2),encoding='utf-8')
 # guarantee no source modification
 assert hashlib.sha256(path.read_bytes()).hexdigest()==hash_before
 reporttitle='2号板 通道2针脚更换后三次压力循环实验报告'
 cards=f'<div class="metrics"><div><small>实验区间</small><b>{cfg["start"]}—{cfg["end"]}</b></div><div><small>实验时长</small><b>{int(duration)//60}分{int(duration)%60}秒</b></div><div><small>记录 / 不同秒</small><b>{len(d)} / {len(s)}</b></div><div><small>分析通道</small><b>传感器{sid[0]}、传感器{sid[1]}</b></div></div>'
 body=f'''<header><div class="eyebrow">{DATE} · AD7682 / LPS22HH · 实验 {cfg['board']}</div><h1>{reporttitle}</h1><p>全实验时间序列、厂家压力换算与模拟／数字传感器一致性分析</p>{cards}</header>
 <nav><a href="#overview">结果概览</a><a href="#protocol">实验与方法</a><a href="#plots">交互图表</a><a href="#cycles">循环分析</a><a href="#linear">相关与线性</a><a href="#quality">数据质量</a></nav>
 <section id="overview"><h2>结果概览</h2>{('<p class="notice"><b>本次关键现象：</b>模拟ADC能够跟随三次加减压，但高压阶段接近满量程并出现顶端平台。需要区分整体相关性与高压端线性；滤波无法恢复被压缩或截断的压力信息。以下模拟hPa及其误差是按原厂家表计算的条件值，表格在2.5V传感器供电下的适用性未确认，不将约250hPa偏差直接判作传感器精度误差。</p>' if NEW else '')}<ul>{summary}<li>两个数字气压计平均差（气压计1−2）为 {dif.mean():.3f} hPa，差值标准差 {dif.std():.3f} hPa。</li><li>记录中气压计1超过1260 hPa的不同秒有 {over} 个。超量程结果不用于认定标称精度；表中另列量程内拟合。</li></ul><p class="notice">结论边界：相关性、线性拟合和相互一致性不等于可溯源的准确度。厂家提供的表格只有标称转换关系，没有精度、温漂或供电容差；本次数字气压计也不是经认证压力标准，不能据此作“合格／不合格”判定。</p></section>
 <section id="protocol"><h2>实验范围、编号与换算方法</h2><p><b>2号板：IN0＝传感器4号；IN2＝传感器3号。</b>气压计1为U7，气压计2为U11。所有曲线、统计和拟合严格使用 {cfg['start']}～{cfg['end']}（含两端），不使用实验外的样本。</p>
 {table([[tm,txt] for tm,txt in cfg['events']],['用户提供时间','实验动作'])}
 <div class="formula">V = ADC × {VREF:.3f} / 65536<br>P(kPa) = 22.5 + 25 × V<br><b>P(hPa) = 225 + 250 × V = 225 + {250*VREF:.0f} × ADC / 65536</b></div>
 <p>按用户提供的IPNZAA135KP-4428-A1表格：0.5V对应35kPa、1.5V对应60kPa、3V对应97.5kPa、4.5V对应135kPa，数学关系为P(kPa)=22.5+25V。ADC参考按{VREF:.3f}V计算，采用16位65536量化级；MCU电池ADC的3.3V参考不参与换算。未加零点、增益或拟合校正。</p>
 {('<p class="notice"><b>2.5V供电与换算适用性：</b>本次按传感器供电2.5V、AD7682 REF=2.5V解释。厂家表含4.5V输出，但未注明适用供电，不能直接当作本次完整标定。按照原表，1023hPa应输出约3.192V，超过2.5V参考输入范围；因此本报告模拟hPa为沿用表格的条件计算值，不视为已验证的实际压力，压力误差、RMSE及斜率仅作条件描述。优先查看ADC码、电压及其与数字压力的响应。固定正斜率线性换算不会改变Pearson相关系数，故ADC趋势与数字气压是否一致可以直接判断。ADC接近满量程可能掩盖高压响应，滤波无法恢复被量程压缩或截断的信号。</p>' if NEW else '')}

 <p>本报告按本次测试命名将IN2标为传感器4号；如果现场实际仍是其他接线，请以硬件接线为准重新解释标签。CSV 的 I10/IN0、I12/IN2视为固件输出的滤波码值；本报告不额外滤波。所有时序图保留原记录。同秒重复值没有亚秒时标，不能给它们虚构毫秒；统计、回归与阶段分析先按秒取均值。标准差包含实验主动加压／减压，不能等同于静态噪声。</p></section>
 <section id="plots"><h2>交互数据图表</h2><div class="controls"><label><input id="sync" type="checkbox" checked> 同步所有时序图的横坐标缩放</label><button onclick="resetAll()">恢复全实验范围</button><button onclick="window.print()">打印／保存PDF</button></div><p>拖动框选放大，滚轮缩放，双击复位；图例点击显隐、双击独显；悬停查看秒级时间和值。每张图可切换曲线／散点并全屏，工具栏可下载PNG。横坐标初始范围完全一致，纵坐标各自适配以保留细节。所有脚本和数据均内嵌，HTML可离线打开。</p>{''.join(sections)}</section>
 <section id="cycles"><h2>阶段与循环分析</h2><h3>按用户操作时间分段的均值</h3>{table(stage_rows,['阶段起始动作','时间区间','不同秒数',*labels,'温度 °C','湿度 %RH','电池 V'])}<p>前几段均为完整阶段均值，包含主动变化，不代表稳定平台。各阶段半开区间划分，最后区间包含终点，边界样本不重复计算。</p>
 <h3>自动识别的高压段与前期操作</h3><p>规则：对气压计1按秒均值，以&gt;1200 hPa且至少连续2个记录秒识别高压段；循环结束时间之前最后3段定义为正式三次高压，其余是前置操作。第二组没有单独提供循环结束时间，识别截止使用连通大气时刻。此阈值段不等于人工操作全周期，也不等于已稳定的平台。持续时间列为“末时刻−首时刻”，不是样本数。</p>{table(eventrows,['归类','起点','终点','端点时差 s','数字气压峰值 hPa'])}
 <h3>正式三次循环的四路高压与回落</h3>{table(cyclerows,['循环','传感器','阈值高压区间','高压段均值 hPa','高压段峰值 hPa','峰值时间','随后低值 hPa','低值时间'])}<p>低值搜索区间为当前高压段结束之后到下一正式高压段起点之前；第三次到用户标记的循环结束时刻之前。该区间也可能包含重新加压动作，表格提供观测极值，不解释为独立精度或回差试验。高压段边缘受各设备滤波时延影响，动态误差不能全部解释为零点误差。</p></section>
 <section id="linear"><h2>相关度、线性度与一致性</h2>{(table(response_rows,["模拟传感器","有效秒","ADC与数字均值r","ADC最小","ADC最大","≥99%满量程秒","占比%"])+"<p>99%满量程为诊断提示阈值，不是硬件饱和的直接证明。顶部压缩还可能来自传感器输出级，需结合电压和实际压力核实。</p>" if NEW else "")}<p>拟合定义：x为指定数字气压计，y为厂家公式换算的模拟压力，采用含截距普通最小二乘 y=a·x+b；r为Pearson相关系数，R²=1−Σ(y−ŷ)²/Σ(y−ȳ)²。平均差与直接RMSE使用y−x；拟合RMSE使用y−ŷ，不能混为准确度。全程拟合覆盖全部实验压力，量程内拟合仅保留指定数字气压计260～1260 hPa。</p>
 {table(fits,['模拟传感器','数字参照','范围','不同秒 n','r','R²','斜率 a','截距 hPa','平均差 hPa','直接RMSE hPa','拟合RMSE hPa'])}
 <p>线性度以斜率、R²与拟合残差描述。一次动态循环的残差还包含量化、滤波、压力变化和时延，不能代替厂家规定的静态非线性指标；未提供可重复稳定压力点和标准压力源，不能声称测得传感器固有非线性。时序自相关使有效独立样本数小于记录数，因此没有把逐秒观测用于夸大的显著性检验。</p>
 <h3>与数字均值的描述性一致性界限</h3>{table(ba,['模拟传感器','平均差 hPa','差值SD hPa','平均差−1.96SD','平均差＋1.96SD'])}
 <h3>误差与辅助量的探索性相关</h3>{table(assoc,['模拟传感器','辅助量','水平相关 r','相邻秒变化相关 r'])}<p>水平相关容易受时间趋势与压力操作共同影响；相邻秒差分仅作补充。这里不能证明电池电压、温湿度导致误差，更不能以电池电压代替REF/供电轨的直接测量。</p></section>
 <section id="quality"><h2>数据质量与全区间统计</h2>{table(stats,['项目','单位','不同秒 n','最小值','最大值','均值','标准差','峰峰值'])}
 <p>区间内记录 {len(d)} 条，不同秒 {len(s)} 个；同秒额外记录 {len(d)-len(s)} 条（不等同于重复错误）。相邻不同秒最大间隔 {dt.max():.0f} s。有效性检查：非有限值、-9999、ADC越界等共 {sum(invalid.values())} 个字段值被排除；不同通道分别保留有效值。ADC等于65535属于满量程码，不能凭此单独区分真实饱和和通信异常。</p>
 <p>来源：{html.escape(cfg['file'])}；厂家换算来自用户提供图片，型号 IPNZAA135KP-4428-A1。源CSV仅用于读取，未修改。SHA-256：<code>{hash_before}</code></p>
 <p>复现实验建议：在传感器规格允许范围内设置多个稳定压力点，测量实际REF与传感器供电；每个平台等待全部滤波稳定后再取均值，分别记录升压／降压并重复，使用覆盖目标压力范围的可溯源标准作准确度与回差评估。</p></section><footer>{DATE} · {reporttitle} · 仅使用指定实验时间内数据</footer>'''
 css='''*{box-sizing:border-box}body{margin:0;background:#f3f6fa;color:#243348;font:15px/1.75 "Microsoft YaHei",Arial,sans-serif}header,nav,main,footer{max-width:1380px;margin:auto}header{background:linear-gradient(120deg,#132f4c,#235b72);color:white;padding:44px 44px 34px;border-radius:0 0 22px 22px}h1{font-size:32px;margin:10px 0}h2{font-size:24px;color:#173f5e;margin:0 0 20px}h3{font-size:18px;margin:10px 0}.eyebrow{color:#badce7;letter-spacing:2px;font-size:12px}.metrics{display:grid;grid-template-columns:repeat(4,1fr);gap:16px;margin-top:26px}.metrics div{background:#ffffff13;padding:13px 16px;border-radius:9px}.metrics small{display:block;color:#c4dce5}.metrics b{font-size:18px}nav{padding:18px 24px;display:flex;gap:25px;flex-wrap:wrap}a{color:#18618d;text-decoration:none}section{max-width:1380px;margin:0 auto 24px;padding:30px 34px;background:white;border:1px solid #e2e8ef;border-radius:14px}.notice{background:#fff7e7;padding:16px;border-left:4px solid #dba541}.formula{background:#eff7f8;padding:16px 22px;font-size:18px;border-radius:8px;margin:18px 0}.chart-card{border:1px solid #dce5ee;border-radius:12px;margin:22px 0;padding:10px 16px;background:#fff}.chart-head{display:flex;align-items:center;justify-content:space-between;gap:10px}.chart-note{font-size:13px;color:#65758b;margin:3px 0;min-height:10px}.plot{height:420px;width:100%}.controls{position:sticky;top:0;z-index:10;background:#eef4f9;padding:12px;border-radius:8px;display:flex;gap:18px;align-items:center;flex-wrap:wrap}button,select{padding:6px 10px;border:1px solid #cbd8e5;background:white;color:#274d6c;border-radius:5px;cursor:pointer}.tablewrap{overflow-x:auto;margin:16px 0}table{border-collapse:collapse;width:100%;font-size:13px;font-variant-numeric:tabular-nums}th,td{border-bottom:1px solid #e2e8ef;text-align:right;padding:10px;white-space:nowrap}th{background:#edf3f8;color:#254c69}td:first-child,th:first-child{text-align:left}tr:nth-child(even){background:#f7f9fc}code{overflow-wrap:anywhere;font-size:12px}footer{padding:24px;color:#6b7e92}.chart-card:fullscreen{padding:25px;overflow:auto}.chart-card:fullscreen .plot{height:85vh}@media(max-width:750px){section{padding:16px}.metrics{grid-template-columns:repeat(2,1fr)}header{padding:24px}h1{font-size:24px}.chart-head{display:block}.plot{height:350px}}@media print{body{background:white}section{break-inside:avoid;padding:12px;border:0}.chart-card{break-inside:avoid}.controls,nav,button,select,.modebar{display:none!important}.plot{height:370px}header{background:#173f5e!important;print-color-adjust:exact}}'''
 script='''const specs=__SPECS__, events=__EVENTS__, tstart=__START__, tend=__END__;const graphs=[];let syncing=false;const timeIds=specs.filter(s=>s.time).map(s=>s.id);
 function changeMode(id,mode){let gd=document.getElementById(id);Plotly.restyle(gd,{mode:mode});}
 function full(id){const card=document.getElementById(id).closest('article');if(document.fullscreenElement){document.exitFullscreen()}else{card.requestFullscreen()}}
 document.addEventListener('fullscreenchange',()=>graphs.forEach(g=>Plotly.Plots.resize(g)));
 function resetAll(){syncing=true;Promise.all(timeIds.map(id=>Plotly.relayout(id,{'xaxis.range':[tstart,tend],'yaxis.autorange':true}))).finally(()=>syncing=false)}
 window.reportReady=Promise.all(specs.map(s=>{let xaxis=s.time?{type:'date',range:[tstart,tend],tickformat:'%H:%M:%S',title:'实验时间',showgrid:true,gridcolor:'#e7edf4'}:{title:'数字气压 / 平均压力 (hPa)',tickformat:'.1f',showgrid:true,gridcolor:'#e7edf4'};
 let shapes=[],annotations=[];if(s.time){events.forEach((e,i)=>{shapes.push({type:'line',xref:'x',yref:'paper',x0:e[0],x1:e[0],y0:0,y1:1,line:{color:'#90a3b5',width:1,dash:'dot'}});annotations.push({x:e[0],xref:'x',y:1.04+(i%2)*.07,yref:'paper',text:e[0].slice(11)+' '+e[1],showarrow:false,font:{size:10,color:'#627a91'},xanchor:i===events.length-1?'right':i===0?'left':'center'})});if(s.unit==='hPa' && !s.title.includes('误差') && !s.title.includes('差值') && (s.title.includes('数字气压计') || s.title.includes('四路压力')))shapes.push({type:'line',xref:'paper',yref:'y',x0:0,x1:1,y0:1260,y1:1260,line:{color:'#b7bec6',width:1,dash:'dash'}})}
 let layout={font:{family:'Microsoft YaHei,Arial',color:'#30485f'},margin:{l:82,r:30,t:s.time?92:35,b:75},paper_bgcolor:'white',plot_bgcolor:'white',xaxis,yaxis:{title:s.unit,tickformat:s.unit==='ADC码'?'.1f':s.unit==='V'?'.3f':'.2f',exponentformat:'none',showexponent:'none',gridcolor:'#e7edf4',zeroline:false,automargin:true},hovermode:s.time?'x unified':'closest',legend:{orientation:'h',y:-.22,x:0},shapes,annotations,dragmode:'zoom'};
 return Plotly.newPlot(s.id,s.traces,layout,{responsive:true,scrollZoom:true,displaylogo:false,toImageButtonOptions:{format:'png',filename:document.title+'_'+s.title,scale:2}}).then(g=>{graphs.push(g);if(s.time)g.on('plotly_relayout',ev=>{if(syncing||!document.getElementById('sync').checked)return;let update;if(ev['xaxis.range'])update={'xaxis.range':ev['xaxis.range']};else if(ev['xaxis.range[0]']!==undefined)update={'xaxis.range':[ev['xaxis.range[0]'],ev['xaxis.range[1]']]};else if(ev['xaxis.autorange'])update={'xaxis.range':[tstart,tend]};if(update){syncing=true;Promise.all(timeIds.filter(id=>id!==s.id).map(id=>Plotly.relayout(id,update))).finally(()=>syncing=false)}});return s.id})}));'''
 script=script.replace('__SPECS__',json.dumps(specs,ensure_ascii=False,allow_nan=False)).replace('__EVENTS__',json.dumps([[str(a),lab] for a,lab in events],ensure_ascii=False)).replace('__START__',json.dumps(str(stamp(cfg['start'])))).replace('__END__',json.dumps(str(stamp(cfg['end']))))
 doc='<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>'+reporttitle+'</title><style>'+css+'</style><script>'+JS+'</script></head><body>'+body+'<script>'+script+'</script></body></html>'
 dest=OUT/'2号板_通道2针脚更换后三次压力循环实验报告.html'; dest.write_text(doc,encoding='utf-8')
 print(json.dumps(dict(output=str(dest),rows=len(d),seconds=len(s),plots=len(specs),formal=meta['formal_high_segments'],metrics=nominal),ensure_ascii=False))
