import pandas as pd,numpy as np
p=r'code/log/10301415_针筒第一次测试.csv'; d=pd.read_csv(p); d.columns=[f'I{i}' for i in range(15)]; d['t']=pd.to_datetime(dict(year=d.I0.astype(int),month=d.I1.astype(int),day=d.I2.astype(int),hour=d.I3.astype(int),minute=d.I4.astype(int),second=d.I5.astype(int))); d=d[(d.t>='2026-10-30 14:00:00')&(d.t<='2026-10-30 14:12:11')].copy()
for c in ['I10','I12']:
 d['P'+c]=22.5+25*d[c]/65535*5
 for ref in ['I8','I9']:
  e=d['P'+c]-d[ref]/10
  print(c,ref,'bias',e.mean(),'MAE',e.abs().mean(),'RMSE',np.sqrt(np.mean(e*e)),'maxabs',e.abs().max(),'corr',d[['P'+c,ref]].corr().iloc[0,1])
