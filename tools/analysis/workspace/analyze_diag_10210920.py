from pathlib import Path
from collections import Counter
import numpy as np

root = Path(__file__).resolve().parents[3]
a = np.loadtxt(root / 'code/log/10210920.csv', delimiter=',', skiprows=1)
print('SHAPE', a.shape)
print('EVENTS', a[np.isin(a[:, 0], [990, 991, 992, 999]), :12].tolist())
data = {}
for ch in (2, 1):
    for phase in range(18):
        tag = ch * 100 + phase
        b = a[a[:, 0] == tag]
        ok = ((b[:, 11] == 0) & (b[:, 7] > 0) &
              (np.diff(b[:, 7:11], axis=1) > 0).all(axis=1))
        b = b[ok]
        if not len(b):
            print('MISSING', tag)
            continue
        raw = b[:, 2:7]
        t = b[:, 1, None] + np.column_stack((np.zeros(len(b)), b[:, 7:11]))
        # Local baseline distinguishes slow signal drift from millisecond pulses.
        base = np.median(raw, axis=1)
        # Use stage median for counts, and neighboring healthy points for amplitude.
        d = raw - np.median(raw)
        high = d > 150
        data[tag] = (t, raw, high)
        print('STATS', tag, 'rows', len(b), 'status', dict(Counter(a[a[:, 0] == tag, 11])),
              'base', np.median(raw), 'range', [raw.min(), raw.max()],
              'high', high.sum(), 'low', (d < -150).sum(),
              'burst', high.any(axis=1).sum(), 'medianfail', (high.sum(axis=1) >= 3).sum(),
              'amp', np.percentile(d[high], [0, 50, 100]).tolist() if high.any() else [],
              'group_us', np.unique(np.diff(b[:, 1]), return_counts=True),
              'point_us', np.unique(np.diff(t, axis=1), return_counts=True))
        if phase >= 15 and ch == 1:
            for j in np.flatnonzero(high.any(axis=1)):
                print('BURST', tag, t[j].tolist(), raw[j].tolist())

def fit(t, high, freqs):
    feasible = []
    best = (100000, float('inf'), 0.)
    for f in freqs:
        p = (t.ravel() * f / 1e6) % 1
        hp = np.sort(p[high.ravel()])
        if len(hp) < 2:
            continue
        gaps = np.diff(np.r_[hp, hp[0] + 1])
        k = np.argmax(gaps)
        start, width = hp[(k + 1) % len(hp)], 1 - gaps[k]
        inside = (p - start) % 1 <= width + 1e-9
        conflicts = int((inside & ~high.ravel()).sum())
        width_us = width / f * 1e6
        best = min(best, (conflicts, width_us, float(f)))
        if conflicts == 0 and width_us < 3000:
            feasible.append(float(f))
    return best, feasible

shared = None
for tag in range(100, 118):
    if tag not in data:
        continue
    t, raw, high = data[tag]
    best, feasible = fit(t, high, np.arange(8.2, 8.65, .0005))
    print('FIT', tag, 'best', best, 'range', [min(feasible), max(feasible)] if feasible else [])
    fs = set(round(f, 4) for f in feasible)
    shared = fs if shared is None else shared & fs
    if tag >= 115:
        for lo, hi in ((4.1, 4.4), (16.4, 17.3)):
            other, fs2 = fit(t, high, np.arange(lo, hi, .0005))
            print('OTHER_FIT', tag, other, [min(fs2), max(fs2)] if fs2 else [])
print('SHARED_FREQUENCY', [min(shared), max(shared)] if shared else [])
normal = a[(a[:, 0] > 1) & (a[:, 0] < 5)]
print('NORMAL_ROWS', len(normal))
if len(normal):
    for c in range(7, 11):
        x = normal[:, c]
        print('NORMAL_ADC', c, 'minmedianmax', np.percentile(x, [0, 50, 100]).tolist(),
              'maxstep', abs(np.diff(x)).max() if len(x) > 1 else 0,
              'step_percentiles', np.percentile(abs(np.diff(x)), [50, 95, 99, 100]).tolist())
