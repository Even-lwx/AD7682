from pathlib import Path
import numpy as np, json,base64
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.font_manager import FontProperties
R=Path('D:/lwx/lwx-project/AD7682');O=R/'output/reports/ADC滤波对比_10301231';O.mkdir(parents=True,exist_ok=True)
v=np.loadtxt(R/'code/log/10301231.csv',delimiter=',',skiprows=1)
plt.rcParams.update({'font.family':FontProperties(fname='C:/Windows/Fonts/msyh.ttc').get_name(),'axes.unicode_minus':False})
# This CSV only captures a subset of the 100Hz samples. Keep comparisons in
# row-index units; never reconstruct unrecorded conversions or exact timing.
def model(x,tau,clip=True):
 out=[];s1=s2=float(np.median(x[:min(31,len(x))]));alpha=1-np.exp(-1/tau)
 for i,z in enumerate(x):
  w=x[max(0,i-100):i+1];med=np.median(w);sigma=1.4826*np.median(abs(w-med))
  # Local raw-data MAD clipping, not a hard gate around the previous output.
  if clip:z=np.clip(z,med-max(4*sigma,8),med+max(4*sigma,8))
  s1+=alpha*(z-s1);s2+=alpha*(s1-s2);out.append(s2)
 return np.array(out)
stats=[];candidates=[]
for c in range(4):
 x=v[:,14+c];current=v[:,7+c];fast=model(x,50);slow=model(x,100);candidates.append(slow)
 for name,y in [('原始快照',x),('当前固件',current),('MAD限幅+两级低通50行',fast),('MAD限幅+两级低通100行',slow)]:
  # Use after initialization only for comparative roughness.
  d=np.diff(y[300:]);stats.append(dict(channel=c,model=name,step95=float(np.percentile(abs(d),95)),maxstep=float(max(abs(d))),start=float(np.median(y[300:350])),end=float(np.median(y[-50:]))))
 fig,ax=plt.subplots(2,1,figsize=(15,8),sharex=True)
 ax[0].scatter(np.arange(len(x)),x,s=3,alpha=.3,label='原始值快照');ax[0].plot(current,lw=1,label='当前滤波');ax[0].plot(slow,lw=1.5,label='候选：MAD限幅+两级低通（100行）');ax[0].legend(ncol=3)
 # Close-up covers the sharpest existing-filter transition, with preceding history.
 k=int(np.argmax(abs(np.diff(current))))+1;left=max(0,k-250);right=min(len(x),k+400)
 for y,label in [(current,'当前滤波'),(fast,'候选50行'),(slow,'候选100行')]:ax[1].plot(np.arange(left,right),y[left:right],label=label)
 ax[1].legend();ax[1].set_xlim(left,right);ax[1].set_xlabel('CSV记录序号（不是100Hz转换序号）')
 for a in ax:a.set_ylabel('ADC codes');a.grid(alpha=.2)
 ax[0].set_title(f'IN{c}：全程');ax[1].set_title(f'最大滤波跳变附近，第{k}行；局部图纵轴独立')
 fig.tight_layout();fig.savefig(O/f'IN{c}.png',dpi=145,bbox_inches='tight');plt.close(fig)
parts=['''<!doctype html><meta charset="utf-8"><title>ADC滤波模型对比</title><style>body{max-width:1400px;margin:30px auto;padding:20px;font:16px/1.7 Microsoft YaHei;color:#24334b}img{width:100%}table{border-collapse:collapse;width:100%;font-size:14px}td,th{border:1px solid #ccd6e2;padding:8px}.note{background:#edf5fc;padding:18px}</style><h1>10301231：ADC原始值与滤波基线对比</h1><p class="note">4255条串口快照，I7～I10为IN0～IN3现有滤波，I14～I17为原始值快照。没有逐行时间戳，也没有完整100Hz转换序列。本页候选模型仅为CSV逐行因果重放，不能当作同样参数在固件100Hz下的实测效果；横轴保留记录序号。固件未修改。</p>
<h2>建议模型：稳健预处理＋两级低通基线跟踪</h2><p>先以过去约1秒窗口的中位数和MAD检测偏离，按局部波动尺度限制孤立离群值，再串联两个一阶低通。两级均用 y←y+α(x−y)，α=1−exp(−Δt/τ)。实际100Hz固件可先选每级τ=1秒，需更稳则2秒。两级低频延迟约2τ，真实阶跃达到90%约3.9τ，不能既极稳又零延迟。</p><p>候选50行/100行的时间常数分别为50/100条CSV记录；如果串口实际50Hz，才近似每级1/2秒。当前串口读取、发送和刷屏会影响行间隔，因此该时间换算仅是估计。初始化使用前31条中值（启动预热），统计剔除前300行。</p>
<h2>数据揭示的问题</h2><p>IN0、IN2存在慢漂移；IN1有明显较长时间的偏移。100点中位数可以去掉不足半窗的短尖峰，却不能去掉占窗口多数的偏移。低通能让边沿平滑，但持续偏移最终仍会被跟随。没有独立参考信号，无法断言这段偏移应被删除，也不能把候选输出叫作真实值。</p><h2>平滑程度比较</h2><p>相邻变化指标只衡量抖动，不衡量准确度。用记录端原始快照拟合候选，与使用完整100Hz数据的当前固件不是严格同输入实验。</p><table><tr><th>通道</th><th>模型</th><th>相邻差绝对值P95 / codes</th><th>最大相邻差 / codes</th></tr>''']
for s in stats:parts.append(f'<tr><td>IN{s["channel"]}</td><td>{s["model"]}</td><td>{s["step95"]:.4f}</td><td>{s["maxstep"]:.4f}</td></tr>')
parts.append('</table>')
for c in range(4):parts.append(f'<h2>IN{c}</h2><img src="data:image/png;base64,'+base64.b64encode((O/f'IN{c}.png').read_bytes()).decode()+'">')
parts.append('<h2>落地建议</h2><p>优先在当前中位数预处理后增加两级低通（每级τ=1～2秒），替代25点均值尾级；四路采用同一结构、分别保存状态，保留浮点输出，并让串口/OLED/Flash继续共用结果。若目标是排除数秒甚至更长的平台偏移，需要先确认传感器允许的真实变化速度，再设计异常持续时间与跟踪规则。固定斜率限幅、永久冻结或只取低值容易掩盖真实变化，不宜作为默认模型。</p>')
(O/'ADC滤波模型对比.html').write_text('\n'.join(parts),encoding='utf-8')
(O/'指标.json').write_text(json.dumps(stats,ensure_ascii=False,indent=2),encoding='utf-8')
np.savetxt(O/'候选基线.csv',np.column_stack([np.arange(len(v)),v[:,7:11],np.array(candidates).T]),delimiter=',',header='row,current_IN0,current_IN1,current_IN2,current_IN3,candidate_IN0,candidate_IN1,candidate_IN2,candidate_IN3',comments='',fmt='%.6f')
print(json.dumps(stats,ensure_ascii=True))
