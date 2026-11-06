import pandas as pd,numpy as np
p=r'code/log/10301415_针筒第一次测试.csv'; d=pd.read_csv(p); d.columns=[f'I{i}' for i in range(15)]; d['t']=pd.to_datetime(dict(year=d.I0.astype(int),month=d.I1.astype(int),day=d.I2.astype(int),hour=d.I3.astype(int),minute=d.I4.astype(int),second=d.I5.astype(int))); d=d[(d.t>='2026-10-30 14:00:00')&(d.t<='2026-10-30 14:12:11')].copy()
for c in ['I10','I12']:
 d[c+'V']=d[c]/65535*5; d[c+'P']=22.5+25*d[c+'V']
for nm,a,b in [('基线','14:00:00','14:04:00'),('第1','14:06:39','14:06:47'),('第2','14:07:55','14:08:09'),('第3','14:09:11','14:09:37')]:
 q=d[(d.t.dt.strftime('%H:%M:%S')>=a)&(d.t.dt.strftime('%H:%M:%S')<=b)]
 print(nm,'U7kPa',q.I8.mean()/10,'expV', (q.I8.mean()/10-22.5)/25)
 for c in ['I10','I12']:
  print(c,'V',q[c+'V'].mean(),'P',q[c+'P'].mean(),'errP',q[c+'P'].mean()-q.I8.mean()/10,'errV',q[c+'V'].mean()-(q.I8.mean()/10-22.5)/25)
