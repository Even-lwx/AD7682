"""Wire protocol and VOFA FireWater CSV serialization (no GUI dependencies)."""
from __future__ import annotations

import csv
import math
import os
import re
import struct
import threading
from datetime import datetime
from pathlib import Path


def crc16(data: bytes) -> int:
    """CRC-16/MODBUS: init FFFF, reflected polynomial A001, no final XOR."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return crc


def data_frame(session: int, sequence: int, payload: bytes) -> bytes:
    body = f"D,{session:08X},{sequence:08X},{len(payload):04X}:".encode() + payload
    return b"\r\n@" + body + f"*{crc16(body):04X}\r\n".encode()


def control_frame(kind: str, session: int, sequence: int) -> bytes:
    assert kind in ("A", "N")
    body = f"{kind},{session:08X},{sequence:08X}".encode()
    return b"@" + body + f"*{crc16(body):04X}\r\n".encode()


def unpack_frame(line: bytes) -> tuple[int, int, bytes]:
    match = re.fullmatch(rb"@D,([0-9A-Fa-f]{8}),([0-9A-Fa-f]{8}),([0-9A-Fa-f]{4}):(.*)\*([0-9A-Fa-f]{4})", line)
    if not match:
        raise ValueError("帧结构损坏")
    session, seq, length, payload, checksum = match.groups()
    if len(payload) != int(length, 16) or crc16(line[1:-5]) != int(checksum, 16):
        raise ValueError("CRC 或长度错误")
    return int(session, 16), int(seq, 16), payload


def sample_values(payload: str, historical: bool = False) -> list[str]:
    if not payload.startswith("samples:"):
        raise ValueError("不是采样行")
    parts = payload[8:].strip().split(",")
    if not 1 <= len(parts) <= 64 or (historical and len(parts) != 15):
        raise ValueError("采样列数不符")
    numbers = [float(part) for part in parts]
    if not all(math.isfinite(value) for value in numbers):
        raise ValueError("非有限数值")
    if historical:
        if any(value != int(value) for value in numbers[:6]):
            raise ValueError("无效日期时间")
        datetime(*map(int, numbers[:6]))
    # VOFA parses FireWater as float32 before displaying/exporting three decimals.
    # e.g. firmware's 41771.30 becomes 41771.30078125 -> CSV 41771.301.
    return [f"{struct.unpack('<f', struct.pack('<f', value))[0]:.3f}" for value in numbers]


class ReliableReceiver:
    """Stop-and-wait: commit once before ACK; ACK duplicate retransmits again.

    NACK once per expected sequence. Subsequent damage waits for MCU's bounded
    retry/failure; a broken newline must not trigger multiple retries at once.
    """

    def __init__(self, session, send, accept, event):
        self.session = session
        self.send = send
        self.accept = accept
        self.event = event
        self.expected = 1
        self.nack_sent = False
        self.valid = 0
        self.bad = 0
        self.duplicates = 0
        self.nacks = 0
        self.failed = False
        self.buffer = bytearray()

    def reject(self, reason):
        self.bad += 1
        self.event("crc_error", reason)
        if not self.nack_sent and not self.failed:
            self.send(control_frame("N", self.session, self.expected))
            self.nack_sent = True
            self.nacks += 1

    def feed(self, chunk: bytes):
        self.buffer.extend(chunk)
        while b"\n" in self.buffer:
            line, _, tail = self.buffer.partition(b"\n")
            self.buffer = bytearray(tail)
            line = bytes(line).rstrip(b"\r")
            if not line:
                continue
            # A partial previous transmit can precede the next @D after resync.
            marker = line.find(b"@D,")
            if marker >= 0:
                line = line[marker:]
            if line.startswith(b"@"):
                if self.failed:
                    continue
                try:
                    session, seq, payload = unpack_frame(line)
                except ValueError as exc:
                    self.reject(str(exc))
                    continue
                if session != self.session:
                    self.event("stale", "忽略其他会话的帧")
                    continue
                if seq == (self.expected - 1) & 0xFFFFFFFF:
                    self.duplicates += 1
                    self.send(control_frame("A", session, seq))
                    continue
                if seq != self.expected:
                    self.failed = True
                    self.event("fatal", f"帧序号不连续：期望 {self.expected}，收到 {seq}，传输中止")
                    continue
                try:
                    text = payload.decode("ascii")
                    # accept() writes the row and checks counts before ACK.
                    self.accept(text)
                except (ValueError, UnicodeError, OverflowError) as exc:
                    self.reject(f"内容错误：{exc}")
                    continue
                self.send(control_frame("A", session, seq))
                self.expected = (self.expected + 1) & 0xFFFFFFFF
                self.nack_sent = False
                self.valid += 1
            else:
                self.event("text", line.decode("ascii", errors="replace"))
        if len(self.buffer) > 2048:
            self.buffer.clear()
            self.reject("接收帧超长或缺少换行")


class Capture:
    """Disk-backed capture. A failed dump stays .part; no unbounded RAM list."""

    def __init__(self, path: Path, historical=False, target: Path | None = None):
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.target = target
        self.historical = historical
        self.columns = 15 if historical else None
        self.rows = 0
        self.complete = False
        self.closed = False
        self.lock = threading.RLock()
        self.file = self.path.open("x", encoding="ascii", newline="")
        self.writer = csv.writer(self.file, lineterminator="\r\n")
        if self.columns:
            self.writer.writerow([f"I{i}" for i in range(self.columns)])
            self.file.flush()

    def append(self, payload: str):
        values = sample_values(payload, self.historical)
        with self.lock:
            if self.closed:
                raise OSError("采集文件已关闭")
            if self.columns is None:
                self.columns = len(values)
                self.writer.writerow([f"I{i}" for i in range(self.columns)])
            if self.columns != len(values):
                raise ValueError("同一 CSV 中不能混入不同列数的数据")
            self.writer.writerow(values)
            self.file.flush()  # Commit to OS before acknowledging this frame.
            self.rows += 1
        return values

    def finish(self, expected: int, read_errors: int):
        with self.lock:
            if self.rows != expected:
                raise ValueError(f"导出条数不一致：电脑 {self.rows} / MCU {expected}")
            self.close()
            self.complete = read_errors == 0
            if self.complete and self.target:
                # .part and target share a directory, so finalization is atomic.
                os.replace(self.path, self.target)
                self.path = self.target

    def close(self):
        with self.lock:
            if not self.closed:
                self.file.flush()
                os.fsync(self.file.fileno())
                self.file.close()
                self.closed = True

    def export(self, destination: Path):
        destination = Path(destination)
        with self.lock:
            if self.columns is None:
                raise ValueError("还没有收到有效数据")
            if not self.closed:
                self.file.flush()
            length = self.path.stat().st_size
            source = self.path.open("rb")
        if destination.resolve() == self.path.resolve():
            source.close()
            return
        staging = destination.with_name(destination.name + ".writing")
        try:
            with source, staging.open("xb") as output:
                remaining = length
                while remaining:
                    block = source.read(min(remaining, 1024 * 1024))
                    if not block:
                        raise OSError("采集文件意外截断")
                    output.write(block)
                    remaining -= len(block)
                output.flush()
                os.fsync(output.fileno())
            os.replace(staging, destination)
        except BaseException:
            # Do not remove a .writing file created by another process.
            raise
