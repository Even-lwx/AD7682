"""Check the 28-bit fraction layout and unchanged date/integer fields."""
import random
random.seed(20261020)


def encode(fractions, date):
    bits = sum(value << (7 * i) for i, value in enumerate(fractions))
    r = bytearray(32)
    r[2] = 3 | (((bits >> 8) & 63) << 2)
    r[3] = bits & 255
    shifts = [(9, 4, 14, 15), (10, 5, 18, 7), (11, 5, 21, 7),
              (12, 6, 24, 3), (13, 6, 26, 3)]
    for (offset, shift, source, mask), value in zip(shifts, date):
        r[offset] = value | (((bits >> source) & mask) << shift)
    return r


def decode(r):
    bits = (r[3] | ((r[2] >> 2) << 8) | ((r[9] >> 4) << 14) |
            ((r[10] >> 5) << 18) | ((r[11] >> 5) << 21) |
            ((r[12] >> 6) << 24) | ((r[13] >> 6) << 26))
    return ([(bits >> (7 * i)) & 127 for i in range(4)],
            [r[9] & 15, r[10] & 31, r[11] & 31, r[12] & 63, r[13] & 63])


for i in range(10000):
    fractions = [random.randrange(100) for _ in range(4)]
    date = [random.randint(1, 12), random.randint(1, 31),
            random.randrange(24), random.randrange(60), random.randrange(60)]
    encoded = encode(fractions, date)
    assert encoded[2] & 3 == 3
    assert decode(encoded) == (fractions, date)
for channel in range(4):
    for fraction in range(100):
        values = [0] * 4
        values[channel] = fraction
        assert decode(encode(values, [12, 31, 23, 59, 59]))[0] == values
print("PASS: 10000 fraction/date round trips; all 100 hundredths in every channel")
