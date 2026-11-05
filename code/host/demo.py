"""用于协议验证的纯软件 MCU 串口模拟器。

Deterministic fake MCU, deliberately damages first copies to exercise NACK."""
import time
import zlib
from datetime import datetime
from protocol import crc16, data_frame


class DemoSerial:
    """纯软件演示串口：模拟 MCU 命令、历史数据和损坏帧。"""
    def __init__(self):
        """初始化本对象的状态、缓存及所需资源；参数含义见下方。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
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
        self.window_enabled = False
        self.sampling_paused = False
        self.batch_size = 0

    def text(self, text):
        """把一行模拟 MCU 文本追加到串口接收缓冲区。

        参数：
        text：命令、数据或提示文本。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        self.buffer.extend((text + "\r\n").encode())

    def write(self, data):
        """解析电脑发送的命令和 ACK/NACK，推进模拟 MCU 状态并生成响应。

        参数：
        data：待发送字节串。

        返回：已消费的字节数。"""
        line = data.decode().strip()
        self.commands.append(line)
        if line.startswith("link crc on "):
            self.session = int(line[12:], 16)
            self.seq = 1
            self.text(f"LINK CRC ON {self.session:08X}")
        elif line == "link crc off":
            self.session = 0
            self.sampling_paused = False
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
        elif line == "link window 16":
            self.window_enabled = True
            self.text("LINK WINDOW OK: 16")
        elif line in ("link batch 100","link batch 1000"):
            self.batch_size = int(line.split()[-1])
            self.window_enabled = True
            self.text(f"LINK BATCH OK: {self.batch_size}")
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
            self.sampling_paused = True
            self.text("LOG DUMP SAMPLING: PAUSED")
            self.count = 0
            self.hpa = " hpa" in line
            self.history = iter(range(self.record_count) if self.order == "old" else range(self.record_count-1, -1, -1))
            self.text(f"LOG DUMP BEGIN: scanned={self.record_count}")
        elif line == "exit":
            if self.sampling_paused:
                self.text("LOG DUMP SAMPLING: RESUMED")
                self.sampling_paused = False
            self.setting = False
            self.window_enabled = False
            self.batch_size = 0
        elif line == "log info":
            self.text(f"LOG INFO: count={self.record_count} capacity=1000 interval={self.interval}s "
                      f"order={'oldest-first' if self.order == 'old' else 'newest-first'} recording=on display=all")
        return len(data)

    @property
    def in_waiting(self):
        """返回模拟接收缓冲区当前可读取的字节数。

        返回：当前缓冲区可读字节数。"""
        return len(self.buffer)

    def read(self, size):
        """按指定上限返回模拟串口数据，必要时生成实时帧或历史批次。

        参数：
        size：测试输入，约束见调用处及断言。

        返回：最多指定长度的字节串。"""
        if self.session and not self.pending:
            payload = None
            if self.history is not None and self.window_enabled:
                frames = []
                for _ in range(self.batch_size or 16):
                    try:
                        i = next(self.history)
                    except StopIteration:
                        break
                    adc = "1021.73,920.83,1004.29,933.10" if self.hpa else "41771.30,36482.80,40860.30,37157.00"
                    frames.append(f"samples: 2026,11,2,8,{i // 60},{i % 60},25.91,53.95,1024.20,1023.90,{adc},8.14".encode())
                if frames:
                    row_count = len(frames)
                    if self.batch_size:
                        checksum = zlib.crc32(b"".join(row+b"\n" for row in frames))
                        wire = data_frame(self.session,self.seq,b"BATCH BEGIN")
                        wire += b"".join(row+b"\r\n" for row in frames)
                        wire += data_frame(self.session,self.seq+1,f"BATCH END count={row_count} crc={checksum:08X}".encode())
                        self.seq += 1
                    else:
                        frames = [b"WINDOW BEGIN"] + frames + [b"WINDOW END"]
                        wire = b"".join(data_frame(self.session, self.seq+j, row) for j,row in enumerate(frames))
                        self.seq += len(frames)-1  # ACK refers to END.
                    self.count += row_count
                    self.pending = wire
                    if self.batch_size or (self.count//16) % 7 == 0:
                        wire = wire[:100] + bytes([wire[100]^1]) + wire[101:]
                    self.buffer.extend(wire)
                else:
                    self.history = None
                    payload = f"LOG DUMP DONE: matched={self.count} read_errors=0"
            if self.history is not None:
                if self.window_enabled:
                    chunk = bytes(self.buffer[:size])
                    del self.buffer[:size]
                    return chunk
                try:
                    i = next(self.history)
                    adc = "1021.729,920.829,1004.287,933.099" if self.hpa else "41771.30,36482.80,40860.30,37157.00"
                    payload = f"samples: 2026,11,2,8,{i // 60},{i % 60},25.91,53.95,1024.20,1023.90,{adc},8.14"
                    self.count += 1
                except StopIteration:
                    payload = f"LOG DUMP DONE: matched={self.count} read_errors=0"
                    self.history = None
            elif payload is None and not self.setting and time.monotonic() > self.next_live:
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
        """关闭模拟串口，保留与 pyserial 一致的调用接口。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        pass
