import re,json
from pathlib import Path
p=next(Path('output/reports/长期测试_1030-1102').glob('*.html'));s=p.read_text(encoding='utf8');m=re.search(r'const specs=(\[.*?\]),start=',s);z=json.loads(m.group(1));print('size',p.stat().st_size,'charts',len(z),'max series point count',max(len(t['x']) for q in z for t in q['traces']),'totalpoints',sum(len(t['x']) for q in z for t in q['traces']));print([q['title'] for q in z[-10:]])
