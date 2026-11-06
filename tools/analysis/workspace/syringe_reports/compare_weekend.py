"""Build the historical comparison section without modifying source logs."""
from pathlib import Path
import hashlib, html, io, base64, json
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

def build_comparison(root, out):
    batches = [
        ('周末5V组', 5.0, pd.Timestamp('2026-10-30 18:00'), None,
         pd.Timestamp('2026-10-31 12:00'), ['1030-1102-1号板合并.csv','1030-1102-2号板.csv']),
        ('本次REF=2.5V组', 2.5, pd.Timestamp('2026-11-02 12:00'), pd.Timestamp('2026-11-03 09:00'),
         pd.Timestamp('2026-11-02 12:00'), ['1102-1103-1号板.csv','1102-1103-2号板.csv'])]
    loaded={}; provenance=[]; rows=[]; lookup={}; fits=[]; correlations={}
    def fmt(x): return f'{x:.3f}' if np.isfinite(x) else 'N/A'
    def table(data, heads):
        return '<div class="tablewrap"><table><thead><tr>'+''.join('<th>'+html.escape(str(x))+'</th>' for x in heads)+'</tr></thead><tbody>'+''.join('<tr>'+''.join('<td>'+html.escape(str(x))+'</td>' for x in row)+'</tr>' for row in data)+'</tbody></table></div>'
    for name,ref,start,end,fair_start,files in batches:
        loaded[name]={}
        for bi,fn in enumerate(files,1):
            path=root/'code/log'/fn; digest=hashlib.sha256(path.read_bytes()).hexdigest()
            d=pd.read_csv(path)
            ts=pd.to_datetime(dict(year=d.I0.astype(int),month=d.I1.astype(int),day=d.I2.astype(int),hour=d.I3.astype(int),minute=d.I4.astype(int),second=d.I5.astype(int)))
            mask=(ts>=start) & ((ts<=end) if end is not None else True)
            d=d.loc[mask].copy(); d.index=pd.DatetimeIndex(ts[mask]);d=d.sort_index(kind='stable')
            for c in ['I8','I9','I10','I12']:
                d.loc[~np.isfinite(d[c]) | (d[c]==-9999),c]=np.nan
                if c in ('I10','I12'):d.loc[(d[c]<0)|(d[c]>65535),c]=np.nan
            loaded[name][bi]=d
            provenance.append([name,f'{bi}号板',fn,str(d.index.min()),str(d.index.max()),len(d),digest])
            measures=[('I8',f'{bi}号板气压计1号',False),('I9',f'{bi}号板气压计2号',False),('I10',f'传感器{1 if bi==1 else 3}号',True)]
            if bi==1:measures.append(('I12','传感器2号',True))
            # Sensor 4 is not a fair historical comparator: weekend contact fault.
            for scope,win in [('全区间',d),('等时长21小时',d.loc[fair_start:fair_start+pd.Timedelta(hours=21)])]:
                sec=win.groupby(level=0).mean(numeric_only=True)
                for col,label,analog in measures:
                    code=sec[col].dropna(); x=225+250*ref*code/65536 if analog else code
                    baseline=x.rolling('5min',center=True,min_periods=30).median();res=x-baseline
                    minute=x.resample('1min').agg(['min','max','count'])
                    spans=(minute['max']-minute['min']).where(minute['count']>=30).dropna()
                    rms=np.sqrt(np.nanmean(res**2));w95=res.quantile(.975)-res.quantile(.025)
                    delta=x.diff().where(x.index.to_series().diff().dt.total_seconds()==1)
                    code_res=code-code.rolling('5min',center=True,min_periods=30).median()
                    code_rms=np.sqrt(np.nanmean(code_res**2)) if analog else np.nan
                    mv_rms=code_rms*ref/65536*1000 if analog else np.nan
                    values=dict(span=spans.median(),span95=spans.quantile(.95),rms=rms,w95=w95,code_rms=code_rms,mv_rms=mv_rms)
                    lookup[(name,scope,label)]=values
                    rows.append([name,scope,label,len(x),fmt(x.mean()),fmt(spans.median()),fmt(spans.quantile(.95)),fmt(rms),fmt(w95),fmt(code_rms),fmt(mv_rms)])
                    if analog:
                        q=pd.concat([x.rename('analog'),sec[['I8','I9']].mean(axis=1,skipna=False).rename('digital')],axis=1).dropna()
                        e=q.analog-q.digital;r=q.analog.corr(q.digital)
                        raw_r=sec[col].reindex(q.index).corr(q.digital)
                        assert np.isclose(r,raw_r,atol=1e-10), 'Positive linear conversion must preserve correlation'
                        correlations[(name,scope,label)]=raw_r
                        fits.append([name,scope,label,fmt(e.mean()),fmt(np.sqrt(np.mean(e**2))),fmt(r)])
    ratio_rows=[]
    for sid in [1,2,3]:
        label=f'传感器{sid}号'
        for scope in ['全区间','等时长21小时']:
            a=lookup[(batches[0][0],scope,label)];b=lookup[(batches[1][0],scope,label)]
            ratio_rows.append([label,scope,fmt(a['rms']),fmt(b['rms']),fmt(b['rms']/a['rms']),fmt(b['code_rms']/a['code_rms']),fmt(a['mv_rms']),fmt(b['mv_rms'])])
    parts=['<section id="weekend"><h2>与周末5V组的历史对比</h2>',
        '<p class="notice">周末数据：2026-10-30 18:00起，使用REF=5V，按用户称谓标记为“5V组”；本次数据：2026-11-02 12:00至11-03 09:00，用户确认传感器供电=2.5V、AD7682 REF=2.5V。ADC VDD不能由REF推断。两批非同时、非受控实验，还可能存在固件、连接及环境差异，不能据此单独认定供电变化的因果效果。厂家表存在4.5V输出点，未注明适用供电，不能直接作为本次2.5V传感器供电下的完整标定。</p>',
        '<p>仅将两批都可用的传感器1号、2号、3号作为模拟压力历史对照。传感器4号周末有接触故障，故不做跨批次性能比较；本次4号数据仍在本报告当日图表保留。数字气压计共四路均参与。两批都已经过当时固件处理，原始ADC字段是日志里的ADC读数，不能称为芯片未经滤波的转换流。</p>',
        '<h3>相同口径与相同时长</h3><p>首先比较各自全区间，再比较预先按时钟选定的21小时窗口：周末10月31日12:00至11月1日09:00，本次11月2日12:00至11月3日09:00。两者都有12:00至次日09:00的时段覆盖，减少记录长度和昼夜覆盖差异，但天气和真实压力仍不同。全部有效点参与，未抽点；统计同秒取均值，时序图保留日志全部有效记录。</p>',
        '<p>条件换算公式P(hPa)=225+250×REF×ADC/65536。厂家表对应P(kPa)=22.5+25×V，但表上未注明供电适用条件。在1023hPa下该表要求约3.192V，超过本次REF=2.5V的输入量程，因此本次绝对压力及hPa波动仅作为沿用表格的条件计算值，不视作有效标定。电压与ADC码的波动仍单独列出。去基线残差使用居中5分钟中位数，属于离线诊断，不是增加固件滤波。ADC残差RMS、等效输入电压mV、换算压力hPa同时列出，避免参考电压下降导致每码压力权重减半而造成“噪声自动改善”的错觉。</p>',
        ]
    trend_rows=[]
    for sid in [1,2,3]:
        label=f'传感器{sid}号'
        trend_rows.append([label,fmt(correlations[(batches[0][0],'全区间',label)]),fmt(correlations[(batches[0][0],'等时长21小时',label)]),fmt(correlations[(batches[1][0],'等时长21小时',label)])])
    parts += ['<h3 id="trend-conclusion">ADC趋势与数字气压的相关性结论</h3>',
        '<p class="notice"><b>周末5V组的ADC读数与数字气压强相关，本次2.5V组相关性明显下降；这个趋势差异不能用电压—压力线性换算公式解释。</b>对固定正斜率P=a×ADC+b，线性换算只改变幅值和偏置，不改变波形方向和Pearson相关系数。以下相关系数直接以日志ADC码计算，并验证与线性换算压力计算的结果一致。</p>',
        table(trend_rows,['模拟传感器','周末全区间ADC相关系数r','周末等时长21小时ADC相关系数r','本次指定21小时ADC相关系数r']),
        '<p>参照为各传感器所在板两个数字气压计的逐秒均值；同秒对齐，缺测不插值。此处“ADC读数”仍是日志保存的、经过当时固件处理的ADC字段，不代表未经滤波的芯片转换流。</p>',
        '<p>需要解释的是本次模拟测量链路为什么出现了更多不随真实气压变化的漂移或波动。传感器改为2.5V供电是重要排查对象，应核实最低工作电压及该供电下输出特性，但不能据此认定供电是唯一原因。两批并非同时受控试验，参考电压、固件、连接与环境也可能不同；本次真实气压变化范围较小，同样的噪声也会使相关系数降低。</p>',
        '<p><b>滤波能减轻快速抖动，但不保证修复缓慢的趋势偏离。</b>若源头是低压供电下传感器工作特性异常，继续增大滤波窗口可能只让错误趋势更平滑，不能保证恢复正确气压趋势。绝对压力标定适用性和ADC趋势跟随性应分别排查。</p>',
        '<h3>三路模拟传感器的波动变化</h3>',table(ratio_rows,['传感器','区间口径','周末残差RMS hPa','本次残差RMS hPa','本次/周末 hPa比','本次/周末 ADC码比','周末残差RMS mV','本次残差RMS mV'])]
    full_ratios=[lookup[(batches[1][0],'全区间',f'传感器{i}号')]['rms']/lookup[(batches[0][0],'全区间',f'传感器{i}号')]['rms'] for i in [1,2,3]]
    parts.append('<p>按全区间5分钟基线残差RMS计算，传感器1、2、3号本次/周末的压力波动比依次为'+ '、'.join(fmt(x) for x in full_ratios)+'。接近1表示变化不大，低于1表示该指标较小，高于1表示较大。上述差异不能单独归因于供电。</p>')
    colors=['#14689d','#ce812b']; image_count=0
    for sid in [1,2,3]:
        bi=1 if sid<3 else 2; col='I12' if sid==2 else 'I10'
        fig,axs=plt.subplots(2,1,figsize=(14,7),sharex=True)
        for i,(name,ref,start,end,fair_start,files) in enumerate(batches):
            d=loaded[name][bi].loc[fair_start:fair_start+pd.Timedelta(hours=21)]
            raw=d[col];res=raw-raw.rolling('5min',center=True,min_periods=30).median()
            hours=(d.index-fair_start).total_seconds()/3600
            # Preserve endpoints and insert breaks across missing intervals.
            xx=np.asarray(hours);yy=res.to_numpy();at=np.flatnonzero(np.r_[False,np.diff(d.index.asi8)>2000000000])
            xx=np.insert(xx,at,xx[at]);yy=np.insert(yy,at,np.nan)
            axs[0].plot(xx,yy*ref*250/65536,lw=.55,color=colors[i],label=name)
            axs[1].plot(xx,yy,lw=.55,color=colors[i],label=name)
        for ax in axs:
            ax.grid(alpha=.25);ax.legend(loc='upper right');ax.spines[['top','right']].set_visible(False);ax.ticklabel_format(axis='y',style='plain',useOffset=False)
        axs[0].set_ylabel('偏离局部基线 / hPa');axs[1].set_ylabel('偏离局部基线 / ADC码');axs[1].set_xlabel('各自21小时窗口开始后的小时（不是同一日的绝对时间）');axs[1].set_xlim(0,21);axs[1].set_xticks(np.arange(0,22,3))
        fig.suptitle(f'传感器{sid}号：相同21小时窗口的波动对比');fig.tight_layout()
        buf=io.BytesIO();fig.savefig(buf,format='png',dpi=150);plt.close(fig)
        img=base64.b64encode(buf.getvalue()).decode();image_count+=1
        parts.append(f'<article><h3>传感器{sid}号的历史波动对比</h3><p>上图用hPa表示，下图用ADC码表示。时间按窗口起点对齐，仅比较波动分布，不能认为不同时日的尖峰属于同一事件。相同纵坐标下保留所有有效记录。</p><img src="data:image/png;base64,{img}" alt="传感器{sid}号历史波动对比"></article>')
    parts += ['<h3>数字与模拟压力的完整波动统计</h3>',table(rows,['批次','区间','测量项','有效秒','均值hPa','分钟峰峰中位hPa','分钟峰峰P95 hPa','基线残差RMS hPa','残差95%宽hPa','ADC残差RMS码','电压残差RMS mV']),
        '<p>每分钟至少30个有效秒才计算该分钟峰峰值。数字气压记录分辨率0.1hPa，不能将其记录残差直接当作芯片真实噪声。全区间峰峰值包含真实天气变化，未用其作为降噪优劣指标。</p>',
        '<h3>模拟压力相对本板两个数字气压计均值</h3>',table(fits,['批次','区间','传感器','平均差hPa','直接RMSE hPa','Pearson r']),
        '<p>本次模拟与数字压力的绝对差约为−250hPa，而周末明显较小。此偏差与厂商公式的适用条件、实际REF及传感器供电等有关，普通低通或中值滤波无法修复，不能用拟合强行校正后声称精度提高。相关系数受真实压力变化范围、温漂和响应影响，不能独立解释为传感器精度。</p>',
        '<h3>历史来源与核验</h3>',table(provenance,['批次','板号','文件','起点','终点','行数','SHA-256']), '</section>']
    audit={'images':image_count,'full_residual_rms_ratios':full_ratios,'ratio_rows':ratio_rows,'source_ranges':provenance,'adc_correlation_rows':trend_rows}
    (out/'历史对比核验.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2),encoding='utf8')
    return ''.join(parts),image_count
