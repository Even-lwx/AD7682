from pathlib import Path
import numpy as np

p = Path(__file__).resolve().parents[3] / 'code/log/10211042.csv'
a = np.loadtxt(p, delimiter=',', skiprows=1)
print('shape', a.shape)
for j, name in zip(range(7, 18), ['f0','f1','f2','f3','u7avg','u11avg','pavg','r0','r1','r2','r3']):
    x = a[:, j]
    dx = np.diff(x)
    print(name, 'pcts', np.percentile(x,[0,1,25,50,75,99,100]).tolist(), 'sd', float(np.std(x)),
          'dabs', np.percentile(np.abs(dx),[50,90,99,100]).tolist(),
          'over10', int((np.abs(dx)>10).sum()), 'over50', int((np.abs(dx)>50).sum()),
          'over150', int((np.abs(dx)>150).sum()))
print('first/last', a[0,:].tolist(), a[-1,:].tolist())
for lo, hi in [(0,1000),(1000,3000),(3000,5000),(5000,len(a))]:
    s = a[lo:hi]
    if len(s) == 0: continue
    print('SEG',lo,hi, 'raw0',np.percentile(s[:,14],[0,50,100]).tolist(),
          'raw1',np.percentile(s[:,15],[0,50,99,100]).tolist(),
          'filtered0',np.percentile(s[:,7],[0,50,100]).tolist(),
          'filtered1',np.percentile(s[:,8],[0,50,99,100]).tolist())
idx = np.flatnonzero(np.abs(np.diff(a[:,15])) > 100)
print('RAW1 jumps >100',len(idx))
for i in idx[:20]: print('JUMP',i, a[i,1:18].tolist(), a[i+1,1:18].tolist())
