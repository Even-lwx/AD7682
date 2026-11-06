from pathlib import Path
from collections import Counter
import numpy as np

ROOT = Path(__file__).resolve().parents[3]
A = np.loadtxt(ROOT / 'code/log/10201703.csv', delimiter=',', skiprows=1)
print('shape', A.shape)
print('events', A[np.isin(A[:, 0], [990, 991, 992, 999]), :12].tolist())
print('counts', Counter(A[:, 0].astype(int)))
for tag in list(range(200, 210)) + list(range(100, 110)):
    ix = np.flatnonzero(A[:, 0] == tag)
    b = A[ix]
    if not len(b):
        print('MISSING', tag)
        continue
    r = b[:, 2:7]
    d = r - np.median(r, axis=1)[:, None]
    pos, neg = d > 150, d < -150
    times = b[:, 1, None] + np.column_stack([np.zeros(len(b)), b[:, 7:11]])
    print('STATS', tag, 'n', len(b), 'status', Counter(b[:, 11]),
          'start_end_us', b[[0, -1], 1].tolist(),
          'group_dt_us', np.unique(np.diff(b[:, 1]), return_counts=True),
          'conversion_dt_us', np.unique(np.diff(times, axis=1), return_counts=True),
          'baseline', float(np.median(r)), 'pos_neg', int(pos.sum()), int(neg.sum()),
          'amp', float(np.median(d[pos])) if pos.any() else None,
          'delta_quantiles', np.percentile(d, [0, 1, 50, 95, 99, 100]).tolist(),
          'bypos', pos.sum(axis=0).tolist(),
          'threshold_counts', [(k, int((d > k).sum()), int((d < -k).sum())) for k in (50,100,150,200,250)],
          'max_bad_per_group', int(pos.sum(axis=1).max()))
    for j in np.flatnonzero(pos.any(axis=1)):
        print('SPIKE', tag, 'line', int(ix[j])+2, 'raw', r[j].tolist(),
              'delta', d[j].tolist(), 'times_us', times[j][pos[j]].tolist())
    if tag >= 100 and tag < 110:
        print('POSITION_RANGE', tag, [(float(np.min(r[:, j])), float(np.max(r[:, j])),
                                     float(np.median(r[:, j]))) for j in range(5)])

normal = A[(A[:, 0] > 1) & (A[:, 0] < 5)]
print('normal_rows', len(normal))
if len(normal):
    r = normal[:, 19:24]
    d = r - np.median(r, axis=1)[:, None]
    print('normal_IN1', 'median', np.median(r), 'pos_neg', int((d > 150).sum()),
          int((d < -150).sum()), 'raw_min_max', r.min(), r.max(),
          'filtered_min_max', normal[:, 8].min(), normal[:, 8].max())

print('ABSOLUTE_BASELINE_ANALYSIS')
for tag in range(100, 110):
    ix = np.flatnonzero(A[:, 0] == tag)
    b = A[ix]
    r = b[:, 2:7]
    center = np.median(r)
    d = r - center
    pos = d > 150
    t = b[:, 1, None] + np.column_stack([np.zeros(len(b)), b[:, 7:11]])
    print('ABS', tag, 'baseline', center, 'pos', pos.sum(), 'neg', (d < -150).sum(),
          'affected_rows', pos.any(axis=1).sum(), 'median_fail_rows', (pos.sum(axis=1) >= 3).sum(),
          'bypos', pos.sum(axis=0).tolist(), 'amp_range', d[pos].tolist(),
          'threshold_counts', [(cut, int((d > cut).sum())) for cut in (100,150,200)],
          'spike_times_us', t[pos].tolist())
    for j in np.flatnonzero(pos.any(axis=1)):
        print('ABS_ROW', tag, 'line', int(ix[j])+2, 'raw', r[j].tolist(), 'times_us', t[j].tolist())
    st = t[pos]
    if len(st):
        print('TIME_GAPS_US', tag, np.diff(st).tolist())

print('AGGREGATE')
total_bad = 0
fail_count = 0
amps = []
for tag in range(100, 110):
    b = A[A[:, 0] == tag]
    r = b[:, 2:7]
    d = r - np.median(r)
    mask = d > 150
    total_bad += int(mask.sum())
    fail_count += int((mask.sum(axis=1) >= 3).sum())
    amps.extend(d[mask].tolist())
    print('MEDIAN_OUT', tag, 'range', np.median(r, axis=1).min(), np.median(r, axis=1).max())
    t = b[:, 1, None] + np.column_stack([np.zeros(len(b)), b[:, 7:11]])
    # One connected high run bracketed by healthy samples in the same burst.
    for j in range(len(b)):
        high = np.flatnonzero(mask[j])
        if len(high) and high[0] > 0 and high[-1] < 4 and np.all(np.diff(high) == 1):
            lo = t[j, high[-1]] - t[j, high[0]]
            hi = t[j, high[-1]+1] - t[j, high[0]-1]
            print('PULSE_WIDTH_BOUND_US', tag, j, lo, hi)
print('TOTAL', total_bad, '/4800', 'median_fail', fail_count, '/960',
      'amp_median', np.median(amps), 'amp_min_max', min(amps), max(amps))

# First rank candidate frequencies using within-stage high-sample phase coherence.
# This alone cannot distinguish aliases or harmonics; check low samples below.
def phase_fit(tag, freqs):
    b = A[A[:, 0] == tag]
    r = b[:, 2:7]
    high = (r - np.median(r) > 150).ravel()
    times = (b[:, 1, None] + np.column_stack([np.zeros(len(b)), b[:, 7:11]])).ravel() / 1e6
    ts = times[high]
    coherence = []
    for k in range(0, len(freqs), 2000):
        f = freqs[k:k+2000]
        coherence.extend(np.abs(np.mean(np.exp(2j*np.pi*f[:, None]*ts[None, :]), axis=1)))
    return np.array(coherence)

freqs = np.arange(1.0, 501.0, 0.005)
tags = list(range(100, 110))
c = np.array([phase_fit(tag, freqs) for tag in tags])
score = c.mean(axis=0)
maxima = np.flatnonzero((score[1:-1] > score[:-2]) & (score[1:-1] >= score[2:])) + 1
best = maxima[np.argsort(score[maxima])[-15:][::-1]]
print('PER_STAGE_COHERENCE_COMBINED_TOP', [(round(float(freqs[k]), 4), round(float(score[k]), 4)) for k in best])
for tag, vals in zip(tags, c):
    narrow = (freqs > 7) & (freqs < 10)
    k = np.flatnonzero(narrow)[np.argmax(vals[narrow])]
    print('FREQ_AROUND_8HZ', tag, float(freqs[k]), float(vals[k]))

def pulse_fold(tag, f):
    b = A[A[:, 0] == tag]
    r = b[:, 2:7]
    high = (r - np.median(r) > 150).ravel()
    times = (b[:, 1, None] + np.column_stack([np.zeros(len(b)), b[:, 7:11]])).ravel() / 1e6
    phase = (times * f) % 1
    hp = np.sort(phase[high])
    gaps = np.diff(np.r_[hp, hp[0]+1])
    k = np.argmax(gaps)
    start = hp[(k+1) % len(hp)]
    width = 1 - gaps[k]
    included = ((phase-start) % 1) <= width+1e-9
    return int((included & ~high).sum()), width/f*1e6

for f in [8.43, 16.86, 25.29, 33.72, 42.155, 50.585, 59.015, 58.42, 49.99]:
    results = [pulse_fold(tag, f) for tag in tags]
    print('PULSE_FOLD', f, 'healthy_inside_high_window', sum(x[0] for x in results),
          'per_stage', [(tag, x[0], round(x[1])) for tag,x in zip(tags, results)])

feasible = []
for f in np.arange(8.3, 8.55, 0.0005):
    results = [pulse_fold(tag, f) for tag in tags]
    if sum(x[0] for x in results) == 0:
        feasible.append(float(f))
print('ZERO_CONFLICT_FREQUENCY_RANGE', min(feasible), max(feasible),
      'NOTE', 'assumes constant frequency, independent per-stage phase and pulse width; not proof of source')

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

labels = ['Baseline', 'SysTick IRQ off', 'TIM14 IRQ off', 'Both IRQs off',
          'SPI 1 MHz', 'SPI 4 MHz', 'Wait 100 us', 'Wait 1000 us',
          'Trigger 17/23 ms', 'Repeat baseline']
counts = []
failed = []
for tag in tags:
    r = A[A[:, 0] == tag, 2:7]
    mask = r - np.median(r) > 150
    counts.append(mask.sum())
    failed.append((mask.sum(axis=1) >= 3).sum())
fig, axes = plt.subplots(2, 2, figsize=(13, 8), constrained_layout=True)
ax = axes[0, 0]
y = np.arange(10)
ax.barh(y, counts, color='#4478a6')
ax.set_yticks(y, labels)
ax.invert_yaxis()
ax.set_xlim(0, 14)
ax.set_xlabel('High raw samples / 480 conversions')
ax.set_title('IN1: spikes remain under every condition')
for j, n in enumerate(counts):
    ax.text(n+0.2, j, str(n), va='center')
ax = axes[0, 1]
tag = 107
b = A[A[:, 0] == tag]
r = b[:, 2:7]
t = b[:, 1, None] + np.column_stack([np.zeros(len(b)), b[:, 7:11]])
d = r - np.median(r)
ax.scatter(t.ravel()/1e6, d.ravel(), s=9, color='#4478a6')
ax.scatter(t[d>150]/1e6, d[d>150], s=24, color='#b54336')
ax.set_title('1 ms wait: events repeat about every 119 ms')
ax.set_xlabel('Time within stage (s)')
ax.set_ylabel('Raw code minus stage median')
ax = axes[1, 0]
for tag, wanted, label in [(100, 3, 'Baseline: 3 high / 5'),
                            (103, 3, 'Both IRQs off: 3 high / 5'),
                            (106, 5, '100 us wait: 5 high / 5')]:
    b = A[A[:, 0] == tag]
    high = b[:, 2:7] - np.median(b[:, 2:7]) > 150
    j = np.flatnonzero(high.sum(axis=1) == wanted)[0]
    rr = b[j, 2:7] - np.median(b[:, 2:7])
    tt = np.r_[0, b[j, 7:11]] / 1000
    ax.plot(tt, rr, 'o-', label=label, markersize=4)
ax.set_title('Five samples can land inside one pulse')
ax.set_xlabel('Time from first conversion (ms)')
ax.set_ylabel('Raw code minus stage median')
ax.legend(fontsize=8)
ax = axes[1, 1]
for tag in tags:
    b = A[A[:, 0] == tag]
    r = b[:, 2:7]
    high = (r - np.median(r) > 150).ravel()
    times = (b[:, 1, None] + np.column_stack([np.zeros(len(b)), b[:, 7:11]])).ravel()/1e6
    phase = (times*8.43) % 1
    angle = np.angle(np.mean(np.exp(2j*np.pi*phase[high]))) / (2*np.pi)
    centered_ms = ((phase-angle+0.5)%1-0.5) / 8.43 * 1000
    ax.scatter(centered_ms, np.repeat(tag-100, len(phase)), s=3, color='#adb8c2', alpha=0.5)
    ax.scatter(centered_ms[high], np.repeat(tag-100, high.sum()), s=12, color='#b54336')
ax.set_xlim(-7, 7)
ax.set_yticks(range(10))
ax.set_ylabel('Test stage')
ax.set_xlabel('Phase relative to high samples (ms)')
ax.set_title('8.43 Hz fold; phase fitted separately per stage')
for ax in axes.ravel():
    ax.spines[['top', 'right']].set_visible(False)
    ax.grid(alpha=0.15)
fig.suptitle('AD7682 diagnostic log: 10201703 (IN1)', fontsize=15)
out = ROOT / 'output/reports/adc_diag_10201703.png'
out.parent.mkdir(exist_ok=True)
fig.savefig(out, dpi=160)
plt.close(fig)
print('PLOT', str(out))
