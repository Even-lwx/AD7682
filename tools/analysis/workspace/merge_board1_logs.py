import csv,datetime as d,collections
from pathlib import Path
base=Path(r'D:/lwx/lwx-project/AD7682/code/log')
merged={};conflicts=[];overlap=0
for name in ['1030-1102-1号板.csv','1030-1102-1号板补充.csv']:
 n=0;dates=[]
 with (base/name).open(encoding='utf-8-sig',newline='') as f:
  r=csv.reader(f);header=next(r)
  assert header==['I'+str(i) for i in range(15)]
  for line,row in enumerate(r,2):
   assert len(row)==15,(name,line)
   vals=list(map(float,row));parts=vals[:6]
   assert all(x==int(x) for x in parts)
   dt=d.datetime(*map(int,parts));n+=1;dates.append(dt)
   if dt in merged:
    overlap+=1
    if list(map(float,merged[dt]))!=vals:conflicts.append((str(dt),merged[dt],row))
   else:merged[dt]=row
 print(name,'rows',n,'range',min(dates),max(dates))
times=sorted(merged)
gaps=[(a,b,int((b-a).total_seconds())) for a,b in zip(times,times[1:]) if (b-a).total_seconds()>1]
print('UNIQUE',len(merged),'OVERLAP',overlap,'CONFLICTS',len(conflicts))
print('CONFLICT_EXAMPLES',conflicts[:5])
print('GAP_DURATIONS',collections.Counter(s for a,b,s in gaps))
print('LARGE_GAPS',[(str(a),str(b),s) for a,b,s in gaps if s>3])
print('MISSING_SECONDS_AT_1HZ',sum(s-1 for a,b,s in gaps))
for lo,hi in [('2026-11-02 07:36:22','2026-11-02 07:43:02'),('2026-10-31 18:56:43','2026-10-31 18:58:12')]:
 a=d.datetime.fromisoformat(lo);b=d.datetime.fromisoformat(hi)
 print('OLD_GAP',lo,hi,'present_seconds',sum(a<x<b for x in merged),'expected',int((b-a).total_seconds())-1)
assert not conflicts, 'Conflicting records require review'
output=base/'1030-1102-1号板合并.csv'
with output.open('x',encoding='utf-8-sig',newline='') as f:
 w=csv.writer(f);w.writerow(header)
 for dt in reversed(times):w.writerow(merged[dt])
with output.open(encoding='utf-8-sig',newline='') as f:
 r=csv.reader(f);assert next(r)==header
 result=list(r)
assert len(result)==len(merged)
assert result==[merged[dt] for dt in reversed(times)]
a=d.datetime(2026,11,2,7,36,22);b=d.datetime(2026,11,2,7,43,2)
missing=[];x=a+d.timedelta(seconds=1)
while x<b:
 if x not in merged:missing.append(str(x))
 x+=d.timedelta(seconds=1)
print('MISSING_IN_NOV2_GAP',missing)
print('SAVED_AND_VERIFIED',str(output),'ROWS',len(result),'BYTES',output.stat().st_size)
