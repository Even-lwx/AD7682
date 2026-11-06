from pathlib import Path
import base64, hashlib, html, io, json
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.dates as mdates

ROOT = Path(r'D:/lwx/lwx-project/AD7682')
OUT = ROOT / 'output/reports/长期测试_1104-1105_BOOST'
OUT.mkdir(parents=True, exist_ok=True)
START = pd.Timestamp('2026-11-04 13:00:00')
END = pd.Timestamp('2026-11-05 08:25:00')
VREF = 5.0
FILES = [('1号板', r'C:/Users/嵌入式软件实习生电脑02/Desktop/1104-05-1号板子.csv', [1, 2]),
         ('2号板', r'C:/Users/嵌入式软件实习生电脑02/Desktop/1104-05-2号板子.csv', [4, 3])]
COLORS = ['#167d78', '#d17b20', '#2867a5', '#a14b86', '#687d8f', '#bd4e48']
plt.rcParams.update({'font.sans-serif': ['Microsoft YaHei'], 'axes.unicode_minus': False,
                     'path.simplify': False, 'agg.path.chunksize': 10000, 'font.size': 10})

def esc(x): return html.escape(str(x))
def f(x, n=3): return 'N/A' if not np.isfinite(x) else f'{float(x):.{n}f}'
def table(rows, heads):
    return '<div class="tablewrap"><table><thead><tr>' + ''.join(f'<th>{esc(x)}</th>' for x in heads) + '</tr></thead><tbody>' + ''.join('<tr>' + ''.join(f'<td>{esc(x)}</td>' for x in row) + '</tr>' for row in rows) + '</tbody></table></div>'

boards = {}
source_audit = []
for board, filename, sensors in FILES:
    path = Path(filename) if Path(filename).is_absolute() else ROOT / 'code/log' / filename
    raw = pd.read_csv(path)
    expected = [f'I{i}' for i in range(15)]
    assert raw.columns.tolist() == expected, f'{filename}: unexpected columns'
    timestamp = pd.to_datetime(dict(year=raw.I0.astype(int), month=raw.I1.astype(int),
                                    day=raw.I2.astype(int), hour=raw.I3.astype(int),
                                    minute=raw.I4.astype(int), second=raw.I5.astype(int)))
    raw['time'] = timestamp
    outside = int(((timestamp < START) | (timestamp > END)).sum())
    d = raw.loc[(timestamp >= START) & (timestamp <= END)].copy()
    d = d.sort_values('time', kind='stable').set_index('time')
    assert d.index.min() >= START and d.index.max() <= END
    invalid = {}
    for col in ['I6', 'I7', 'I8', 'I9', 'I10', 'I12', 'I14']:
        bad = ~np.isfinite(d[col]) | (d[col] == -9999)
        if col in ('I8', 'I9'): bad |= d[col] <= 0
        if col in ('I10', 'I12'): bad |= (d[col] < 0) | (d[col] > 65535)
        invalid[col] = int(bad.sum())
        d.loc[bad, col] = np.nan
    for ch, col in enumerate(['I10', 'I12']):
        d[f'IN{0 if ch == 0 else 2}_mV'] = d[col] * VREF * 1000 / 65536
        d[f'S{ sensors[ch] }_hPa'] = 225 + 250 * VREF * d[col] / 65536
    d['数字压力均值_hPa'] = d[['I8', 'I9']].mean(axis=1)
    gaps = d.index.to_series().diff().dt.total_seconds().dropna()
    boards[board] = dict(data=d, sensors=sensors, filename=filename, digest=hashlib.sha256(path.read_bytes()).hexdigest(),
                         input_rows=len(raw), outside=outside, invalid=invalid, gaps=gaps)
    source_audit.append([board, filename, len(raw), len(d), outside,
                         d.index.min().strftime('%Y-%m-%d %H:%M:%S'),
                         d.index.max().strftime('%Y-%m-%d %H:%M:%S'),
                         f(gaps.median()), f(gaps.max()), str(invalid), hashlib.sha256(path.read_bytes()).hexdigest()])

# Nearest-time pairing is used only for cross-board statistics. Samples remain
# untouched in all time-series plots; no interpolation or resampling is used.
a = boards['1号板']['data'].reset_index().sort_values('time')
b = boards['2号板']['data'].reset_index().sort_values('time')
paired = pd.merge_asof(a, b, on='time', direction='nearest', tolerance=pd.Timedelta(seconds=5),
                       suffixes=('_b1', '_b2'))
paired = paired.dropna(subset=['I6_b2'])
lag_seconds = (paired.time - paired.apply(lambda r: b.iloc[(b.time-r.time).abs().argmin()].time, axis=1)).dt.total_seconds()

def add_time_axis(ax):
    ax.set_xlim(START, END)
    ax.xaxis.set_major_locator(mdates.HourLocator(interval=2))
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%m-%d\n%H:%M'))
    ax.grid(color='#dbe5ed', lw=.6, alpha=.8)
    ax.spines[['top', 'right']].set_visible(False)
    ax.ticklabel_format(axis='y', style='plain', useOffset=False)

figures = []
def figure(title, plot_fn, note, time_axis=True):
    fig, ax = plt.subplots(figsize=(14, 4.7)); fig.patch.set_facecolor('white'); ax.set_facecolor('#fcfdff')
    plot_fn(fig, ax)
    if time_axis:
        add_time_axis(ax)
    else:
        ax.grid(color='#dbe5ed', lw=.6, alpha=.8); ax.spines[['top', 'right']].set_visible(False)
        ax.ticklabel_format(axis='both', style='plain', useOffset=False)
    fig.subplots_adjust(left=.085, right=.98, bottom=.19, top=.85)
    buf = io.BytesIO(); fig.savefig(buf, format='png', dpi=150); plt.close(fig)
    image = base64.b64encode(buf.getvalue()).decode('ascii'); figures.append(title)
    return f'<article><h3>{esc(title)}</h3><p>{esc(note)}</p><a href="data:image/png;base64,{image}" target="_blank"><img src="data:image/png;base64,{image}" alt="{esc(title)}" loading="lazy"></a></article>'

def paired_lines(title, fields, ylabel, note):
    def draw(fig, ax):
        for i, (board, col, label) in enumerate(fields):
            d = boards[board]['data']; ax.plot(d.index, d[col], color=COLORS[i % len(COLORS)], lw=.65, label=label)
        ax.set_ylabel(ylabel); ax.set_xlabel('设备RTC时间（北京时间）'); ax.legend(ncol=3, fontsize=9, frameon=False)
    return figure(title, draw, note)

parts = []
parts.append(paired_lines('两板电池电压', [(b, 'I14', b) for b, _, _ in FILES], '电压 / V', '完整保留选定区间内所有记录。'))
parts.append(paired_lines('两板温度', [(b, 'I6', b) for b, _, _ in FILES], '温度 / °C', '完整保留选定区间内所有记录。'))
parts.append(paired_lines('两板相对湿度', [(b, 'I7', b) for b, _, _ in FILES], '相对湿度 / %RH', '完整保留选定区间内所有记录。'))
digital_fields = [(board, col, f'{board}气压计{n}号') for board, _, _ in FILES for n, col in [(1, 'I8'), (2, 'I9')]]
parts.append(paired_lines('两板四个数字气压计', digital_fields, '压力 / hPa', '四个数字压力传感器共用同一绝对时间轴。'))
adc_fields = []
for board, _, sensors in FILES:
    adc_fields += [(board, 'I10', f'传感器{sensors[0]}号 IN0'), (board, 'I12', f'传感器{sensors[1]}号 IN2')]
parts.append(paired_lines('四个模拟传感器Flash滤波ADC码', adc_fields, '滤波ADC码', '日志中保存的是滤波后码值，非ADC单次原始转换；I10=IN0，I12=IN2。'))
adc_fields_no4 = [x for x in adc_fields if '传感器4号' not in x[2]]
parts.append(paired_lines('模拟传感器Flash滤波ADC码（不含传感器4号）', adc_fields_no4, '滤波ADC码', '排除传感器4号，仅作为对照视图；全通道原图仍保留，数据与统计均未删除。'))
voltage_fields = [(board, f'IN{ch}_mV', f'传感器{sid}号 IN{ch}') for board, _, sensors in FILES for ch, sid in [(0, sensors[0]), (2, sensors[1])]]
parts.append(paired_lines('四个模拟传感器等效输入电压', voltage_fields, '等效电压 / mV', '按5.0V AD7682 REF换算ADC码值；只代表ADC输入端等效电压。'))
voltage_fields_no4 = [x for x in voltage_fields if '传感器4号' not in x[2]]
parts.append(paired_lines('模拟传感器等效输入电压（不含传感器4号）', voltage_fields_no4, '等效电压 / mV', '排除传感器4号，仅作为对照视图；全通道原图仍保留，数据与统计均未删除。'))
pressure_fields = [(board, f'S{sid}_hPa', f'传感器{sid}号') for board, _, sensors in FILES for sid in sensors]
parts.append(paired_lines('四个模拟传感器厂家公式换算压力', pressure_fields, '派生压力 / hPa', '根据IPNZAA135KP-4428-A1表格线性公式换算，作为派生比较值，适用供电条件需以厂家标定条件为准。'))
pressure_fields_no4 = [x for x in pressure_fields if '传感器4号' not in x[2]]
parts.append(paired_lines('模拟压力厂家公式换算（不含传感器4号）', pressure_fields_no4, '派生压力 / hPa', '排除传感器4号，仅作为对照视图；全通道原图仍保留，数据与统计均未删除。'))
digital_mean_fields = [(board, '数字压力均值_hPa', f'{board}数字气压计均值') for board, _, _ in FILES]
parts.append(paired_lines('数字压力均值与模拟换算压力', digital_mean_fields + pressure_fields, '压力 / hPa', '共用同一绝对时间轴；显示数字压力均值和四路模拟公式换算结果。'))
parts.append(paired_lines('数字压力均值与模拟换算压力（不含传感器4号）', digital_mean_fields + pressure_fields_no4, '压力 / hPa', '排除传感器4号模拟压力，保留两板数字气压均值和其他三路模拟压力；全数据未删除。'))

def draw_adc_delta(fig, ax):
    ax.plot(paired.time, paired.I10_b2-paired.I10_b1, lw=.6, color=COLORS[0], label='IN0：传感器3−传感器1')
    ax.plot(paired.time, paired.I12_b2-paired.I12_b1, lw=.6, color=COLORS[1], label='IN2：传感器4−传感器2')
    ax.axhline(0,color='#475569',lw=.8,ls='--');ax.set_ylabel('2号板−1号板 / ADC码');ax.set_xlabel('最近邻配对，以1号板记录时间为横坐标');ax.legend(frameon=False)
parts.append(figure('两板模拟ADC同通道差值', draw_adc_delta, '最近邻配对时差不超过5秒，不插值；日志中的ADC为Flash滤波值。'))
def draw_adc_delta_no4(fig, ax):
    ax.plot(paired.time, paired.I10_b2-paired.I10_b1, lw=.6, color=COLORS[0], label='IN0：传感器3−传感器1')
    ax.axhline(0,color='#475569',lw=.8,ls='--');ax.set_ylabel('2号板−1号板 / ADC码');ax.set_xlabel('最近邻配对，以1号板记录时间为横坐标');ax.legend(frameon=False)
parts.append(figure('两板模拟ADC同通道差值（不含传感器4号）', draw_adc_delta_no4, '仅保留IN0板间差值；全通道差值图仍保留。'))

def draw_pressure_delta(fig, ax):
    ax.plot(paired.time, paired.S3_hPa-paired.S1_hPa, lw=.6, color=COLORS[2], label='IN0：传感器3−传感器1')
    ax.plot(paired.time, paired.S4_hPa-paired.S2_hPa, lw=.6, color=COLORS[3], label='IN2：传感器4−传感器2')
    ax.axhline(0,color='#475569',lw=.8,ls='--');ax.set_ylabel('厂家公式换算差 / hPa');ax.set_xlabel('最近邻配对，以1号板记录时间为横坐标');ax.legend(frameon=False)
parts.append(figure('两板模拟压力同通道差值', draw_pressure_delta, '两板各自不同的传感器，差值包括传感器个体偏置和趋势差异，不代表板级误差。'))
def draw_pressure_delta_no4(fig, ax):
    ax.plot(paired.time, paired.S3_hPa-paired.S1_hPa, lw=.6, color=COLORS[2], label='IN0：传感器3−传感器1')
    ax.axhline(0,color='#475569',lw=.8,ls='--');ax.set_ylabel('厂家公式换算差 / hPa');ax.set_xlabel('最近邻配对，以1号板记录时间为横坐标');ax.legend(frameon=False)
parts.append(figure('两板模拟压力同通道差值（不含传感器4号）', draw_pressure_delta_no4, '仅保留IN0板间差值；全通道差值图仍保留。'))

noise_rows = []
residual_fields = []
for board, _, sensors in FILES:
    d = boards[board]['data']
    measures = [('I8', f'{board}气压计1号'), ('I9', f'{board}气压计2号')]
    measures += [(f'S{sid}_hPa', f'传感器{sid}号模拟压力') for sid in sensors]
    for col, label in measures:
        x = d[col].dropna()
        baseline = x.rolling('5min', center=True, min_periods=15).median()
        residual = x - baseline
        residual_fields.append((board, col, x.index, residual, label))
        delta = x.diff().where(x.index.to_series().diff().dt.total_seconds() <= 11)
        per_min = x.resample('1min').agg(['min', 'max', 'count'])
        span = (per_min['max'] - per_min['min']).where(per_min['count'] >= 4).dropna()
        noise_rows.append([label, len(x), f(span.median()), f(span.quantile(.95)), f(np.sqrt(np.nanmean(residual.to_numpy()**2))),
                           f(residual.quantile(.975)-residual.quantile(.025)), f(delta.abs().quantile(.95)), f(x.std()), f(x.max()-x.min())])

def draw_res(fig, ax):
    for i, (board, col, idx, res, label) in enumerate(residual_fields):
        ax.plot(idx, res, lw=.55, color=COLORS[i % len(COLORS)], label=label)
    ax.set_ylabel('偏离5分钟局部中位基线 / hPa'); ax.set_xlabel('设备RTC时间（北京时间）'); ax.legend(ncol=3, fontsize=8, frameon=False)
parts.append(figure('模拟与数字压力去局部基线后的波动', draw_res, '居中5分钟中位数仅作离线波动诊断，不是固件滤波；保留全部有效点。'))
def draw_res_no4(fig, ax):
    filtered = [x for x in residual_fields if '传感器4号' not in x[4]]
    for i, (board, col, idx, res, label) in enumerate(filtered):
        ax.plot(idx, res, lw=.55, color=COLORS[i % len(COLORS)], label=label)
    ax.set_ylabel('偏离5分钟局部中位基线 / hPa');ax.set_xlabel('设备RTC时间（北京时间）');ax.legend(ncol=3,fontsize=8,frameon=False)
parts.append(figure('模拟与数字压力去局部基线后的波动（不含传感器4号）', draw_res_no4, '仅隐藏传感器4号曲线，其他数字压力及模拟压力曲线保留；全通道原图与统计均未删除。'))

fit_rows = []
fit_fields = []
for board, _, sensors in FILES:
    d = boards[board]['data']
    for sid in sensors:
        analog = f'S{sid}_hPa'; digital = d[['I8', 'I9']].mean(axis=1)
        q = pd.DataFrame({'digital': digital, 'analog': d[analog]}).dropna()
        x, y = q.digital.to_numpy(), q.analog.to_numpy()
        r = np.corrcoef(x, y)[0, 1] if np.ptp(x) > 0 and np.ptp(y) > 0 else np.nan
        slope, intercept = np.polyfit(x, y, 1) if len(q) > 2 and np.ptp(x) > 0 else (np.nan, np.nan)
        fitted = slope*x + intercept
        residual = y - fitted
        fit_rows.append([board, f'传感器{sid}号', len(q), f(r, 4), f(slope, 4), f(intercept), f(np.mean(y-x)), f(np.sqrt(np.mean((y-x)**2))), f(np.sqrt(np.mean(residual**2)))])
        fit_fields.append((x, y, sid, r, slope, intercept))

def draw_scatter(fig, ax):
    for i, (x, y, sid, r, slope, intercept) in enumerate(fit_fields):
        ax.scatter(x, y, s=5, alpha=.18, color=COLORS[i], label=f'传感器{sid}号 r={r:.3f}')
    lo = min(min(x.min(), y.min()) for x, y, *_ in fit_fields); hi = max(max(x.max(), y.max()) for x, y, *_ in fit_fields)
    ax.plot([lo, hi], [lo, hi], color='#475569', lw=1, ls='--', label='y=x参考线')
    ax.set_xlabel('同板数字气压计均值 / hPa'); ax.set_ylabel('模拟压力厂家公式换算 / hPa'); ax.legend(ncol=3, fontsize=8, frameon=False)
parts.append(figure('模拟换算压力与同板数字气压计散点', draw_scatter, '所有样本点均保留，不对回归作校正；斜率、相关与误差见下表。', time_axis=False))
def draw_scatter_no4(fig, ax):
    selected = [v for v in fit_fields if v[2] != 4]
    for i, (x, y, sid, r, slope, intercept) in enumerate(selected):
        ax.scatter(x, y, s=5, alpha=.18, color=COLORS[i], label=f'传感器{sid}号 r={r:.3f}')
    lo=min(min(x.min(),y.min()) for x,y,*_ in selected);hi=max(max(x.max(),y.max()) for x,y,*_ in selected)
    ax.plot([lo,hi],[lo,hi],color='#475569',lw=1,ls='--',label='y=x参考线')
    ax.set_xlabel('同板数字气压计均值 / hPa');ax.set_ylabel('模拟压力厂家公式换算 / hPa');ax.legend(ncol=3,fontsize=8,frameon=False)
parts.append(figure('模拟换算压力与同板数字气压计散点（不含传感器4号）', draw_scatter_no4, '排除传感器4号，仅作为趋势对照；原始散点图、回归统计及所有样本数据仍保留。', time_axis=False))

def pair_stats(board, col, unit):
    q = paired[[f'{col}_b1', f'{col}_b2']].dropna()
    x, y = q[f'{col}_b1'].to_numpy(), q[f'{col}_b2'].to_numpy()
    return [board, len(q), f(y.mean()-x.mean()), f(np.sqrt(np.mean((y-x)**2))), f(np.corrcoef(x,y)[0,1]) if np.ptp(x)>0 and np.ptp(y)>0 else 'N/A', unit]

cross_rows = []
for col, unit, label in [('I14','V','电池电压'), ('I6','°C','温度'), ('I7','%RH','湿度'), ('I8','hPa','数字气压计1'), ('I9','hPa','数字气压计2')]:
    q = paired[[f'{col}_b1', f'{col}_b2']].dropna()
    if len(q):
        x,y=q[f'{col}_b1'].to_numpy(),q[f'{col}_b2'].to_numpy()
        cross_rows.append([label, len(q), f(np.mean(y-x)), f(np.sqrt(np.mean((y-x)**2))), f(np.corrcoef(x,y)[0,1]) if np.ptp(x)>0 and np.ptp(y)>0 else 'N/A', unit])
for label,c1,c2,unit in [('IN0滤波ADC码：传感器3−1','I10_b1','I10_b2','码'),('IN2滤波ADC码：传感器4−2','I12_b1','I12_b2','码'),('IN0换算压力：传感器3−1','S1_hPa','S3_hPa','hPa'),('IN2换算压力：传感器4−2','S2_hPa','S4_hPa','hPa')]:
    q=paired[[c1,c2]].dropna();x,y=q[c1].to_numpy(),q[c2].to_numpy()
    cross_rows.append([label,len(q),f(np.mean(y-x)),f(np.sqrt(np.mean((y-x)**2))),f(np.corrcoef(x,y)[0,1]) if np.ptp(x)>0 and np.ptp(y)>0 else 'N/A',unit])

stat_rows=[]; continuity=[]; daily=[]
for board, filename, sensors in FILES:
    d=boards[board]['data']; gaps=boards[board]['gaps']
    columns=[('电池电压','I14','V'),('温度','I6','°C'),('湿度','I7','%RH'),('气压计1','I8','hPa'),('气压计2','I9','hPa')]
    columns += [(f'传感器{sid}号 IN{ch}滤波ADC',col,'码') for sid,ch,col in [(sensors[0],0,'I10'),(sensors[1],2,'I12')]]
    columns += [(f'传感器{sid}号模拟压力',f'S{sid}_hPa','hPa') for sid in sensors]
    for label,col,unit in columns:
        z=d[col].dropna();stat_rows.append([board,label,unit,len(z),f(z.min()),f(z.max()),f(z.mean()),f(z.std()),f(z.max()-z.min())])
    continuity.append([board, len(d), d.index.min().strftime('%H:%M:%S'), d.index.max().strftime('%H:%M:%S'), f(gaps.median()), f(gaps.quantile(.95)), f(gaps.max()), int((gaps>11).sum())])
    for date, q in d.groupby(d.index.date):
        for label,col,unit in columns:
            z=q[col].dropna();daily.append([board,str(date),label,unit,len(z),f(z.mean()),f(z.min()),f(z.max()),f(z.std())])

range_rows=[]
for board, filename, sensors in FILES:
    d=boards[board]['data']
    range_rows.append([board,len(boards[board]['data']),boards[board]['outside'],len(d),d.index.min().strftime('%Y-%m-%d %H:%M:%S'),d.index.max().strftime('%Y-%m-%d %H:%M:%S'),
                       f(boards[board]['gaps'].median()),f(boards[board]['gaps'].quantile(.95)),f(boards[board]['gaps'].max()),str(boards[board]['invalid'])])

overlap_start=max(v['data'].index.min() for v in boards.values());overlap_end=min(v['data'].index.max() for v in boards.values())
header=f'<header><p>2026年11月4日—5日 · 5V BOOST长期静态数据</p><h1>两块板ADC与压力传感器长期对比报告</h1><p>分析区间：{START:%Y-%m-%d %H:%M:%S} 至 {END:%Y-%m-%d %H:%M:%S}（北京时间，按设备RTC）</p><p>1号板、2号板各 {len(boards["1号板"]["data"]):,} 条区间内记录；记录约每10秒一条。设备时间轴重叠区间：{overlap_start:%H:%M:%S}—{overlap_end:%H:%M:%S}。</p></header>'
body=header+'<nav><a href="#summary">结论</a><a href="#method">方法</a><a href="#charts">图表</a><a href="#stats">统计</a><a href="#quality">数据质量</a></nav>'
summary_rows=[]
for board, filename, sensors in FILES:
 d=boards[board]['data']
 for sid,ch in zip(sensors,[0,2]):
  analog=f'S{sid}_hPa'; digital=d[['I8','I9']].mean(axis=1); q=pd.DataFrame({'x':digital,'y':d[analog]}).dropna(); r=np.corrcoef(q.x,q.y)[0,1] if q.x.nunique()>1 and q.y.nunique()>1 else np.nan
  summary_rows.append([board,f'传感器{sid}号 IN{ch}',f(d[analog].mean()),f(digital.mean()),f(r,4),f((d[analog]-digital).mean()),f(d[analog].max()-d[analog].min()),f(d[analog].std())])

body += '<section id="summary"><h2>主要结果</h2><p class="notice"><b>本报告的传感器映射：</b>1号板IN0=传感器1号，IN2=传感器2号；2号板IN0=传感器4号，IN2=传感器3号。图例、表格和统计均按此映射生成。</p><p>本报告只使用两份指定日志中11月4日13:00至11月5日08:25（含端点）的记录。时序图按实际RTC时间绘制，统一绝对横坐标；不降采样、不插值、不平滑原始保存值。</p>'+table(summary_rows,['板号','模拟通道','模拟换算均值hPa','数字气压均值hPa','相关系数r','模拟−数字均值hPa','模拟峰峰值hPa','模拟标准差hPa'])+'<p>相关系数只表示模拟压力是否跟随数字压力的整体趋势，不能单独判断噪声大小。传感器3号相关系数最低，是因为它的曲线较平、没有跟随这段数字压力变化；传感器4号蓝线相关系数较高，但峰峰值和短时波动更大，异常应通过峰峰值、标准差和下方局部残差指标判断。模拟压力采用厂家线性表格换算，结果依赖传感器供电/校准条件；相关性不等同于绝对准确度。</p></section>'
body += f'<section id="method"><h2>数据与计算口径</h2><p>用户指定区间为2026-11-04 13:00:00至2026-11-05 08:25:00，首尾均包含。CSV原始行按倒序排列，先根据年、月、日、时、分、秒字段解析，再按时间升序排序。范围外行不参与分析。两份日志每条时间间隔中位数约10秒，时序图用实际时间戳，未把点强行插值到同一秒。</p><p><b>字段映射：</b>I0～I5为设备RTC年月日时分秒；I6温度；I7湿度；I8/I9为本板两个数字气压计；I10～I13为IN0～IN3的Flash滤波ADC码；I14为电池电压。1号板：IN0=传感器1号、IN2=传感器2号；2号板：IN0=传感器4号、IN2=传感器3号。</p><p></p><p>本次为5V BOOST组，按AD7682 REF=5.0V计算等效输入电压V=ADC×5/65536。模拟压力按IPNZAA135KP-4428-A1表中P(kPa)=22.5+25V换算为hPa，即P=225+1250×ADC/65536。其为厂家曲线的线性插值/外推结果；如果传感器供电、器件型号或REF与该标定条件不同，绝对压力会有系统误差。数字压力使用日志中的设备读数，不作校正。</p><p>短时波动以居中5分钟滚动中位数为局部基线，残差RMS与中央95%宽度描述短时偏离。每分钟峰峰幅值至少需要4条有效记录。滤波ADC码来自Flash保存值，本报告不能恢复ADC的滤波前单次转换。板间统计使用最近邻时间配对，最大时差5秒，不插值；趋势图保留各自原始时间戳。不同板上的传感器为不同个体，板间差异包含传感器个体差异。</p></section>'
body += '<section id="charts"><h2>全区间时间曲线与比较</h2>'+''.join(parts)+'</section>'
body += '<section id="stats"><h2>模拟与数字压力短时波动</h2><p>每条记录保留，计算中不删除压力尖峰。由于采样约10秒一次，分钟峰峰值基于每分钟至少4点。</p>'+table(noise_rows,['测量项','有效点数','分钟峰峰中位hPa','分钟峰峰P95 hPa','5分钟局部基线残差RMS hPa','残差中央95%宽hPa','相邻点变化P95 hPa','全段标准差hPa','全段峰峰值hPa'])+'<h2>分通道全区间统计</h2>'+table(stat_rows,['板号','测量项','单位','有效点数','最小','最大','均值','标准差','峰峰值'])+'<h2>同板模拟与数字气压关系</h2>'+table(fit_rows,['板号','模拟传感器','匹配点数','相关系数r','线性斜率','截距hPa','模拟−数字均差hPa','直接RMSE hPa','拟合残差RMSE hPa'])+'<p>线性拟合只描述本次环境压力变化范围内的关系，不代表厂家全量程线性度，也不用于修正数据。</p><h2>板间近同时刻差异</h2>'+table(cross_rows,['测量项','最近邻配对数','2号−1号均差','板间差RMSE','相关系数r','单位'])+'</section>'
body += '<section id="quality"><h2>日统计与记录质量</h2>'+table(daily,['板号','日期','测量项','单位','有效点数','均值','最小','最大','标准差'])+'<h3>区间覆盖和采样连续性</h3>'+table(range_rows,['板号','区间记录数','区间外行数','保留记录数','首记录','末记录','间隔中位数s','间隔P95s','最大间隔s','各字段无效数量'])+'<p>无效数按字段分别统计，非ADC尖峰剔除规则。源CSV保持不变。原始有效记录全部用于曲线及描述性统计。</p><h3>文件追溯</h3>'+table(source_audit,['板号','CSV','源文件行数','区间内行数','区间外行数','实际首条','实际末条','间隔中位s','最大间隔s','无效数','SHA-256'])+'</section><footer>静态全数据报告 · 生成于2026-11-05 · 源CSV未修改</footer>'

css='''*{box-sizing:border-box}body{margin:0;background:#f2f6fa;color:#24384c;font:15px/1.8 "Microsoft YaHei",Arial}header,section,nav,footer{max-width:1400px;margin:auto}header{background:linear-gradient(120deg,#173d59,#26706a);padding:38px;color:white;border-radius:0 0 14px 14px}h1{font-size:30px}h2{color:#20536b;font-size:23px;margin-top:12px}h3{font-size:18px}section{padding:26px 32px;background:white;border-radius:9px;margin:20px auto;border:1px solid #e0e8ef}nav{padding:16px;display:flex;gap:22px;flex-wrap:wrap}a{color:#146d97}article{border:1px solid #e0e7ee;border-radius:8px;padding:13px;margin:17px 0;break-inside:avoid}article p{font-size:13px;color:#647b8e}img{width:100%;height:auto}.tablewrap{overflow-x:auto;margin:15px 0}table{border-collapse:collapse;width:100%;font-size:13px}th,td{padding:8px 10px;border-bottom:1px solid #e1e8ef;white-space:nowrap;text-align:right}th{background:#edf4f7}th:first-child,td:first-child{text-align:left}footer{padding:22px;color:#718391}@media print{nav{display:none}article{break-inside:avoid}section{border:0}header{print-color-adjust:exact}}'''
report=OUT/'两块板5VBOOST长期测试报告_1104-1105.html'
report.write_text('<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>两块板5V BOOST长期测试报告</title><style>'+css+'</style></head><body>'+body+'</body></html>',encoding='utf-8')
metrics=[]
for board, filename, sensors in FILES:
 d=boards[board]['data']
 for label,col in [('battery_v','I14'),('temperature_c','I6'),('humidity_rh','I7'),('digital1_hpa','I8'),('digital2_hpa','I9'),('adc_in0_code','I10'),('adc_in2_code','I12')]+[(f'sensor{sid}_derived_hpa',f'S{sid}_hPa') for sid in sensors]:
  z=d[col].dropna();metrics.append(dict(board=board,metric=label,n=len(z),min=float(z.min()),max=float(z.max()),mean=float(z.mean()),std=float(z.std()),p2p=float(z.max()-z.min())))
audit={'start_inclusive':str(START),'end_inclusive':str(END),'reference_v':VREF,'adc_saved_values':'filtered','timestamp_sort':'ascending; source is reverse chronological','downsampled':False,'interpolated':False,'nearest_pair_tolerance_s':5,'nearest_pair_count':len(paired),'nearest_pair_abs_lag_median_s':float(lag_seconds.abs().median()),'nearest_pair_abs_lag_max_s':float(lag_seconds.abs().max()),'charts':figures,'sources':source_audit,'metrics':metrics,'noise':noise_rows,'sensor_pressure_fits':fit_rows}
(OUT/'分析核验.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'report':str(report),'charts':len(figures),'paired':len(paired),'lag_median':audit['nearest_pair_abs_lag_median_s'],'lag_max':audit['nearest_pair_abs_lag_max_s'],'sample_ranges':{k:[str(v['data'].index.min()),str(v['data'].index.max()),len(v['data'])] for k,v in boards.items()}},ensure_ascii=False,indent=2))
