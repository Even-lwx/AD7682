from pathlib import Path
import json
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

root = Path(__file__).resolve().parents[3]
out = root / 'output/reports/ADC诊断_10231122'
out.mkdir(parents=True, exist_ok=True)
a = np.loadtxt(root / 'code/log/10231122.csv', delimiter=',', skiprows=1)
blocks = []
for tag in range(140, 146):
    for block in range(1, 4):
        z = a[(a[:, 0] == tag) & (a[:, 7] == block)]
        if not len(z):
            continue
        t, raw = z[:, 1] / 1e6, z[:, 2]
        baseline = np.median(raw)
        high = raw - baseline > 150
        assert (z[:, 3] == 0).all()
        assert np.all(np.diff(z[:, 4]) == 1)
        assert np.all(np.diff(t) > 0)
        blocks.append(dict(tag=tag, block=block, z=z, t=t, raw=raw,
                           baseline=baseline, high=high))

# Search much wider than the old 8.2..8.65 Hz range. Blocks have independent
# time origins: only compare concentration within each, never concatenate them.
freqs = np.arange(.5, 200.0001, .001)
score = np.zeros(len(freqs))
for b in blocks:
    ts = b['t'][b['high']]
    score += np.abs(np.exp(2j * np.pi * freqs[:, None] * ts).sum(axis=1))
score /= sum(int(b['high'].sum()) for b in blocks)
peaks = np.flatnonzero((score[1:-1] > score[:-2]) & (score[1:-1] >= score[2:])) + 1
peaks = peaks[np.argsort(score[peaks])[::-1]]
print('BROAD_SEARCH', [(round(float(freqs[i]), 4), round(float(score[i]), 6)) for i in peaks[:12]])

def classify(f):
    false_positive = 0
    widths = []
    for b in blocks:
        p = (b['t'] * f) % 1
        hp = np.sort(p[b['high']])
        gaps = np.diff(np.r_[hp, hp[0] + 1])
        k = int(np.argmax(gaps))
        width = 1 - gaps[k]
        start = hp[(k + 1) % len(hp)]
        inside = (p - start) % 1 <= width + 1e-8
        false_positive += int((inside & ~b['high']).sum())
        widths.append(width / f * 1000)
    return false_positive, float(np.max(widths)), float(np.median(widths))

for i in peaks[:12]:
    print('CANDIDATE', round(float(freqs[i]), 4), classify(float(freqs[i])))

# Search fine grid around the strongest broad peak. Check pulse timing against
# healthy samples as well, rather than relying on positive-only phase coherence.
best_i = int(peaks[0])
fine = np.arange(freqs[best_i] - .025, freqs[best_i] + .025, .00005)
fits = [(classify(float(f)), float(f)) for f in fine]
fits.sort()
quality, best_f = fits[0]
print('FINE_MODEL', best_f, 'PERIOD_MS', 1000 / best_f, 'QUALITY', quality)
feasible = [f for q, f in fits if q[0] == 0 and q[1] < 3]
print('ZERO_CONFLICT_UNDER_3MS', [min(feasible), max(feasible)] if feasible else [])

stats = []
for tag in range(140, 146):
    bb = [b for b in blocks if b['tag'] == tag]
    n = sum(len(b['raw']) for b in bb)
    nh = sum(int(b['high'].sum()) for b in bb)
    amps = np.concatenate([b['raw'][b['high']] - b['baseline'] for b in bb])
    dt = np.concatenate([np.diff(b['t']) * 1e6 for b in bb])
    s = dict(tag=tag, samples=n, spikes=nh, fraction=nh/n,
             median_amplitude=float(np.median(amps)),
             dt_us=np.percentile(dt, [0, 50, 100]).tolist())
    stats.append(s)
    print('PHASE', s)

for b in [b for b in blocks if b['tag'] == 142]:
    ids = np.flatnonzero(b['high'])
    clusters = np.split(ids, np.flatnonzero(np.diff(ids) > 1) + 1)
    centers = np.array([np.mean(b['t'][c]) for c in clusters])
    bounds = [(float((b['t'][c[-1]]-b['t'][c[0]])*1000),
               float((b['t'][min(c[-1]+1,len(b['t'])-1)]-
                      b['t'][max(c[0]-1,0)])*1000)) for c in clusters]
    print('DENSE', b['block'], 'centers_s', centers.tolist(),
          'period_ms', (np.diff(centers)*1000).tolist(),
          'pulse_duration_bounds_ms', bounds)

plt.rcParams['font.sans-serif'] = ['Microsoft YaHei', 'SimHei', 'DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False
fig, axes = plt.subplots(3, 1, figsize=(12, 8), constrained_layout=True)
for ax, tag, title in zip(axes, [140, 141, 144],
                         ['固定100 Hz', '变化采样间隔5～15 ms', '突发采样：组内1 ms，组首间隔50 ms']):
    b = next(b for b in blocks if b['tag'] == tag and b['block'] == 2)
    ax.plot(b['t'], b['raw']-b['baseline'], '.-', ms=3, lw=.5)
    ax.scatter(b['t'][b['high']], (b['raw']-b['baseline'])[b['high']], color='#d14932', s=20)
    ax.set(title=title+'（第2块）', ylabel='相对本块中位数 / codes', xlim=(0, 3), ylim=(-125, 330))
    ax.grid(alpha=.25)
axes[-1].set_xlabel('块内实际CNV时间 / s（各块时间起点独立）')
fig.savefig(out/'采样节奏对照.png', dpi=160)
plt.close(fig)

fig, axes = plt.subplots(2, 1, figsize=(12, 6), constrained_layout=True)
b = next(b for b in blocks if b['tag'] == 142 and b['block'] == 1)
d = b['raw']-b['baseline']
axes[0].plot(b['t']*1000, d, '.-', ms=3, lw=.7)
axes[0].set(xlabel='块内实际CNV时间 / ms', ylabel='偏差 / codes', title='1 kHz：连续两点尖峰，三次脉冲间隔均约126 ms')
axes[1].plot(b['t']*1000, d, 'o-', ms=4, lw=.8)
axes[1].set(xlabel='块内实际CNV时间 / ms', ylabel='偏差 / codes', xlim=(4, 14), title='首个脉冲放大：持续时间约束为1～3 ms')
for ax in axes:
    ax.grid(alpha=.25)
fig.savefig(out/'1kHz脉冲细节.png', dpi=160)
plt.close(fig)

done = np.flatnonzero(a[:,0] == 999)
normal = a[done[-1]+1:] if len(done) else a[:0]
print('NORMAL_AFTER',len(normal))
for c in [8, 10, 15, 17]:
    if len(normal):
        print('NORMAL_FIELD',c,np.percentile(normal[:,c],[0,50,100]).tolist())

result = dict(source=str(root/'code/log/10231122.csv'), phase_stats=stats,
              candidate_hz=best_f, candidate_period_ms=1000/best_f,
              candidate_quality=quality, zero_conflict_range=([min(feasible),max(feasible)] if feasible else []),
              missing_initial_points=17, statuses_all_zero=True)
(out/'统计结果.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
