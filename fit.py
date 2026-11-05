import pandas as pd,numpy as np
p=r'code/log/10301415_针筒第一次测试.csv'; d=pd.read_csv(p); d.columns=[f'I{i}' for i in range(15)]; d['t']=pd.to_datetime(dict(year=d.I0.astype(int),month=d.I1.astype(int),day=d.I2.astype(int),hour=d.I3.astype(int),minute=d.I4.astype(int),second=d.I5.astype(int))); d=d[(d.t>='2026-10-30 14:00:00')&(d.t<='2026-10-30 14:12:11')].copy()
for c in ['I10','I12']:
 p=22.5+25*d[c]/65535*5; x=d.I8/10; a,b=np.polyfit(x,p,1); print(c,'slope',a,'intercept',b)
