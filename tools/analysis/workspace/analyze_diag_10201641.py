from pathlib import Path
from collections import Counter
import numpy as np

ROOT = Path(__file__).resolve().parents[3]
a = np.loadtxt(ROOT / 'code/log/10201641.csv', delimiter=',', skiprows=1)
print('shape', a.shape)
print('events', a[np.isin(a[:, 0], [90, 99]), :7])
print('counts', Counter(a[:, 0]))
for tag in list(range(20, 29)) + list(range(10, 19)):
    b = a[a[:, 0] == tag]
    r = b[:, 2:7]
    d = r - np.median(r, axis=1)[:, None]
    pos = d > 150
    neg = d < -150
    print(tag, 'rows', len(b), 'dt', np.unique(b[1:, 1], return_counts=True),
          'baseline', np.median(r), 'pos', int(pos.sum()), 'neg', int(neg.sum()),
          'amp', np.median(d[pos]) if pos.any() else None,
          'max', d.max(), 'invalid', int((r == 65535).sum()),
          'indices', np.where(pos)[0].tolist(), 'bypos', pos.sum(axis=0).tolist())
    if tag in (10, 16, 18):
        for i in np.flatnonzero(pos.any(axis=1))[:2]:
            print('example', tag, 'csv_line', int(np.flatnonzero(a[:, 0] == tag)[i])+2,
                  'raw', r[i].tolist(), 'delta', d[i].tolist())
print('threshold_sensitivity')
for tag in range(10, 19):
    r = a[a[:, 0] == tag, 2:7]
    d = r - np.median(r, axis=1)[:, None]
    print(tag, [(cut, int((d > cut).sum())) for cut in (100, 150, 200, 250)],
          'max_bad_per_group', int((d > 150).sum(axis=1).max()))
