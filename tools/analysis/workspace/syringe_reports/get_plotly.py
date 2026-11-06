from pathlib import Path
import urllib.request
p=Path('tools/analysis/workspace/syringe_reports/plotly.min.js')
if not p.exists():
 urllib.request.urlretrieve('https://cdn.plot.ly/plotly-2.35.2.min.js',p)
print(p.stat().st_size)
