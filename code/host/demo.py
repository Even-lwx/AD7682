"""Deterministic fake MCU, deliberately damages first copies to exercise NACK."""
import time
from datetime import datetime
from protocol import crc16, data_frame


class DemoSerial:
    def __init__(self):
        self.buffer = bytearray()
        self.seq = 1
        self.session = 0
        self.pending = None
        self.setting = False
        self.history = None
        self.count = 0
        self.next_live = 0
        self.order = "old"
        self.interval = "0.5"
        self.record_count = 1000
        self.commands = []

    def text(self, text):
        self.buffer.extend((text + "\r\n").encode())

    def write(self, data):
        line = data.decode().strip()
        self.commands.append(line)
        if line.startswith("link crc on "):
            self.session = int(line[12:], 16)
            self.seq = 1
            self.text(f"LINK CRC ON {self.session:08X}")
        elif line == "link crc off":
            self.session = 0
        elif line.startswith("@"):
            body, check = line[1:].split("*")
            if crc16(body.encode()) == int(check, 16):
                kind, session, seq = body.split(",")
                if self.pending and int(session, 16) == self.session and int(seq, 16) == self.seq:
                    if kind == "N":
                        self.buffer.extend(self.pending)
                    else:
                        self.pending = None
                        self.seq += 1
        elif line == "setting":
            self.setting = True
            self.text("SETTING MODE")
        elif line.startswith("log order "):
            self.order = line.split()[-1]
            self.text("LOG ORDER OK: " + ("oldest-first" if self.order == "old" else "newest-first"))
        elif line.startswith("log interval "):
            self.interval = f"{float(line.split()[-1]):.1f}"
            self.text("LOG INTERVAL OK")
        elif line == "log clear":
            self.record_count = 0
            self.text("LOG CLEAR OK")
        elif line.startswith("SET "):
            self.rtc = datetime.strptime("20"+line[4:], "%Y-%m-%d %H:%M:%S")
            self.text("RTC SET OK: " + self.rtc.strftime("%Y-%m-%d %H:%M:%S") + " VL=0")
        elif line.startswith("log dump "):
            self.count = 0
            self.hpa = " hpa" in line
            self.history = iter(range(self.record_count) if self.order == "old" else range(self.record_count-1, -1, -1))
            self.text(f"LOG DUMP BEGIN: scanned={self.record_count}")
        elif line == "exit":
            self.setting = False
        elif line == "log info":
            self.text(f"LOG INFO: count={self.record_count} capacity=1000 interval={self.interval}s "
                      f"order={'oldest-first' if self.order == 'old' else 'newest-first'} recording=on display=all")
        return len(data)

    def read(self, size):
        if self.session and not self.pending:
            payload = None
            if self.history is not None:
                try:
                    i = next(self.history)
                    adc = "1021.729,920.829,1004.287,933.099" if self.hpa else "41771.30,36482.80,40860.30,37157.00"
                    payload = f"samples: 2026,11,2,8,{i // 60},{i % 60},25.91,53.95,1024.20,1023.90,{adc},8.14"
                    self.count += 1
                except StopIteration:
                    payload = f"LOG DUMP DONE: matched={self.count} read_errors=0"
                    self.history = None
            elif not self.setting and time.monotonic() > self.next_live:
                payload = "samples: 8.14,25.91,53.95,1024.2,25.8,1023.9,25.7,41771.3,36482.8,40860.3,37157,1021,1022,1023,41770,36480,40860,37150"
                self.next_live = time.monotonic() + 0.2
            if payload:
                self.pending = data_frame(self.session, self.seq, payload.encode())
                frame = self.pending
                if self.seq % 71 == 0:
                    frame = frame[:-8] + bytes([frame[-8] ^ 1]) + frame[-7:]
                self.buffer.extend(frame)
        if not self.buffer:
            time.sleep(0.005)
        chunk = bytes(self.buffer[:size])
        del self.buffer[:size]
        return chunk

    def close(self):
        pass
