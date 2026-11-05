import csv
from pathlib import Path
p=Path(r'code/log/10301415_针筒第一次测试 - 副本.csv')
rows=[]
with p.open('r',encoding='utf-8-sig',newline='') as f:
    rd=csv.reader(f); header=next(rd); rows=list(rd)
for h in ['IN0_pressure_kPa','IN2_pressure_kPa']:
    if h in header:
        idx=header.index(h)
        header.pop(idx)
        for row in rows: row.pop(idx)
header += ['IN0_pressure_kPa','IN2_pressure_kPa']
for row in rows:
    in0=float(row[10]); in2=float(row[12])
    p0=22.5 + 125.0*in0/65535.0
    p2=22.5 + 125.0*in2/65535.0
    row += [f'{p0:.3f}',f'{p2:.3f}']
with p.open('w',encoding='utf-8',newline='') as f:
    wr=csv.writer(f,lineterminator='\n'); wr.writerow(header); wr.writerows(rows)
print('rows',len(rows),'headers',header)
print('first',rows[0])
print('last',rows[-1])
