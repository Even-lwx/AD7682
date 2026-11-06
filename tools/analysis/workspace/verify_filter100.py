"""Reference checks for median100 -> fractional mean25 at 100 Hz."""
from collections import deque
import random


def filtered(samples):
    raw, medians = deque(maxlen=100), deque(maxlen=25)
    ordered = []
    for x in samples:
        if len(raw) == 100:
            ordered.remove(raw[0])
        raw.append(x)
        # Mirror the firmware's incremental remove/insert; cross-check against
        # independent full sorting each time, including duplicates and rails.
        i = len(ordered)
        ordered.append(x)
        while i > 0 and ordered[i - 1] > x:
            ordered[i] = ordered[i - 1]
            i -= 1
        ordered[i] = x
        assert ordered == sorted(raw)
        median = ordered[len(raw) // 2]
        if len(raw) % 2 == 0:
            median = (median + ordered[len(raw) // 2 - 1]) / 2
        if len(raw) == 100:
            medians.append(median)
            yield sum(medians) / len(medians)
        else:
            yield median


# Sweep pulse phase at the observed approximate frequency. Include broader
# 20 ms pulses and positive/negative disturbances, not only isolated samples.
for width_ms in (2, 10, 20):
    for phase_ms in range(119):
        for amplitude in (-280, 280):
            samples = [
                30000 + (amplitude if (i * 10 + phase_ms) % 118.6 < width_ms else 0)
                for i in range(1200)
            ]
            result = list(filtered(samples))
            assert set(result[150:]) == {30000}, (width_ms, phase_ms, amplitude)

# Up to 49 consecutive corrupt scans in a full window are rejected.
samples = [30000] * 150 + [30280] * 49 + [30000] * 150
assert set(list(filtered(samples))[150:]) == {30000}
# A persistent true step is fully followed within 75 scans / 750 ms.
result = list(filtered([30000] * 100 + [31000] * 100))
assert result[174] == 31000
assert all(30000 <= x <= 31000 for x in result)
# Keep arithmetic valid at both code rails.
assert list(filtered([65535] * 100))[-1] == 65535
assert list(filtered([0] * 100))[-1] == 0
random.seed(100)
list(filtered([random.randrange(65536) for _ in range(10000)]))
assert list(filtered([30000, 30001] * 100))[-1] == 30000.5
print("PASS: 714 periodic phase/sign/width cases, burst49, step750ms, rails, 10000 random multiset checks")
