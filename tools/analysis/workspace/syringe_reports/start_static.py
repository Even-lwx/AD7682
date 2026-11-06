from pathlib import Path
s=Path('tools/analysis/workspace/syringe_reports/build_longterm.py').read_text(encoding='utf8')
s=s[:s.index('# chart data and markup')]
Path('tools/analysis/workspace/syringe_reports/build_long_static.py').write_text(s,encoding='utf8')
