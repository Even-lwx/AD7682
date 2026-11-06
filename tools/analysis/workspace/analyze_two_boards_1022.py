from pathlib import Path
import numpy as np

root = Path(r'D:/lwx/lwx-project/AD7682/code/log')
files = ['10221638_前几天一直用的板子.csv', '10221659_下午用新传感器的板子.csv']

def rolling_median(x, radius=50):
    # 1-second local baseline at approximately 50 output rows/second.
    out = np.empty(len(x), dtype=float)
    for i in range(len(x)):
        lo=max(0,i-radius); hi=min(len(x),i+radius+1)
        out[i]=np.median(x[lo:hi])
    return out

for fn in files:
    a=np.loadtxt(root/fn, delimiter=',', skiprows=1)
    print('\nFILE',fn,'shape',a.shape,'duration_est_s',len(a)/50.0)
    if a.shape[1] != 18: print('WARNING unexpected column count')
    for ch in range(4):
        raw=a[:,14+ch]; filt=a[:,7+ch]
        base=rolling_median(raw)
        resid=raw-base
        dr=np.diff(raw); df=np.diff(filt)
        q=np.percentile(raw,[0,1,25,50,75,99,100])
        rq=np.percentile(np.abs(resid),[50,90,95,99,99.9,100])
        dq=np.percentile(np.abs(dr),[50,90,95,99,100])
        fq=np.percentile(np.abs(df),[50,90,95,99,100])
        spikes={th:int((resid>th).sum()) for th in (100,150,250,500)}
        neg={th:int((resid < -th).sum()) for th in (100,150,250,500)}
        print('CH',ch,'raw_pct',np.round(q,2).tolist(),'raw_sd',round(float(np.std(raw)),2),
              'local_abs_resid_pct',np.round(rq,2).tolist(),'resid_sd',round(float(np.std(resid)),2),
              'raw_absdiff_pct',np.round(dq,2).tolist(),'filtered_absdiff_pct',np.round(fq,3).tolist(),
              'positive_resid_counts',spikes,'negative_resid_counts',neg,
              'max_code',float(raw.max()),'sat65535',int((raw>=65530).sum()))
        # Divide into 10 equal time segments to expose phases/slow drift.
        bounds=np.linspace(0,len(raw),11,dtype=int)
        seg=[]
        for j in range(10):
            s=raw[bounds[j]:bounds[j+1]]
            if not len(s): continue
            seg.append((round(float(np.median(s)),1),round(float(np.std(s)),1),round(float(np.percentile(np.abs(np.diff(s)),90)),1)))
        print('CH',ch,'10pct(median,sd,diff90)',seg)
    # overall changes in temperature/pressure for context
    for col,name in [(1,'SHT_T'),(2,'SHT_RH'),(3,'U7_P'),(4,'U7_T'),(5,'U11_P'),(6,'U11_T')]:
        x=a[:,col]
        print(name,'p01/median/p99',np.round(np.percentile(x,[1,50,99]),3).tolist())
