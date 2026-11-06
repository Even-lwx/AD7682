from pathlib import Path
import numpy as np

p = Path(__file__).resolve().parents[3] / 'code/log/10211027.csv'
a = np.loadtxt(p, delimiter=',', skiprows=1)
print('shape', a.shape)
for e in (990, 992, 999):
    print('event', e, a[a[:, 0] == e, :12].tolist())
print('setup', len(a[a[:, 0] == 995]), 'restore', len(a[a[:, 0] == 996]))
for ch in (0, 1):
    print('\nCHANNEL', ch)
    for phase in range(29, 40):
        b = a[a[:, 0] == ch * 100 + phase]
        if not len(b):
            print(phase, 'missing')
            continue
        blocks = np.split(b, np.flatnonzero(np.diff(b[:, 1]) <= 0) + 1)
        out = []
        for block in blocks:
            r = block[:, 2:7]
            base = np.median(r)
            high = r - base > 150
            amp = []
            for row, mask in zip(r, high):
                if mask.any() and (~mask).any():
                    amp.extend((row[mask] - np.median(row[~mask])).tolist())
            out.append((len(block), int(high.sum()), int(high.any(1).sum()),
                        int((high.sum(1) >= 3).sum()),
                        round(float(np.median(amp)), 1) if amp else None,
                        high.sum(0).tolist(), [float(r.min()), float(r.max())]))
        print(phase, out)
done = np.flatnonzero(a[:, 0] == 999)
normal = a[done[-1] + 1:] if len(done) else a[:0]
print('normal rows', len(normal),
      'IN0', np.percentile(normal[:, 7], [0, 50, 100]).tolist() if len(normal) else None,
      'IN1', np.percentile(normal[:, 8], [0, 50, 100]).tolist() if len(normal) else None)
