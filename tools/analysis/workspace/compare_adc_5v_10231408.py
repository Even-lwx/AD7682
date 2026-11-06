from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

root = Path(__file__).resolve().parents[3]
out = root/'output/reports/ADC对比_10231408'
out.mkdir(parents=True, exist_ok=True)
a = np.loadtxt(root/'code/log/10231408-换成5V参考电压.csv', delimiter=',', skiprows=1)
b = np.loadtxt(root/'code/log/10231122.csv', delimiter=',', skiprows=1)
def residual(x, width=101):
    p = np.pad(x, (width//2,width//2), mode='edge')
    return x - np.median(np.lib.stride_tricks.sliding_window_view(p,width),axis=1)

plt.rcParams['font.sans-serif']=['Microsoft YaHei','SimHei','DejaVu Sans']
plt.rcParams['axes.unicode_minus']=False
fig, axes=plt.subplots(3,1,figsize=(12,9),constrained_layout=True)
masks=[]
for ch,raw_col,filt_col,color in [(1,15,8,'#226da8'),(3,17,10,'#d77b21')]:
    x=a[:,raw_col];r=residual(x);h=r>80;masks.append(h)
    amps=r[h]
    print('CHANNEL',ch,'raw',np.percentile(x,[0,50,100]).tolist(),
          'filtered',np.percentile(a[:,filt_col],[0,50,100]).tolist(),
          'residual',np.percentile(r,[0,1,50,95,98,99,100]).tolist(),
          'positive_count',int(h.sum()),'negative80',int((r< -80).sum()),
          'amplitude',np.percentile(amps,[0,10,50,90,100]).tolist(),
          'amplitude_mv',np.percentile(amps,[10,50,90])*5000/65536,
          'healthy_sd',np.std(r[abs(r)<40]))
    ids=np.flatnonzero(h)
    print('SPACING',ch, sorted(zip(*np.unique(np.diff(ids),return_counts=True)),key=lambda p:-p[1])[:12])
    # The serial stream lacks CNV timestamps: only row-lag relationships here.
    for lag in range(-3,4):
        if ch==3:
            shifted=np.roll(h,lag); print('COINCIDENCE_LAG',lag,int((masks[0]&shifted).sum()))
    axes[0].plot(x,lw=.6,color=color,label=f'IN{ch} 原始')
    axes[0].plot(a[:,filt_col],lw=1,color='black',alpha=.6,label=f'IN{ch} 滤波')
    axes[1].plot(r[:800]*5000/65536,'.-',ms=2,lw=.5,color=color,label=f'IN{ch} 5V参考')
    axes[2].hist(r,bins=np.arange(-100,200,2),histtype='step',color=color,label=f'IN{ch}',lw=1.2)

print('JOINT',len(a),int((masks[0]&masks[1]).sum()),int((masks[0]|masks[1]).sum()))
for width in [51,101,201]:
    for col in [15,17]:
        r=residual(a[:,col],width)
        print('SENSITIVITY',width,col,[(th,int((r>th).sum()),float(np.median(r[r>th]))) for th in [70,80,90,100]])
for tag in [140,141,142,143,144,145]:
    z=b[b[:,0]==tag,2]; print('OLD',tag,'base',np.median(z),'amp',np.median(z[z-np.median(z)>150]-np.median(z)))

axes[0].set(title='5V参考：原始值与滤波值',xlabel='串口数据行号（无转换时间戳）',ylabel='ADC codes')
axes[1].set(title='去除慢基线后：两路均仍有约10mV量级的正尖峰',xlabel='串口数据行号（前800行）',ylabel='输入等效偏差 / mV',ylim=(-10,16))
axes[2].set(title='局部基线残差分布：正侧仍有分离的尖峰群',xlabel='相对101点局部中位数 / codes',ylabel='样本数',yscale='log')
for ax in axes:
    ax.legend(); ax.grid(alpha=.2)
fig.savefig(out/'5V参考噪声分析.png',dpi=150)
