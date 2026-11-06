from pathlib import Path
p=Path('tools/analysis/workspace/syringe_reports/_generated_nov5_6_ad7682.py')
s=p.read_text(encoding='utf8')
needle="body += '<section id=\"charts\"><h2>全区间时间曲线与比较</h2>'+''.join(parts)+'</section>'"
insert=r'''
# Temperature influence analysis: same-record pairing, no interpolation.
temp_rows=[]
temp_parts=[]
def temp_fit(board, col, label, unit):
    d=boards[board]['data'][['I6',col]].dropna()
    x=d['I6'].to_numpy(); y=d[col].to_numpy()
    if len(d)>2 and np.ptp(x)>0:
        slope, intercept=np.polyfit(x,y,1); r=np.corrcoef(x,y)[0,1]
    else: slope=intercept=r=np.nan
    temp_rows.append([board,label,unit,len(d),f(r,4),f(slope,5),f(intercept,3),f(y.min()),f(y.max()),f(y.max()-y.min())])
    return d, slope, intercept
for board, filename, sensors in FILES:
    d=boards[board]['data']
    targets=[(f'S{sid}_hPa',f'传感器{sid}号 IN{ch}换算压力','hPa') for sid,ch in zip(sensors,[0,2])]
    targets += [('I8','气压计1','hPa'),('I9','气压计2','hPa'),('I14','电池电压','V'),('I7','湿度','%RH')]
    fitted=[]
    for col,label,unit in targets:
        q,sl,it=temp_fit(board,col,label,unit); fitted.append((col,label,unit,q,sl,it))
    # Analog pressure scatter with independent regression lines.
    def draw_analog_temp(fig, ax, fitted=fitted, board=board):
        for i,(col,label,unit,q,sl,it) in enumerate(fitted[:2]):
            x=q.I6.to_numpy(); y=q[col].to_numpy(); ax.scatter(x,y,s=2,alpha=.18,color=COLORS[i],label=label+'散点')
            if np.isfinite(sl):
                xx=np.array([x.min(),x.max()]); ax.plot(xx,sl*xx+it,color=COLORS[i],lw=2,label=f'{label}拟合 ({sl:.3f} hPa/°C)')
        ax.set_xlabel('温度 / °C'); ax.set_ylabel('厂家换算压力 / hPa'); ax.legend(fontsize=8,ncol=2,frameon=False)
    temp_parts.append(figure(f'{board} 温度与两个模拟传感器压力关系',draw_analog_temp,'每个点为同一条日志记录；斜率表示温度每升高1°C时的表观压力变化，包含真实环境变化与温度耦合，不能单独视为传感器温漂。',time_axis=False))
    # Other sensor scatter: digital gauges, battery and humidity are shown separately by axis units.
    def draw_other_temp(fig, ax, fitted=fitted, board=board):
        for i,(col,label,unit,q,sl,it) in enumerate(fitted[2:4]):
            x=q.I6.to_numpy(); y=q[col].to_numpy(); ax.scatter(x,y,s=2,alpha=.18,color=COLORS[i+2],label=label+'散点')
            if np.isfinite(sl):
                xx=np.array([x.min(),x.max()]); ax.plot(xx,sl*xx+it,color=COLORS[i+2],lw=2,label=f'{label}拟合 ({sl:.3f} hPa/°C)')
        ax.set_xlabel('温度 / °C'); ax.set_ylabel('数字气压 / hPa'); ax.legend(fontsize=8,ncol=2,frameon=False)
    temp_parts.append(figure(f'{board} 温度与数字气压计关系',draw_other_temp,'用于判断环境温度变化是否伴随数字气压读数变化；相关性不等于因果。',time_axis=False))
# Combined binned trend for all four analog sensors. Bins are per-board 1°C temperature means.
def draw_binned_temp(fig, ax):
    for i,(board,filename,sensors) in enumerate(FILES):
        d=boards[board]['data']
        for j,(sid,ch) in enumerate(zip(sensors,[0,2])):
            q=d[['I6',f'S{sid}_hPa']].dropna().copy(); q['bin']=q.I6.round(0)
            z=q.groupby('bin')[f'S{sid}_hPa'].mean(); ax.plot(z.index,z.values,'o-',ms=3,lw=1.2,color=COLORS[i*2+j],label=f'{board} 传感器{sid}号')
    ax.set_xlabel('温度分箱中心 / °C'); ax.set_ylabel('厂家换算压力均值 / hPa'); ax.legend(fontsize=8,ncol=2,frameon=False)
temp_parts.append(figure('四个模拟传感器的温度分箱均值',draw_binned_temp,'每个温度整数°C分箱取均值，便于观察温度区间变化；分箱图不替代同记录散点回归。',time_axis=False))
'''
if needle not in s: raise RuntimeError('needle missing')
s=s.replace(needle, needle+'\n'+insert)
# Add temperature section after stats section line, before quality
needle2="body += '<section id=\"quality\"><h2>日统计与记录质量</h2>"
section="body += '<section id=\"temperature\"><h2>温度对传感器读数的影响</h2><p>以下分析使用每条记录中的温度 I6 与同一时刻的目标量配对，不插值、不降采样。Pearson r 描述温度与读数的共同变化，线性斜率是该测试区间内的表观温度敏感度；压力变化、环境位置和传感器热惯性也会同时影响结果，因此不能仅凭相关性断言因果。</p>'+table(temp_rows,['板号','目标量','单位','有效点数','温度相关系数r','线性斜率/°C','截距','最小','最大','峰峰值'])+''.join(temp_parts)+'</section>'\n"
if needle2 not in s: raise RuntimeError('needle2 missing')
s=s.replace(needle2,section+needle2)
# nav link
s=s.replace('<a href="#stats">统计</a>', '<a href="#stats">统计</a><a href="#temperature">温度影响</a>')
p.write_text(s,encoding='utf8')
