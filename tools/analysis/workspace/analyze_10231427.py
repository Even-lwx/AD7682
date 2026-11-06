from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path(__file__).resolve().parents[3]
out=root/'output/reports/ADC供电对比_10231427'
out.mkdir(parents=True,exist_ok=True)
plt.rcParams['font.sans-serif']=['Microsoft YaHei','SimHei','DejaVu Sans']
plt.rcParams['axes.unicode_minus']=False
files=['10231408-换成5V参考电压.csv','10231427-换成5V参考电压5V传感器供电.csv']
names=['REF=5V，传感器=2.5V','REF=5V，传感器=5V']
def resid(x,w=101):
 return x-np.median(np.lib.stride_tricks.sliding_window_view(np.pad(x,(w//2,w//2),mode='edge'),w),axis=1)
def acf(x):
 y=x-x.mean(); f=np.fft.rfft(y,n=2**int(np.ceil(np.log2(2*len(y))))); c=np.fft.irfft(abs(f)**2)[:1001];return c/c[0] if c[0]>1e-12 else np.zeros_like(c)
fig,axes=plt.subplots(3,2,figsize=(14,11),constrained_layout=True)
for j,fn in enumerate(files):
 a=np.loadtxt(root/'code/log'/fn,delimiter=',',skiprows=1)
 print('FILE',fn,'n',len(a))
 for ch,col,fc,color in [(1,15,8,'#226da8'),(3,17,10,'#d77b21')]:
  x=a[:,col];r=resid(x);pos=r>80;neg=r< -80
  mad=np.median(abs(r-np.median(r)))*1.4826
  print('STATS',ch,'inputV',np.median(x)*5/65536,'median',np.median(x),
    'MADsigma',mad,'MADsigma_mV',mad*5000/65536,'res_sd',np.std(r),
    'pos80',int(pos.sum()),'neg80',int(neg.sum()),
    'pos150',int((r>150).sum()),'neg150',int((r< -150).sum()),
    'minmax',r.min(),r.max(),'filtered_start_end',a[:100,fc].mean(),a[-100:,fc].mean())
  for signal,label in [(r,'residual'),(pos.astype(float),'positive80'),(neg.astype(float),'negative80')]:
   c=acf(signal);pk=np.flatnonzero((c[2:-1]>c[1:-2])&(c[2:-1]>=c[3:]))+2;pk=pk[np.argsort(c[pk])[::-1]][:8]
   print('ACF',ch,label,[(int(k),round(float(c[k]),3)) for k in pk])
  if j==1:
   for w in [51,101,201]:
    rr=resid(x,w);print('WINDOW',ch,w,'negative80',int((rr< -80).sum()),'negative150',int((rr< -150).sum()),'min',rr.min())
   for k,z in enumerate(np.array_split(r,4)):
    print('QUARTER',ch,k+1,'n',len(z),'neg80',int((z< -80).sum()),'pos80',int((z>80).sum()),'sd',np.std(z))
  axes[0,j].plot(r[:1200]*5000/65536,'.-',ms=2,lw=.5,label=f'IN{ch}',color=color)
  axes[1,j].hist(r*5000/65536,bins=np.arange(-40,21,.5),histtype='step',label=f'IN{ch}',color=color)
  axes[2,j].plot(np.arange(1,401),acf(r)[1:401],label=f'IN{ch}',color=color,lw=.8)
 for i in range(3):axes[i,j].grid(alpha=.2);axes[i,j].legend()
 axes[0,j].set(title=names[j]+'：前1200行残差',xlabel='串口行号',ylabel='等效输入偏差/mV',ylim=(-40,21))
 axes[1,j].set(title='残差分布',xlabel='等效输入偏差/mV',ylabel='样本数',yscale='log',xlim=(-40,21))
 axes[2,j].set(title='原始残差自相关（无时间戳，不能换算Hz）',xlabel='延迟/串口行',ylabel='归一化相关',ylim=(-.15,.65))
 if j==1:
  r1=resid(a[:,15]);r3=resid(a[:,17]);print('CROSS',np.corrcoef(r1,r3)[0,1], 'jointneg80',int(((r1< -80)&(r3< -80)).sum()))
fig.savefig(out/'传感器供电对比.png',dpi=150)
plt.close(fig)
# Slow trend and filtered traces are kept separate from residual plots.
a=np.loadtxt(root/'code/log'/files[1],delimiter=',',skiprows=1)
fig,ax=plt.subplots(2,1,figsize=(12,6),sharex=True,constrained_layout=True)
for t,(ch,c,f) in zip(ax,[(1,15,8),(3,17,10)]):
 t.plot(a[:,c]*5/65536,lw=.5,label='原始');t.plot(a[:,f]*5/65536,lw=1,label='滤波');t.set(ylabel='输入等效电压/V',title=f'传感器5V供电 IN{ch}');t.legend();t.grid(alpha=.2)
ax[-1].set_xlabel('串口数据行号')
fig.savefig(out/'5V传感器原始与滤波.png',dpi=150)
