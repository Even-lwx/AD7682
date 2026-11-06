import csv
from datetime import datetime
from pathlib import Path

root = Path(r'D:/lwx/lwx-project/AD7682/code/log')
names = ['周四到周五-下午用新传感器的板子 - 副本.csv', '周四到周五-老板子 - 副本.csv']
boards = []
for name in names:
    with (root / name).open(encoding='utf-8-sig', newline='') as stream:
        rows = list(csv.reader(stream))[1:]
    boards.append(sorted((datetime(*[int(float(v)) for v in r[:6]]), r) for r in rows if r))

pairs = []
used = set()
for stamp, row in boards[0]:
    nearest = min(range(len(boards[1])), key=lambda j: abs((boards[1][j][0] - stamp).total_seconds()))
    assert nearest not in used, 'Nearest pairing conflict; must resolve before export'
    used.add(nearest)
    other_stamp, other_row = boards[1][nearest]
    delta = abs((other_stamp - stamp).total_seconds())
    assert delta <= 30, (stamp, other_stamp, delta)
    pairs.append((row + other_row[6:], delta))

output = root / '周四到周五_两块板子按分钟对齐.csv'
with output.open('w', encoding='utf-8-sig', newline='') as stream:
    writer = csv.writer(stream)
    writer.writerow([f'I{i}' for i in range(22)])
    writer.writerows(row for row, _ in pairs)

with output.open(encoding='utf-8-sig', newline='') as stream:
    saved = list(csv.reader(stream))[1:]
assert len(saved) == len(boards[0])
assert all(len(row) == 22 and all(cell != '' for cell in row) for row in saved)
assert [row[:14] for row in saved] == [row for _, row in boards[0]]
assert [row[14:] for row in saved] == [boards[1][j][1][6:] for j in sorted(used)]
print('matched', len(pairs), 'old_unmatched', len(boards[1]) - len(used))
print('time_difference_seconds_min_max', min(d for _, d in pairs), max(d for _, d in pairs))
print('new_sensor_ADC_ranges', [(min(float(r[c]) for r in saved), max(float(r[c]) for r in saved)) for c in (11, 13)])
print('unmatched_old_times', [stamp.isoformat(' ') for j, (stamp, _) in enumerate(boards[1]) if j not in used])
