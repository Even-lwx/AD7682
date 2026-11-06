import csv, json, base64, html, hashlib
from pathlib import Path
from datetime import datetime, timedelta
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.dates as md
from matplotlib.font_manager import FontProperties

ROOT=Path('D:/lwx/lwx-project/AD7682')
OUT=ROOT/'output/reports/两板2S供电数据分析_20261029-30'
OUT.mkdir(parents=True,exist_ok=True)
NAMES=['温度','湿度','1号气压计','2号气压计','IN0','IN1','IN2','IN3','电池电压']
UNITS=['℃','%RH','hPa','hPa','codes','codes','codes','codes','V']
plt.rcParams.update({'font.family':FontProperties(fname='C:/Windows/Fonts/msyh.ttc').get_name(),'axes.unicode_minus':False,'font.size':10,'axes.grid':True,'grid.alpha':.2})
B=[]
for n in [1,2]:
 p=ROOT/'code/log'/f'1029-30-2s供电{n}号板子.csv'
 rows=list(csv.reader(p.open(encoding='utf-8-sig')));parsed=[]
 for row in rows[1:]:
  if not row:continue
  assert len(row)==15
  parsed.append((datetime(*[int(float(x)) for x in row[:6]]),list(map(float,row[6:]))))
 parsed.sort(key=lambda x:x[0]); t=[r[0] for r in parsed];v=np.array([r[1] for r in parsed]);assert np.isfinite(v).all()
 ts=np.array([(x-datetime(1970,1,1)).total_seconds() for x in t]);assert (np.diff(ts)>0).all()
 B.append(dict(n=n,file=p.name,t=t,ts=ts,v=v,hash=hashlib.sha256(p.read_bytes()).hexdigest()))
START=min(b['t'][0] for b in B);END=max(b['t'][-1] for b in B)
# Fixed, visible startup exclusion: 15 minutes per board, never rewrite sources.
for b in B:b['stable']=np.array([t>=b['t'][0]+timedelta(minutes=15) for t in b['t']])
lo=max(b['ts'][np.flatnonzero(b['stable'])[0]] for b in B);hi=min(b['ts'][-1] for b in B)
pairs=[];used=set()
for i,s in enumerate(B[0]['ts']):
 if not lo<=s<=hi:continue
 j=int(np.searchsorted(B[1]['ts'],s));cand=[k for k in [j-1,j] if 0<=k<len(B[1]['ts']) and k not in used]
 if not cand:continue
 j=min(cand,key=lambda k:abs(B[1]['ts'][k]-s))
 if abs(B[1]['ts'][j]-s)<=16 and B[1]['stable'][j]:pairs.append((i,j));used.add(j)
P=np.array(pairs);a=B[0]['v'][P[:,0]];b2=B[1]['v'][P[:,1]]

def fmt(x):
 if x is None or not np.isfinite(x):return '不适用'
 # Ordinary decimal notation; retain small correlations/slopes without exponents.
 text=f'{x:.8f}'.rstrip('0').rstrip('.')
 return '0' if text in ('-0','') else text
def table(headers,rows):return '<div class="scroll"><table><thead><tr>'+''.join('<th>'+html.escape(str(s))+'</th>' for s in headers)+'</tr></thead><tbody>'+''.join('<tr>'+''.join('<td>'+html.escape(str(s))+'</td>' for s in r)+'</tr>' for r in rows)+'</tbody></table></div>'
def fit(x,y):
 if len(x)<4 or np.ptp(x)<1e-9 or np.ptp(y)<1e-9:return dict(r=None,r2=None,slope=None,rmse=None)
 slope,inter=np.polyfit(x,y,1);r=float(np.corrcoef(x,y)[0,1]);return dict(r=r,r2=r*r,slope=float(slope),rmse=float(np.sqrt(np.mean((y-slope*x-inter)**2))))
def rolling(x,w=31):return np.array([np.median(x[max(0,i-w//2):min(len(x),i+w//2+1)]) for i in range(len(x))])
def style(ax):
 ax.set_xlim(START,END);ax.xaxis.set_major_locator(md.HourLocator(interval=2));ax.xaxis.set_major_formatter(md.DateFormatter('%m-%d\n%H:%M'));ax.tick_params(labelbottom=True)
 ax.axvline(datetime(2026,10,30),ls='--',c='#777',lw=.7)
def save(fig,name):
 fig.tight_layout();fig.savefig(OUT/name,dpi=140,bbox_inches='tight');plt.close(fig)
 return '<img loading="lazy" src="data:image/png;base64,'+base64.b64encode((OUT/name).read_bytes()).decode()+'">'
def scatter(ax,x,y,label=None):ax.scatter(x,y,s=5,alpha=.7,linewidths=0,label=label)

parts=['''<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>两板2S供电数据分析｜10月29日至30日</title><style>
body{max-width:1450px;margin:30px auto;padding:0 22px;font:15px/1.7 "Microsoft YaHei",sans-serif;color:#24334b}h1{font-size:30px}h2{margin-top:42px;border-bottom:2px solid #dbe5ef;padding-bottom:8px}h3{margin-top:26px}.note{background:#edf5fc;border-left:4px solid #3975ad;padding:16px;margin:18px 0}.scroll{overflow:auto;max-height:520px}table{border-collapse:collapse;width:100%;font-size:13px}td,th{border:1px solid #dbe3eb;padding:8px;text-align:right;white-space:nowrap}td:first-child,th:first-child{text-align:left}th{background:#edf2f8;position:sticky;top:0}img{width:100%;height:auto}summary{cursor:pointer;font-weight:bold}canvas{width:100%;height:230px;display:block}button,input{padding:6px;margin:5px}.panel{border:1px solid #dbe3eb;margin:14px 0;padding:12px}.hint{color:#576982;font-size:13px}</style></head><body>
<h1>两板 2S 供电数据分析</h1><p>2026年10月29日至30日 · 实际记录时间 · 散点展示 · 共同时间轴</p>''']
parts.append('<div class="note">名称与字段：1号、2号分别对应文件名中的板子编号；1号气压计对应 U7，2号气压计对应 U11。I0～I5 为年月日时分秒，I6/I7 为温湿度，I8/I9 为气压，I10～I13 为 IN0～IN3，I14 为电池电压。ADC 字段是日志保存的结果，不将其误称为未经滤波的单次原始转换值。所有源文件保持原样。</div>')
info=[]
for b in B:
 dt=np.diff(b['ts']);info.append([f'{b["n"]}号',len(b['v']),str(b['t'][0]),str(b['t'][-1]),fmt((b['ts'][-1]-b['ts'][0])/3600),f'{int(dt.min())}～{int(dt.max())}',int((dt>45).sum()),int((b['v']==-9999).sum())])
parts.append(table(['板子','记录数','开始','结束','时长/h','间隔/s','超过45秒的间隔','−9999数量'],info))
parts.append('<p>两板均无重复时间、缺字段、−9999 或 65535；实际间隔为30～31秒。2号板结束比1号板早约80分钟，尾部不补零、不外推。稳定段默认剔除每板开头15分钟（分析约定，并非自动认定焊接影响已经结束）；全量数据可在交互图及启动明细中查看。温湿度图使用稳定段自适应纵轴，不让启动变化压缩细节。</p>')
parts.append('<div class="note"><b>本次主要发现：</b><ul><li>两板温湿度和气压的慢趋势接近：温度相关r约0.979、湿度约0.958、对应气压约0.991～0.992，但两板基值不同。</li><li>1号板IN1仍反复出现约220码的正向跳变，且有负向跳变；2号板IN1于10月30日07:05:13上升约176.1码，随后维持较高水平。这些不是相同的变化模式。</li><li>稳定段1号板电池约8.29→7.84V；2号板约8.29→6.14V，末段下降更快。1号板IN1的大阶跃没有对应的同间隔大电压跳变。</li><li>本次仅覆盖一夜，未重现足以确认“每天两次温湿度突变”的多日样本；启动和结尾变化不能直接归为周期故障。</li></ul></div>')

stats=[]
for b in B:
 v=b['v'][b['stable']];rr=[]
 for c in range(9):
  x=v[:,c];d=np.diff(x);s=dict(board=b['n'],field=NAMES[c],min=float(x.min()),max=float(x.max()),mean=float(x.mean()),sd=float(x.std()),change=float(np.median(x[-10:])-np.median(x[:10])),step95=float(np.percentile(abs(d),95)))
  stats.append(s);rr.append([NAMES[c],UNITS[c],*[fmt(s[k]) for k in ['min','max','mean','sd','change','step95']]])
 parts.append(f'<h2>{b["n"]}号板：单板趋势与统计</h2>'+table(['字段','单位','最小','最大','均值','全程标准差','前后10条中值差','相邻变化绝对值P95'],rr))
 fig,ax=plt.subplots(7,1,figsize=(15,17),sharex=True);m=b['stable'];t=np.array(b['t'])[m];v=b['v'][m]
 for c,r in [(8,0),(0,1),(1,2)]:scatter(ax[r],t,v[:,c]);ax[r].set_ylabel(NAMES[c]+' / '+UNITS[c])
 for c in [2,3]:scatter(ax[3],t,v[:,c],NAMES[c])
 ax[3].legend(ncol=2);ax[3].set_ylabel('压力 / hPa')
 for c,r in [(4,4),(5,5),(6,6)]:scatter(ax[r],t,v[:,c],NAMES[c]);ax[r].set_ylabel(NAMES[c]+' / codes')
 # IN3 gets its own plot below; never stretch other channel scales to fit IN2.
 for aa in ax:style(aa)
 fig.suptitle(f'{b["n"]}号板稳定段：独立纵轴、共同时间轴',y=1.002)
 parts.append(save(fig,f'{b["n"]}号板趋势.png'))

parts.append('<h2>两板对应字段对比</h2><p>每个字段上下两图共用时间轴、各自纵轴，避免两板基值不同掩盖细节。IN3也单独展示，2号板约8码的结果保留，不据此认定该通道接有有效传感器。</p>')
for c in range(9):
 fig,ax=plt.subplots(2,1,figsize=(15,5.4),sharex=True)
 for bi,b in enumerate(B):
  m=b['stable'];scatter(ax[bi],np.array(b['t'])[m],b['v'][m,c]);ax[bi].set_ylabel(f'{bi+1}号 / {UNITS[c]}');style(ax[bi])
 fig.suptitle(NAMES[c]+'：两板对照',y=1.01);parts.append(save(fig,f'对比_{c}.png'))

fig,ax=plt.subplots(1,1,figsize=(15,5))
for b in B:
 for c in [2,3]:scatter(ax,np.array(b['t'])[b['stable']],b['v'][b['stable'],c],f'{b["n"]}号 · {NAMES[c]}')
style(ax);ax.set_ylabel('hPa');ax.legend(ncol=4);ax.set_title('四个气压计同一坐标体系：本次四路均有有效压力')
parts.append('<h2>四个气压计同图</h2>'+save(fig,'四气压计.png'))

cross=[]
for c in range(9):
 f=fit(a[:,c],b2[:,c]);cross.append(dict(field=NAMES[c],**f));
parts.append('<h2>对应传感器趋势与线性关系</h2>')
offset=B[1]['ts'][P[:,1]]-B[0]['ts'][P[:,0]]
parts.append(f'<p>稳定重叠区间单调一对一就近匹配 {len(P)} 对，容差16秒；实际时间差绝对值最大 {max(abs(offset)):.0f} 秒。未将不同时间点强行视为同时测量。下表Y为2号、X为1号；“线性关系”指本段记录拟合的一致性，不是标定线性度。</p>')
parts.append(table(['字段','配对数','r','R²','斜率 Y/X','拟合RMSE'],[[NAMES[c],len(P),*[fmt(cross[c][k]) for k in ['r','r2','slope','rmse']]] for c in range(9)]))
rr=[]
for b in B:
 v=b['v'][b['stable']]
 for x,y in [(2,3),(4,2),(5,2),(6,2),(7,2),(0,1),(8,4),(8,5),(8,6),(8,7)]:
  f=fit(v[:,x],v[:,y]);fd=fit(np.diff(v[:,x]),np.diff(v[:,y]));rr.append([f'{b["n"]}号',NAMES[x]+' → '+NAMES[y],fmt(f['r']),fmt(f['r2']),fmt(f['slope']),fmt(fd['r'])])
parts.append('<h3>单板字段关系：原值与一阶差分</h3><p>原值相关可能只是共同慢漂移；相邻差分相关更接近同步变化，但仍不能证明因果。</p>'+table(['板子','X → Y','原值r','原值R²','拟合斜率','差分r'],rr))
fig,axs=plt.subplots(2,3,figsize=(15,9))
for ax,c in zip(axs.ravel(),[0,2,4,5,6,8]):
 ax.scatter(a[:,c],b2[:,c],c=np.arange(len(P)),s=5,cmap='viridis',alpha=.6);ax.set_xlabel('1号 '+NAMES[c]);ax.set_ylabel('2号 '+NAMES[c]);ax.set_title('r = '+fmt(cross[c]['r']))
parts.append(save(fig,'配对散点.png'))

# Event thresholds are fixed and exposed. Do not call every natural drift a fault.
threshold=[.2,.5,.3,.3,50,100,100,15,.05]
events=[]
for b in B:
 d=np.diff(b['v'],axis=0)
 for c,th in enumerate(threshold):
  for i in np.flatnonzero(abs(d[:,c])>=th)+1:
   events.append(dict(board=b['n'],field=NAMES[c],c=c,i=int(i),time=str(b['t'][i]),before=float(b['v'][i-1,c]),after=float(b['v'][i,c]),delta=float(d[i-1,c]),dv=float(d[i-1,8]),startup=not bool(b['stable'][i])))
events.sort(key=lambda e:(e['time'],e['board'],e['c']))
parts.append('<h2>突变的具体时间、幅度与持续时间</h2><p>事件由相邻两条记录的变化筛选：温度≥0.2℃、湿度≥0.5%RH、气压≥0.3hPa、IN0≥50码、IN1/IN2≥100码、IN3≥15码、电池≥0.05V。这里只称“候选突变”，不把真实环境变化直接判为异常。表中时间为突变后首次记录，实际起点在前一条和该条之间（约30～31秒）。</p>')
for bi in [1,2]:
 ev=[e for e in events if e['board']==bi and not e['startup']]
 parts.append(f'<h3>{bi}号板稳定段事件：{len(ev)}项字段变化</h3><details><summary>展开全部时间明细</summary>'+table(['时间','字段','前值','后值','变化量','同一间隔电池变化/V'],[[e['time'],e['field'],*[fmt(e[k]) for k in ['before','after','delta','dv']]] for e in ev])+'</details>')
 # top events: min separation 5 min to avoid repeating same transition.
 selected=[]
 for e in sorted(ev,key=lambda e:abs(e['delta'])/threshold[e['c']],reverse=True):
  if all(e['c']!=q['c'] or abs((datetime.fromisoformat(e['time'])-datetime.fromisoformat(q['time'])).total_seconds())>300 for q in selected):selected.append(e)
  if len(selected)>=12:break
 parts.append(table(['主要事件时间','字段','变化','同步电压变化/V'],[[e['time'],e['field'],fmt(e['delta']),fmt(e['dv'])] for e in sorted(selected,key=lambda e:e['time'])]))

# 1-board IN1 large rise/fall pairing. Duration is between observed edges only.
b=B[0];d=np.diff(b['v'][:,5]);up=np.flatnonzero((d>=150)&b['stable'][1:])+1;down=np.flatnonzero((d<=-150)&b['stable'][1:])+1
dur=[]
for i in up:
 nxt=down[down>i];u=up[up>i]
 if len(nxt) and (not len(u) or nxt[0]<u[0]):
  j=int(nxt[0]);dur.append([str(b['t'][i]),str(b['t'][j]),fmt((b['ts'][j]-b['ts'][i])/60),fmt(d[i-1]),fmt(d[j-1]),fmt(b['v'][j,8]-b['v'][i,8])])
ints=np.diff(b['ts'][up])/60
parts.append('<h3>1号板 IN1 反复阶跃</h3>'+f'<p>稳定段向上≥150码 {len(up)} 次、向下≥150码 {len(down)} 次；可配对上升—下降 {len(dur)} 段。相邻大上升间隔中位数 {fmt(np.median(ints)) if len(ints) else "不适用"} 分钟，范围 {fmt(ints.min()) if len(ints) else "不适用"}～{fmt(ints.max()) if len(ints) else "不适用"} 分钟。持续时间为两个观测边沿的间距，单段约有±31秒不确定性，不能从这些慢速日志恢复ADC内部尖峰宽度。</p>'+table(['上升首次记录','下降首次记录','高位持续/min','上升/codes','下降/codes','高位起止电压差/V'],dur))

# Robust detrended ACF provides recurrence evidence, never a precise noise-source ID.
period=[]
fig,axs=plt.subplots(2,1,figsize=(15,7),sharex=True)
for bi,b in enumerate(B):
 x=b['v'][b['stable'],5];res=x-rolling(x,121);res-=res.mean();ac=np.correlate(res,res,'full')[len(res)-1:];ac/=ac[0] if ac[0] else 1
 dt=np.median(np.diff(b['ts']));lag=np.arange(len(ac))*dt/60;maxlag=min(240,len(ac)-2);peaks=[i for i in range(4,maxlag) if ac[i]>ac[i-1] and ac[i]>=ac[i+1]]
 picks=sorted(peaks,key=lambda i:ac[i],reverse=True)[:3];period.append([f'{bi+1}号',[(round(float(lag[i]),2),round(float(ac[i]),3)) for i in picks]])
 axs[bi].plot(lag[:maxlag],ac[:maxlag]);axs[bi].set_ylabel(f'{bi+1}号 IN1 自相关');axs[bi].axhline(0,color='gray',lw=.7)
axs[-1].set_xlabel('滞后 / 分钟')
parts.append('<h2>周期性变化与电池电压的关联</h2><p>以下对 IN1 减去居中121条中值趋势（约60.5分钟）后计算自相关。只展示2～120分钟内的局部峰；慢速、异步、已处理日志只能支持“记录值有重复性”，不能据此锁定某个数字时钟、开关电源频率或程序中断。</p>'+save(fig,'IN1自相关.png')+table(['板子','较强局部峰：滞后min / 自相关'],period))
ev1=[e for e in events if e['board']==1 and e['c']==5 and not e['startup']]
ev2=[e for e in events if e['board']==2 and e['c']==5 and not e['startup']]
parts.append(f'<p>1号板 IN1 ≥100码事件 {len(ev1)} 次，同一采样间隔电池变化绝对值最大 {fmt(max([abs(e["dv"]) for e in ev1],default=0))} V；2号板对应 {len(ev2)} 次、最大 {fmt(max([abs(e["dv"]) for e in ev2],default=0))} V。不能把1号板反复约200码阶跃简单解释成日志里可见的电池同步跳变。电压只保留约0.01V且经过滤波，仍无法排除更快或更小的供电/参考扰动。</p>')
parts.append('<h3>启动阶段与结束阶段需单独看</h3><details><summary>启动15分钟内全部候选事件</summary>'+table(['板子','时间','字段','前值','后值','变化量'],[[e['board'],e['time'],e['field'],fmt(e['before']),fmt(e['after']),fmt(e['delta'])] for e in events if e['startup']])+'</details>')
b=B[1];x=b['v'][:,8];i=int(np.argmax(x));parts.append(f'<p>2号板电池记录首条 {x[0]:.2f} V，第二条 {x[1]:.2f} V；首条同时存在多路ADC大变化，应作为启动结果单独排查。全程最高 {x[i]:.2f} V，末条 {x[-1]:.2f} V；末段电压下降是否代表电池接近耗尽、供电切换或测量变化，需要实测确认，不能仅凭本表判定。1号板结束后温湿度最后一条也有变化，末尾没有后续样本，无法判断持续多久。</p>')

# Small standalone canvas explorer. Same range is applied to every chart.
parts.append('''<h2>全量数据交互查看</h2><p>全部记录均保留；默认显示15分钟启动段之后。修改起止时间会同步所有图的横坐标；纵坐标分别自适应，悬停可查看每板最近的实际记录。这里不修改、插值或补齐数据。</p>
<label>开始 <input id="start" type="datetime-local" step="1"></label><label>结束 <input id="end" type="datetime-local" step="1"></label><button onclick="drawAll()">应用时间范围</button><button onclick="resetRange(true)">完整时段（含启动）</button><button onclick="resetRange(false)">稳定时段</button><div id="charts"></div>''')
data=[dict(name=f'{b["n"]}号',t=[str(t).replace(' ','T') for t in b['t']],v=b['v'].tolist()) for b in B]
parts.append('<script>const data='+json.dumps(data,ensure_ascii=False)+',names='+json.dumps(NAMES,ensure_ascii=False)+',units='+json.dumps(UNITS,ensure_ascii=False)+';const stableStart="'+(datetime(1970,1,1)+timedelta(seconds=lo)).isoformat()+'";</script>')
parts.append('''<script>
const colors=['#2166ac','#d6604d'];data.forEach(b=>b.ms=b.t.map(x=>new Date(x).getTime()));
for(let c=0;c<9;c++){let d=document.createElement('div');d.className='panel';d.innerHTML='<b>'+names[c]+' / '+units[c]+'</b><canvas id="cv'+c+'"></canvas><div class="hint" id="hint'+c+'">蓝色：1号；红色：2号。悬停查看数据。</div>';document.getElementById('charts').appendChild(d);d.querySelector('canvas').addEventListener('mousemove',e=>hover(c,e))}
function resetRange(full){document.getElementById('start').value=full?data[1].t[0]:stableStart;document.getElementById('end').value=data[0].t.at(-1);drawAll()}
function drawAll(){let l=new Date(document.getElementById('start').value).getTime(),r=new Date(document.getElementById('end').value).getTime();if(!(r>l))return;for(let c=0;c<9;c++){let cv=document.getElementById('cv'+c),w=cv.clientWidth,h=230,ratio=devicePixelRatio||1;cv.width=w*ratio;cv.height=h*ratio;let ctx=cv.getContext('2d');ctx.scale(ratio,ratio);let vals=data.flatMap(b=>b.v.filter((v,i)=>b.ms[i]>=l&&b.ms[i]<=r).map(v=>v[c]));if(!vals.length)continue;let mn=Math.min(...vals),mx=Math.max(...vals),pad=Math.max((mx-mn)*.08,.01);mn-=pad;mx+=pad;let x=t=>75+(t-l)/(r-l)*(w-100),y=v=>195-(v-mn)/(mx-mn)*170;cv.range=[l,r];ctx.font='11px Microsoft YaHei';for(let i=0;i<=4;i++){let yy=25+i*42.5;ctx.strokeStyle='#e2e8ef';ctx.beginPath();ctx.moveTo(75,yy);ctx.lineTo(w-25,yy);ctx.stroke();ctx.fillStyle='#53647c';ctx.fillText((mx-i*(mx-mn)/4).toFixed(c>=4&&c<=7?1:2),2,yy+4);let tt=l+i*(r-l)/4;ctx.fillText(new Date(tt).toLocaleString('zh-CN',{month:'2-digit',day:'2-digit',hour:'2-digit',minute:'2-digit'}),Math.min(w-115,Math.max(75,x(tt)-40)),220)}data.forEach((b,k)=>{ctx.fillStyle=colors[k];b.v.forEach((v,i)=>{if(b.ms[i]>=l&&b.ms[i]<=r){ctx.beginPath();ctx.arc(x(b.ms[i]),y(v[c]),1.6,0,7);ctx.fill()}})})}}
function hover(c,e){let cv=e.target;if(!cv.range)return;let [l,r]=cv.range,w=cv.clientWidth,t=l+(e.offsetX-75)/(w-100)*(r-l);let text=data.map(b=>{let best=0;for(let i=1;i<b.ms.length;i++)if(Math.abs(b.ms[i]-t)<Math.abs(b.ms[best]-t))best=i;return b.name+' '+b.t[best].replace('T',' ')+'：'+b.v[best][c]+' '+units[c]});document.getElementById('hint'+c).textContent=text.join(' ｜ ')}
window.addEventListener('resize',drawAll);resetRange(false);
</script>''')
parts.append('<h2>结论与边界</h2><ul><li>1号板IN1仍有反复阶跃；2号板IN1主要变化模式不同，不能称为两板完全同频同幅噪声。</li><li>本次四个气压计正常保留；压力的共同趋势与固定偏差可由同图和拟合表比较。</li><li>本次记录不能支持“每天两次”温湿度异常的重复规律：仅覆盖一夜，启动段、结束段及具体候选事件已独立列出。</li><li>IN0～IN3接线、传感器种类和负载未在日志中提供，不把不同基值直接判为故障。尤其2号板IN3约8码，需结合实际接线解释。</li><li>标准差包含真实变化与漂移，不是本底噪声；慢速日志无法确定ADC尖峰的真实频率，也不能替代已知标准输入下的校准线性度测试。</li><li>电池与环境/ADC的相关不等于因果；没有充电状态、VDDA、AD7682参考电压的同步记录，不能单独锁定干扰源。</li></ul>')
parts.append('<h2>来源与可复核结果</h2>'+table(['板子','源文件','SHA256'],[[f'{b["n"]}号',b['file'],b['hash']] for b in B])+'<p>时间按原CSV本地时间解释；没有手工平移时间。配对表、事件表、统计JSON及独立PNG与报告保存在同一目录。</p></body></html>')
(OUT/'两板2S供电数据分析.html').write_text('\n'.join(parts),encoding='utf-8')
with (OUT/'就近时间匹配.csv').open('w',encoding='utf-8-sig',newline='') as f:
 w=csv.writer(f);w.writerow(['1号时间','2号时间','2号减1号时间差/s']+[f'{n}号_{s}' for n in [1,2] for s in NAMES])
 for i,j in P:w.writerow([str(B[0]['t'][i]),str(B[1]['t'][j]),B[1]['ts'][j]-B[0]['ts'][i],*B[0]['v'][i],*B[1]['v'][j]])
with (OUT/'候选突变明细.csv').open('w',encoding='utf-8-sig',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(events[0]));w.writeheader();w.writerows(events)
summary=dict(rows=[len(b['v']) for b in B],pairs=len(P),max_offset=float(max(abs(offset))),stats=stats,cross=cross,period=period,large_in1_rises=len(up),large_in1_falls=len(down),high_duration=dur,events=len(events))
(OUT/'统计.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in summary.items() if k not in ['stats','high_duration']},ensure_ascii=False,indent=2))
print(OUT/'两板2S供电数据分析.html')
