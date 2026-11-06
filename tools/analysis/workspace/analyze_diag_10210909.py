from pathlib import Path
from collections import Counter
import numpy as np

root = Path(__file__).resolve().parents[3]
a = np.loadtxt(root / "code/log/10210909.csv", delimiter=",", skiprows=1)
print("SHAPE", a.shape)
print("EVENTS", a[np.isin(a[:, 0], [990, 991, 992, 999]), :12].tolist())
print("COUNTS", Counter(a[:, 0].astype(int)))
for ch in (2, 1):
    for phase in range(15):
        tag = ch * 100 + phase
        indices = np.flatnonzero(a[:, 0] == tag)
        b = a[indices]
        if len(b) == 0:
            print("MISSING", tag)
            continue
        # Check packet integrity before counting signal spikes.
        ok = ((b[:, 11] == 0) & (b[:, 7] > 0) &
              (np.diff(b[:, 7:11], axis=1) > 0).all(axis=1))
        good = b[ok]
        if len(good) == 0:
            print("INVALID", tag, b[:, :12].tolist())
            continue
        raw = good[:, 2:7]
        base = np.median(raw)
        d = raw - base
        high = d > 150
        low = d < -150
        t = good[:, 1, None] + np.column_stack((np.zeros(len(good)), good[:, 7:11]))
        print("STATS", tag, "rows", len(b), "valid", len(good),
              "status", dict(Counter(b[:, 11])), "base", base,
              "high", int(high.sum()), "low", int(low.sum()),
              "affected", int(high.any(axis=1).sum()),
              "medianfail", int((high.sum(axis=1) >= 3).sum()),
              "range", [float(raw.min()), float(raw.max())],
              "amp", np.percentile(d[high], [0, 50, 100]).tolist() if high.any() else [],
              "group_dt", np.unique(np.diff(good[:, 1]), return_counts=True),
              "point_dt", np.unique(np.diff(t, axis=1), return_counts=True))
        if ch == 1:
            print("HIGHTIMES", tag, t[high].tolist())
            for j in np.flatnonzero(high.any(axis=1)):
                print("BURST", tag, good[j, 1], raw[j].tolist())
            for cut in (50, 100, 150, 200):
                print("THRESHOLD", tag, cut, int((d > cut).sum()), int((d < -cut).sum()))
            # Score candidate ~8.43 Hz against both healthy and high samples.
            if high.any():
                feasible = []
                best = (10**9, 0, 0)
                for f in np.arange(8.2, 8.65, 0.0005):
                    phase_values = (t.ravel() * f / 1e6) % 1
                    hp = np.sort(phase_values[high.ravel()])
                    gaps = np.diff(np.r_[hp, hp[0] + 1])
                    k = np.argmax(gaps)
                    start = hp[(k + 1) % len(hp)]
                    width = 1 - gaps[k]
                    inside = (phase_values - start) % 1 <= width + 1e-9
                    conflicts = int((inside & ~high.ravel()).sum())
                    if conflicts < best[0]:
                        best = (conflicts, float(f), float(width / f * 1e6))
                    if conflicts == 0:
                        feasible.append(float(f))
                print("FREQFIT", tag, "best", best, "zero_range",
                      [min(feasible), max(feasible)] if feasible else [])
normal = a[(a[:, 0] > 1) & (a[:, 0] < 5)]
print("NORMAL", len(normal))
if len(normal):
    print("NORMAL_ADC", [(float(normal[:, c].min()), float(normal[:, c].max()),
                          len(np.unique(normal[:, c] % 1))) for c in range(7, 11)])
