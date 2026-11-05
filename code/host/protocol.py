"""可靠串口协议与 VOFA 格式 CSV 读写。

Wire protocol and VOFA FireWater CSV serialization (no GUI dependencies)."""
from __future__ import annotations

import csv
import math
import os
import re
import struct
import threading
import zlib
from datetime import datetime
from pathlib import Path


def crc16(data: bytes) -> int:
    """计算 CRC16/MODBUS，初值 FFFF、多项式 A001，无最终异或。

    参数：
    data：待发送字节串。

    返回：16 位 CRC 整数。

    实现说明：CRC-16/MODBUS: init FFFF, reflected polynomial A001, no final XOR."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return crc


def data_frame(session: int, sequence: int, payload: bytes) -> bytes:
    """组装带 CRC16 的数据帧，负载长度、会话号和序号一并参与校验。

    参数：
    session：32 位链路会话号。
    sequence：测试输入，约束见调用处及断言。
    payload：数据负载。

    返回：可直接发送的完整帧字节串。"""
    body = f"D,{session:08X},{sequence:08X},{len(payload):04X}:".encode() + payload
    return b"\r\n@" + body + f"*{crc16(body):04X}\r\n".encode()


def control_frame(kind: str, session: int, sequence: int) -> bytes:
    """组装 ACK/NACK 控制帧，kind 必须是 A 或 N。

    参数：
    kind：配置或事件类型。
    session：32 位链路会话号。
    sequence：测试输入，约束见调用处及断言。

    返回：可直接发送的完整帧字节串。"""
    assert kind in ("A", "N")
    body = f"{kind},{session:08X},{sequence:08X}".encode()
    return b"@" + body + f"*{crc16(body):04X}\r\n".encode()


def unpack_frame(line: bytes) -> tuple[int, int, bytes]:
    """校验数据帧格式、长度和 CRC16；失败抛出 ValueError。

    参数：
    line：不含 CRLF 的原始字节行。

    返回：三元组 (会话号, 序号, 负载字节)。"""
    match = re.fullmatch(rb"@D,([0-9A-Fa-f]{8}),([0-9A-Fa-f]{8}),([0-9A-Fa-f]{4}):(.*)\*([0-9A-Fa-f]{4})", line)
    if not match:
        raise ValueError("帧结构损坏")
    session, seq, length, payload, checksum = match.groups()
    if len(payload) != int(length, 16) or crc16(line[1:-5]) != int(checksum, 16):
        raise ValueError("CRC 或长度错误")
    return int(session, 16), int(seq, 16), payload


def sample_values(payload: str, historical: bool = False) -> list[str]:
    """校验采样行并按 VOFA 的 float32 与三位小数规则转换，历史行须为 15 列。

    参数：
    payload：数据负载。
    historical：是否为 15 列历史数据。

    返回：格式化后的数字字符串列表。"""
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
    # VOFA 先将 FireWater 解析为 float32，再显示或导出三位小数。
    # 例如固件的 41771.30 转为 41771.30078125，最终 CSV 为 41771.301。
    return [f"{struct.unpack('<f', struct.pack('<f', value))[0]:.3f}" for value in numbers]


class ReliableReceiver:
    """先提交再确认的可靠接收器，支持逐条、窗口与整批去重。

Stop-and-wait: commit once before ACK; ACK duplicate retransmits again.

    NACK once per expected sequence. Subsequent damage waits for MCU's bounded
    retry/failure; a broken newline must not trigger multiple retries at once.
    """

    def __init__(self, session, send, accept, event):
        """初始化本对象的状态、缓存及所需资源；参数含义见下方。

        参数：
        session：32 位链路会话号。
        send：发送控制帧的回调。
        accept：提交已验证负载的回调。
        event：上报协议事件的回调。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
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
        self.window_mode = False
        self.window_start = None
        self.window_next = None
        self.window_rows = []
        self.window_bad = False
        self.window_last = None
        self.window_nacked = None
        self.commit = lambda: None
        self.batch_size = 0
        self.batch_crc = 0

    def batch_row(self, line):
        """累计一行原始数据的 CRC32，校验内容后暂存至当前批次。

        参数：
        line：不含 CRLF 的原始字节行。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        if self.window_start is None:
            self.window_bad = True
            return
        self.batch_crc = zlib.crc32(line+b"\n", self.batch_crc)
        if len(self.window_rows) >= self.batch_size:
            self.window_bad = True
            return
        try:
            text = line.decode("ascii")
            sample_values(text, historical=True)
            self.window_rows.append(text)
        except (ValueError, UnicodeError, OverflowError):
            self.window_bad = True

    def batch_frame(self, session, seq, payload):
        """处理批次起止帧，整批校验通过后提交 CSV 并确认；损坏则请求重发。

        参数：
        session：32 位链路会话号。
        seq：控制帧序号。
        payload：数据负载。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        if payload == b"BATCH BEGIN":
            if seq != self.expected and not (self.window_last and seq == self.window_last[0]):
                self.failed = True
                self.event("fatal", "批次起点不连续，导出中止")
                return
            self.window_start = seq
            self.window_next = (seq+1) & 0xFFFFFFFF
            self.window_rows = []
            self.batch_crc = 0
            self.window_bad = False
            return
        match = re.fullmatch(rb"BATCH END count=(\d+) crc=([0-9A-F]{8})",payload)
        if not match:
            self.reject("批次结束信息无效")
            return
        count, checksum = int(match[1]), int(match[2],16)
        good = (self.window_start is not None and seq == self.window_next and
                not self.window_bad and count == len(self.window_rows) and
                checksum == self.batch_crc and count <= self.batch_size)
        if good:
            if self.window_last == (self.window_start,seq):
                self.duplicates += count
            else:
                for text in self.window_rows:
                    self.accept(text)
                self.commit()
                self.valid += count
                self.expected = (seq+1) & 0xFFFFFFFF
                self.window_last = (self.window_start,seq)
                self.window_nacked = None
            self.send(control_frame("A",session,seq))
        else:
            self.bad += 1
            self.event("crc_error","批次 CRC32、条数或顺序错误，整批重发")
            if self.window_nacked != seq:
                self.send(control_frame("N",session,seq))
                self.nacks += 1
                self.window_nacked = seq
        self.window_start = None
        self.window_rows = []

    def window_frame(self, session, seq, payload):
        """处理兼容的 16 条窗口，整组有效才提交，重复窗口只补 ACK。

        参数：
        session：32 位链路会话号。
        seq：控制帧序号。
        payload：数据负载。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        if payload == b"WINDOW BEGIN":
            if seq != self.expected and not (self.window_last and seq == self.window_last[0]):
                self.failed = True
                self.event("fatal", "窗口起点不连续，导出已中止")
                return
            self.window_start = seq
            self.window_next = (seq+1) & 0xFFFFFFFF
            self.window_rows = []
            self.window_bad = False
            return
        if self.window_start is None:
            # Missing/corrupted BEGIN: a valid END gives the sequence to NACK.
            if payload == b"WINDOW END" and self.window_nacked != seq:
                self.send(control_frame("N", session, seq))
                self.nacks += 1
                self.window_nacked = seq
            else:
                self.window_bad = True
            return
        if seq != self.window_next:
            self.window_bad = True
        self.window_next = (seq+1) & 0xFFFFFFFF
        if payload == b"WINDOW END":
            if self.window_bad:
                if self.window_nacked != seq:
                    self.send(control_frame("N", session, seq))
                    self.nacks += 1
                    self.window_nacked = seq
            elif self.window_last == (self.window_start, seq):
                self.duplicates += len(self.window_rows)
                self.send(control_frame("A", session, seq))
            else:
                for text in self.window_rows:
                    self.accept(text)
                self.commit()
                self.valid += len(self.window_rows)
                self.expected = (seq+1) & 0xFFFFFFFF
                self.window_last = (self.window_start, seq)
                self.window_nacked = None
                self.send(control_frame("A", session, seq))
            self.window_start = None
            self.window_rows = []
            return
        try:
            text = payload.decode("ascii")
            sample_values(text, historical=True)
            if len(self.window_rows) >= 16:
                raise ValueError("窗口超过 16 条")
            self.window_rows.append(text)
        except (ValueError, UnicodeError, OverflowError) as exc:
            self.window_bad = True
            self.bad += 1
            self.event("crc_error", str(exc))

    def reject(self, reason):
        """登记接收错误；逐帧模式最多请求一次重发，窗口模式延迟到组尾处理。

        参数：
        reason：拒绝接收的原因。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        self.bad += 1
        self.event("crc_error", reason)
        if self.window_mode:
            self.window_bad = True
            return  # NACK at valid END; damaged END is recovered by MCU timeout.
        if not self.nack_sent and not self.failed:
            self.send(control_frame("N", self.session, self.expected))
            self.nack_sent = True
            self.nacks += 1

    def feed(self, chunk: bytes):
        """接收任意分片字节流，按行重同步、验帧并分发数据或状态事件。

        参数：
        chunk：本次接收的任意分片字节。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        self.buffer.extend(chunk)
        while b"\n" in self.buffer:
            line, _, tail = self.buffer.partition(b"\n")
            self.buffer = bytearray(tail)
            line = bytes(line).rstrip(b"\r")
            if not line:
                continue
            # 重新同步后，下一个 @D 之前可能残留上次不完整的发送内容。
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
                if self.batch_size and payload.startswith(b"BATCH "):
                    self.batch_frame(session,seq,payload)
                    continue
                if self.window_mode and not self.batch_size and (self.window_start is not None or
                                         payload.startswith(b"WINDOW ") or payload.startswith(b"samples:")):
                    self.window_frame(session, seq, payload)
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
                    # accept() 先写入数据行并核对计数，然后才发送确认。
                    self.accept(text)
                except (ValueError, UnicodeError, OverflowError) as exc:
                    self.reject(f"内容错误：{exc}")
                    continue
                self.send(control_frame("A", session, seq))
                self.expected = (self.expected + 1) & 0xFFFFFFFF
                self.nack_sent = False
                self.valid += 1
            else:
                if self.batch_size and self.window_start is not None:
                    # Progress/diagnostic lines are out of band. Everything
                    # else contributes to CRC, including damaged row prefixes.
                    if not line.startswith((b"LOG DUMP PROGRESS:", b"LOG DUMP SAMPLING:", b"LINK CRC FAILED")):
                        self.batch_row(line)
                        continue
                self.event("text", line.decode("ascii", errors="replace"))
        if len(self.buffer) > 2048:
            self.buffer.clear()
            self.reject("接收帧超长或缺少换行")


class Capture:
    """基于磁盘的采集文件，失败历史保持为 .part 临时文件。

Disk-backed capture. A failed dump stays .part; no unbounded RAM list."""

    def __init__(self, path: Path, historical=False, target: Path | None = None):
        """初始化本对象的状态、缓存及所需资源；参数含义见下方。

        参数：
        path：采集临时文件或源文件路径。
        historical：是否为 15 列历史数据。
        target：历史完成后的最终文件路径。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
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

    def append(self, payload: str, flush=True):
        """校验采样列并写入 CSV；可选择延迟 flush 到批次提交时执行。

        参数：
        payload：数据负载。
        flush：本次写行后是否立即刷新文件。

        返回：按 VOFA 规则格式化的采样列列表。"""
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
            if flush:
                self.file.flush()  # Flush once per ACK (row or window).
            self.rows += 1
        return values

    def flush(self):
        """在文件锁内刷新尚未关闭的采集文件，供确认发送前调用。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        with self.lock:
            if not self.closed:
                self.file.flush()

    def finish(self, expected: int, read_errors: int):
        """核对历史总行数和读取错误，只有完整成功才把临时文件替换成最终 CSV。

        参数：
        expected：MCU 报告的总记录数。
        read_errors：MCU 报告的 Flash 读取错误数。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        with self.lock:
            if self.rows != expected:
                raise ValueError(f"导出条数不一致：电脑 {self.rows} / MCU {expected}")
            self.close()
            self.complete = read_errors == 0
            if self.complete and self.target:
                # .part 文件与目标文件位于同一目录，因此最终替换为原子操作。
                os.replace(self.path, self.target)
                self.path = self.target

    def close(self):
        """在文件锁内 flush、fsync 并关闭采集文件，允许重复调用。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        with self.lock:
            if not self.closed:
                self.file.flush()
                os.fsync(self.file.fileno())
                self.file.close()
                self.closed = True

    def export(self, destination: Path):
        """按当前文件长度复制采集快照到临时文件，再原子替换目标 CSV。

        参数：
        destination：CSV 快照目标路径。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
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
            # 不要删除其他进程创建的 .writing 文件。
            raise
