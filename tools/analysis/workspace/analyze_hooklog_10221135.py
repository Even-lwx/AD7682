from pathlib import Path
import numpy as np

p = Path(r'D:/lwx/lwx-project/AD7682/code/log/10221135-用称重板测得数据.csv')
a = np.loadtxt(p, delimiter=',', skiprows=1)
print('shape', a.shape)
names = ['raw','median5','weight_x1000']
for j,n in enumerate(names):
    x=a[:,j]; d=np.diff(x)
    print(n,'pct',np.percentile(x,[0,1,25,50,75,99,100]).tolist(),'sd',float(np.std(x)),
          'dabs',np.percentile(np.abs(d),[50,90,99,100]).tolist())
print('raw saturation >=8388000',int((a[:,0]>=8388000).sum()),'raw max',float(a[:,0].max()))
print('raw near zero',int((np.abs(a[:,0])<100000).sum()))
for lo,hi in [(0,200),(200,400),(400,600),(600,800),(800,1000),(1000,len(a))]:
    s=a[lo:hi]
    if len(s): print('SEG',lo,hi,'raw',np.percentile(s[:,0],[0,50,100]).tolist(),'median',np.percentile(s[:,1],[0,50,100]).tolist(),'weight',np.percentile(s[:,2],[0,50,100]).tolist())
idx=np.flatnonzero(np.abs(np.diff(a[:,0]))>100000)
print('raw jumps >100k',len(idx))
for i in idx[:15]: print('jump',i,a[i].tolist(),a[i+1].tolist())
