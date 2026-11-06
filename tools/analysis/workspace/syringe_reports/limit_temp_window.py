from pathlib import Path
p=Path('tools/analysis/workspace/syringe_reports/_generated_nov5_6_ad7682.py')
s=p.read_text(encoding='utf8')
s=s.replace("def temp_fit(board, col, label, unit):\n    d=boards[board]['data'][['I6',col]].dropna()", "def temp_fit(board, col, label, unit, frame=None):\n    d=(boards[board]['data'] if frame is None else frame)[['I6',col]].dropna()")
s=s.replace("    d=boards[board]['data']\n    targets=[(f'S{sid}_hPa'", "    full_d=boards[board]['data']\n    _ge40=full_d[full_d['I6']>=40]\n    temp_end=_ge40.index.min() if len(_ge40) else full_d.index.max()\n    d=full_d.loc[:temp_end]\n    temp_ranges[board]=(d.index.min(), d.index.max(), float(d['I6'].max()))\n    targets=[(f'S{sid}_hPa'", 1)
s=s.replace("        q,sl,it=temp_fit(board,col,label,unit); fitted.append((col,label,unit,q,sl,it))", "        q,sl,it=temp_fit(board,col,label,unit,frame=d); fitted.append((col,label,unit,q,sl,it))")
s=s.replace("# Temperature influence analysis: same-record pairing, no interpolation.\ntemp_rows=[]", "# Temperature influence analysis: only from 15:22 until first reaching 40 C, no interpolation.\ntemp_rows=[]\ntemp_ranges={}")
# binned chart use truncated d
s=s.replace("        d=boards[board]['data']\n        for j,(sid,ch) in enumerate(zip(sensors,[0,2])):", "        d=boards[board]['data'].loc[:temp_ranges[board][1]]\n        for j,(sid,ch) in enumerate(zip(sensors,[0,2])):", 1)
s=s.replace("body += '<section id=\"temperature\"><h2>温度对传感器读数的影响</h2><p>以下分析使用每条记录中的温度 I6 与同一时刻的目标量配对，不插值、不降采样。", "body += '<section id=\"temperature\"><h2>温度对传感器读数的影响</h2><p>本节只分析各板从 2026-11-05 15:22:00 开始，到该板温度首次达到40°C为止的区间。1号板截止 '+str(temp_ranges['1号板'][1])+'，2号板截止 '+str(temp_ranges['2号板'][1])+'。区间外的温度影响数据不纳入本节。以下分析使用同一条记录中的温度 I6 与目标量配对，不插值、不降采样。")
# navigation method text perhaps no need
p.write_text(s,encoding='utf8')
