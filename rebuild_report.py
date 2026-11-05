import pandas as pd, numpy as np, matplotlib.pyplot as plt, base64, io, json
from pathlib import Path
from datetime import datetime

root=Path(r'D:/lwx/lwx-project/AD7682')
csvp=root/'code/log/10301415_针筒第一次测试.csv'
out=root/'output/针筒第一次测试_10301415'
out.mkdir(parents=True, exist_ok=True)
cols=[f'I{i}' for i in range(15)]
df=pd.read_csv(csvp)
df.columns=cols
df['time']=pd.to_datetime(dict(year=df.I0.astype(int),month=df.I1.astype(int),day=df.I2.astype(int),hour=df.I3.astype(int),minute=df.I4.astype(int),second=df.I5.astype(int)))
df=df.sort_values('time').set_index('time')
start=pd.Timestamp('2026-10-30 14:00:00'); end=pd.Timestamp('2026-10-30 14:12:11')
d=df.loc[(df.index>=start)&(df.index<=end)].copy()
# 数据列与显示名称
names={'I6':'温度 (°C)','I7':'湿度 (%)','I8':'U7数字气压 (hPa)','I9':'U11数字气压 (hPa)','I10':'IN0模拟气压 (ADC码)','I12':'IN2模拟气压 (ADC码)','I14':'电池电压 (V)'}
plt.rcParams['font.sans-serif']=['Microsoft YaHei','SimHei','DejaVu Sans']; plt.rcParams['axes.unicode_minus']=False

def savefig(fig, name):
    """保存统计图并关闭图对象，返回图片的 Base64 文本。

    参数：
    fig：待保存的 matplotlib 图对象。
    name：任务名或待提取函数名。

    返回：提取或格式化后的文本。"""
    p=out/name; fig.savefig(p,dpi=160,bbox_inches='tight'); plt.close(fig)
    return base64.b64encode(p.read_bytes()).decode()
# 第一部分：压力数据对齐
fig,axs=plt.subplots(2,1,figsize=(15,9),sharex=True,gridspec_kw={'height_ratios':[1.2,1]})
ax=axs[0]
for c,label,color in [('I8','U7数字气压','tab:blue'),('I9','U11数字气压','tab:orange')]: ax.plot(d.index,d[c],lw=1.3,label=label,color=color)
ax.set_ylabel('压力 (hPa)'); ax.set_title('数字气压计与模拟气压通道同一时间轴'); ax.grid(alpha=.25); ax.legend()
ax2=axs[1]
for c,label,color in [('I10','IN0模拟气压','tab:green'),('I12','IN2模拟气压','tab:red')]: ax2.plot(d.index,d[c],lw=1.0,label=label,color=color)
ax2.set_ylabel('ADC码'); ax2.set_xlabel('时间'); ax2.grid(alpha=.25); ax2.legend()
for a in axs:
  for x in [pd.Timestamp('2026-10-30 14:11:10'),pd.Timestamp('2026-10-30 14:12:11')]: a.axvline(x,color='k',ls='--',lw=.8,alpha=.6)
fig.autofmt_xdate(); p1=savefig(fig,'裁剪区间_四路气压对比.png')
# 第二部分：其他传感器
fig,axs=plt.subplots(2,1,figsize=(15,7),sharex=True)
axs[0].plot(d.index,d.I6,label='温度',color='tab:red'); axs[0].set_ylabel('°C'); axs[0].grid(alpha=.25); axs[0].legend()
axs[1].plot(d.index,d.I7,label='湿度',color='tab:purple'); axs[1].set_ylabel('%'); axs[1].set_xlabel('时间'); axs[1].grid(alpha=.25); axs[1].legend(); fig.autofmt_xdate(); p2=savefig(fig,'裁剪区间_温湿度.png')
# 第三部分：正式测试周期局部放大
cycles=[('第1次',pd.Timestamp('2026-10-30 14:06:39'),pd.Timestamp('2026-10-30 14:06:47')),('第2次',pd.Timestamp('2026-10-30 14:07:55'),pd.Timestamp('2026-10-30 14:08:09')),('第3次',pd.Timestamp('2026-10-30 14:09:11'),pd.Timestamp('2026-10-30 14:09:37'))]
fig,axs=plt.subplots(3,1,figsize=(15,10),sharey=False)
for ax,(lab,a,b) in zip(axs,cycles):
  q=d.loc[(d.index>=a-pd.Timedelta(seconds=10))&(d.index<=b+pd.Timedelta(seconds=10))]
  ax.plot(q.index,q.I8,label='U7',lw=1.5); ax.plot(q.index,q.I9,label='U11',lw=1.2); ax.set_title(f'{lab}：{a.time()}–{b.time()}'); ax.grid(alpha=.25); ax.legend(ncol=2)
fig.autofmt_xdate(); p3=savefig(fig,'正式三次循环放大.png')
# 各周期统计指标
metric_rows=[]
for lab,a,b in cycles:
 q=d.loc[(d.index>=a)&(d.index<=b)]
 row={'循环':lab,'开始':a.strftime('%H:%M:%S'),'结束':b.strftime('%H:%M:%S'),'持续(s)':round((b-a).total_seconds(),1)}
 for c in ['I8','I9','I10','I12']:
  row[c+'均值']=float(q[c].mean()); row[c+'峰值']=float(q[c].max()); row[c+'谷值']=float(q[c].min())
 # 标准化后计算相关性，避免量纲影响比较
 for dc in ['I8','I9']:
  for ac in ['I10','I12']:
   x=q[dc].to_numpy(); y=q[ac].to_numpy();
   if len(q)>1 and np.std(x)>0 and np.std(y)>0:
    r=float(np.corrcoef(x,y)[0,1]); coef=np.polyfit(x,y,1); pred=coef[0]*x+coef[1]; rmse=float(np.sqrt(np.mean((y-pred)**2)))
   else:r=coef[0]=rmse=float('nan')
   row[f'{dc}-{ac} r']=r; row[f'{dc}-{ac} R²']=r*r; row[f'{dc}-{ac} RMSE']=rmse
 metric_rows.append(row)
# HTML 页面

def fmt(v):
 """格式化报告单元格，返回三位小数文本或空串。"""
 if pd.isna(v): return ''
 if isinstance(v,(float,np.floating)): return f'{v:.3f}'
 return str(v)
def table(rows, keys):
 """把统计数据转换为报告中的 HTML 表格。

 参数：
 rows：测试输入，约束见调用处及断言。
 keys：测试输入，约束见调用处及断言。

 返回：提取或格式化后的文本。"""
 h='<table><tr>'+''.join(f'<th>{k}</th>' for k in keys)+'</tr>'
 for r in rows: h+='<tr>'+''.join(f'<td>{fmt(r.get(k,""))}</td>' for k in keys)+'</tr>'
 return h+'</table>'
keys=['循环','开始','结束','持续(s)']+[c+s for c in ['I8','I9','I10','I12'] for s in ['均值','峰值','谷值']]
cor_keys=['循环']+[f'{d}-{a} {m}' for d in ['I8','I9'] for a in ['I10','I12'] for m in ['r','R²','RMSE']]
# 可见裁剪数据的统计结果
stats=[]
for c,label in names.items(): stats.append({'项目':label,'最小值':d[c].min(),'最大值':d[c].max(),'平均值':d[c].mean(),'标准差':d[c].std()})
html=f'''<!doctype html><meta charset="utf-8"><title>针筒第一次测试分析（裁剪区间）</title><style>body{{max-width:1500px;margin:25px auto;padding:20px;font:15px/1.7 Microsoft YaHei;color:#24334b}}h1{{font-size:28px}}h2{{margin-top:35px;border-bottom:2px solid #dbe5ef}}table{{border-collapse:collapse;width:100%;font-size:12px;display:block;overflow:auto}}td,th{{border:1px solid #ccd6e2;padding:6px;white-space:nowrap}}th{{background:#edf2f7}}.note{{background:#eef6fc;padding:15px;border-left:4px solid #3975ad}}.warn{{background:#fff7e6;padding:12px;border-left:4px solid #d18b00}}img{{width:100%}}</style>
<h1>10301415 针筒第一次测试数据分析</h1>
<div class="note">本报告展示范围已严格限制为 <b>2026-10-30 14:00:00～14:12:11</b>（含边界）。14:00:00 之前和 14:12:11 之后的数据不参与图表、统计和表格；原始 CSV 未修改。共展示 {len(d)} 条记录，时间间隔约 1 秒。</div>
<div class="warn">通道映射：U7/U11 为数字气压计；IN0（I10）和 IN2（I12）为模拟气压计 ADC 码值。模拟通道尚未提供 ADC 码值到 hPa 的标定关系，因此与数字气压计的横向比较同时采用同时间轴、相关系数和线性拟合指标，不能直接把 ADC 码值当作 hPa。</div>
<h2>裁剪区间内四路气压对比</h2><img src="data:image/png;base64,{p1}">
<h2>正式三次高压循环</h2><p>正式循环按用户指定取最后三次：14:06:39–14:06:47、14:07:55–14:08:09、14:09:11–14:09:37。14:05 左右的前置高压片段不列入正式循环。</p><img src="data:image/png;base64,{p3}">
<h3>每次循环的峰值、谷值和均值</h3>{table(metric_rows,keys)}
<h3>数字气压与模拟气压的同步性</h3><p>r 越接近 1 或 -1 表示变化越同步；R² 越接近 1 表示线性关系越强；RMSE 是 ADC 码值拟合误差，仅用于同一模拟通道内部比较。</p>{table(metric_rows,cor_keys)}
<h2>温湿度（同一裁剪时间轴）</h2><img src="data:image/png;base64,{p2}">
<h2>裁剪区间统计</h2>{table(stats,['项目','最小值','最大值','平均值','标准差'])}
<h2>时间节点</h2>{table([{'节点':'分析开始','时间':'14:00:00','说明':'开始显示和统计'},{'节点':'正式第1次循环','时间':'14:06:39–14:06:47','说明':'高压平台'},{'节点':'正式第2次循环','时间':'14:07:55–14:08:09','说明':'高压平台'},{'节点':'正式第3次循环','时间':'14:09:11–14:09:37','说明':'高压平台'},{'节点':'三次循环结束','时间':'14:11:10','说明':'结束标记'},{'节点':'取出针筒','时间':'14:12:11','说明':'显示和统计截止'}],['节点','时间','说明'])}
'''
(out/'针筒第一次测试分析.html').write_text(html,encoding='utf-8')
print('written',len(d),out/'针筒第一次测试分析.html')
