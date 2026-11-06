import pandas as pd,numpy as np
for n in [1,2]:
 p='code/log/'+('1030-1102-1号板合并.csv' if n==1 else '1030-1102-2号板.csv');d=pd.read_csv(p); t=pd.to_datetime(dict(year=d.I0,month=d.I1,day=d.I2,hour=d.I3,minute=d.I4,second=d.I5));d.index=t;d=d[d.index>='2026-10-30 18:00:00'].sort_index();q=d.groupby(level=0).mean();g=q.index.to_series().diff().dt.total_seconds(); print(n,len(d),len(q),q.index.min(),q.index.max(),'maxgap',g.max(),'gaps>2',sum(g>2));print(d[['I6','I7','I8','I9','I10','I12','I14']].agg(['min','max']));print('gaps',g[g>60].head(12).to_dict())
