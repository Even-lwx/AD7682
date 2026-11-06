import csv,datetime as d,collections,json
from pathlib import Path
p=Path(r'D:/lwx/lwx-project/AD7682/code/log/1030-1102-1号板.csv')
rows=[];bad=[];widths=collections.Counter()
with p.open(encoding='utf-8-sig',newline='') as f:
 r=csv.reader(f);header=next(r)
 for line,row in enumerate(r,2):
  widths[len(row)]+=1
  try:
   parts=[float(x) for x in row[:6]]
   if any(x!=int(x) for x in parts):raise ValueError('fractional timestamp')
   dt=d.datetime(*map(int,parts))
   rows.append((dt,line,row))
  except Exception as e:bad.append((line,str(e),row[:6]))
diffs=collections.Counter(int((b[0]-a[0]).total_seconds()) for a,b in zip(rows,rows[1:]))
u=sorted(set(x[0] for x in rows));gaps=[]
for a,b in zip(u,u[1:]):
 sec=int((b-a).total_seconds())
 if sec>1:gaps.append((a,b,sec))
print('TOTAL',len(rows),'BAD',len(bad),'WIDTHS',dict(widths))
print('FIRST_LAST',rows[0][:2],rows[-1][:2],'RANGE',u[0],u[-1])
print('ORDER_DIFFS',diffs.most_common(15))
print('UNIQUE',len(u),'DUPLICATE_TIMESTAMPS',len(rows)-len(u),'SPAN_SECONDS',int((u[-1]-u[0]).total_seconds()),'MISSING_SECONDS',int((u[-1]-u[0]).total_seconds())+1-len(u))
print('GAP_COUNT',len(gaps),'GAP_DURATIONS',collections.Counter(s for a,b,s in gaps).most_common(15))
for a,b,s in sorted(gaps,key=lambda x:x[2],reverse=True)[:25]:
 print('GAP',a,b,'seconds',s,'missing_seconds',s-1)
 for i,(dt,line,row) in enumerate(rows):
  if dt==a or dt==b:
   print(' BOUNDARY row',line,'values',row)
print('BAD_ROWS',bad[:10])
print('DAILY_COUNTS',sorted(collections.Counter(x[0].date().isoformat() for x in rows).items()))
