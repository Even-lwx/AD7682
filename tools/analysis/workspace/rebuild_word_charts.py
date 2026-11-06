"""Regenerate the 11 figures in the supplied Word, from untouched source CSVs."""
import csv
import json
from pathlib import Path
from datetime import datetime
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from matplotlib.font_manager import FontProperties

ROOT=Path('D:/lwx/lwx-project/AD7682')
OUT=ROOT/'output/reports/两板过夜数据分析_20261023/Word对应曲线'
OUT.mkdir(parents=True,exist_ok=True)
plt.rcParams.update({'font.family':FontProperties(fname='C:/Windows/Fonts/msyh.ttc').get_name(),
    'axes.unicode_minus':False,'font.size':13,'legend.fontsize':12,'axes.grid':True,'grid.alpha':.22,
    'axes.spines.top':False,'axes.spines.right':False})
boards=[]
for name in ['周四到周五-下午用新传感器的板子 - 副本.csv','周四到周五-老板子 - 副本.csv']:
    with (ROOT/'code/log'/name).open(encoding='utf-8-sig',newline='') as f:
        rows=list(csv.reader(f))[1:]
    rows=sorted((datetime(*[int(float(v)) for v in r[:6]]),r) for r in rows if r)
    vals=np.array([[float(v) for v in r[6:]] for _,r in rows])
    vals[vals==-9999]=np.nan
    boards.append(([t for t,_ in rows],vals))

TEMPERATURE_START=datetime(2026,10,22,18,40)
DISPLAY_START=TEMPERATURE_START
def shared_display_limits(channel):
    values=np.concatenate([v[np.array([t>=DISPLAY_START for t in times]),channel]
                           for times,v in boards])
    values=values[np.isfinite(values)]
    pad=max(float(np.ptp(values))*.07,.05)
    return float(values.min()-pad),float(values.max()+pad)
temperature_limits=shared_display_limits(0)
humidity_limits=shared_display_limits(1)

def lim(cols):
    vals=np.concatenate([b[1][:,cols].ravel() for b in boards])
    vals=vals[np.isfinite(vals)]
    pad=max(np.ptp(vals)*.07,.05)
    return float(vals.min()-pad),float(vals.max()+pad)

specs=[
 ('01_两板温度','两板温度对比',[(0,0,'新板','#245e9b'),(1,0,'老板','#ce3c76')],lim([0]),'温度 (℃)'),
 ('02_新板温度','新板温度',[(0,0,'新板','#245e9b')],lim([0]),'温度 (℃)'),
 ('03_老板温度','老板温度',[(1,0,'老板','#ce3c76')],lim([0]),'温度 (℃)'),
 ('04_两板湿度','两板湿度对比',[(0,1,'新板','#d58b00'),(1,1,'老板','#333333')],lim([1]),'湿度 (%RH)'),
 ('05_新板湿度','新板湿度',[(0,1,'新板','#d58b00')],lim([1]),'湿度 (%RH)'),
 ('06_老板湿度','老板湿度',[(1,1,'老板','#333333')],lim([1]),'湿度 (%RH)'),
 ('07_气压计对比','两板气压计对比',[(0,2,'新板 U7','#245e9b'),(0,3,'新板 U11','#8b54a3'),(1,2,'老板 U7','#df8125')],lim([2,3]),'气压 (hPa)'),
 ('08_新板通道2','新板 IN1',[(0,5,'新板 IN1','#8c459c')],lim([5,7]),'ADC (codes)'),
 ('09_新板通道4','新板 IN3',[(0,7,'新板 IN3','#333333')],lim([5,7]),'ADC (codes)'),
 ('10_老板通道2','老板 IN1',[(1,5,'老板 IN1','#2d8b4c')],lim([5,7]),'ADC (codes)'),
 ('11_老板通道4','老板 IN3',[(1,7,'老板 IN3','#2d8b4c')],lim([5,7]),'ADC (codes)'),
]
checks=[]
for filename,title,lines,ylim,ylabel in specs:
    fig,ax=plt.subplots(figsize=(14,4.6))
    # Only the time axis is shared. A single trace fills its own measured
    # vertical range so its small changes remain visible without altering data.
    if len(lines)==1:
        bi,c,_,_=lines[0]
        values=boards[bi][1][:,c]
        values=values[np.isfinite(values)]
        pad=max(float(np.ptp(values))*.07,.05)
        ylim=(float(values.min()-pad),float(values.max()+pad))
    if lines[0][1]==0:
        ylim=temperature_limits
    if lines[0][1]==1:
        ylim=humidity_limits
    for bi,c,label,color in lines:
        t,v=boards[bi]
        visible=np.array([stamp>=DISPLAY_START for stamp in t]) if c in (0,1) else np.ones(len(t),dtype=bool)
        ax.plot(np.asarray(t)[visible],v[visible,c],label=label,color=color,lw=1.25)
    ax.set_xlim(datetime(2026,10,22,17,45),datetime(2026,10,23,9,15))
    ax.set_ylim(*ylim)
    ax.xaxis.set_major_locator(mdates.HourLocator(byhour=[0,3,6,9,12,15,18,21]))
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%m-%d\n%H:%M'))
    ax.xaxis.set_minor_locator(mdates.HourLocator())
    ax.grid(axis='x',which='minor',alpha=.12)
    for stamp in [datetime(2026,10,22,21),datetime(2026,10,23,3)]:
        ax.axvline(stamp,color='#64748b',ls='--',alpha=.45,lw=.8)
    ax.set_ylabel(ylabel)
    ax.set_title(title,loc='left',fontsize=16,pad=10)
    ax.legend(loc='upper right',framealpha=.9,ncol=len(lines))
    if lines[0][1] in (0,1):
        ax.text(.015,.06,'18:40 起显示，排除上电/焊接后初始过渡段',transform=ax.transAxes,color='#64748b',fontsize=11)
    if filename.startswith('07'):
        ax.text(.015,.06,'老板 U11 全程为 -9999，未绘制',transform=ax.transAxes,color='#b53030',fontsize=13)
    ax.ticklabel_format(axis='y',style='plain',useOffset=False)
    fig.subplots_adjust(left=.11,right=.89,bottom=.18,top=.88)
    # No tight bounding-box cropping: every image has precisely equal geometry.
    fig.savefig(OUT/(filename+'.png'),dpi=180)
    checks.append({'file':filename+'.png','xlim':list(ax.get_xlim()),'xticks':list(ax.get_xticks()),
                   'bounds':list(ax.get_position().bounds),'ylim':list(ax.get_ylim()),
                   'size_inches':list(fig.get_size_inches())})
    plt.close(fig)
assert all(c['xlim']==checks[0]['xlim'] and c['xticks']==checks[0]['xticks'] and
           c['bounds']==checks[0]['bounds'] and c['size_inches']==checks[0]['size_inches'] for c in checks)
(OUT/'坐标核对.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2),encoding='utf-8')
print('Generated 11 figures; all time limits, ticks, canvas sizes and plot bounds match.')
