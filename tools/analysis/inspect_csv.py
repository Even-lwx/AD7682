import pandas as pd
for p in [r'code/log/10301415_针筒第一次测试.csv',r'code/log/10301655_2号板针筒第一次测试.csv']:
 d=pd.read_csv(p); d['t']=pd.to_datetime(dict(year=d.I0.astype(int),month=d.I1.astype(int),day=d.I2.astype(int),hour=d.I3.astype(int),minute=d.I4.astype(int),second=d.I5.astype(int))); print(p,len(d),d.t.min(),d.t.max(),d.t.sort_values().iloc[0],d.t.sort_values().iloc[-1])
