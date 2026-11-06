from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from adc_period_analysis import LOG, ROOT, baseline

plt.rcParams['font.sans-serif']=['Microsoft YaHei','DejaVu Sans']
plt.rcParams['axes.unicode_minus']=False
new=np.loadtxt(LOG/'10201507-通道2和通道1.csv',delimiter=',',skiprows=1)
specs=[('10201112_增加转换时间.csv',0,1523,2,'11:12 IN2'),
       ('10201355_感觉有一点改善.csv',0,5100,2,'13:55 IN2（连续异常段前）'),
       ('10201507-通道2和通道1.csv',0,6000,2,'15:07 IN2'),
       ('10201507-通道2和通道1.csv',6500,None,1,'15:07 IN1')]
fig,ax=plt.subplots(3,1,figsize=(12,10),layout='constrained')
for axis,(st,ch) in zip(ax[:2],[(0,2),(6500,1)]):
    raw=new[st:st+250,14+ch*5:19+ch*5]
    d=raw-baseline(np.median(raw,axis=1))[:,None]
    axis.plot(np.arange(st+1,st+len(raw)+1),d.max(axis=1),lw=.9,color='#245f9b')
    axis.axhline(150,color='#ad452d',ls='--',lw=.8,label='识别阈值 +150 codes')
    axis.set(title=f'新日志 IN{ch}：每轮 5 个原始值中的最大偏差',ylabel='偏差（codes）',xlabel='数据记录序号（从 1 开始）')
    axis.set_ylim(-30,370); axis.grid(alpha=.2); axis.legend(loc='upper right')
for fn,st,en,ch,label in specs:
    a=new if fn.startswith('10201507') else np.loadtxt(LOG/fn,delimiter=',',skiprows=1)
    raw=a[st:en,14+5*ch:19+5*ch]
    d=raw-baseline(np.median(raw,axis=1))[:,None]
    e=(d>150).any(axis=1).astype(float)
    nfft=262144
    p=abs(np.fft.rfft((e-e.mean())*np.hanning(len(e)),nfft))**2
    f=np.fft.rfftfreq(nfft)
    ax[2].plot(f,p/p.max(),lw=1,label=label)
ax[2].set(xlim=(.12,.14),ylim=(0,1.1),title='近期日志的尖峰事件频谱：主峰均在约 0.13 周期/记录',xlabel='归一化频率（周期/记录；不是 Hz）',ylabel='相对谱功率')
ax[2].legend(); ax[2].grid(alpha=.2)
fig.suptitle('ADC 尖峰周期对比：换通道后仍保留相近的周期和幅度',fontsize=15)
out=ROOT/'output/reports/adc_noise_frequency_10201507.png'
out.parent.mkdir(exist_ok=True)
fig.savefig(out,dpi=160)
print(out)
