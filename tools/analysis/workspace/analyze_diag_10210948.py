from pathlib import Path
from collections import Counter
import numpy as np

a = np.loadtxt(Path(__file__).resolve().parents[3] / 'code/log/10210948.csv', delimiter=',', skiprows=1)
print('SHAPE', a.shape)
for event in (990, 992, 999):
    print('EVENT', event, a[a[:, 0] == event, :12].tolist())
masks = [0, 1, 0, 2, 0, 4, 0, 3, 0, 7, 0]
for event in (995, 996):
    rows = a[a[:, 0] == event]
    errors = []
    for r in rows:
        mask = masks[int(r[1]) % 100 - 29]
        expected = [2 if mask & (1 << j) else 0 for j in range(3)]
        ok = (np.array_equal(r[5:8], expected) and r[11] == 0 and r[8] == 64000000 and r[9] == 2000000 and r[10] == 20000) if event == 995 else (r[3] == 0 and (r[4:7] == 0).all() and r[7] == 64000000)
        if not ok:
            errors.append(r.tolist())
    print('EVENT_CHECK', event, len(rows), 'errors', errors)
meta = a[a[:, 0] == 991]
print('META_ERRORS', meta[(meta[:, 7:9] != 0).any(axis=1) | (meta[:, 11] != 0)].tolist())
allstats = {}
for ch in (2, 1):
    for phase in range(29, 40):
        tag = ch * 100 + phase
        b = a[a[:, 0] == tag]
        blocks = np.split(b, np.flatnonzero(np.diff(b[:, 1]) <= 0) + 1)
        stats = []
        for bi, block in enumerate(blocks, 1):
            raw = block[:, 2:7]
            base = np.median(raw)
            high = raw - base > 150
            healthy = raw[~high]
            amps = []
            for row, h in zip(raw, high):
                if h.any() and (~h).any():
                    amps.extend((row[h] - np.median(row[~h])).tolist())
            t = block[:, 1, None] + np.column_stack((np.zeros(len(block)), block[:, 7:11]))
            s = dict(n=len(block), high=int(high.sum()), fail=int((high.sum(axis=1) >= 3).sum()),
                     bursts=int(high.any(axis=1).sum()), base=float(base), range=[float(raw.min()), float(raw.max())],
                     amp=float(np.median(amps)) if amps else None,
                     healthy_sd=round(float(np.std(healthy)), 3), healthy_range=np.percentile(healthy, [0, 1, 99, 100]).tolist(),
                     negative=int((raw-base < -150).sum()), status=dict(Counter(block[:, 11])),
                     gap=np.unique(np.diff(t, axis=1)).tolist(), groupgap=np.unique(np.diff(block[:, 1])).tolist(),
                     sensitivity=[int((raw-base > th).sum()) for th in (50, 100, 150, 200)],
                     positions=high.sum(axis=0).tolist())
            stats.append(s)
            print('BLOCK', tag, bi, s)
        allstats[tag] = stats
        print('SUMMARY', tag, 'rows', [s['n'] for s in stats], 'high', [s['high'] for s in stats],
              'fail', [s['fail'] for s in stats], 'amp', [s['amp'] for s in stats], 'base', [s['base'] for s in stats])
print('COMPARISONS_IN1')
for phase in (30, 32, 34, 36, 38):
    for p in (phase-1, phase, phase+1):
        ss = allstats[100+p]
        print(p, 'high', sum(s['high'] for s in ss), 'of', sum(s['n']*5 for s in ss), 'fail', sum(s['fail'] for s in ss),
              'amps', [s['amp'] for s in ss], 'sd', [s['healthy_sd'] for s in ss])
done = np.flatnonzero(a[:, 0] == 999)
normal = a[done[-1]+1:] if len(done) else a[:0]
print('NORMAL', len(normal))
for c in range(7, 11):
    if len(normal):
        print('NORMAL_ADC', c, np.percentile(normal[:, c], [0,50,100]).tolist(), 'maxstep', float(abs(np.diff(normal[:, c])).max()) if len(normal)>1 else 0)
