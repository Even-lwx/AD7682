import base64, csv, html, json, re
from datetime import datetime
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from matplotlib.font_manager import FontProperties

ROOT=Path('D:/lwx/lwx-project/AD7682'); LOG=ROOT/'code/log'
OUT=ROOT/'output/reports/两板数据分析_20261023-1026'; OUT.mkdir(parents=True,exist_ok=True)
plt.rcParams.update({'font.family':FontProperties(fname='C:/Windows/Fonts/msyh.ttc').get_name(), 'axes.unicode_minus':False,'font.size':10,'figure.facecolor':'white','axes.spines.top':False,'axes.spines.right':False,'axes.grid':True,'grid.alpha':.2})
FILES=['1023-1026_新板子.csv','1023-1026_老板子.csv']; LABELS=['新板子','老板子']
COLS={'温度':0,'湿度':1,'U7气压':2,'U11气压':3,'IN0':4,'IN1':5,'IN2':6,'IN3':7}; UNITS=['℃','%RH','hPa','hPa','codes','codes','codes','codes']
boards=[]
for fn in FILES:
    with (LOG/fn).open(encoding='utf-8-sig',newline='') as f: rows=list(csv.reader(f))
    parsed=[]; bad=0
    for row in rows[1:]:
        if not row: continue
        try:
            stamp=datetime(*[int(float(x)) for x in row[:6]])
            vals=np.array([float(x) for x in row[6:]],dtype=float)
            if len(vals)!=8: raise ValueError('field count')
            parsed.append((stamp,vals))
        except Exception: bad+=1
    parsed.sort(key=lambda q:q[0]); stamps=np.array([x[0].timestamp() for x in parsed]); v=np.vstack([x[1] for x in parsed]); v[v==-9999]=np.nan
    boards.append({'name':fn,'time':[x[0] for x in parsed],'ts':stamps,'v':v,'bad':bad})
new,old=boards
# The new board's U7 pressure channel is the anomalous 9xx-hPa series.
# Keep the source CSV untouched, but suppress it from report plots and metrics.
new_u7_suppressed=int(np.isfinite(new['v'][:,2]).sum())
for b in boards: b['v_raw']=b['v'].copy()
new['v'][:,2]=np.nan
# Match monotonically, one-to-one, nearest timestamp.
matches=[]; j=0
for i,stamp in enumerate(new['ts']):
    while j+1<len(old['ts']) and abs(old['ts'][j+1]-stamp)<=abs(old['ts'][j]-stamp): j+=1
    if j<len(old['ts']) and (not matches or j>matches[-1][1]): matches.append((i,j))
idx=np.array(matches,dtype=int); ni,oi=idx[:,0],idx[:,1]; offset=old['ts'][oi]-new['ts'][ni]
aligned=[new['v'][ni],old['v'][oi]]; hours=(new['ts'][ni]-new['ts'][0])/3600
assert len(new['ts'])==3762 and len(old['ts'])==3755 and len(set(oi))==len(oi)

def fit(x,y):
    ok=np.isfinite(x)&np.isfinite(y); x=np.asarray(x)[ok]; y=np.asarray(y)[ok]
    if len(x)<3 or np.ptp(x)==0 or np.ptp(y)==0:return {'n':int(len(x)),'r':None,'r2':None,'slope':None,'rmse':None}
    slope,intercept=np.polyfit(x,y,1); err=y-(slope*x+intercept); r=np.corrcoef(x,y)[0,1]
    return {'n':int(len(x)),'r':float(r),'r2':float(r*r),'slope':float(slope),'intercept':float(intercept),'rmse':float(np.sqrt(np.mean(err*err)))}
def firstlast(x):
    a=x[np.isfinite(x)]; n=min(10,len(a)); return (float(np.median(a[:n])),float(np.median(a[-n:]))) if n else (np.nan,np.nan)
def lim(arrays,pad=.05):
    x=np.concatenate([np.asarray(a).ravel() for a in arrays]);x=x[np.isfinite(x)];d=float(np.ptp(x));return float(x.min()-max(.07*d,pad)),float(x.max()+max(.07*d,pad))
def axis_time(ax):
    ax.xaxis.set_major_locator(mdates.HourLocator(interval=6));ax.xaxis.set_major_formatter(mdates.DateFormatter('%m-%d\n%H:%M'));ax.xaxis.set_minor_locator(mdates.HourLocator());ax.grid(axis='x',which='minor',alpha=.12)
    for boundary in [datetime(2026,10,24),datetime(2026,10,25),datetime(2026,10,26)]:ax.axvline(boundary,color='#64748b',ls='--',lw=.8,alpha=.45)
    ax.tick_params(axis='x',labelbottom=True)
def display_name(text):
    for source,target in [('新旧板','1号与2号'),('老板子','1号'),('新板子','2号'),
                          ('老板','1号'),('新板','2号'),('U11','2号气压计'),('U7','1号气压计')]:
        text=text.replace(source,target)
    return text
def save(fig,name):
    # Normalize all figure titles, axis labels, legends and annotations together.
    for item in fig.findobj(match=matplotlib.text.Text):
        item.set_text(display_name(item.get_text()))
    fig.savefig(OUT/name,dpi=160,bbox_inches='tight');plt.close(fig)
def img(name):return '<img src="data:image/png;base64,'+base64.b64encode((OUT/name).read_bytes()).decode()+'">'
def num(x,d=3):return '—' if x is None or not np.isfinite(x) else f'{x:.{d}f}'
def table(headers,rows):return '<table><thead><tr>'+''.join('<th>'+html.escape(str(x))+'</th>' for x in headers)+'</tr></thead><tbody>'+''.join('<tr>'+''.join('<td>'+html.escape(str(x))+'</td>' for x in r)+'</tr>' for r in rows)+'</tbody></table>'

# Flag large one-minute steps with a robust per-signal threshold. The timestamp
# reported is the later sample; each event includes the two bracketing samples.
def detect_steps(b,c,floor):
    x=b['v_raw'][:,c]; d=np.diff(x); ok=np.isfinite(d)
    med=float(np.median(d[ok])) if ok.any() else 0.0
    mad=float(np.median(np.abs(d[ok]-med))) if ok.any() else 0.0
    threshold=max(floor,6*1.4826*mad)
    ids=np.flatnonzero(ok & (np.abs(d-med)>=threshold))
    return [{'i':int(i+1),'time':b['time'][i+1],'before':float(x[i]),'after':float(x[i+1]),'delta':float(d[i]),'threshold':threshold} for i in ids]
step_specs=[('温度',0,.5),('湿度',1,.5),('IN1',5,100),('IN3',7,100)]
events={}
for bi,b in enumerate(boards):
    events[LABELS[bi]]={name:detect_steps(b,c,floor) for name,c,floor in step_specs}

stats=[]; within=[]
for b in boards:
    cols={}
    for name,c in COLS.items():
        x=b['v'][:,c]; xx=x[np.isfinite(x)]; d=np.diff(x); d=d[np.isfinite(d)]; f,l=firstlast(x)
        cols[name]={'n':int(len(xx)),'invalid':int((~np.isfinite(x)).sum())}
        if len(xx):cols[name].update({'min':float(xx.min()),'max':float(xx.max()),'sd':float(xx.std()),'change10':l-f,'step_p95':float(np.percentile(abs(d),95)) if len(d) else np.nan})
    stats.append(cols); pairs={}
    for xn,yn in [('U7气压','U11气压'),('U7气压','IN1'),('U7气压','IN3'),('IN1','IN3')]:pairs[xn+' → '+yn]=fit(b['v'][:,COLS[xn]],b['v'][:,COLS[yn]])
    within.append(pairs)
cross={}
for name,c in COLS.items():
    x,y=aligned[0][:,c],aligned[1][:,c]; d=y-x; ok=np.isfinite(d); cross[name]={'all':fit(x,y)}
    if ok.any():cross[name].update({'pairs':int(ok.sum()),'difference_mean':float(d[ok].mean()),'difference_sd':float(d[ok].std())})

pressurelim=lim([b['v'][:,2:4] for b in boards]); templim=lim([b['v'][:,0] for b in boards]); rhlim=lim([b['v'][:,1] for b in boards]); zerolim=lim([b['v'][:,[4,6]] for b in boards],1); adclim=lim([b['v'][:,[5,7]] for b in boards],10)
for bi,b in enumerate(boards):
    fig,ax=plt.subplots(4,1,figsize=(14,12),sharex=True)
    ax[0].scatter(b['time'],b['v'][:,0],label='温度 (℃)',color='#c55237',s=7,alpha=.65,linewidths=0); r=ax[0].twinx();r.scatter(b['time'],b['v'][:,1],label='湿度 (%RH)',color='#3478a0',s=7,alpha=.60,linewidths=0)
    for name,c,_ in step_specs[:2]:
        for e in events[LABELS[bi]][name]:
            ax[0].axvline(e['time'],color='#6b7280',lw=.6,alpha=.22)
    ax[0].set_ylabel('温度 (℃)');r.set_ylabel('湿度 (%RH)');ax[0].set_ylim(*templim);r.set_ylim(*rhlim);ax[0].legend(loc='upper left');r.legend(loc='upper right')
    for c,n,col in [(2,'U7','#245e9b'),(3,'U11','#d38a20')]:ax[1].scatter(b['time'],b['v'][:,c],label=n,color=col,s=7,alpha=.65,linewidths=0)
    ax[1].set_ylabel('气压 (hPa)');ax[1].set_ylim(*pressurelim);ax[1].legend()
    for c,n,col in [(5,'IN1','#317b59'),(7,'IN3','#8445a2')]:ax[2].scatter(b['time'],b['v'][:,c]-np.nanmedian(b['v'][:10,c]),label=n+' 相对前10条中值',color=col,s=7,alpha=.65,linewidths=0)
    for name in ['IN1','IN3']:
        for e in events[LABELS[bi]][name]:ax[2].axvline(e['time'],color='#6b7280',lw=.6,alpha=.18)
    ax[2].set_ylabel('ADC变化 (codes)');ax[2].legend()
    for c,n,col in [(4,'IN0','#65758b'),(6,'IN2','#be5360')]:ax[3].scatter(b['time'],b['v'][:,c],label=n,color=col,s=7,alpha=.65,linewidths=0)
    ax[3].set_ylabel('其余ADC (codes)');ax[3].set_ylim(*zerolim);ax[3].legend()
    for x in ax:axis_time(x)
    ax[0].set_title(LABELS[bi]+'｜单板传感器及模拟通道趋势',loc='left',fontsize=14);fig.subplots_adjust(left=.10,right=.90,bottom=.07,top=.94,hspace=.36);save(fig,['01_新板趋势.png','02_老板趋势.png'][bi])

fig,axes=plt.subplots(8,1,figsize=(14,21),sharex=True); colors=['#245e9b','#db7d2a']
for ax,(name,c) in zip(axes,COLS.items()):
    for bi,b in enumerate(boards):ax.scatter(b['time'],b['v'][:,c],label=LABELS[bi],color=colors[bi],s=7,alpha=.6,linewidths=0)
    ax.set_ylabel(name+'\n('+UNITS[c]+')');ax.legend(loc='upper right')
    if c==2:ax.text(.02,.12,'新板U7存在900多hPa异常值，按要求不显示；原始CSV未修改',transform=ax.transAxes,color='#9c3328')
    ax.set_ylim(*(templim if c==0 else rhlim if c==1 else pressurelim if c in (2,3) else zerolim if c in (4,6) else adclim))
for ax in axes:axis_time(ax)
axes[0].set_title('两板对应通道｜共同时间范围，相同字段共享纵坐标',loc='left',fontsize=14);fig.subplots_adjust(left=.10,right=.90,bottom=.05,top=.96,hspace=.48);save(fig,'03_两板对应通道.png')

fig,ax=plt.subplots(figsize=(14,6))
for b,c,label,color in [(new,3,'新板 U11','#8e5aa7'),(old,2,'老板 U7','#df8125'),(old,3,'老板 U11','#245e9b')]:ax.scatter(b['time'],b['v'][:,c],label=label,color=color,s=9,alpha=.62,linewidths=0)
ax.set_ylim(*pressurelim);ax.set_ylabel('气压 (hPa)');ax.set_title('三个有效气压计｜同一时间轴、同一纵坐标',loc='left',fontsize=14);ax.legend(ncol=3);axis_time(ax);fig.subplots_adjust(left=.10,right=.95,bottom=.12,top=.90);save(fig,'04_三个气压计同图.png')

fig,axes=plt.subplots(2,2,figsize=(14,10))
for ax,(name,c) in zip(axes.ravel(),[('U7气压',2),('IN1',5),('IN3',7),('温度',0)]):
    x,y=aligned[0][:,c],aligned[1][:,c];ok=np.isfinite(x)&np.isfinite(y);x=x[ok];y=y[ok]
    if len(x):
        ax.scatter(x,y,c=hours[ok],cmap='viridis',s=7,alpha=.45);f=fit(x,y)
        if f['slope'] is not None:
            xp=np.array([x.min(),x.max()]);ax.plot(xp,f['slope']*xp+f['intercept'],color='crimson',lw=1);ax.set_title(f'{name}：r={f["r"]:.3f}，R²={f["r2"]:.3f}，n={len(x)}')
    ax.set_xlabel('新板 '+name);ax.set_ylabel('老板 '+name);ax.grid(alpha=.2)
fig.suptitle('两板对应值散点｜就近时间配对，颜色由早到晚',fontsize=14);fig.tight_layout();save(fig,'05_两板对应值.png')

with (OUT/'两板时间就近匹配.csv').open('w',encoding='utf-8-sig',newline='') as f:
    w=csv.writer(f);w.writerow(['新板时间','老板匹配时间','时间差(秒)']+[f'新板_{n}' for n in COLS]+[f'老板_{n}' for n in COLS])
    for i,j in matches:w.writerow([new['time'][i].isoformat(sep=' '),old['time'][j].isoformat(sep=' '),round(old['ts'][j]-new['ts'][i],3),*new['v_raw'][i].tolist(),*old['v_raw'][j].tolist()])
metrics={'sources':FILES,'rows':[len(b['ts']) for b in boards],'bad_rows':[b['bad'] for b in boards],'start':[b['time'][0].isoformat(sep=' ') for b in boards],'end':[b['time'][-1].isoformat(sep=' ') for b in boards],'matched':len(matches),'offset_sec':{'min':float(offset.min()),'median':float(np.median(offset)),'max':float(offset.max()),'p95_abs':float(np.percentile(abs(offset),95))},'stats':stats,'within':within,'cross':cross}
(OUT/'分析统计.json').write_text(json.dumps(metrics,ensure_ascii=False,indent=2),encoding='utf-8')
parts=['<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>两板数据分析 2026-10-23至10-26</title><style>body{font:16px/1.7 Microsoft YaHei,sans-serif;color:#213047;max-width:1240px;margin:30px auto;padding:0 24px}h1{font-size:29px}h2{margin-top:42px;border-bottom:2px solid #dce5ef;padding-bottom:8px}.note{padding:14px 18px;background:#eef5fa;border-left:4px solid #38719c}table{border-collapse:collapse;width:100%;font-size:13px;margin:16px 0}th,td{border:1px solid #dce5ef;padding:7px;text-align:right}th:first-child,td:first-child{text-align:left}th{background:#edf2f7}img{width:100%;height:auto;margin:10px 0}li{margin:4px 0}@media print{body{max-width:none}h2{break-before:page}img,table{break-inside:avoid}}</style><h1>两块板 10月23日至26日数据分析</h1><p>两板传感器趋势与同类通道比较，统一时间坐标。时间采用记录的本地时间（Asia/Shanghai）。</p>']
parts.append('<div class="note"><b>对齐与数据边界：</b>'+f'新板 {len(new["ts"])} 条、老板 {len(old["ts"])} 条；按时间单调就近匹配 {len(matches)} 对。老板匹配时间相对新板差值最小/中位/最大为 {offset.min():.0f}/{np.median(offset):.0f}/{offset.max():.0f} 秒，绝对差P95为 {np.percentile(abs(offset),95):.0f} 秒。图中曲线仍按各板实际记录时刻绘制。CSV为14列：前6列时间，后8列按既有格式解释为温湿度、U7/U11压力、IN0～IN3。未把所记录ADC字段称为单次转换原始码。新板U7的900多hPa异常数据仅从本报告展示和统计中排除（'+str(new_u7_suppressed)+'条）；原始CSV保持不变。两份CSV均未发现−9999。</div>')
parts.append('<h2>1. 单板趋势</h2><p>每板按实际时间绘制；IN1/IN3变化图以本板前10条记录中值为基准，便于看趋势细节。</p>')
for i,label in enumerate(LABELS):
    rows=[]
    for name,c in COLS.items():
        s=stats[i][name];rows.append([name+' ('+UNITS[c]+')',s['n'],s['invalid'],num(s.get('min')),num(s.get('max')),num(s.get('sd')),num(s.get('change10')),num(s.get('step_p95'))])
    parts.append('<h3>'+label+'</h3>'+table(['字段','有效条数','无效条数','最小','最大','全程标准差','前后10条中值差','相邻变化绝对值P95'],rows)+img(['01_新板趋势.png','02_老板趋势.png'][i]))
parts.append('<h2>2. 两板对应通道</h2><p>相同字段使用共同纵坐标，曲线各自保留实际记录时间。虚线为跨日零点。</p>'+img('03_两板对应通道.png'))
parts.append('<h2>突变事件时间明细</h2><p>表中时刻为突变后的那条记录时间，事件用相邻样本差识别：温度/湿度变化绝对值至少0.5℃/%RH；IN1/IN3变化至少100 codes，且超过各自相邻差分中位数的6倍稳健离散度。时间精度约1分钟，不能代表精确物理发生时刻或串口发送时刻。单次采样可能是突变前后状态的首次观测。</p>')
for bi,label in enumerate(LABELS):
    evrows=[]
    for name,_,_ in step_specs:
        for e in events[label][name]:
            evrows.append([e['time'].strftime('%Y-%m-%d %H:%M:%S'),name,num(e['before']),num(e['after']),num(e['delta'])])
    evrows.sort(key=lambda r:r[0])
    parts.append('<h3>'+label+'：温湿度及IN1/IN3突变</h3>'+table(['记录时间（突变后样本）','字段','前一条','后一条','变化量'],evrows))
parts.append('<h2>3. 三个气压计同图</h2><p>按你的要求，不显示新板U7的900多hPa异常数据；图中展示新板U11与老板U7、U11三条正常气压曲线，共用同一时间轴和纵坐标。原始CSV未修改。</p>'+img('04_三个气压计同图.png'))
fnew=fit(new['v'][:,2],new['v'][:,3]); fold=fit(aligned[1][:,2],aligned[0][:,2])
parts.append(f'<p>新板U7/U11有效重叠 {fnew["n"]} 点，r={num(fnew["r"])}，R²={num(fnew["r2"])}，RMSE={num(fnew["rmse"])} hPa。新旧板U7就近配对 {fold["n"]} 点，r={num(fold["r"])}，RMSE={num(fold["rmse"])} hPa。此为记录一致性，不代表相对标准源的绝对准确度。</p>')
parts.append('<h2>4. 两板对应值散点</h2>'+img('05_两板对应值.png'))
rows=[]
for n,s in cross.items():rows.append([n,s.get('pairs',0),num(s['all']['r']),num(s['all']['r2']),num(s['all']['rmse']),num(s.get('difference_mean')),num(s.get('difference_sd'))])
parts.append(table(['字段','有效匹配数','Pearson r','R²','拟合RMSE','老板−新板平均差','差值标准差'],rows))
parts.append('<h2>5. 单板线性关系</h2>'); rows=[]
for bi,label in enumerate(LABELS):
    for pair,s in within[bi].items():rows.append([label,pair,s['n'],num(s['r']),num(s['r2']),num(s['slope']),num(s['rmse'])])
parts.append(table(['板子','X → Y','有效数','r','R²','斜率 Y/X','RMSE'],rows))
parts.append('<p>相关性只描述本次时段和环境变化，不等于校准线性度或因果。两板没有共同标准加载/校准信息，ADC通道的斜率与偏差不能用于判断绝对准确性。两板异步记录的就近匹配不适合比较快速瞬态。</p><h2>6. 来源与限制</h2><ul><li>CSV每分钟级记录约一条；没有100Hz单次转换值和毫秒时间戳，不能用本报告分析ADC周期尖峰。</li><li>全程标准差混合了真实变化、慢漂移和噪声，不是本底噪声。</li><li>新板U7异常序列不参与报告统计与对比，仅从展示副本排除；原始数据仍保存在CSV中。</li><li>两板传感器、接线、供电及加载条件未独立确认。</li></ul><p>匹配明细和统计JSON随HTML放在同一输出目录。原始CSV未修改。</p></html>')
report='\n'.join(parts)
# Replace visible text only, preserving image base64 data and source filenames.
report=re.sub(r'>([^<>]+)<',lambda m:'>'+display_name(m.group(1))+'<',report)
report=report.replace('<h2>1. 单板趋势</h2>',
    '<p class="note"><b>名称对应：</b>1号对应原“老板子”，2号对应原“新板子”；1号气压计对应U7，2号气压计对应U11。</p><h2>1. 单板趋势</h2>',1)
(OUT/'两板数据分析.html').write_text(report,encoding='utf-8')
print(json.dumps({'output':str(OUT/'两板数据分析.html'),'rows':[len(b['ts']) for b in boards],'matched':len(matches),'offset':[float(offset.min()),float(np.median(offset)),float(offset.max()),float(np.percentile(abs(offset),95))],'invalid':[{n:s[n]['invalid'] for n in s} for s in stats],'new_u7_u11':fnew,'old_new_u7':fold},ensure_ascii=False,indent=2))
