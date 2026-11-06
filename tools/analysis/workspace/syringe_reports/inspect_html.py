from pathlib import Path
s=next(Path('output/reports/针筒实验报告_20261030').glob('1*.html')).read_text(encoding='utf8');i=s.find('class="plot"');print(s[i-350:i+100])
