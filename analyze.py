import pandas as pd, numpy as np
p=r'code/log/10301415_针筒第一次测试.csv'
df=pd.read_csv(p); df.columns=[f'I{i}' for i in range(15)]
df['t']=pd.to_datetime(dict(year=df.I0.astype(int),month=df.I1.astype(int),day=df.I2.astype(int),hour=df.I3.astype(int),minute=df.I4.astype(int),second=df.I5.astype(int)))
df=df.sort_values('t'); d=df[(df.t>='2026-10-30 14:00:00')&(df.t<='2026-10-30 14:12:11')].copy()
# voltage assumptions
for c in ['I10','I12']:
 d[c+'_V5']=d[c]/65535*5
 d[c+'_V33']=d[c]/65535*3.3
 # table inverse: P=22.5 + 25*V (since 0V=22.5 and 4.5=135)
 d[c+'_P']=22.5+25*d[c+'_V5']
print('rows',len(d))
for c in ['I10','I12']:
 print('\n',c,'code min max mean',d[c].min(),d[c].max(),d[c].mean())
 print('V5 min max mean',d[c+'_V5'].min(),d[c+'_V5'].max(),d[c+'_V5'].mean())
 print('P(table) min max mean',d[c+'_P'].min(),d[c+'_P'].max(),d[c+'_P'].mean())
 print('corr U7/U11',d[[c+'_P','I8','I9']].corr().iloc[0,1:].to_dict())
# intervals
ints=[('c1','14:06:39','14:06:47'),('c2','14:07:55','14:08:09'),('c3','14:09:11','14:09:37')]
for nm,a,b in ints:
 q=d[(d.t.dt.strftime('%H:%M:%S')>=a)&(d.t.dt.strftime('%H:%M:%S')<=b)]
 print('\n',nm,len(q))
 for c in ['I8','I9','I10','I12']:
  print(c,'mean',round(q[c].mean(),2),'min',round(q[c].min(),2),'max',round(q[c].max(),2))
 for c in ['I10','I12']:
  print(c,'V5 mean',round(q[c+'_V5'].mean(),4),'P mean',round(q[c+'_P'].mean(),2))
# baseline around 14:00-14:04
q=d[d.t<'2026-10-30 14:04:00']
print('\nbaseline',len(q),[(c,round(q[c].mean(),2),round(q[c+'_V5'].mean(),4),round(q[c+'_P'].mean(),2)) for c in ['I10','I12']])
