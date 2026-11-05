"""Serial worker: all I/O and capture writes run outside Tk's UI thread."""
import os
import queue
import re
import secrets
import threading
import time
from datetime import datetime
from decimal import Decimal, InvalidOperation
from pathlib import Path

from protocol import Capture, ReliableReceiver


def interval_seconds(value):
    """Canonical MCU seconds: 0.5..86400 in exact half-second increments."""
    try:
        seconds = Decimal(str(value).strip())
        if not seconds.is_finite() or not Decimal("0.5") <= seconds <= 86400 or seconds % Decimal("0.5"):
            raise ValueError()
    except (InvalidOperation, ValueError):
        raise ValueError("记录间隔须为 0.5～86400 秒，且为 0.5 秒的整数倍") from None
    return format(seconds.quantize(Decimal("0.1")), "f")


def rtc_datetime(value):
    try:
        result = datetime.strptime(str(value).strip(), "%Y-%m-%d %H:%M:%S")
        if not 2000 <= result.year <= 2099:
            raise ValueError()
    except ValueError:
        raise ValueError("日期时间须为 YYYY-MM-DD HH:MM:SS，年份 2000～2099") from None
    return result


def log_info(text):
    match = re.fullmatch(r"LOG INFO: count=(\d+) capacity=(\d+) interval=(\d+\.\d)s "
                         r"order=(oldest-first|newest-first) recording=(on|off) display=(all|adc)", text)
    if not match:
        raise ValueError("MCU 返回的日志状态格式无效")
    count, capacity, interval, order, recording, display = match.groups()
    return dict(count=int(count), capacity=int(capacity), interval=interval,
                order=order, recording=recording, display=display)


class Device:
    def __init__(self, events):
        self.events = events
        self.jobs = queue.Queue()
        self.capture = None
        self.running = False
        self.ready = False
        self.phase = ""
        self.deadline = 0
        self.dump = None
        self.finished = True
        self.config = None

    def start_config(self, kind, value):
        if kind == "interval":
            value = interval_seconds(value)
        elif kind == "order":
            if value not in ("old", "new"):
                raise ValueError("输出顺序只能为从旧到新或从新到旧")
        elif kind == "rtc":
            value = rtc_datetime(value)
        elif kind not in ("clear", "rtc_now"):
            raise ValueError("未知设备设置")
        self.config = (kind, value)
        self.phase = "config_setting"
        self.deadline = time.monotonic() + 5
        self.line("setting")

    def apply_config(self):
        kind, value = self.config
        if kind == "rtc_now":
            value = datetime.now().replace(microsecond=0)
            if not 2000 <= value.year <= 2099:
                raise ValueError("电脑日期不在 MCU 支持的 2000～2099 年范围")
            self.config = (kind, value)
        if kind in ("rtc", "rtc_now"):
            command = "SET " + value.strftime("%y-%m-%d %H:%M:%S")
        elif kind == "clear":
            command = "log clear"
        else:
            command = f"log {kind} {value}"
        self.phase = "config_apply"
        self.deadline = time.monotonic() + 10
        self.line(command)  # 绝不自动重试破坏性命令或配置命令。

    def finish_config(self, success, message, info=None):
        self.config = None
        self.phase = ""
        self.activity = time.monotonic()
        if not self.capture.historical:
            self.line("exit")
        self.event("config_result", (success, message, info))

    def config_response(self, text):
        if not self.phase.startswith("config_"):
            return False
        kind, value = self.config
        if "ERROR" in text or text == "Unknown setting command":
            self.finish_config(False, "设置失败：" + text)
        elif self.phase == "config_setting" and text == "SETTING MODE":
            self.apply_config()
        elif self.phase == "config_apply":
            if kind in ("rtc", "rtc_now"):
                match = re.fullmatch(r"RTC SET OK: (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}) VL=(\d+)", text)
                if match:
                    actual = rtc_datetime(match[1])
                    if match[2] != "0" or not 0 <= (actual-value).total_seconds() <= 2:
                        self.finish_config(False, "RTC 回读校验未通过：" + text)
                    else:
                        self.finish_config(True, "日期时间已校准，MCU 回读：" + match[1])
            else:
                expected = {"clear": "LOG CLEAR OK", "interval": "LOG INTERVAL OK",
                            "order": "LOG ORDER OK: " + ("oldest-first" if value == "old" else "newest-first")}[kind]
                if text == expected:
                    self.phase = "config_verify"
                    self.deadline = time.monotonic() + 5
                    self.line("log info")
        elif self.phase == "config_verify" and text.startswith("LOG INFO:"):
            info = log_info(text)
            if kind == "interval" and Decimal(info["interval"]) != Decimal(value):
                self.finish_config(False, "记录间隔回读不一致：" + text, info)
            elif kind == "order" and info["order"] != ("oldest-first" if value == "old" else "newest-first"):
                self.finish_config(False, "输出顺序回读不一致：" + text, info)
            else:
                message = {"clear": "Flash 日志已清空；若记录开启，设备会继续写入新记录",
                           "interval": "记录间隔已设为 " + info["interval"] + " 秒",
                           "order": "输出顺序已设为" + ("从旧到新" if info["order"] == "oldest-first" else "从新到旧")}[kind]
                self.finish_config(True, message, info)
        return True

    def event(self, kind, value):
        self.events.put((kind, value))

    def start(self, port, baud):
        self.running = True
        self.finished = False
        threading.Thread(target=self.run, args=(port, baud), daemon=True).start()

    def command(self, name, value=None):
        self.jobs.put((name, value))

    def send(self, data):
        if self.serial.write(data) != len(data):
            raise OSError("串口写入不完整")

    def line(self, text):
        self.send((text + "\r\n").encode("ascii"))

    def spool(self, historical=False, target=None):
        self.activity = time.monotonic()
        folder = Path(os.environ.get("LOCALAPPDATA", Path.home())) / "AD7682Host" / "captures"
        path = (Path(target).with_name(Path(target).name + "." + secrets.token_hex(4) + ".part")
                if target else folder / (time.strftime("%Y%m%d-%H%M%S-") + secrets.token_hex(4) + ".csv.part"))
        self.capture = Capture(path, historical, Path(target) if target else None)
        self.event("capture", str(path))

    def accept(self, text):
        self.activity = time.monotonic()
        if text.startswith("samples:"):
            values = self.capture.append(text)
            self.last = values
        else:
            match = re.fullmatch(r"LOG DUMP DONE: matched=(\d+) read_errors=(\d+)", text)
            if not match or self.phase != "dump":
                raise ValueError("未知数据帧或导出状态不符")
            self.capture.finish(*map(int, match.groups()))
            done_path = str(self.capture.path)
            done_complete = self.capture.complete
            done_rows = self.capture.rows
            self.phase = ""
            self.dump = None
            # 退出设置模式时使用新的实时记录文件。历史
            # 采集文件已经关闭，后续实时帧不能写入该文件。
            self.spool()
            self.line("exit")
            self.event("done", (done_complete, done_path, done_rows))

    def protocol_event(self, kind, value):
        if kind == "fatal":
            raise OSError(value)
        if kind != "text":
            if kind == "crc_error":
                self.event("log", "校验失败，已请求重发：" + value)
            return
        self.event("log", value)
        if value.startswith("LOG DUMP PROGRESS:"):
            self.activity = time.monotonic()
        if value.startswith("LINK CRC FAILED"):
            raise OSError("MCU 重发后仍未收到确认，传输已停止，请重新连接")
        if self.config_response(value):
            return
        if value == f"LINK CRC ON {self.session:08X}":
            self.ready = True
            self.phase = ""
            self.line("exit")
            self.event("ready", True)
        elif self.phase == "setting" and value == "SETTING MODE":
            self.phase = "order"
            self.deadline = time.monotonic() + 4
            self.line("log order " + self.dump[2])
        elif self.phase == "order" and value.startswith("LOG ORDER OK:"):
            self.capture.close()
            target, mode, order, interval = self.dump
            self.spool(True, target)
            self.phase = "begin"
            self.deadline = time.monotonic() + 5
            self.line("log dump " + mode + (" " + interval if interval else ""))
        elif self.phase == "begin" and value.startswith("LOG DUMP BEGIN:"):
            self.phase = "dump"
            self.activity = time.monotonic()
        elif self.phase == "info_setting" and value == "SETTING MODE":
            self.phase = "info"
            self.deadline = time.monotonic() + 4
            self.line("log info")
        elif self.phase == "info" and value.startswith("LOG INFO:"):
            self.event("device_info", log_info(value))
            self.phase = ""
            if not self.capture.historical:
                self.line("exit")
            self.event("ready", True)
        elif self.dump and ("ERROR" in value):
            raise OSError(value)

    def run(self, port, baud):
        self.last = []
        self.activity = time.monotonic()
        self.serial = None
        try:
            if port == "DEMO":
                from demo import DemoSerial
                self.serial = DemoSerial()
            else:
                import serial
                self.serial = serial.Serial(port, baud, bytesize=8, parity="E", stopbits=1,
                                            timeout=0.03, write_timeout=1)
                self.serial.reset_input_buffer()
            self.session = secrets.randbits(32)
            self.rx = ReliableReceiver(self.session, self.send, self.accept, self.protocol_event)
            self.spool()
            self.phase = "handshake"
            self.deadline = time.monotonic() + 4
            self.line(f"link crc on {self.session:08X}")
            last_stats = 0
            while self.running:
                self.rx.feed(self.serial.read(4096))
                now = time.monotonic()
                if self.phase in ("handshake", "setting", "order", "begin", "info_setting", "info") and now > self.deadline:
                    raise OSError("等待 MCU 响应超时。请确认已烧录配套固件、串口为 921600 / 8E1，且电脑→MCU 通路正常")
                if self.phase.startswith("config_") and now > self.deadline:
                    raise OSError("设备设置响应超时，执行结果未知；未自动重发。请重新连接并检查设备状态")
                if self.ready and self.phase in ("", "dump") and not self.capture.closed and now - self.activity > 30:
                    raise OSError("超过 30 秒没有有效数据或扫描进度，传输中止并保留部分文件")
                if now - last_stats > 0.2:
                    self.event("stats", (self.capture.rows, self.rx.bad, self.rx.nacks, self.rx.duplicates, self.last))
                    last_stats = now
                try:
                    name, value = self.jobs.get_nowait()
                except queue.Empty:
                    continue
                if name == "disconnect":
                    self.running = False
                elif name == "export":
                    self.capture.export(Path(value))
                    self.event("saved", value)
                elif name == "dump" and self.ready and not self.phase:
                    self.dump = value
                    self.phase = "setting"
                    self.deadline = now + 4
                    self.line("setting")
                elif name == "live" and self.ready and not self.phase:
                    self.capture.close()
                    self.spool()
                    self.line("exit")
                elif name == "info" and self.ready and not self.phase:
                    self.phase = "info_setting"
                    self.deadline = now + 4
                    self.line("setting")
                elif name == "configure":
                    if not self.ready or self.phase:
                        self.event("config_result", (False, "设备忙，请等待当前操作结束", None))
                    else:
                        try:
                            self.start_config(*value)
                        except ValueError as exc:
                            self.finish_config(False, str(exc))
        except Exception as exc:
            self.event("error", str(exc))
        finally:
            self.ready = self.running = False
            if self.serial:
                try:
                    self.line("link crc off")
                except Exception:
                    pass
                try:
                    self.serial.close()
                except Exception as exc:
                    self.event("log", "关闭串口时出错：" + str(exc))
            if self.capture:
                try:
                    self.capture.close()
                except OSError as exc:
                    self.event("error", str(exc))
            self.finished = True
            self.event("disconnected", None)
