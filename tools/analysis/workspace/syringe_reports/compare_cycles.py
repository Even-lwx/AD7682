from pathlib import Path
import json,html,base64,io
import pandas as pd,numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path(r'D:/lwx/lwx-project/AD7682');out=root/'output/reports/针筒实验报告_20261103'
plt.rcParams.update({'font.sans-serif':['Microsoft YaHei'],'axes.unicode_minus':False,'path.simplify':False})
configs=[('5V',1,'10301415_针筒第一次测试.csv','2026-10-30 14:00:00','2026-10-30 14:12:11',5),('5V',2,'10301655_2号板针筒第一次测试.csv','2026-10-30 16:44:40','2026-10-30 16:52:47',5),('2.5V',1,'2611031033_1号板2V5电压循环测试.csv','2026-11-03 10:23:56','2026-11-03 10:31:19',2.5),('2.5V',2,'2611031047_2号板2V5电压循环测试.csv','2026-11-03 10:39:05','2026-11-03 10:44:39',2.5)]
data={};rows=[];audit=[]
for batch,board,fn,start,end,ref in configs:
 d=pd.read_csv(root/'code/log'/fn);t=pd.to_datetime(dict(year=d.I0.astype(int),month=d.I1.astype(int),day=d.I2.astype(int),hour=d.I3.astype(int),minute=d.I4.astype(int),second=d.I5.astype(int)));d.index=t;d=d.sort_index().loc[start:end];d=d.groupby(level=0).mean(numeric_only=True)
 for c,sid in [('I10',board*2-1),('I12',board*2)]:
  q=d[['I8',c]].dropna();data[(batch,sid)]=q
  for scope,z in [('全实验',q),('共同压力范围850～1200hPa',q[q.I8.between(850,1200)])]:
   x=z.I8.to_numpy();y=z[c].to_numpy();slope,inter=np.polyfit(x,y,1);r=np.corrcoef(x,y)[0,1];res=y-(slope*x+inter);rmse=np.sqrt(np.mean(res**2));r2=r*r
   row=[sid,batch,scope,len(z),f'{r:.5f}',f'{r2:.5f}',f'{slope:.2f}',f'{rmse:.2f}',f'{rmse/abs(slope):.2f}',f'{z[c].max():.1f}',int((z[c]>=.99*65536).sum())];rows.append(row)
   audit.append(dict(sensor=sid,batch=batch,scope=scope,r=r,r2=r2,codes_per_hpa=slope,fit_residual_codes=rmse,input_equivalent_residual_hpa=rmse/abs(slope)))
def table(rs,heads):return '<div class="tablewrap"><table><tr>'+''.join('<th>'+html.escape(str(x))+'</th>' for x in heads)+'</tr>'+''.join('<tr>'+''.join('<td>'+html.escape(str(x))+'</td>' for x in r)+'</tr>' for r in rs)+'</table></div>'
parts=['<header><h1>5V与2.5V针筒循环测试对比</h1><p>2026-10-30 与 2026-11-03 · 传感器1～4号</p></header><section><h2>结论</h2><p><b>本次2.5V条件下的动态跟随和高压端线性明显弱于上周5V条件。</b>此判断直接来自日志ADC码与同板数字气压计1号的关系，不依赖厂家电压—压力公式。5V组全程相关系数约0.9996～0.9999；2.5V组约0.926～0.954。四个传感器本次都在高压段接近满量程并形成顶端平台，继续加压时ADC变化被压缩，低通滤波无法恢复丢失的高压信息。</p><p>两批参考电压、传感器供电条件不同，操作速度、压力分布及当时固件也可能不同。本比较说明两批记录的系统表现差异，不是单独控制供电的因果试验。不能只凭码值判断限制来自ADC输入范围还是传感器输出级。</p><h2>统一口径</h2><p>全实验结果用于展示实际测试表现；另在两批共同覆盖的850～1200hPa范围重算，避开LPS22HH标称上限1260hPa以上数据。共同范围仍包含动态转换、滤波时延，且样本压力分布不同，不能等同于静态精度。参照为各板气压计1号；同秒多条取均值，不插值。日志ADC已经过当时固件处理。</p><p>拟合直接使用ADC=a×数字气压+b；拟合残差先用ADC码表示，再除以斜率得到输入等效残差hPa，只用于对比拟合偏离程度，绝非厂家公式换算或压力准确度。99%满量程是诊断阈值，不是证明发生硬件饱和。</p>',table(rows,['传感器号','条件','范围','有效秒','r','R²','斜率码/hPa','拟合残差RMS码','输入等效残差hPa','最高ADC码','≥99%满量程秒']),'</section>']
for sid in range(1,5):
 fig,ax=plt.subplots(figsize=(12,5))
 for batch,color in [('5V','#1769aa'),('2.5V','#d48124')]:
  q=data[(batch,sid)];ax.scatter(q.I8,q.iloc[:,1],s=10,alpha=.5,color=color,label=batch)
 ax.set_xlabel('同板气压计1号 / hPa');ax.set_ylabel('日志ADC码');ax.ticklabel_format(axis='y',style='plain',useOffset=False);ax.axhline(.99*65536,ls='--',c='#8a96a6',lw=1,label='99%满量程提示');ax.axvspan(1260,1400,color='#ddd',alpha=.25,label='数字气压超标称量程');ax.set_xlim(750,1400);ax.set_ylim(28000,66500);ax.grid(alpha=.2);ax.legend();ax.set_title(f'传感器{sid}号：ADC对数字气压直接响应');fig.tight_layout();buf=io.BytesIO();fig.savefig(buf,format='png',dpi=150);plt.close(fig);im=base64.b64encode(buf.getvalue()).decode();parts.append(f'<section><h2>传感器{sid}号</h2><img src="data:image/png;base64,{im}" alt="传感器{sid}号ADC响应对比"></section>')
parts.append('<section><h2>对滤波与下一步测试的意义</h2><p>5V组仍有零点偏差与动态残差，不能将强相关等同于绝对准确。但它的ADC—数字气压关系接近直线，而本次2.5V组出现明显顶端平台，因此当前优先级应是核实传感器允许供电范围、测量实际输出与REF，解决量程或输出级压缩。确认满量程与标定后，再调滤波平衡抖动和响应速度。</p><p>本次2.5V供电下原厂家公式的适用性未确认，未把约250hPa的条件换算偏差作为性能结论。高于1260hPa的数字读数仅作探索性参照。</p><h2>来源与范围</h2>'+table([[b,n,fn,s,e] for b,n,fn,s,e,v in configs],['条件','板号','文件','起点','终点'])+'</section>')
css='body{background:#f3f6fa;color:#243348;font:15px/1.8 "Microsoft YaHei";margin:0}header,section{max-width:1300px;margin:20px auto;padding:28px;background:white;border-radius:12px}header{background:#173f5e;color:white}img{width:100%}.tablewrap{overflow-x:auto}table{border-collapse:collapse;width:100%;font-size:13px}th,td{padding:9px;border-bottom:1px solid #ddd;white-space:nowrap;text-align:right}th{background:#edf3f8}h2{color:#173f5e}'
p=out/'5V与2V5_针筒循环性能对比.html';p.write_text('<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>5V与2.5V针筒循环性能对比</title><style>'+css+'</style>'+''.join(parts)+'</html>',encoding='utf8');(out/'循环性能对比核验.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2),encoding='utf8');print(json.dumps(audit,ensure_ascii=False));print(p)
