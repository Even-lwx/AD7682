"""后台串口线程与设备状态机，避免阻塞 Tk 主线程。

Serial worker: all I/O and capture writes run outside Tk's UI thread."""
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
    """校验 0.5～86400 秒的记录间隔，须为 0.5 秒整数倍。

    参数：
    value：配置值或事件负载。

    返回：提取或格式化后的文本。

    实现说明：Canonical MCU seconds: 0.5..86400 in exact half-second increments."""
    try:
        seconds = Decimal(str(value).strip())
        if not seconds.is_finite() or not Decimal("0.5") <= seconds <= 86400 or seconds % Decimal("0.5"):
            raise ValueError()
    except (InvalidOperation, ValueError):
        raise ValueError("记录间隔须为 0.5～86400 秒，且为 0.5 秒的整数倍") from None
    return format(seconds.quantize(Decimal("0.1")), "f")


def rtc_datetime(value):
    """解析 YYYY-MM-DD HH:MM:SS，限制为 2000～2099 年合法日期。

    参数：
    value：配置值或事件负载。

    返回：校验通过的 datetime。"""
    try:
        result = datetime.strptime(str(value).strip(), "%Y-%m-%d %H:%M:%S")
        if not 2000 <= result.year <= 2099:
            raise ValueError()
    except ValueError:
        raise ValueError("日期时间须为 YYYY-MM-DD HH:MM:SS，年份 2000～2099") from None
    return result


def log_info(text):
    """解析日志状态文本，错误格式抛出 ValueError。

    参数：
    text：命令、数据或提示文本。

    返回：包含条数、容量、间隔、顺序和开关的字典。"""
    match = re.fullmatch(r"LOG INFO: count=(\d+) capacity=(\d+) interval=(\d+\.\d)s "
                         r"order=(oldest-first|newest-first) recording=(on|off) display=(all|adc)", text)
    if not match:
        raise ValueError("MCU 返回的日志状态格式无效")
    count, capacity, interval, order, recording, display = match.groups()
    return dict(count=int(count), capacity=int(capacity), interval=interval,
                order=order, recording=recording, display=display)


class Device:
    """串口设备工作线程：负责连接、可靠协议、配置、历史导出和实时采集。"""
    def __init__(self, events):
        """初始化本对象的状态、缓存及所需资源；参数含义见下方。

        参数：
        events：线程安全的界面事件队列。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
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
        self.batch_size = 100

    def start_config(self, kind, value):
        """校验配置参数，进入设置握手阶段并发送 setting。

        参数：
        kind：配置或事件类型。
        value：配置值或事件负载。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
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
        """生成设置命令并发送一次，进入等待设备响应阶段。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
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
        """结束设置状态并上报结果，必要时退出设备设置模式。

        参数：
        success：设置是否成功。
        message：结果提示文字。
        info：设备回读状态字典。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        self.config = None
        self.phase = ""
        self.activity = time.monotonic()
        if not self.capture.historical:
            self.line("exit")
        self.event("config_result", (success, message, info))

    def config_response(self, text):
        """推进设置命令与回读校验状态机，返回是否已处理该文本。

        参数：
        text：命令、数据或提示文本。

        返回：True 已处理配置响应，False 交给其他状态处理。"""
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
        """向线程安全事件队列发布一条通知，供界面主线程消费。

        参数：
        kind：配置或事件类型。
        value：配置值或事件负载。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        self.events.put((kind, value))

    def start(self, port, baud):
        """启动串口后台线程并标记连接生命周期状态。

        参数：
        port：串口名称或 DEMO。
        baud：串口波特率。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        self.running = True
        self.finished = False
        threading.Thread(target=self.run, args=(port, baud), daemon=True).start()

    def command(self, name, value=None):
        """把界面请求放入任务队列，供串口线程按顺序处理。

        参数：
        name：任务名或待提取函数名。
        value：配置值或事件负载。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        self.jobs.put((name, value))

    def send(self, data):
        """向串口完整写入字节串；短写时抛出 OSError。

        参数：
        data：待发送字节串。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        if self.serial.write(data) != len(data):
            raise OSError("串口写入不完整")

    def line(self, text):
        """把文本编码为 ASCII 并附加真实 CRLF 后发送。

        参数：
        text：命令、数据或提示文本。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        self.send((text + "\r\n").encode("ascii"))

    def spool(self, historical=False, target=None):
        """建立新的磁盘采集临时文件，并重置采集计时与状态。

        参数：
        historical：是否为 15 列历史数据。
        target：历史完成后的最终文件路径。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        self.activity = time.monotonic()
        self.capture_started = self.activity
        folder = Path(os.environ.get("LOCALAPPDATA", Path.home())) / "AD7682Host" / "captures"
        path = (Path(target).with_name(Path(target).name + "." + secrets.token_hex(4) + ".part")
                if target else folder / (time.strftime("%Y%m%d-%H%M%S-") + secrets.token_hex(4) + ".csv.part"))
        self.capture = Capture(path, historical, Path(target) if target else None)
        self.event("capture", str(path))

    def accept(self, text):
        """接收已验证负载；保存采样行或核对历史完成计数并切回实时采集。

        参数：
        text：命令、数据或提示文本。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        self.activity = time.monotonic()
        if text.startswith("samples:"):
            values = self.capture.append(text, flush=not self.rx.window_mode)
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
            self.rx.window_mode = False
            self.rx.batch_size = 0
            # 退出设置模式时使用新的实时记录文件。历史
            # 采集文件已经关闭，后续实时帧不能写入该文件。
            self.spool()
            self.line("exit")
            self.event("done", (done_complete, done_path, done_rows))

    def protocol_event(self, kind, value):
        """处理协议事件和普通文本，推进握手、导出及配置状态机。

        参数：
        kind：配置或事件类型。
        value：配置值或事件负载。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        if kind == "fatal":
            raise OSError(value)
        if kind != "text":
            if kind == "crc_error":
                self.event("log", "校验失败，已请求重发：" + value)
            return
        self.event("log", value)
        if value == "LOG DUMP SAMPLING: PAUSED":
            self.event("sampling", False)
        elif value == "LOG DUMP SAMPLING: RESUMED":
            self.event("sampling", True)
        if value.startswith("LOG DUMP PROGRESS:"):
            self.activity = time.monotonic()
        if value.startswith("LINK CRC FAILED"):
            raise OSError("MCU 重发后仍未收到确认，传输已停止，请重新连接")
        if self.config_response(value):
            return
        if value == f"LINK CRC ON {self.session:08X}":
            self.ready = True
            if self._retrying_dump and self.dump:
                self.phase = "setting"
                self.deadline = time.monotonic() + 4
                self.event("log", "串口已恢复，自动重新开始本次历史导出")
                self.line("setting")
            else:
                self.phase = ""
                self.line("exit")
                self.event("ready", True)
        elif self.phase == "setting" and value == "SETTING MODE":
            self.phase = "order"
            self.deadline = time.monotonic() + 4
            self.line("log order " + self.dump[2])
        elif self.phase == "order" and value.startswith("LOG ORDER OK:"):
            self.phase = "batch"
            self.deadline = time.monotonic()+5
            self.line(f"link batch {self.batch_size}")
        elif self.phase == "batch" and value == "Unknown setting command":
            self.phase = "window"
            self.deadline = time.monotonic()+5
            self.line("link window 16")
        elif ((self.phase == "batch" and value == f"LINK BATCH OK: {self.batch_size}") or
              (self.phase == "window" and value in ("LINK WINDOW OK: 16","Unknown setting command"))):
            self.rx.batch_size = self.batch_size if self.phase == "batch" else 0
            self.rx.window_mode = bool(self.rx.batch_size) or value == "LINK WINDOW OK: 16"
            self.rx.window_last = None
            self.rx.window_start = None
            self.rx.window_nacked = None
            self.event("log", f"Flash {self.rx.batch_size} 条整批 CRC32 导出" if self.rx.batch_size else
                       "Flash 16 条窗口导出" if self.rx.window_mode else "旧固件：逐条确认导出")
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

    def _prepare_dump_retry(self):
        """清理一次失败的历史临时文件，准备从 Flash 快照重新导出。

        当前协议的 MCU 导出序号从快照起点开始，电脑端不能把新会话收到的
        行直接追加到旧文件，否则会产生重复记录。因此自动重连时丢弃未完成
        的 .part 文件并重新建立文件，完整导出成功后才替换目标 CSV。
        """
        if self.capture:
            old_path = self.capture.path
            try:
                self.capture.close()
            except OSError:
                pass
            try:
                if old_path.exists():
                    old_path.unlink()
            except OSError:
                pass
        self.phase = ""
        self.rx = None
        self._retrying_dump = True

    def run(self, port, baud):
        """运行串口线程；历史导出断线时自动重连并重试，最多三次。

        普通连接或设备设置失败仍按原流程报告错误。历史导出期间如果串口
        暂时断开，后台线程会关闭旧句柄、等待后重新握手，并自动重新发送
        原来的导出参数；三次均失败后才通知界面中止。
        """
        self.last = []
        self.activity = time.monotonic()
        self.running = True
        self.finished = False
        self._retrying_dump = False
        attempts = 0
        while self.running:
            self.last_error = None
            self._run_once(port, baud)
            if not self.last_error or not self.dump or attempts >= 3 or not self.running:
                break
            attempts += 1
            self.event("reconnecting", (attempts, 3, str(self.last_error)))
            self._prepare_dump_retry()
            time.sleep(min(2.0, 0.5 * attempts))
        self.ready = self.running = False
        self.finished = True
        if self.last_error:
            if attempts >= 3 and self.dump:
                self.event("error", f"串口断开，自动重试 {attempts} 次后仍未恢复：{self.last_error}")
            else:
                self.event("error", str(self.last_error))
        self.event("disconnected", None)

    def _run_once(self, port, baud):
        """在后台线程打开串口、读写协议与任务，退出时关闭串口和采集文件。

        参数：
        port：串口名称或 DEMO。
        baud：串口波特率。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        self.last = []
        self.activity = time.monotonic()
        self.serial = None
        self.last_error = None
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
            self.rx.commit = lambda: self.capture.flush()
            self.spool()
            self.phase = "handshake"
            self.deadline = time.monotonic() + 4
            self.line(f"link crc on {self.session:08X}")
            last_stats = 0
            while self.running:
                # Read one byte to wait, then drain available data. read(4096)
                # otherwise waits the full 30 ms timeout for every short ACK.
                chunk = self.serial.read(1)
                available = getattr(self.serial, "in_waiting", 0)
                if available:
                    chunk += self.serial.read(min(available, 8192))
                self.rx.feed(chunk)
                now = time.monotonic()
                if self.phase in ("handshake", "setting", "order", "batch", "window", "begin", "info_setting", "info") and now > self.deadline:
                    raise OSError("等待 MCU 响应超时。请确认已烧录配套固件、串口为 921600 / 8E1，且电脑→MCU 通路正常")
                if self.phase.startswith("config_") and now > self.deadline:
                    raise OSError("设备设置响应超时，执行结果未知；未自动重发。请重新连接并检查设备状态")
                if self.ready and self.phase in ("", "dump") and not self.capture.closed and now - self.activity > 30:
                    raise OSError("超过 30 秒没有有效数据或扫描进度，传输中止并保留部分文件")
                if now - last_stats > 0.2:
                    rate = self.capture.rows/max(now-self.capture_started, 0.001) if self.phase == "dump" else None
                    self.event("stats", (self.capture.rows, self.rx.bad, self.rx.nacks, self.rx.duplicates, self.last, rate))
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
                    self.batch_size = int(value[4]) if len(value)>4 else 100
                    if self.batch_size not in (100,1000):
                        raise ValueError("批量大小只支持 100 或 1000")
                    self.dump = value[:4]
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
            self.last_error = exc
        finally:
            self.ready = False
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
            # 由外层 run() 决定是否自动重连；这里不能提前发送 disconnected。
