from pathlib import Path
import numpy as np
import pandas as pd

root = Path(r'D:/lwx/lwx-project/AD7682')
files = [('1号板', '1030-1102-1号板合并.csv'), ('2号板', '1030-1102-2号板.csv')]
start = pd.Timestamp('2026-10-30 18:00:00')

for board, filename in files:
    raw = pd.read_csv(root / 'code/log' / filename)
    timestamp = pd.to_datetime(dict(
        year=raw.I0.astype(int), month=raw.I1.astype(int), day=raw.I2.astype(int),
        hour=raw.I3.astype(int), minute=raw.I4.astype(int), second=raw.I5.astype(int)))
    raw.index = timestamp
    raw = raw.loc[raw.index >= start].copy()
    for col in ['I8', 'I9', 'I10', 'I12']:
        raw.loc[(raw[col] == -9999) | ~np.isfinite(raw[col]), col] = np.nan
    raw['传感器1号'] = 225 + 1250 * raw.I10 / 65535 if board == '1号板' else np.nan
    if board == '1号板':
        raw['传感器2号'] = 225 + 1250 * raw.I12 / 65535
    else:
        raw['传感器3号'] = 225 + 1250 * raw.I10 / 65535
    sec = raw.groupby(level=0).mean(numeric_only=True).sort_index()
    print('\n', board, sec.index.min(), sec.index.max(), len(sec))
    cols = ['I8', 'I9'] + (['传感器1号', '传感器2号'] if board == '1号板' else ['传感器3号'])
    for col in cols:
        x = sec[col].dropna()
        diff = x.diff().dropna()
        # A centered five-minute median approximates the local pressure baseline.
        base = x.rolling('5min', center=True, min_periods=30).median()
        residual = (x - base).dropna()
        mad = np.median(np.abs(residual - np.median(residual)))
        # Summarize the distribution of one-minute peak-to-peak values.
        minute = x.resample('1min').agg(lambda z: z.max() - z.min() if len(z) >= 30 else np.nan).dropna()
        print(f'{col:8s} range={x.min():.3f}..{x.max():.3f} span={x.max()-x.min():.3f} '
              f'sd={x.std():.4f}; diff abs median/p95/max={diff.abs().median():.4f}/'
              f'{diff.abs().quantile(.95):.4f}/{diff.abs().max():.4f}; '
              f'5min-baseline resid rms={np.sqrt(np.mean(residual**2)):.4f} '
              f'robust_sigma={1.4826*mad:.4f} p2p95={residual.quantile(.975)-residual.quantile(.025):.4f}; '
              f'1min span median/p95={minute.median():.4f}/{minute.quantile(.95):.4f}')
