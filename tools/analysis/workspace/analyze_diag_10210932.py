from pathlib import Path
from collections import Counter
import numpy as np

root = Path(__file__).resolve().parents[3]
a = np.loadtxt(root / 'code/log/10210932.csv', delimiter=',', skiprows=1)
print('SHAPE', a.shape)
for event in (990, 993, 994, 999):
    print('EVENT', event, a[a[:, 0] == event, :12].tolist())
metadata = a[a[:, 0] == 991]
print('META_ERRORS', metadata[(metadata[:, 7:9] != 0).any(axis=1) | (metadata[:, 11] != 0)].tolist())

def fit(t, high):
    feasible = []
    for f in np.arange(8.2, 8.65, .0005):
        p = (t.ravel() * f / 1e6) % 1
        hp = np.sort(p[high.ravel()])
        if len(hp) < 2:
            continue
        gaps = np.diff(np.r_[hp, hp[0] + 1])
        k = np.argmax(gaps)
        width = 1 - gaps[k]
        inside = (p - hp[(k + 1) % len(hp)]) % 1 <= width + 1e-9
        if not (inside & ~high.ravel()).any() and width / f < .003:
            feasible.append(round(float(f), 4))
    return set(feasible)

shared = None
totalhigh = 0
totalfail = 0
for ch in (2, 1):
    for phase in range(29):
        tag = 100 * ch + phase
        allrows = a[a[:, 0] == tag]
        good = ((allrows[:, 11] == 0) & (allrows[:, 7] > 0) &
                (np.diff(allrows[:, 7:11], axis=1) > 0).all(axis=1))
        b = allrows[good]
        if not len(b):
            print('NO_VALID', tag, len(allrows), Counter(allrows[:, 11]))
            continue
        raw = b[:, 2:7]
        high = raw - np.median(raw) > 150
        t = b[:, 1, None] + np.column_stack((np.zeros(len(b)), b[:, 7:11]))
        amps = []
        for row, mask in zip(raw, high):
            if mask.any() and (~mask).any():
                amps.extend((row[mask] - np.median(row[~mask])).tolist())
        if ch == 1 or phase >= 18:
            print('STATS', tag, 'rows', len(b), 'states', dict(Counter(allrows[:, 11])),
                  'base', np.median(raw), 'range', [raw.min(), raw.max()],
                  'high', high.sum(), 'low', (raw - np.median(raw) < -150).sum(),
                  'bursts', high.any(axis=1).sum(), 'medianfail', (high.sum(axis=1) >= 3).sum(),
                  'localamp', np.percentile(amps, [0,50,100]).tolist() if amps else [],
                  'gap', np.unique(np.diff(t, axis=1)).tolist(),
                  'groupgap', np.unique(np.diff(b[:, 1])).tolist())
        if ch == 1:
            totalhigh += int(high.sum())
            totalfail += int((high.sum(axis=1) >= 3).sum())
            fs = fit(t, high)
            shared = fs if shared is None else shared & fs
            print('FIT', tag, [min(fs), max(fs)] if fs else [])
            if phase >= 18:
                for j in np.flatnonzero(high.any(axis=1)):
                    print('BURST', tag, t[j].tolist(), raw[j].tolist())
print('SHARED', [min(shared), max(shared)] if shared else [], 'HIGH', totalhigh, 'MEDIAN_FAIL', totalfail)
done = np.flatnonzero(a[:, 0] == 999)
normal = a[done[-1]+1:] if len(done) else a[:0]
print('NORMAL_COUNT', len(normal))
for c in range(7, 11):
    x = normal[:, c]
    if len(x):
        print('NORMAL_ADC', c, np.percentile(x, [0,50,100]).tolist(),
              'maxstep', float(abs(np.diff(x)).max()) if len(x)>1 else 0)
