import csv
from collections import defaultdict
from datetime import datetime
from pathlib import Path

root = Path(r'D:/lwx/lwx-project/AD7682/code/log')
names = ['周四到周五-下午用新传感器的板子 - 副本.csv', '周四到周五-老板子 - 副本.csv']
groups = []
originals = []
for name in names:
    with (root / name).open(encoding='utf-8-sig', newline='') as stream:
        reader = csv.reader(stream)
        header = next(reader)
        rows = [r for r in reader if r]
    grouped = defaultdict(list)
    for row in rows:
        assert len(row) == 14, (name, row)
        stamp = datetime(*[int(float(v)) for v in row[:6]])
        grouped[stamp.replace(second=0)].append((stamp, row))
    for entries in grouped.values():
        entries.sort(key=lambda item: item[0])
    groups.append(grouped)
    originals.append(rows)

output = root / '周四到周五_两块板子按分钟对齐.csv'
out_rows = []
counts = defaultdict(int)
for minute in sorted(set(groups[0]) | set(groups[1])):
    left, right = groups[0].get(minute, []), groups[1].get(minute, [])
    for index in range(max(len(left), len(right))):
        l = left[index] if index < len(left) else None
        r = right[index] if index < len(right) else None
        status = '两板都有' if l and r else '仅新传感器板' if l else '仅老板子'
        counts[status] += 1
        # Keep the original numeric date/time layout; use the first board's
        # timestamp when both records fall within the same minute.
        cells = (l or r)[1][:6]
        for item in (l, r):
            cells.extend(item[1][6:] if item else [''] * 8)
        out_rows.append(cells)

headers = [f'I{i}' for i in range(22)]
with output.open('w', encoding='utf-8-sig', newline='') as stream:
    writer = csv.writer(stream)
    writer.writerow(headers)
    writer.writerows(out_rows)

with output.open(encoding='utf-8-sig', newline='') as stream:
    saved = list(csv.reader(stream))
assert len(saved) == len(out_rows) + 1
assert all(len(row) == len(headers) for row in saved)
for board, offset in [(0, 6), (1, 14)]:
    recovered = [r[offset:offset+8] for r in saved[1:] if r[offset]]
    assert sorted(recovered) == sorted(r[6:] for r in originals[board])
print('OUTPUT', output)
print('INPUT_ROWS', [len(rows) for rows in originals])
print('OUTPUT_ROWS', len(out_rows), 'COUNTS', dict(counts))
print('DUPLICATE_MINUTES', [sum(len(v)>1 for v in group.values()) for group in groups])
print('RANGE', out_rows[0][0], out_rows[-1][0])
