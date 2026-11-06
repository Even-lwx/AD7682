import base64
import csv
import html
import json
from datetime import datetime
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from matplotlib.font_manager import FontProperties

ROOT = Path(r'D:/lwx/lwx-project/AD7682')
OUT = ROOT / 'output/reports/两板过夜数据分析_20261023'
OUT.mkdir(parents=True, exist_ok=True)
font = FontProperties(fname=r'C:/Windows/Fonts/msyh.ttc')
plt.rcParams.update({'font.family': font.get_name(), 'axes.unicode_minus': False,
                     'font.size': 10, 'figure.facecolor': 'white', 'axes.spines.top': False,
                     'axes.spines.right': False, 'axes.grid': True, 'grid.alpha': .2})
NAMES = ['周四到周五-下午用新传感器的板子 - 副本.csv', '周四到周五-老板子 - 副本.csv']
LABELS = ['新传感器板', '老板子']
COLS = {'温度': 0, '湿度': 1, 'U7气压': 2, 'U11气压': 3,
        'IN0': 4, 'IN1': 5, 'IN2': 6, 'IN3': 7}
UNITS = ['℃', '%RH', 'hPa', 'hPa', 'codes', 'codes', 'codes', 'codes']
boards = []
for name in NAMES:
    with (ROOT/'code/log'/name).open(encoding='utf-8-sig', newline='') as f:
        source = list(csv.reader(f))[1:]
    rows = sorted((datetime(*[int(float(x)) for x in row[:6]]), row) for row in source if row)
    ts = np.array([r[0].timestamp() for r in rows])
    values = np.array([[float(x) for x in r[1][6:]] for r in rows])
    values[values == -9999] = np.nan
    boards.append({'time': [r[0] for r in rows], 'ts': ts, 'v': values})

new, old = boards
indices = np.array([np.argmin(np.abs(old['ts'] - stamp)) for stamp in new['ts']])
assert len(set(indices)) == len(indices)
assert np.max(np.abs(old['ts'][indices]-new['ts'])) <= 30
aligned = [new['v'], old['v'][indices]]
t = new['time']
hours = (new['ts'] - new['ts'][0])/3600
# At least one hour after each board's first logged record. Not a claim that
# thermal equilibrium has been reached; this is an explicit sensitivity check.
late = (new['ts'] >= max(b['ts'][0] for b in boards)+3600)

def fit(x, y):
    ok = np.isfinite(x) & np.isfinite(y)
    x, y = np.asarray(x)[ok], np.asarray(y)[ok]
    if len(x) < 3 or np.ptp(x) == 0 or np.ptp(y) == 0:
        return {'n': int(len(x)), 'r': None, 'r2': None, 'slope': None, 'rmse': None}
    slope, intercept = np.polyfit(x, y, 1)
    err = y-(slope*x+intercept)
    r = np.corrcoef(x, y)[0, 1]
    return {'n': int(len(x)), 'r': float(r), 'r2': float(r*r),
            'slope': float(slope), 'intercept': float(intercept),
            'rmse': float(np.sqrt(np.mean(err*err)))}

def partial(x, y, z):
    ok = np.isfinite(x) & np.isfinite(y) & np.all(np.isfinite(z), axis=1)
    x,y,z=x[ok],y[ok],z[ok]
    if len(x)<5: return None
    z = np.column_stack([np.ones(len(z)),z])
    rx = x-z@np.linalg.lstsq(z,x,rcond=None)[0]
    ry = y-z@np.linalg.lstsq(z,y,rcond=None)[0]
    return float(np.corrcoef(rx,ry)[0,1])

stats = []
for b, label in zip(boards, LABELS):
    bh = (b['ts']-b['ts'][0])/3600
    cols = {}
    for name,c in COLS.items():
        x=b['v'][:,c]; ok=np.isfinite(x); xv=x[ok]
        step=np.diff(x); validstep=np.isfinite(step)
        cols[name]={'n': int(ok.sum()), 'invalid': int((~ok).sum())}
        if len(xv):
            cols[name].update({'min':float(xv.min()),'max':float(xv.max()),
                'mean':float(xv.mean()), 'sd':float(xv.std()),'range':float(np.ptp(xv)),
                'first10median':float(np.nanmedian(x[:10])),
                'last10median':float(np.nanmedian(x[-10:])),
                'change10':float(np.nanmedian(x[-10:])-np.nanmedian(x[:10])),
                'step_p95':float(np.percentile(np.abs(step[validstep]),95)),
                'step_max':float(np.max(np.abs(step[validstep]))),
                'timefit':fit(bh,x)})
    stats.append(cols)

within=[]
for a in aligned:
    pairs={}
    for xname,yname in [('U7气压','IN1'),('U7气压','IN3'),('U11气压','IN1'),('U11气压','IN3'),('U7气压','U11气压'),('IN1','IN3')]:
        x,y=a[:,COLS[xname]],a[:,COLS[yname]]
        pairs[xname+' → '+yname]={'all':fit(x,y),'after1h':fit(x[late],y[late]),
             'minute_difference':fit(np.diff(x),np.diff(y)),
             'partial_temperature_humidity':partial(x,y,a[:,[0,1]])}
    within.append(pairs)
cross={}
for name in ['温度','湿度','U7气压','U11气压','IN0','IN1','IN2','IN3']:
    c=COLS[name]; x,y=aligned[0][:,c],aligned[1][:,c]
    d=y-x; valid=np.isfinite(d)
    cross[name]={'all':fit(x,y),'after1h':fit(x[late],y[late]),
                 'minute_difference':fit(np.diff(x),np.diff(y))}
    if valid.any(): cross[name].update({'difference_mean':float(d[valid].mean()),
        'difference_sd':float(d[valid].std()),'difference_min':float(d[valid].min()),'difference_max':float(d[valid].max())})

metrics={'sources':NAMES,'stats':stats,'within':within,'cross':cross,
         'matched':len(indices),'time_offset_s': [float(np.min(old['ts'][indices]-new['ts'])),float(np.max(old['ts'][indices]-new['ts']))],
         'late_count':int(late.sum())}
segments=[]
for label,start,end in [('18:00～21:00','2026-10-22 18:00','2026-10-22 21:00'),
                        ('21:00～次日03:00','2026-10-22 21:00','2026-10-23 03:00'),
                        ('次日03:00～09:00','2026-10-23 03:00','2026-10-23 09:01')]:
    mask=np.array([datetime.fromisoformat(start)<=stamp<datetime.fromisoformat(end) for stamp in t])
    for bi,a in enumerate(aligned):
        for c in [5,7]:
            x=a[mask,c];p=a[mask,2]
            segments.append({'period':label,'board':LABELS[bi], 'channel':f'IN{c-4}',
                'n':int(mask.sum()),'r_pressure':fit(p,x)['r'],
                'change10':float(np.median(x[-10:])-np.median(x[:10])),
                'step_p95':float(np.percentile(np.abs(np.diff(x)),95))})
metrics['segments']=segments
(OUT/'分析统计.json').write_text(json.dumps(metrics,ensure_ascii=False,indent=2),encoding='utf-8')

def save(fig, name):
    fig.savefig(OUT/name,dpi=160)
    plt.close(fig)

TIME_LIMITS = (datetime(2026,10,22,17,45), datetime(2026,10,23,9,15))
TEMPERATURE_START = datetime(2026,10,22,18,40)

def displayed_temperature(b):
    values=b['v'][:,0].copy()
    values[np.array([stamp<TEMPERATURE_START for stamp in b['time']])]=np.nan
    return values

def displayed_humidity(b):
    values=b['v'][:,1].copy()
    values[np.array([stamp<TEMPERATURE_START for stamp in b['time']])]=np.nan
    return values

def limits(*arrays):
    x = np.concatenate([np.asarray(a).ravel() for a in arrays])
    x = x[np.isfinite(x)]
    lo,hi = x.min(),x.max()
    pad = max((hi-lo)*.07, .05)
    return lo-pad,hi+pad

def timeaxis(ax):
    ax.set_xlim(*TIME_LIMITS)
    ax.xaxis.set_major_locator(mdates.HourLocator(byhour=[0,3,6,9,12,15,18,21]))
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%m-%d\n%H:%M'))
    ax.xaxis.set_minor_locator(mdates.HourLocator())
    ax.grid(axis='x',which='minor',alpha=.12)
    for boundary in [datetime(2026,10,22,21),datetime(2026,10,23,3)]:
        ax.axvline(boundary,color='#64748b',ls='--',lw=.8,alpha=.5)

def finish_time(fig, axes):
    for ax in np.asarray(axes).ravel():
        timeaxis(ax)
        ax.tick_params(axis='x',labelbottom=True)
    # Fixed canvas width and margins keep a given timestamp at the exact same
    # horizontal position, including between separately embedded report images.
    fig.subplots_adjust(left=.11,right=.89,bottom=.065,top=.94,hspace=.50)

pressure_limits = limits(*(b['v'][:,2:4] for b in boards))
temperature_limits = limits(*(displayed_temperature(b) for b in boards))
humidity_limits = limits(*(displayed_humidity(b) for b in boards))
delta_limits = limits(*(b['v'][:,c]-np.nanmedian(b['v'][:10,c])
                        for b in boards for c in [5,7]))
zero_limits = limits(*(b['v'][:,[4,6]] for b in boards))

for b,label,filename in zip(boards,LABELS,['01_新板趋势.png','02_老板趋势.png']):
    fig,axes=plt.subplots(4,1,figsize=(14,12),sharex=True)
    axes[0].plot(b['time'],displayed_temperature(b),label='温度 (℃，18:40起)',color='#c55237')
    rh=axes[0].twinx(); rh.plot(b['time'],displayed_humidity(b),label='湿度 (%RH，18:40起)',color='#3478a0',alpha=.7)
    axes[0].set_ylabel('温度 (℃)'); rh.set_ylabel('湿度 (%RH)')
    axes[0].set_ylim(*temperature_limits)
    rh.set_ylim(*humidity_limits)
    axes[0].legend(loc='upper left'); rh.legend(loc='upper right')
    for c,color in [(2,'#245e9b'),(3,'#d38a20')]:
        if np.isfinite(b['v'][:,c]).any(): axes[1].plot(b['time'],b['v'][:,c],label=['U7','U11'][c-2],color=color)
    if not np.isfinite(b['v'][:,3]).any(): axes[1].text(.03,.15,'U11 全部为 -9999，未作为气压绘制',transform=axes[1].transAxes,color='#ac2929')
    axes[1].set_ylabel('气压 (hPa)');axes[1].legend()
    axes[1].set_ylim(*pressure_limits)
    for c,color in [(5,'#317b59'),(7,'#8445a2')]:
        axes[2].plot(b['time'],b['v'][:,c]-np.nanmedian(b['v'][:10,c]),label=['IN1','IN3'][(c-5)//2]+' 相对前10点中值',color=color)
    axes[2].set_ylabel('ADC变化量 (codes)');axes[2].legend()
    axes[2].set_ylim(*delta_limits)
    for c in [4,6]: axes[3].plot(b['time'],b['v'][:,c],label=['IN0','IN2'][(c-4)//2],alpha=.8)
    axes[3].set_ylabel('低电平通道 (codes)');axes[3].legend()
    axes[3].set_ylim(*zero_limits)
    axes[0].set_title(f'{label}｜各路记录随时间变化（原始日志中的存储值）',loc='left',fontsize=15)
    finish_time(fig,axes);save(fig,filename)

fig,axes=plt.subplots(8,1,figsize=(14,21),sharex=True)
for ax,(name,c) in zip(axes,COLS.items()):
    for b,label,color in zip(boards,LABELS,['#245e9b','#db7d2a']):
        if np.isfinite(b['v'][:,c]).any():
            values=displayed_temperature(b) if c==0 else displayed_humidity(b) if c==1 else b['v'][:,c]
            ax.plot(b['time'],values,label=label,color=color,lw=1.1)
    ax.set_ylabel(name+'\n('+UNITS[c]+')')
    ax.set_ylim(*limits(*(b['v'][:,c] for b in boards)))
    if c in (0,1):
        ax.set_ylim(*(temperature_limits if c==0 else humidity_limits))
        ax.text(.015,.08,('温度' if c==0 else '湿度')+'仅显示18:40之后，排除初始过渡段',transform=ax.transAxes,color='#64748b')
    ax.legend(loc='upper right')
    if c==3:
        ax.text(.02,.15,'老板 U11 全部无效，仅绘制新板',transform=ax.transAxes,color='#ac2929')
axes[0].set_title('两板全部通道｜统一时间轴，相同通道共用纵轴，保留各板实际记录时刻',loc='left',fontsize=14)
finish_time(fig,axes);save(fig,'03_两板对应通道.png')

fig,axes=plt.subplots(3,1,figsize=(10,14))
for row,c in enumerate([2,5,7]):
    ax=axes[row]
    x,y=aligned[0][:,c],aligned[1][:,c]
    ax.scatter(x,y,c=hours,cmap='viridis',s=8,alpha=.55)
    f=fit(x,y); xp=np.array([x.min(),x.max()]);ax.plot(xp,f['slope']*xp+f['intercept'],color='crimson',lw=1)
    ax.set_xlabel('新板 '+['U7 (hPa)','IN1 (codes)','IN3 (codes)'][row]);ax.set_ylabel('老板对应读数')
    ax.set_title(f"r={f['r']:.3f}，R²={f['r2']:.3f}；相邻变化 r={cross[['U7气压','IN1','IN3'][row]]['minute_difference']['r']:.3f}")
fig.suptitle('两板线性拟合｜横轴为新板读数，颜色由早到晚',fontsize=14)
fig.tight_layout();save(fig,'07_两板线性拟合.png')

fig,axes=plt.subplots(2,3,figsize=(15,9))
for board in range(2):
    a=aligned[board]
    for col,(xc,yc) in enumerate([(2,5),(2,7),(5,7)]):
        ax=axes[board,col];x,y=a[:,xc],a[:,yc]
        ax.scatter(x,y,c=hours,cmap='viridis',s=8,alpha=.5)
        f=fit(x,y); xp=np.array([np.nanmin(x),np.nanmax(x)])
        ax.plot(xp,f['slope']*xp+f['intercept'],color='crimson',lw=1)
        ax.set_xlabel(['U7 (hPa)','U7 (hPa)','IN1 (codes)'][col]);ax.set_ylabel(['IN1 (codes)','IN3 (codes)','IN3 (codes)'][col])
        ax.set_title(f"{LABELS[board]}：r={f['r']:.3f}，R²={f['r2']:.3f}")
for col,(xc,yc) in enumerate([(2,5),(2,7),(5,7)]):
    for ax in axes[:,col]:
        ax.set_xlim(*limits(*(a[:,xc] for a in aligned)))
        ax.set_ylim(*limits(*(a[:,yc] for a in aligned)))
        ax.set_xticks(axes[0,col].get_xticks())
        ax.set_xlim(*limits(*(a[:,xc] for a in aligned)))
fig.suptitle('单板线性关系｜同一颜色代表相近记录时间；成环或分段意味着单条直线解释不足',fontsize=14)
fig.tight_layout();save(fig,'04_单板线性关系.png')

fig,axes=plt.subplots(3,1,figsize=(14,10),sharex=True)
axes[0].plot(t,aligned[0][:,3]-aligned[0][:,2],color='#6846a3')
axes[0].set_ylabel('新板 U11−U7\n(hPa)')
axes[1].plot(t,aligned[1][:,2]-aligned[0][:,2],color='#245e9b')
axes[1].set_ylabel('老板U7−新板U7\n(hPa)')
for c,label in [(5,'IN1'),(7,'IN3')]:
    d=aligned[1][:,c]-aligned[0][:,c]
    axes[2].plot(t,d-np.median(d[:10]),label=label+' 板间差值相对前10点')
axes[2].set_ylabel('板间差值变化\n(codes)');axes[2].legend()
axes[0].set_title('差值稳定性｜固定偏置与随时间变化的偏差分开观察',loc='left',fontsize=15)
finish_time(fig,axes);save(fig,'05_差值稳定性.png')

fig,axes=plt.subplots(1,2,figsize=(13,5))
for j,(ax,(x,y,title)) in enumerate(zip(axes,[(aligned[0][:,2],aligned[0][:,3],'新板 U7 与 U11'),
                              (aligned[0][:,2],aligned[1][:,2],'新板 U7 与老板 U7')])):
    f=fit(x,y)
    ax.scatter(x,y,c=hours,cmap='viridis',s=10,alpha=.5)
    xp=np.array([x.min(),x.max()]);ax.plot(xp,f['slope']*xp+f['intercept'],color='crimson')
    ax.set_xlabel('新板 U7 (hPa)');ax.set_ylabel('U11 (hPa)' if j==0 else '老板 U7 (hPa)')
    ax.set_title(f"{title}\nr={f['r']:.4f}，R²={f['r2']:.4f}，RMSE={f['rmse']:.3f} hPa")
for ax in axes:
    ax.set_xlim(*pressure_limits);ax.set_ylim(*pressure_limits)
    ax.set_aspect('equal',adjustable='box')
fig.suptitle('气压记录的线性一致性｜仅覆盖本次约4 hPa变化范围',fontsize=14)
fig.tight_layout();save(fig,'06_气压计线性对比.png')

fig,axes=plt.subplots(3,1,figsize=(14,11),sharex=True,sharey=True)
for ax,(bi,c,label,color) in zip(axes,[(0,2,'新板 U7','#245e9b'),
                                      (0,3,'新板 U11','#8b54a3'),
                                      (1,2,'老板 U7','#df8125')]):
    b=boards[bi]
    ax.plot(b['time'],b['v'][:,c],color=color,lw=1.3,label=label)
    ax.set_ylabel('气压 (hPa)')
    ax.set_ylim(*pressure_limits)
    ax.legend(loc='upper right')
axes[0].set_title('三个气压计对照｜共享时间轴和气压刻度',loc='left',fontsize=15)
finish_time(fig,axes);save(fig,'08_三个气压计同图.png')

def num(x,d=3): return '不可评估' if x is None else f'{x:.{d}f}'
def table(headers,rows):
    return '<table><thead><tr>'+''.join('<th>'+html.escape(str(x))+'</th>' for x in headers)+'</tr></thead><tbody>'+''.join('<tr>'+''.join('<td>'+html.escape(str(x))+'</td>' for x in r)+'</tr>' for r in rows)+'</tbody></table>'
def img(name):
    return '<img src="data:image/png;base64,'+base64.b64encode((OUT/name).read_bytes()).decode()+'">'

parts=['''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>两块板过夜数据分析</title>
<style>body{font:16px/1.75 'Microsoft YaHei',sans-serif;color:#213047;max-width:1200px;margin:32px auto;padding:0 24px}h1{font-size:30px}h2{margin-top:44px;border-bottom:2px solid #dce5ef;padding-bottom:8px}h3{margin-top:28px}p{max-width:1050px}.note{padding:16px 20px;background:#eef5fa;border-left:4px solid #38719c}table{border-collapse:collapse;width:100%;font-size:14px;margin:16px 0}th,td{border:1px solid #dce5ef;padding:9px;text-align:right}th:first-child,td:first-child{text-align:left}th{background:#edf2f7}img{width:100%;height:auto;margin:10px 0}code{background:#edf2f7;padding:2px 5px}a{color:#245e9b}@media print{body{max-width:none}h2{break-before:page}img,table{break-inside:avoid}}</style>
<h1>两块板过夜记录：趋势、线性关系与一致性</h1>
<p>记录日期：2026-10-22 至 2026-10-23。报告使用两份原始 CSV，未修改源数据。</p>
<div class="note"><b>评估边界：</b>这里的“线性”指两路记录之间能否用直线描述，采用 Pearson r、线性回归 R² 与拟合残差 RMSE。没有标准压力/标准电压和已知加载过程，不能据此给出校准线性度、满量程非线性误差或绝对精度。ADC 维持 codes，不推测其物理量。</div>''']
parts.append('<h2>Word 中的曲线对照</h2><p>以下11张图按原Word顺序重画。所有图的横坐标范围、时间刻度和绘图区左右位置完全一致；单独展示的曲线使用各自合适的纵轴范围，便于看清波动。<b>这些单图的纵轴刻度不同，比较波动幅度时请读取实际数值。</b>叠加图中的各曲线仍共用同一纵轴。数据没有平移、重新滤波或裁掉极值。</p>')
for heading,filenames in [
    ('温度', ['01_两板温度','02_新板温度','03_老板温度']),
    ('湿度', ['04_两板湿度','05_新板湿度','06_老板湿度']),
    ('气压计', ['07_气压计对比']),
    ('模拟通道 IN1 与 IN3', ['08_新板通道2','09_新板通道4','10_老板通道2','11_老板通道4'])
]:
    parts.append('<h3>'+heading+'</h3>')
    if heading=='温度':
        parts.append('<p>老板起始高温来自刚焊接后的余热；起始湿度也有明显上电过渡。两板温度和湿度曲线均从10月22日18:40开始显示，并按该时刻之后的数据重新设置纵轴，便于观察后续变化。统一时间轴仍保留原范围，18:40前曲线留空。原始CSV和下文整晚统计、回归未改动。</p>')
    for filename in filenames:
        parts.append(img('Word对应曲线/'+filename+'.png'))
    if heading=='气压计':
        parts.append(img('08_三个气压计同图.png'))
        parts.append('<details><summary>展开三个气压计的时间对齐数据表（908组，单位hPa）</summary><p>以新板记录时刻为基准，老板按最近时间一对一匹配，时间差11～29秒；表内同时保留老板实际记录时间。无插值。</p>')
        parts.append(table(['新板时间','新板 U7 (hPa)','新板 U11 (hPa)','老板 U7 (hPa)','老板对应时间'],
                           [[stamp.strftime('%m-%d %H:%M:%S'),num(aligned[0][i,2],1),
                             num(aligned[0][i,3],1),num(aligned[1][i,2],1),
                             old['time'][indices[i]].strftime('%m-%d %H:%M:%S')]
                            for i,stamp in enumerate(t)]))
        parts.append('</details>')
parts.append('''<h2>主要发现</h2><ol>
<li><b>可用的三个气压计长期趋势一致。</b>新板两气压计r=0.9995，差值仅在−0.1～+0.1 hPa；两板U7之间r=0.9983。但老板U7相对新板的偏差从早段约−0.3扩大到末段约−0.7 hPa，不能仅用一个固定偏置解释全程。</li>
<li><b>四路传感器ADC表现不同。</b>新板IN1总体下降，IN3前半程上升、后半程近平台；老板IN1和IN3总体下降。不能将四路的变化视为同一种统一ADC误差。</li>
<li><b>“与气压相关”不等于稳定线性响应。</b>新板IN1和老板IN3全程与U7相关较高，但18～21时的相关符号与21～次日03时不同。存在共同慢趋势、早段松弛或未记录输入条件的影响，不能直接从整晚拟合得到标定系数。</li>
<li><b>若比较分钟记录的平稳程度，老板两路的相邻变化较小。</b>IN1/IN3的相邻变化绝对值P95分别约46/48 codes，新板约73/122 codes；这不等于证明老板传感器精度更高，因为实际输入和灵敏度未知。</li>
<li><b>老板U11全程失败。</b>912条均为−9999，不能计算其趋势、线性或一致性。</li></ol>''')
parts.append('<h2>1. 数据范围与方法</h2>')
parts.append('<p><b>统一坐标：</b>所有趋势图采用相同的10月22日17:45至23日09:15时间范围、相同刻度及绘图区左右边界；每小时细网格，21:00与次日03:00虚线对应分时段分析。两张单板趋势图中同类数据的纵轴范围也一致。两板总览按各板实际记录时间绘制；散点拟合仍使用最近时间一对一配对。同类散点图统一坐标范围，不同单位各自保留物理含义。</p>')
parts.append(table(['项目','新传感器板','老板子'],[
 ['原始行数',len(new['ts']),len(old['ts'])],['开始',new['time'][0],old['time'][0]],['结束',new['time'][-1],old['time'][-1]],
 ['U7有效行数',stats[0]['U7气压']['n'],stats[1]['U7气压']['n']],['U11有效行数',stats[0]['U11气压']['n'],stats[1]['U11气压']['n']]]))
parts.append(f'<p>单板统计保留全部记录。板间按实际时间最近、一对一匹配，共 {len(indices)} 对，绝对时间差 11～29 秒；老板子额外4条不参与板间统计。未插值、未补零、未移动传感器波形。两板 RTC 是否存在真实时钟偏差无法从日志独立验证。</p>')
parts.append('<p>字段映射：I6温度、I7湿度、I8/U7气压、I9/U11气压，I10～I13分别是IN0～IN3。-9999仅作为无效标记排除；其他值不删点。当前固件表明记录的是ADC滤波值、气压滑动平均值，因此这些分钟记录不能用于重新判定原始短尖峰是否存在。Flash气压以0.1 hPa量化；可见的0.1 hPa台阶不必然是实际气压跳变。</p>')
parts.append('<p>“前后变化”使用最前10条与最后10条的中值之差；“去首小时”是敏感性检查，不表示已确认设备完全热稳定。图表中的逐分钟变化指相邻记录差分，日志以约一分钟为主，少数间隔更长，未对间隔进行归一化。差分排除了一部分共同慢趋势影响；去温湿度相关性仅去除线性温湿度分量，不能证明因果。散点颜色由紫色（早）逐渐变为黄色（晚）。</p>')
for bi,(label,st) in enumerate(zip(LABELS,stats)):
    parts.append(f'<h2>{bi+2}. {label}：单板分析</h2>')
    rows=[]
    for name,c in COLS.items():
        s=st[name]
        rows.append([name+' ('+UNITS[c]+')',s['n'],num(s.get('min')),num(s.get('max')),num(s.get('sd')),num(s.get('change10')),num(s.get('step_p95'))])
    parts.append(table(['通道','有效条数','最小','最大','全程标准差','前后变化','相邻记录变化绝对值P95'],rows))
    parts.append('<p>全程标准差包含真实变化、慢漂移及噪声，不能当作ADC本底噪声或重复性。IN0/IN2虽为低电平读数，但本次接线未独立确认，不将其直接视为精密零点标准。</p>')
    parts.append(img(['01_新板趋势.png','02_老板趋势.png'][bi]))
    if bi == 0:
        parts.append('''<p><b>趋势解读：</b>温度整体降低、湿度升高；两气压计先升至约1020.4 hPa，再共同回落并在后半夜小幅波动。IN1前后10点中值下降约1048 codes；IN3上升约1559 codes，变化主要发生在前半程，之后接近平台。IN1与IN3方向相反，且IN3的弯曲/平台形态明显，不支持两路同一条固定线性关系。</p>
<p><b>线性解读：</b>两气压计互相拟合R²=0.9990、RMSE=0.048 hPa，说明本次窄范围内一致性好；IN1与U7全程R²=0.897，IN3与U7只有0.363。温湿度线性去趋势后，IN3与气压的相关性从负变正，提示变量间混杂严重，不能将原始相关性当作器件特性。</p>''')
    else:
        parts.append('''<p><b>趋势解读：</b>起始记录有明显温湿度过渡，温度从34.56℃快速下降；单板原始日志的前几条变化大于新板。U7先升后降，与新板气压走势接近；IN1前后10点中值下降约280 codes，IN3下降约652 codes，IN1在后半夜比IN3更接近平稳。IN0/IN2维持23～24 codes。</p>
<p><b>线性解读：</b>IN3与U7全程R²=0.912、去首小时r=0.975，长期形态较相似；IN1与U7全程R²=0.620。去除线性温湿度分量后，IN1与U7相关性降到0.237，说明IN1全程的正相关有明显混杂因素。U11无有效数据，未将−9999画成实际气压。</p>''')
    pairs=within[bi];rows=[]
    for name,s in pairs.items():
        rows.append([name,num(s['all']['r']),num(s['all']['r2']),num(s['all']['slope']),num(s['all']['rmse']),num(s['after1h']['r']),num(s['minute_difference']['r'])])
    parts.append(table(['X → Y','r','R²','斜率Y/X','RMSE (Y单位)','去首小时r','相邻变化r'],rows))
parts.append('<h2>4. 单板散点图：是否存在稳定线性关系</h2>'+img('04_单板线性关系.png'))
parts.append('<p>散点按记录时间着色。即使总体r很大，如果颜色随时间形成分段、回环或出现平行带，也可能主要由时间/温度趋势造成。每分钟相邻变化相关性比水平相关性弱时，不能仅凭高R²将ADC当成气压计的替代读数。</p>')
parts.append('<h3>分时段验证：ADC与气压的相关性是否保持同一方向</h3>')
parts.append(table(['时段','板子','ADC通道','记录数','与本板U7的r','ADC前后变化 (codes)','相邻变化P95 (codes)'],
                   [[s['period'],s['board'],s['channel'],s['n'],num(s['r_pressure']),num(s['change10'],1),num(s['step_p95'],1)] for s in segments]))
parts.append('<p><b>18～21时气压在升高，四路ADC中的三路在降低；21～次日03时气压降低，新板IN1及老板两路也降低，新板IN3则继续升高。</b>多个关系随时段改变符号，这是比“整晚R²高”更重要的线索。此处时段按时钟预先分块用于对照，未选取最大相关的滞后。</p>')
parts.append('<h2>5. 两板对应传感器对比</h2>')
rows=[]
for name,s in cross.items():
    rows.append([name,num(s['all']['r']),num(s['all']['r2']),num(s['after1h']['r']),num(s['minute_difference']['r']),num(s.get('difference_mean')),num(s.get('difference_sd'))])
parts.append(table(['通道','r','R²','去首小时r','相邻变化r','老板−新板平均差','板间差标准差'],rows))
parts.append('<p>IN1对IN1、IN3对IN3按“相同通道号”比较。未收到四个模拟传感器的具体编号、标定或两板共同加载说明，不能断言这些通道的物理灵敏度应相同。不同通道的绝对ADC偏置不等于测量误差。老板U11全程无效，不能比较它与新板U11的趋势或线性。</p>')
parts.append(img('03_两板对应通道.png'))
parts.append(img('07_两板线性拟合.png'))
parts.append('<p><b>板间趋势结论：</b>U7吻合最好；IN1总体同向但变化幅度差异明显，板间差标准差约313 codes；IN3总体反向，板间差标准差约573 codes。两路ADC板间的相邻变化相关都接近0，未观察到稳定同步的分钟级波动。这不能反推毫秒尖峰是否同步，也不能在未知输入条件下判定哪块板更准确。</p>')
parts.append(img('06_气压计线性对比.png'))
parts.append('<h2>6. 差值和稳定性</h2>'+img('05_差值稳定性.png'))
npdiff=aligned[0][:,3]-aligned[0][:,2]
parts.append(f'<p>新板U11−U7：平均 {npdiff.mean():.3f} hPa，标准差 {npdiff.std():.3f} hPa，范围 {npdiff.min():.3f}～{npdiff.max():.3f} hPa。不同传感器间的偏置可能来自校准、温度或实际局部条件，不能在没有标准源时分配到某一颗器件。</p>')
parts.append('<h2>7. 温湿度与ADC：相关不等于温漂系数</h2>')
rows=[]
for bi,a in enumerate(aligned):
    for name in ['IN1','IN3']:
        y=a[:,COLS[name]];s=fit(a[:,0],y)
        pair=within[bi]['U7气压 → '+name]
        rows.append([LABELS[bi]+' '+name,num(s['r']),num(s['slope']),num(pair['all']['r']),num(pair['partial_temperature_humidity'])])
parts.append(table(['通道','与温度r','温度单变量斜率 codes/℃','与U7的原始r','去线性温湿度后的U7相关r'],rows))
parts.append('<p>这只是观测回归，不是传感器温漂标定。温度、湿度、气压与时间在过夜记录中可能共同变化，需要控温/定载实验才能确定温度系数。温度单变量斜率不能直接拿去做补偿。</p>')
parts.append('<h2>8. 来源与限制</h2><ul>'+''.join('<li>'+html.escape(str(ROOT/'code/log'/name))+'</li>' for name in NAMES)+'</ul><p>统计方法可复算；配套JSON保存全部系数。此报告不修改采样代码、不重新滤波原始CSV。分钟级滤波记录的结论仅覆盖慢变化，不能估计此前100 Hz原始数据的尖峰频率。数据自相关、共同环境趋势和未知加载条件使显著性检验不可靠，因此不报告独立样本p值。</p></html>')
(OUT/'两块板过夜数据分析.html').write_text('\n'.join(parts),encoding='utf-8')
print(json.dumps(metrics,ensure_ascii=False,indent=2))
