from pathlib import Path
import html,json,hashlib,io,base64
import numpy as np,pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
ROOT=Path(r'D:/lwx/lwx-project/AD7682');OUT=ROOT/'output/reports/三种供电ADC纹波对比_20261103';OUT.mkdir(parents=True,exist_ok=True)
plt.rcParams.update({'font.sans-serif':['Microsoft YaHei'],'axes.unicode_minus':False,'path.simplify':False,'font.size':10})
FILES=[(1,'2V5','1103-1号板2V5.csv',2.5),(1,'5V BOOST','1103-1号板5VBOOST.csv',5),(1,'5V LDO','1103-1号板5VLDO.csv',5),(2,'2V5','1103-2号板2V5.csv',2.5),(2,'5V BOOST','1103-2号板5VBOOST.csv',5),(2,'5V LDO 首次','1103-2号板5VLDO.csv',5),(2,'5V LDO 第二次','1103-2号板5VLDO_第二次.csv',5)]
FILES.append((2,'5V BOOST 第二次','1103-2号板5VBOOST_第二次.csv',5))
FILES.append((2,'5V BOOST 第三次','1103-2号板5VBOOST_第三次.csv',5))
FILES.append((2,'5V BOOST 第四次','1103-2号板5VBOOST_第4次.csv',5))
COLORS=['#168579','#d48124','#1769aa','#a150a8','#c63d53','#475b95','#795a32']; loaded=[];audit=[];rows=[];metrics=[];images=[]
def fmt(x):return f'{x:.3f}' if np.isfinite(x) else 'N/A'
def table(rs,heads):return '<div class="tablewrap"><table><thead><tr>'+''.join('<th>'+html.escape(str(x))+'</th>' for x in heads)+'</tr></thead><tbody>'+''.join('<tr>'+''.join('<td>'+html.escape(str(x))+'</td>' for x in r)+'</tr>' for r in rs)+'</tbody></table></div>'
def addfig(title,fig,note=''):
 fig.tight_layout();buf=io.BytesIO();fig.savefig(buf,format='png',dpi=160);plt.close(fig);im=base64.b64encode(buf.getvalue()).decode();images.append(title)
 if len(images)==1:(OUT/'图表核验.png').write_bytes(buf.getvalue())
 return f'<article><h3>{title}</h3><p>{note}</p><img src="data:image/png;base64,{im}" alt="{title}" loading="lazy"></article>'
for board,mode,fn,ref in FILES:
 path=ROOT/'code/log'/fn;source=pd.read_csv(path);assert source.columns.tolist()==[f'I{i}' for i in range(18)]
 # Structural validation, unrelated to ADC spike amplitude: retain actual outliers.
 good=source.I0.between(0,20)&source.I1.between(-40,125)&source.I2.between(0,100)&source.I3.between(260,1260)&source.I5.between(260,1260)
 d=source.loc[good].copy()
 invalid={}
 for col in ['I7','I9','I14','I16']:
  bad=~np.isfinite(d[col])|~d[col].between(0,65535);invalid[col]=int(bad.sum());d.loc[bad,col]=np.nan
  if col in ['I14','I16']:assert d[col].dropna().mod(1).eq(0).all()
 obj=dict(board=board,mode=mode,file=fn,ref=ref,d=d);loaded.append(obj)
 audit.append([board,mode,len(source),len(d),','.join(str(i+2) for i in source.index[~good]) or '无',str(invalid),hashlib.sha256(path.read_bytes()).hexdigest()])
for obj in loaded:
 board,mode,ref,d=[obj[k] for k in ['board','mode','ref','d']]
 for ch,rawcol,filtcol,sid in [(0,'I14','I7',board*2-1),(2,'I16','I9',board*2)]:
  for scope,seg in [('全段',d),('末5000条',d.tail(5000))]:
   raw=seg[rawcol].reset_index(drop=True);filtered=seg[filtcol].reset_index(drop=True);scale=ref*1000/65536
   base=raw.rolling(501,center=True,min_periods=250).median();res=(raw-base).dropna();mv=res*scale
   # Strict non-overlapping blocks, ignore only incomplete terminal block.
   spans=[]
   for i in range(0,len(raw)-199,200):spans.append(raw.iloc[i:i+200].max()-raw.iloc[i:i+200].min())
   diff=raw.diff().dropna();rm=np.sqrt(np.mean(res**2));q99=res.quantile(.995)-res.quantile(.005);mad=np.median(np.abs(res-np.median(res)));threshold=max(6*1.4826*mad,5)
   m=dict(board=board,mode=mode,sensor=sid,ch=ch,scope=scope,n=len(raw),mean=raw.mean(),p2p=raw.max()-raw.min(),p2p_mv=(raw.max()-raw.min())*scale,std=raw.std(),res_rms=rm,res_mv=rm*scale,res99_mv=q99*scale,block_med_mv=np.median(spans)*scale,block95_mv=np.quantile(spans,.95)*scale,diff95_mv=diff.abs().quantile(.95)*scale,step_noise_mv=diff.std()/np.sqrt(2)*scale,tail_percent=(np.abs(res-np.median(res))>threshold).mean()*100,drift_mv=(raw.tail(500).median()-raw.head(500).median())*scale,filt_p2p_mv=(filtered.max()-filtered.min())*scale,res=res,raw=raw,filtered=filtered,scale=scale)
   metrics.append(m)
   rows.append([board,mode,f'传感器{sid}号 IN{ch}',scope,len(raw),fmt(raw.mean()),fmt(m['p2p']),fmt(m['p2p_mv']),fmt(m['res_mv']),fmt(m['res99_mv']),fmt(m['block_med_mv']),fmt(m['block95_mv']),fmt(m['diff95_mv']),fmt(m['drift_mv']),fmt(m['tail_percent'])])
tail=[m for m in metrics if m['scope']=='末5000条'];heads=['板号','供电','通道','统计区间','样本数','平均ADC码','全段峰峰码','全段峰峰mV','去基线RMS mV','去基线中央99%宽mV','200点峰峰中位mV','200点峰峰P95 mV','相邻变化P95 mV','首末漂移mV','6MAD尾部比例%']
summary=[]
for board in [1,2]:
 for sid in ([1,2] if board==1 else [3,4]):
  ms=[m for m in tail if m['sensor']==sid];rank=sorted(ms,key=lambda m:m['res_mv']);cells=[f'传感器{sid}号',*[f"{m['mode']}：{m['res_mv']:.3f}mV" for m in rank]];summary.append(cells+['—']*(8-len(cells)))
boost=[m for m in tail if m['mode']=='5V BOOST'];comp=[]
for m in tail:
 if m['mode']!='5V BOOST':
  b=next(z for z in boost if z['sensor']==m['sensor']);comp.append([f"传感器{m['sensor']}号",m['mode'],fmt(b['res_mv']),fmt(m['res_mv']),fmt(m['res_mv']/b['res_mv']),fmt(b['block95_mv']),fmt(m['block95_mv'])])
parts=['<header><p>2026-11-03 · 十份静态大气日志</p><h1>三种供电下ADC IN0与IN2纹波对比实验报告</h1><p>2.5V、5V BOOST、5V LDO · 两块板四个模拟传感器 · 原始ADC与滤波输出</p></header>',
 '<nav><a href="#summary">结论与选择</a><a href="#method">方法</a><a href="#figs">图表</a><a href="#stats">统计</a><a href="#quality">数据质量</a></nav>',
 '<section id="summary"><h2>结果与供电选择</h2><p>主要依据各文件末5000条记录，在同板、同传感器内比较等效输入电压的短时波动。全段统计也保留，用于检查慢漂移。不是简单把全段峰峰值最小的一组判为最佳。</p>',
 table(summary,['传感器','短时残差RMS最低','其次','再次','额外重复1','额外重复2','额外重复3','额外重复4']),
 '<h3>相对5V BOOST的波动变化</h3>',table(comp,['传感器','对照供电','BOOST残差RMS mV','对照残差RMS mV','对照/BOOST','BOOST块峰峰P95 mV','对照块峰峰P95 mV']),
 '<p class="notice">本报告仅依据本次九份静态大气日志比较ADC码、等效输入电压的噪声纹波、尖峰、峰峰值和漂移，不引用历史实验。供电选择只针对本次观测到的波动表现；不同指标、不同通道的优劣分别列出。</p></section>',
 '<section id="method"><h2>实验条件、字段与口径</h2><p>用户确认全部为静态大气测试，ADC采样与滤波设置相同，REF与组别电压一致：2V5为2.5V，BOOST/LDO为5V。两种5V方案使用不同的输入电源路径，电池字段约4.1V或8.3V，故这是整套供电方案的比较，不能把差异完全归因于稳压芯片本身。</p><p>根据当前固件串口格式：I14=IN0原始码，I16=IN2原始码；I7=IN0滤波输出，I9=IN2滤波输出。I3/I5为两个数字气压计实时压力，I11/I12为各自平均值，I0电池、I1温度、I2湿度。原始码为串口截取到的最新转换结果，不是完整的ADC转换流。</p><p>电压(mV)=ADC×REF×1000/65536。固定正斜率换算不改变相关系数，但REF不同会改变每码电压；本报告不使用适用性待确认的厂家压力公式来排名供电。</p><p>CSV没有时间戳，横轴使用有效记录序号，不虚构采样Hz或秒数，不将序号频率认作开关电源频率。两板记录不同时，无绝对时间对齐；末5000点图仅按各自末段起点对齐。全部有效样本参与绘图，不降采样、关闭路径简化。末段相同条数不保证已经达到热稳定。</p><p>短时残差=原始码−居中501点滚动中位数（至少250个有效点）；此基线只用于离线分析，不替换原始数据。RMS与中央99%宽用于幅值比较，200点不重叠窗口的峰峰值分布用于降低单一偶发极值的影响。首末漂移=最后500点中位数−前500点中位数。6MAD尾部比例仅描述超出稳健阈值的尾部，阈值下限5码；不是硬件故障判据，也未据此删点。</p></section><section id="figs"><h2>全量图表</h2>']
parts.append('<h3>两板差异与同板重复性</h3><p>下表在同供电、同通道位置比较两板的末5000条。两块板连接的是不同传感器，因此“板间差异”包含板、电源路径、传感器个体和不同记录时段的共同影响，不能从现有数据单独分离PCB本身的贡献。2号板的各次重复均保留，未挑选最好的一次。</p>')
boardrows=[]
for mode in ['2V5','5V BOOST','5V LDO']:
 for ch in [0,2]:
  one=next(m for m in tail if m['board']==1 and m['mode']==mode and m['ch']==ch)
  twos=[m for m in tail if m['board']==2 and m['mode'].startswith(mode) and m['ch']==ch]
  for two in twos:
   boardrows.append([mode,f'IN{ch}',two['mode'],fmt(one['res_mv']),fmt(two['res_mv']),f"{(two['res_mv']/one['res_mv']-1)*100:+.1f}%",fmt(one['block95_mv']),fmt(two['block95_mv']),fmt(one['mean']*one['scale']),fmt(two['mean']*two['scale'])])
parts.append(table(boardrows,['供电','位置','2号板记录','1号板RMS mV','2号板RMS mV','2号相对1号','1号块峰峰P95 mV','2号块峰峰P95 mV','1号平均输入mV','2号平均输入mV']))
fig,axs=plt.subplots(1,2,figsize=(14,5))
for board,ch,color in [(1,0,'#168579'),(1,2,'#1769aa'),(2,0,'#d48124'),(2,2,'#a150a8')]:
 for ax,key in zip(axs,['res_mv','block95_mv']):
  xs=[];ys=[]
  for j,mode in enumerate(['2V5','5V BOOST','5V LDO']):
   ms=[m for m in tail if m['board']==board and m['ch']==ch and m['mode'].startswith(mode)]
   offset=([1,2].index(board)*2+[0,2].index(ch)-1.5)*.13
   for k,m in enumerate(ms):
    x=j+offset+(k-(len(ms)-1)/2)*.025;xs.append(x);ys.append(m[key])
    ax.annotate(('首次' if k==0 else f'第{k+1}次') if len(ms)>1 else '',(x,m[key]),xytext=(3,5),textcoords='offset points',fontsize=7)
  ax.scatter(xs,ys,color=color,label=f'{board}号板 IN{ch}（传感器{board*2-1 if ch==0 else board*2}号）',s=35)
for ax,label in zip(axs,['去局部基线残差RMS / mV','200点块峰峰P95 / mV']):
 ax.set_xticks([0,1,2],['2V5','5V BOOST','5V LDO']);ax.set_ylabel(label);ax.grid(alpha=.2);ax.legend(fontsize=8);ax.set_ylim(bottom=0)
parts.append(addfig('两块板同供电比较：每个点代表一次完整记录的末段指标',fig,'所有重复测试分别显示。横轴小幅错开仅为了区分点，不表示实际电压差异；同通道位置对应不同的传感器个体。'))
for board in [1,2]:
 objs=[o for o in loaded if o['board']==board]
 fig,axs=plt.subplots(2,1,figsize=(14,7))
 for i,o in enumerate(objs):
  for ax,ch,col in zip(axs,[0,2],['I14','I16']):
   y=o['d'][col].reset_index(drop=True);volts=y*o['ref']*1000/65536;ax.plot(np.arange(len(y)),volts-volts.head(500).median(),lw=.5,color=COLORS[i],label=o['mode']);ax.set_ylabel(f'IN{ch}相对起始电压 / mV');ax.grid(alpha=.2);ax.legend(ncol=4)
 axs[-1].set_xlabel('各自有效记录序号（非绝对时间）');parts.append(addfig(f'{board}号板全段原始ADC电压变化',fig,'各段只减去前500点的中位电压，以比较漂移和波动，不是压力校准。两通道使用同一横轴范围。'))
 for sid in ([1,2] if board==1 else [3,4]):
  ms=[m for m in tail if m['sensor']==sid];fig,axs=plt.subplots(2,1,figsize=(14,7),sharex=True)
  for i,m in enumerate(ms):
   raw=m['raw'];v=raw*m['scale'];axs[0].plot(np.arange(len(raw)),v-v.median(),lw=.5,color=COLORS[i],label=m['mode']);axs[1].plot(m['res'].index,m['res']*m['scale'],lw=.5,color=COLORS[i],label=m['mode'])
  for ax in axs:ax.grid(alpha=.2);ax.legend(ncol=4);ax.ticklabel_format(axis='y',style='plain',useOffset=False)
  axs[0].set_ylabel('原始电压−本段中位值 / mV');axs[1].set_ylabel('去501点基线残差 / mV');axs[1].set_xlabel('各自末5000条的记录序号');axs[1].set_xlim(0,4999)
  parts.append(addfig(f'传感器{sid}号：三种供电末5000点对比',fig,'同图同纵坐标比较，保留全部尖峰；上图包含缓慢漂移，下图突出局部波动。'))
  fig,axs=plt.subplots(1,2,figsize=(14,4.6))
  for i,m in enumerate(ms):
   vals=np.sort(np.abs(m['res'].to_numpy()*m['scale']));ecdf=np.arange(1,len(vals)+1)/len(vals);axs[0].plot(vals,ecdf,color=COLORS[i],label=m['mode'])
   res=m['res'].to_numpy();res=res-res.mean();ps=np.abs(np.fft.rfft(res))**2;freq=np.fft.rfftfreq(len(res));power=ps*m['scale']**2/len(res)**2;axs[1].plot(freq[1:],power[1:],lw=.5,color=COLORS[i],label=m['mode'])
  axs[0].set_xlabel('残差绝对值 / mV');axs[0].set_ylabel('累计比例');axs[0].set_ylim(.5,1.001);axs[1].set_xlabel('归一化频率 / 周期每记录');axs[1].set_ylabel('频点幅值平方 / mV²');axs[1].set_yscale('log')
  for ax in axs:ax.grid(alpha=.2);ax.legend(fontsize=8)
  parts.append(addfig(f'传感器{sid}号：残差分布与序号频谱',fig,'频谱为去局部基线后的整段DFT诊断，未加窗，存在泄漏；只有相对记录频率，无法确定真实纹波Hz或电源开关频率，不能据此认定干扰源。'))
 # Output filter effectiveness: does not substitute raw data quality.
 fig,axs=plt.subplots(2,1,figsize=(14,7),sharex=True)
 for i,o in enumerate(objs):
  for ax,ch,rawcol,fc in zip(axs,[0,2],['I14','I16'],['I7','I9']):
   z=o['d'].tail(5000).reset_index(drop=True);delta=(z[rawcol]-z[fc])*o['ref']*1000/65536;ax.plot(delta,lw=.5,color=COLORS[i],label=o['mode']);ax.set_ylabel(f'IN{ch}原始−滤波 / mV');ax.legend(ncol=4);ax.grid(alpha=.2)
 axs[-1].set_xlabel('各自末5000条的记录序号');parts.append(addfig(f'{board}号板原始与滤波输出差值',fig,'原始与滤波差值同时包含抑噪和滤波滞后，不能直接当作随机噪声或滤波误差。'))
parts+=['</section><section id="stats"><h2>全段及等样本数末段统计</h2>',table(rows,heads),'<h3>辅助环境变化</h3>',table([[o['board'],o['mode'],fmt(o['d'].I1.min()),fmt(o['d'].I1.max()),fmt(o['d'].I2.min()),fmt(o['d'].I2.max()),fmt(o['d'].I3.max()-o['d'].I3.min()),fmt(o['d'].I0.min()),fmt(o['d'].I0.max())] for o in loaded],['板号','供电','最低温°C','最高温°C','最低湿%RH','最高湿%RH','数字气压峰峰hPa','最低电池V','最高电池V']),'<p>温度、湿度和真实气压在不同记录段内仍有变化；未通过拟合将其从ADC数据强行扣除，不能把全部ADC漂移归因于电源。无同时测得的传感器电源/REF波形，无法直接证明哪一电源轨产生了噪声。</p></section><section id="quality"><h2>数据质量与追溯</h2>',table(audit,['板号','供电','源行数','有效行数','剔除CSV行号（含表头）','各ADC字段无效数','SHA-256']),'<p>2号板LDO第二次首条为串口截断或错位记录，不符合电池/温湿度字段的基本结构，整条剔除。未按ADC尖峰幅度删点。各源文件仅用于读取，没有修改。最小/最大与峰峰值依赖样本数量和记录长度，因此同时提供等样本数末段及稳健幅度指标。</p></section>']
# Derive conclusion from data; no assumed ranking by regulator name.
recommend=[]
for sid in range(1,5):
 ms=[m for m in tail if m['sensor']==sid and m['mode']!='2V5'];best=min(ms,key=lambda x:x['res_mv']);recommend.append(f"传感器{sid}号在5V方案中，末段短时残差RMS最低的是{best['mode']}（{best['res_mv']:.3f}mV）。")
boost_wins=sum('BOOST' in min([m for m in tail if m['sensor']==sid and m['mode']!='2V5'],key=lambda x:x['res_mv'])['mode'] for sid in range(1,5))
newms=[m for m in tail if m['mode']=='5V BOOST 第三次']
newtext='；'.join(f"IN{m['ch']}残差RMS {m['res_mv']:.3f}mV、200点峰峰P95 {m['block95_mv']:.3f}mV、首末漂移 {m['drift_mv']:+.3f}mV" for m in newms)
conclusion='<p><b>九份静态日志显示：两板的部分指标确实不同，但没有证据说明某块板在各供电、各通道上始终更差。2号板重复测试自身也有明显差异，不能把单次测量差异全部归因于板子。</b></p><p><b>新增2号板BOOST第三次：</b>'+newtext+'。与前两次相比，IN0短时纹波增大，IN2与第二次接近。第三次IN0的RMS超过1号板BOOST，但前两次低于1号板，板间排名发生反转。</p><p><b>同供电的两板比较：</b>2V5下，2号板IN0/IN2的残差RMS分别比1号板高约13.5%/4.5%。BOOST下，2号板IN0三次为0.722、0.632、'+f"{newms[0]['res_mv']:.3f}"+'mV，1号板为0.762mV；2号板IN2三次为0.915、0.747、'+f"{newms[1]['res_mv']:.3f}"+'mV，均高于1号板0.699mV。LDO下，IN0两板接近（1号0.697，2号0.694/0.684mV），IN2则是2号板0.700～1.067mV低于1号板1.103mV。差异随通道、供电和重复测试变化。</p><p><b>尖峰与平均纹波要分别看：</b>RMS相近不代表尾部峰峰值相近，例如LDO的IN0两板RMS接近，但1号板200点峰峰P95为8.942mV，2号板两次为12.085/12.527mV。报告新增两板对照表和每次记录的指标散点图，避免只比较某一次或只比较RMS。</p><p>两板使用不同的传感器个体，且不是同时、同预热状态采集。因此现有数据支持“两套板与传感器组合表现有差异”，尚不能确定差异来自PCB、传感器还是供电运行状态。区分板与传感器贡献需要交换同一只传感器并保持其他条件一致，最好再用共同的稳定输入源比较。</p><p><b>供电比较：</b>末5000条中2V5四路RMS为1.266～1.443mV，5V全部记录为'+f"{min(m['res_mv'] for m in tail if m['mode']!='2V5'):.3f}～{max(m['res_mv'] for m in tail if m['mode']!='2V5'):.3f}"+'mV。5V组短时残差整体更低，但BOOST与LDO均有通道和重复测试差异，不能选定统一最优方案。各次样本长度不一，主要短时比较统一取末5000条；全段保留全部有效记录，不降采样。</p>'
third0=next(m for m in newms if m['ch']==0)
large=int((third0['res'].abs()*third0['scale']>10).sum())
conclusion+=f'<p><b>第三次BOOST的IN0不是所有点一起变吵：</b>末5000条仅{large}条的去基线残差绝对值超过10mV，中央99%残差宽度为{third0["res99_mv"]:.3f}mV，但全段峰峰值达到{third0["p2p_mv"]:.3f}mV。这说明少量大尖峰明显拉高RMS与峰峰值，应将偶发尖峰和常态纹波区分评价。所有这些点均保留在统计和图中。</p>'
fourths=[m for m in tail if m['mode']=='5V BOOST 第四次']
repeatrows=[]
for m in tail:
 if m['board']==2 and 'BOOST' in m['mode']:
  r=m['res']*m['scale'];repeatrows.append([m['mode'],f"IN{m['ch']}",fmt(m['res_mv']),fmt(m['res99_mv']),fmt(m['p2p_mv']),fmt(m['block95_mv']),int((r.abs()>10).sum()),fmt(r.min()),fmt(r.max()),fmt(m['drift_mv'])])
fourthtext='<h3>第四次BOOST与前三次的差异</h3><p><b>常态纹波接近，偶发尖峰仍存在，两通道的变化并不一致。</b>末5000条中，第四次IN0残差RMS为1.054mV，比第三次1.099mV低约4.1%，仍高于前两次；IN2为0.981mV，比第三次0.756mV高约29.7%，也高于前两次。但第四次IN0/IN2中央99%残差宽度为3.739/3.777mV，与前三次约3.586～3.967mV接近，因此不应将RMS增大解释为所有样本的背景纹波都变大。</p><p>第四次末5000条两路各只有1个残差绝对值超过10mV的点，最大偏离分别为−58.746/−49.057mV。IN0全幅峰峰值从第三次87.128降到64.392mV，IN2则从21.362升到58.823mV。200点块峰峰P95分别为12.344/12.146mV，说明整体尖峰分布和最极端单点需要分开看。这里没有删除尖峰，全部指标保留原样。</p><p>第四次共13013条，全段IN0/IN2峰峰值为100.327/83.160mV，末段首末漂移分别−4.501/+4.425mV。电池字段中位值约3.950V，前三次约4.128/4.123/4.142V；供电输入状态不同，但未直接测得5V输出或REF波形，不能仅凭电池电压变化确定尖峰原因。首条电池字段仍为4.142V且I11为0，属于启动边界记录，应单独看待；末5000条不包含首条。</p>'
fourthtext+=table(repeatrows,['BOOST记录','通道','残差RMS mV','中央99%宽mV','末段峰峰mV','块峰峰P95 mV','超过±10mV点数','最小残差mV','最大残差mV','首末漂移mV'])
fourthtext+='<p><b>本次结论：</b>第四次没有消除尖峰，也没有呈现两路一致恶化。常态波动幅度保持接近，RMS与总峰峰值的变化主要需结合稀少的大偏离点解释。前三次与两板对比结论继续保留如下。</p>'
parts[parts.index('<h3>相对5V BOOST的波动变化</h3>'):parts.index('<h3>相对5V BOOST的波动变化</h3>')]=[fourthtext,conclusion.replace('九份静态日志显示','此前九份静态日志显示')]
parts=[p.replace('本报告仅依据本次九份','本报告仅依据本次十份') for p in parts]
css='body{margin:0;background:#f3f6fa;color:#24384c;font:15px/1.8 "Microsoft YaHei"}header,section,nav{max-width:1400px;margin:18px auto;padding:28px;background:white;border-radius:12px}header{background:linear-gradient(120deg,#173f5e,#24636b);color:white}h2{color:#173f5e}nav{display:flex;gap:22px;padding:16px 28px}a{color:#1769aa}article{border:1px solid #dce5ee;border-radius:10px;padding:16px;margin:20px 0}article p{font-size:13px;color:#65758b}img{width:100%}.tablewrap{overflow-x:auto}table{width:100%;border-collapse:collapse;font-size:13px}td,th{padding:9px;white-space:nowrap;border-bottom:1px solid #ddd;text-align:right}th{background:#edf3f8}.notice{background:#fff5df;padding:16px;border-left:4px solid #d48124}'
report=OUT/'三种供电ADC纹波对比实验报告.html';report.write_text('<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>三种供电ADC纹波对比实验报告</title><style>'+css+'</style>'+''.join(parts)+'</html>',encoding='utf8')
serial=[{k:v for k,v in m.items() if k not in ['res','raw','filtered']} for m in metrics];(OUT/'分析核验.json').write_text(json.dumps(dict(metrics=serial,images=images,files=audit),ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps([{k:m[k] for k in ['board','mode','sensor','res_mv','block95_mv','drift_mv','p2p_mv']} for m in tail],ensure_ascii=False,indent=2));print(report,'images',len(images))
