from pathlib import Path
import numpy as np

root = Path(__file__).resolve().parents[3]
names = ['10191323_16重复.csv', '10191323_通道216重复.csv',
         '10191420_换了一块板子通道216重复.csv']
for name in names:
    a = np.loadtxt(root / 'code/log' / name, delimiter=',', skiprows=1)
    x = a[:, 7:23]
    d = x - np.median(x, axis=1)[:, None]
    high = d > 150
    print(name, 'rows', len(x), 'high', high.sum(), 'rate', high.mean(),
          'amp', np.percentile(d[high], [0, 50, 100]).tolist(),
          'by_position', high.sum(axis=0).tolist())
a = np.loadtxt(root / 'code/log/10210932.csv', delimiter=',', skiprows=1)
highs = []
for tag in range(100,129):
    raw = a[a[:,0] == tag, 2:7]
    highs.append(raw - np.median(raw) > 150)
h = np.vstack(highs)
print('LATEST_by_position', h.sum(axis=0).tolist(), 'rate', h.mean())
