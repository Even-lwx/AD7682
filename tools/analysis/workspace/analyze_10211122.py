from pathlib import Path
import numpy as np

p = Path(__file__).resolve().parents[3] / 'code/log/10211122_换内部参考源.csv'
a = np.loadtxt(p, delimiter=',', skiprows=1)
print('shape', a.shape)
for e in (990, 992, 999):
    print('event', e, a[a[:,0] == e, :12].tolist())
for ch in (0, 1):
    print('\nCHANNEL',ch)
    for phase in range(29,40):
        b=a[a[:,0] == ch*100+phase]
        if not len(b): print(phase,'missing'); continue
        blocks=np.split(b,np.flatnonzero(np.diff(b[:,1])<=0)+1)
        res=[]
        for z in blocks:
            r=z[:,2:7]; base=np.median(r,axis=1,keepdims=True)
            h=r-base>150
            amps=[]
            for row,mask in zip(r,h):
                if mask.any() and (~mask).any(): amps.extend((row[mask]-np.median(row[~mask])).tolist())
            res.append({'n':len(z),'base':round(float(np.median(r)),2),'range':[float(r.min()),float(r.max())],
                        'spikes':int(h.sum()),'bursts':int(h.any(1).sum()),'median_fails':int((h.sum(1)>=3).sum()),
                        'amp':round(float(np.median(amps)),1) if amps else None,'pos':h.sum(0).tolist(),
                        'states':np.unique(z[:,11]).tolist()})
        print(phase,res)
done=np.flatnonzero(a[:,0]==999)
n=a[done[-1]+1:] if len(done) else a[:0]
print('normal',len(n))
for j,name in zip([7,8,14,15],['filtered0','filtered1','raw0','raw1']):
    if len(n):
        x=n[:,j]; d=np.diff(x)
        print(name, 'pct',np.percentile(x,[0,1,50,99,100]).tolist(),'sd',float(np.std(x)),
              'dabs',np.percentile(np.abs(d),[50,90,99,100]).tolist())
