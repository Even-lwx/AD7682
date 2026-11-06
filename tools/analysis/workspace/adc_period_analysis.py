import json
from collections import Counter
from pathlib import Path
import numpy as np
from numpy.lib.stride_tricks import sliding_window_view

ROOT = Path(__file__).resolve().parents[3]
LOG = ROOT / 'code/log'

def baseline(x, window=101):
    w=min(window, len(x)//2*2-1)
    return np.median(sliding_window_view(np.pad(x, (w//2,w//2), mode='edge'),w),axis=-1)

def periodic(x):
    x=np.asarray(x,float); x=x-x.mean(); n=len(x)
    nfft=1 << (4*n-1).bit_length()
    z=np.fft.rfft(x*np.hanning(n), nfft)
    p=abs(z)**2
    f=np.fft.rfftfreq(nfft)
    good=(f>max(3/n,0.002))&(f<0.5)
    loc=np.flatnonzero(good & (p>np.roll(p,1)) & (p>np.roll(p,-1)))
    top=sorted(loc,key=lambda k:p[k],reverse=True)[:8]
    ac=np.fft.irfft(abs(np.fft.rfft(x,nfft))**2,nfft)[:min(n//3,600)]
    ac=ac/(np.arange(n,n-len(ac),-1)*max(float(np.var(x)),1e-12))
    pk=[i for i in range(2,len(ac)-1) if ac[i]>ac[i-1] and ac[i]>ac[i+1]]
    pk=sorted(pk,key=lambda k:ac[k],reverse=True)[:12]
    return {'spectrum':[[round(float(f[i]),7),round(float(1/f[i]),4),round(float(p[i]/max(p.max(),1e-12)),4)] for i in top],
            'acf':[[i,round(float(ac[i]),4)] for i in pk]}

def analyze(fn,start=0,end=None,ch=2,raw16=False):
    a=np.loadtxt(LOG/fn,delimiter=',',skiprows=1)[start:end]
    if raw16:
        raw=a[:,7:23]
        out=np.median(raw,axis=1)
    else:
        raw=a[:,14+5*ch:19+5*ch]
        out=a[:,7+ch]
    b=baseline(np.median(raw,axis=1))
    delta=raw-b[:,None]
    mask=delta>150
    mask_abs=abs(delta)>150
    events=np.any(mask,axis=1)
    inds=np.flatnonzero(events)
    intervals=np.diff(inds)
    d={'name':fn,'slice':[start,start+len(a)],'ch':ch,'rows':len(a),
       'raw_rate':round(float(mask.mean()),5),'event_rate':round(float(events.mean()),5),
       'raw_positive_by_position':mask.sum(axis=0).tolist(),
       'positive_count_hist':dict(Counter(map(int,mask.sum(axis=1)))),
       'intervals':Counter(map(int,intervals)).most_common(12),
       'event_first':(inds[:35]+start).tolist(),
       'events':periodic(events), 'raw_magnitude':periodic(np.max(delta,axis=1)),
       'output':periodic(out-baseline(out)),
       'max_out_step':float(np.max(abs(np.diff(out)))),
       'negative':int(np.sum(delta < -150)),
       'baseline_med':float(np.median(b)),
       'raw_spread_gt200':int(np.sum(np.ptp(raw,axis=1)>200))}
    print(json.dumps(d,ensure_ascii=True))
    return a,raw,delta,events

if __name__=='__main__':
    for f,st,en,ch,r16 in [
        ('10201005.csv',0,None,2,False),
        ('10201112_增加转换时间.csv',0,None,2,False),
        ('10201355_感觉有一点改善.csv',0,5100,2,False),
        ('10201507-通道2和通道1.csv',100,5800,2,False),
        ('10201507-通道2和通道1.csv',7000,21000,1,False),
        ('10191323_16重复.csv',0,None,0,True),
        ('10191323_通道216重复.csv',0,None,2,True),
        ('10191420_换了一块板子通道216重复.csv',0,None,2,True)]:
        analyze(f,st,en,ch,r16)
