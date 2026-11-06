import pandas as pd, numpy as np
for fn,a,b in [('10301415_针筒第一次测试.csv','14:00:00','14:12:11'),('10301655_2号板针筒第一次测试.csv','16:44:40','16:52:47')]:
 d=pd.read_csv('code/log/'+fn); d['t']=pd.to_datetime(dict(year=d.I0,month=d.I1,day=d.I2,hour=d.I3,minute=d.I4,second=d.I5)); d=d.set_index('t').sort_index().between_time(a,b); s=d.groupby(level=0).mean(); mask=s.I8>1200; blocks=(mask!=mask.shift()).cumsum(); print(fn,len(d),len(s),'gaps',s.index.to_series().diff().dt.total_seconds().max())
 for _,q in s[mask].groupby(blocks[mask]): print(q.index.min(),q.index.max(),q.I8.max())
 print(d[['I8','I9','I10','I12']].agg(['min','max']))
