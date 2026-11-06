import csv
import queue
import tempfile
import time
import unittest
from datetime import datetime
from pathlib import Path
from unittest.mock import patch

from protocol import Capture, ReliableReceiver, control_frame, crc16, data_frame, sample_values, unpack_frame
from device import Device, interval_seconds, rtc_datetime

SAMPLE = "samples: 2026,11,2,8,50,9,25.91,53.95,1024.20,1023.90,41771.30,36482.80,40860.30,37157.00,8.14"
ROW = b"2026.000,11.000,2.000,8.000,50.000,9.000,25.910,53.950,1024.200,1023.900,41771.301,36482.801,40860.301,37157.000,8.140\r\n"


class ProtocolTests(unittest.TestCase):
    """本模块的 ProtocolTests 数据和行为封装。"""
    def receiver(self, accept=None):
        """建立测试接收器及用于断言的数据、发送和事件缓存。

        参数：
        accept：提交已验证负载的回调。

        返回：用于测试断言的帧、接收器或匹配事件。"""
        self.sent, self.rows, self.events = [], [], []
        return ReliableReceiver(0x1234, self.sent.append, accept or self.rows.append,
                                lambda *event: self.events.append(event))

    def test_crc_standard_vector(self):
        """校验 CRC16 标准向量，确认算法参数一致。无参数；断言失败抛出测试异常，无返回值。"""
        self.assertEqual(crc16(b"123456789"), 0x4B37)

    def test_fragmented_stream_and_duplicate_ack(self):
        """验证分片输入重组及重复帧补 ACK。无参数；断言失败抛出测试异常，无返回值。"""
        rx = self.receiver()
        frame = data_frame(0x1234, 1, SAMPLE.encode())
        for b in frame:
            rx.feed(bytes([b]))
        rx.feed(frame)
        self.assertEqual(self.rows, [SAMPLE])
        self.assertEqual(rx.duplicates, 1)
        self.assertEqual(self.sent, [control_frame("A", 0x1234, 1)] * 2)

    def test_each_header_payload_and_crc_bit_error(self):
        """逐位损坏帧头、负载和 CRC，确认均被拒绝。无参数；断言失败抛出测试异常，无返回值。"""
        frame = data_frame(0x1234, 1, SAMPLE.encode())
        for index in (6, 15, 24, 35, len(frame)-4):
            with self.subTest(index=index):
                rx = self.receiver()
                bad = frame[:index] + bytes([frame[index] ^ 1]) + frame[index+1:]
                rx.feed(bad)
                self.assertEqual(self.sent, [control_frame("N", 0x1234, 1)])
                self.assertFalse(self.rows)
                rx.feed(frame)
                self.assertEqual(self.rows, [SAMPLE])

    def test_one_nack_only_then_timeout_responsibility_is_mcu(self):
        """确认同一预期帧只发一次 NACK，随后由 MCU 超时处理。无参数；断言失败抛出测试异常，无返回值。"""
        rx = self.receiver()
        rx.feed(b"@broken\r\n@broken\r\n")
        self.assertEqual(rx.nacks, 1)
        self.assertEqual(rx.bad, 2)

    def test_stale_session_and_sequence_gap(self):
        """拒绝旧会话并检测序号缺口。无参数；断言失败抛出测试异常，无返回值。"""
        rx = self.receiver()
        rx.feed(data_frame(99, 1, SAMPLE.encode()))
        self.assertFalse(self.sent)
        rx.feed(data_frame(0x1234, 3, SAMPLE.encode()))
        self.assertTrue(rx.failed)
        self.assertFalse(self.rows)

    def test_length_error_and_resync(self):
        """拒绝长度错误并从下一个帧标记恢复同步。无参数；断言失败抛出测试异常，无返回值。"""
        rx = self.receiver()
        rx.feed(b"partial\x00" + data_frame(0x1234, 1, SAMPLE.encode()))
        self.assertEqual(self.rows, [SAMPLE])
        frame = data_frame(0x1234, 2, b"abc").strip()
        with self.assertRaises(ValueError):
            unpack_frame(frame.replace(b"0003", b"0004"))

    def test_wrap(self):
        """验证 32 位序号回绕后仍能连续接收。无参数；断言失败抛出测试异常，无返回值。"""
        rx = self.receiver()
        rx.expected = 0xFFFFFFFF
        rx.feed(data_frame(0x1234, 0xFFFFFFFF, SAMPLE.encode()))
        rx.feed(data_frame(0x1234, 0, SAMPLE.encode()))
        self.assertEqual(rx.expected, 1)
        self.assertEqual(len(self.rows), 2)

    def test_disk_failure_must_not_ack(self):
        """验证磁盘写入失败时不会向 MCU 确认。无参数；断言失败抛出测试异常，无返回值。"""
        def fail(text):
            """模拟文件写入失败，验证失败时不会确认未保存的数据。

            参数：
            text：命令、数据或提示文本。

            返回：无返回值；结果通过对象状态、文件或事件通知。"""
            raise OSError("disk full")
        rx = self.receiver(fail)
        with self.assertRaises(OSError):
            rx.feed(data_frame(0x1234, 1, SAMPLE.encode()))
        self.assertFalse(self.sent)


class CsvTests(unittest.TestCase):
    """本模块的 CsvTests 数据和行为封装。"""
    def test_exact_vofa_bytes_and_same_second_preserved(self):
        """逐字节核对 VOFA CSV，保留同秒多条记录。无参数；断言失败抛出测试异常，无返回值。"""
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "a.part"
            target = Path(folder) / "a.csv"
            cap = Capture(path, True, target)
            cap.append(SAMPLE)
            cap.append(SAMPLE)
            cap.finish(2, 0)
            self.assertEqual(target.read_bytes(), b",".join(f"I{i}".encode() for i in range(15))+b"\r\n"+ROW*2)
            self.assertFalse(path.exists())

    def test_real_vofa_sample_compatibility(self):
        # 导出样本来自用户已有的 VOFA 文件。
        """核对实测 VOFA 样例的 float32 与 CSV 格式兼容性。无参数；断言失败抛出测试异常，无返回值。"""
        sample = Path(__file__).parents[1] / "log" / "1030-1102-1号板.csv"
        if not sample.exists():
            self.skipTest("Original measurement CSV not present")
        with sample.open(encoding="utf-8-sig", newline="") as stream:
            reader = csv.reader(stream)
            self.assertEqual(next(reader), [f"I{i}" for i in range(15)])
            row = next(reader)
        # 固件将前六项按整数发送，其他数值保留两位小数。
        payload = "samples: " + ",".join(str(int(float(v))) if i < 6 else f"{float(v):.2f}" for i, v in enumerate(row))
        self.assertEqual(sample_values(payload, True), row)

    def test_partial_not_promoted_and_snapshot(self):
        """确认未完成历史不生成最终文件，快照仅包含已接收内容。无参数；断言失败抛出测试异常，无返回值。"""
        with tempfile.TemporaryDirectory() as folder:
            path, target = Path(folder)/"a.part", Path(folder)/"a.csv"
            cap = Capture(path, True, target)
            cap.append(SAMPLE)
            copy = Path(folder)/"snapshot.csv"
            cap.export(copy)
            self.assertEqual(path.read_bytes(), copy.read_bytes())
            with self.assertRaises(ValueError):
                cap.finish(2, 0)
            self.assertFalse(target.exists())
            cap.finish(1, 3)
            self.assertFalse(cap.complete)
            self.assertTrue(path.exists())
            self.assertFalse(target.exists())

    def test_empty_dump_header(self):
        """确认空历史导出仍有正确 CSV 表头。无参数；断言失败抛出测试异常，无返回值。"""
        with tempfile.TemporaryDirectory() as folder:
            cap = Capture(Path(folder)/"a.part", True, Path(folder)/"a.csv")
            cap.finish(0, 0)
            self.assertEqual(len(cap.path.read_bytes().splitlines()), 1)

    def test_mixed_live_history_rejected(self):
        """拒绝实时与历史不同列数混写。无参数；断言失败抛出测试异常，无返回值。"""
        with tempfile.TemporaryDirectory() as folder:
            cap = Capture(Path(folder)/"a.part", True)
            try:
                with self.assertRaises(ValueError):
                    cap.append("samples: 1,2,3")
            finally:
                cap.close()


class IntegrationTests(unittest.TestCase):
    """本模块的 IntegrationTests 数据和行为封装。"""
    def test_1000_batch_full_worker_retransmits_without_duplicate_csv(self):
        """验证 1000 条批次完整导出及重发去重。无参数；断言失败抛出测试异常，无返回值。"""
        from demo import DemoSerial
        class LargeDemo(DemoSerial):
            """本模块的 LargeDemo 数据和行为封装。"""
            def __init__(self):
                """初始化本对象的状态、缓存及所需资源；参数含义见下方。

                返回：无返回值；结果通过对象状态、文件或事件通知。"""
                super().__init__()
                self.record_count=2103
        with tempfile.TemporaryDirectory() as folder, patch.dict("os.environ",{"LOCALAPPDATA":folder}), patch("demo.DemoSerial",LargeDemo):
            events=queue.Queue()
            device=Device(events)
            device.start("DEMO",921600)
            try:
                self.wait_event(events,"ready")
                target=Path(folder)/"large.csv"
                device.command("dump",(str(target),"raw","old","",1000))
                self.assertEqual(self.wait_event(events,"done")[2],2103)
                lines=target.read_bytes().splitlines()
                self.assertEqual(len(lines),2104)
                self.assertEqual(len(set(lines[1:])),2103)
                self.assertEqual(device.rx.nacks,3)
                self.assertFalse(device.serial.sampling_paused)
            finally:
                device.command("disconnect")
                self.wait_event(events,"disconnected")

    def test_old_firmware_falls_back_to_single_row_ack(self):
        """验证旧固件回退到逐条确认。无参数；断言失败抛出测试异常，无返回值。"""
        from demo import DemoSerial

        class OldFirmware(DemoSerial):
            """本模块的 OldFirmware 数据和行为封装。"""
            def write(self, data):
                """解析电脑发送的命令和 ACK/NACK，推进模拟 MCU 状态并生成响应。

                参数：
                data：待发送字节串。

                返回：已消费的字节数。"""
                if data.decode().strip() == "link window 16" or data.decode().strip().startswith("link batch "):
                    self.text("Unknown setting command")
                    return len(data)
                return super().write(data)

        with tempfile.TemporaryDirectory() as folder, patch.dict("os.environ", {"LOCALAPPDATA": folder}), patch("demo.DemoSerial", OldFirmware):
            events = queue.Queue()
            device = Device(events)
            device.start("DEMO", 921600)
            try:
                self.wait_event(events, "ready")
                device.command("dump", (str(Path(folder)/"old.csv"), "raw", "old", ""))
                self.assertEqual(self.wait_event(events, "done")[2], 1000)
                self.assertFalse(device.serial.window_enabled)
            finally:
                device.command("disconnect")
                self.wait_event(events, "disconnected")

    def test_dump_disconnect_reconnect_releases_exclusive_port(self):
        """验证导出、断开、重新连接会释放独占串口。无参数；断言失败抛出测试异常，无返回值。"""
        from demo import DemoSerial

        class ExclusiveSerial(DemoSerial):
            """本模块的 ExclusiveSerial 数据和行为封装。"""
            occupied = False

            def __init__(self):
                """初始化本对象的状态、缓存及所需资源；参数含义见下方。

                返回：无返回值；结果通过对象状态、文件或事件通知。"""
                if type(self).occupied:
                    raise PermissionError("port still owned")
                type(self).occupied = True
                super().__init__()

            def close(self):
                """关闭测试串口并释放模拟占用状态。

                返回：无返回值；结果通过对象状态、文件或事件通知。"""
                time.sleep(0.03)
                type(self).occupied = False

        with tempfile.TemporaryDirectory() as folder, patch.dict("os.environ", {"LOCALAPPDATA": folder}), patch("demo.DemoSerial", ExclusiveSerial):
            for attempt in range(4):
                events = queue.Queue()
                device = Device(events)
                device.start("DEMO", 921600)
                try:
                    self.wait_event(events, "ready")
                    target = Path(folder)/f"history-{attempt}.csv"
                    device.command("dump", (str(target), "raw", "old", ""))
                    self.assertEqual(self.wait_event(events, "done")[2], 1000)
                    time.sleep(0.25)
                    self.assertFalse(device.capture.historical)
                    self.assertEqual(device.capture.columns, 18)
                    self.assertFalse(device.serial.setting)
                finally:
                    device.command("disconnect")
                    self.wait_event(events, "disconnected")
                    self.assertTrue(device.finished)
                    self.assertFalse(ExclusiveSerial.occupied)

    def wait_event(self, events, name, seconds=15):
        """在截止时间内等待指定设备事件，超时使测试失败。

        参数：
        events：线程安全的界面事件队列。
        name：任务名或待提取函数名。
        seconds：测试输入，约束见调用处及断言。

        返回：用于测试断言的帧、接收器或匹配事件。"""
        end = time.monotonic()+seconds
        while time.monotonic() < end:
            try:
                kind, value = events.get(timeout=0.1)
            except queue.Empty:
                continue
            self.assertNotEqual(kind, "error", value)
            if kind == name:
                return value
        self.fail("Timed out waiting for " + name)

    def test_serial_worker_full_dump_with_corruption_and_live_restart(self):
        """验证串口线程损坏帧重发、完整导出和实时接收恢复。无参数；断言失败抛出测试异常，无返回值。"""
        with tempfile.TemporaryDirectory() as folder, patch.dict("os.environ", {"LOCALAPPDATA": folder}):
            events = queue.Queue()
            device = Device(events)
            device.start("DEMO", 921600)
            try:
                self.wait_event(events, "ready")
                time.sleep(0.25)
                self.assertEqual(device.capture.columns, 18)
                target = Path(folder)/"test.csv"
                device.command("dump", (str(target), "raw", "old", ""))
                complete, path, rows = self.wait_event(events, "done")
                self.assertTrue(complete)
                self.assertEqual(rows, 1000)
                self.assertEqual(Path(path), target)
                self.assertGreater(device.rx.nacks, 5)
                self.assertFalse(device.serial.sampling_paused)
                lines = target.read_bytes().splitlines()
                self.assertEqual(len(lines), 1001)
                self.assertEqual(lines[0], b",".join(f"I{i}".encode() for i in range(15)))
                self.assertEqual(len(set(lines[1:])), 1000)
                self.assertIn(b"41771.301", lines[1])
                device.command("live")
                self.wait_event(events, "capture")
                time.sleep(0.3)
                self.assertEqual(device.capture.columns, 18)
                device.command("info")
                time.sleep(0.2)
                self.assertEqual(device.phase, "")
                # 第二次导出必须使用独立的 15 列文件。
                target2 = Path(folder)/"test-hpa.csv"
                device.command("dump", (str(target2), "hpa", "new", ""))
                self.assertTrue(self.wait_event(events, "done")[0])
                with target2.open(newline="") as stream:
                    first = list(csv.reader(stream))[1]
                self.assertEqual(first[4:6], ["16.000", "39.000"])
                self.assertLess(float(first[10]), 1350)
            finally:
                device.command("disconnect")
                end = time.monotonic()+3
                while not device.finished and time.monotonic()<end:
                    time.sleep(0.02)
                self.assertTrue(device.finished)

    def test_device_settings_then_clear_and_empty_export(self):
        """验证设置、清空与空历史导出流程。无参数；断言失败抛出测试异常，无返回值。"""
        with tempfile.TemporaryDirectory() as folder, patch.dict("os.environ", {"LOCALAPPDATA": folder}):
            events = queue.Queue()
            device = Device(events)
            device.start("DEMO", 921600)
            try:
                self.wait_event(events, "ready")
                for kind, value in (("interval", "2.5"), ("order", "new"),
                                    ("rtc", "2028-02-29 23:59:58"), ("rtc_now", None)):
                    device.command("configure", (kind, value))
                    success, message, info = self.wait_event(events, "config_result")
                    self.assertTrue(success, message)
                    self.assertFalse(device.serial.setting)  # 恢复实时模式。
                    if kind == "interval":
                        self.assertEqual(info["interval"], "2.5")
                    if kind == "order":
                        self.assertEqual(info["order"], "newest-first")
                    if kind == "rtc":
                        self.assertEqual(device.serial.rtc, datetime(2028, 2, 29, 23, 59, 58))
                    if kind == "rtc_now":
                        self.assertLess(abs((device.serial.rtc-datetime.now()).total_seconds()), 3)
                device.command("configure", ("clear", None))
                self.assertTrue(self.wait_event(events, "config_result")[0])
                self.assertEqual(device.serial.commands.count("log clear"), 1)
                self.assertEqual(device.serial.record_count, 0)
                target = Path(folder)/"empty.csv"
                device.command("dump", (str(target), "raw", "old", ""))
                self.assertEqual(self.wait_event(events, "done")[2], 0)
                original = target.read_bytes()
                device.command("configure", ("interval", "5"))
                self.assertTrue(self.wait_event(events, "config_result")[0])
                self.assertFalse(device.serial.setting)  # 导出完成后恢复实时模式。
                self.assertEqual(target.read_bytes(), original)
            finally:
                device.command("disconnect")
                end = time.monotonic()+3
                while not device.finished and time.monotonic()<end:
                    time.sleep(0.02)
                self.assertTrue(device.finished)

    def test_dump_reconnects_and_restarts_after_serial_loss(self):
        """验证历史导出遇到串口断开时自动重连并重新完成，不要求用户重新点击导出。"""
        from demo import DemoSerial

        created = []

        class DropOnce(DemoSerial):
            """第一次实例在已发送部分历史后模拟 USB 串口断开。"""
            def read(self, size):
                if self.history is not None and self.count >= 100 and not self.dropped:
                    self.dropped = True
                    raise OSError("模拟 CH343 接触不良")
                return super().read(size)

            def __init__(self):
                super().__init__()
                self.dropped = False

        def factory():
            serial = DropOnce() if not created else DemoSerial()
            created.append(serial)
            return serial

        with tempfile.TemporaryDirectory() as folder, patch.dict("os.environ", {"LOCALAPPDATA": folder}), patch("demo.DemoSerial", factory):
            events = queue.Queue()
            device = Device(events)
            device.start("DEMO", 921600)
            try:
                self.wait_event(events, "ready")
                target = Path(folder) / "reconnect.csv"
                device.command("dump", (str(target), "raw", "old", "", 100))
                done = self.wait_event(events, "done", seconds=15)
                self.assertTrue(done[0])
                self.assertEqual(done[2], 1000)
                self.assertGreaterEqual(len(created), 2)
                with target.open(newline="") as stream:
                    self.assertEqual(len(list(csv.reader(stream))) - 1, 1000)
            finally:
                device.command("disconnect")
                until = time.monotonic() + 3
                while not device.finished and time.monotonic() < until:
                    time.sleep(0.02)
                self.assertTrue(device.finished)

    def test_missing_clear_reply_is_unknown_and_not_retried(self):
        """确认清空响应丢失时报告未知结果且不重发命令。无参数；断言失败抛出测试异常，无返回值。"""
        from demo import DemoSerial

        class MissingClearReply(DemoSerial):
            """本模块的 MissingClearReply 数据和行为封装。"""
            def text(self, text):
                """把一行模拟 MCU 文本追加到串口接收缓冲区。

                参数：
                text：命令、数据或提示文本。

                返回：无返回值；结果通过对象状态、文件或事件通知。"""
                if text != "LOG CLEAR OK":
                    super().text(text)

        with tempfile.TemporaryDirectory() as folder, patch.dict("os.environ", {"LOCALAPPDATA": folder}), patch("demo.DemoSerial", MissingClearReply):
            events = queue.Queue()
            device = Device(events)
            device.start("DEMO", 921600)
            try:
                self.wait_event(events, "ready")
                device.command("configure", ("clear", None))
                until = time.monotonic()+3
                while "log clear" not in device.serial.commands and time.monotonic()<until:
                    time.sleep(0.01)
                self.assertIn("log clear", device.serial.commands)
                device.deadline = 0  # 推进配置的超时时间，无需实际等待十秒。
                error = None
                until = time.monotonic()+3
                while time.monotonic()<until:
                    kind, value = events.get(timeout=1)
                    if kind == "error":
                        error = value
                        break
                self.assertIsNotNone(error)
                self.assertIn("结果未知", error)
                self.assertEqual(device.serial.commands.count("log clear"), 1)
                self.assertTrue(device.capture.path.exists())
            finally:
                device.command("disconnect")
                until = time.monotonic()+3
                while not device.finished and time.monotonic()<until:
                    time.sleep(0.02)
                self.assertTrue(device.finished)


class SettingsTests(unittest.TestCase):
    """本模块的 SettingsTests 数据和行为封装。"""
    def test_interval_and_date_validation(self):
        """校验记录间隔步进、范围与日期合法性。无参数；断言失败抛出测试异常，无返回值。"""
        for value, expected in (("0.5", "0.5"), (" 2 ", "2.0"), ("86400", "86400.0"), ("2.500", "2.5")):
            self.assertEqual(interval_seconds(value), expected)
        for value in ("0", "0.1", "86400.5", "nan", "inf", "abc", "1\r\nlog clear", "-5"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                interval_seconds(value)
        self.assertEqual(rtc_datetime("2028-02-29 23:59:59"), datetime(2028, 2, 29, 23, 59, 59))
        for value in ("2026-02-29 12:00:00", "2100-01-01 00:00:00", "1999-12-31 00:00:00", "2026-11-04 24:00:00"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                rtc_datetime(value)

    def test_settings_ack_error_readback_mismatch_and_rtc_voltage_flag(self):
        """覆盖设置错误、回读不一致及 RTC 低电压标志。无参数；断言失败抛出测试异常，无返回值。"""
        from demo import DemoSerial
        with tempfile.TemporaryDirectory() as folder:
            events = queue.Queue()
            device = Device(events)
            device.serial = DemoSerial()
            device.session = 7
            device.capture = Capture(Path(folder)/"capture.part")
            try:
                device.start_config("interval", "2.5")
                device.protocol_event("text", "SETTING MODE")
                device.protocol_event("text", "LOG INTERVAL OK")
                device.protocol_event("text", "LOG INFO: count=1 capacity=1000 interval=5.0s order=oldest-first recording=on display=all")
                results = []
                while not events.empty():
                    kind, value = events.get_nowait()
                    if kind == "config_result":
                        results.append(value)
                self.assertEqual(len(results), 1)
                self.assertFalse(results[0][0])
                device.start_config("clear", None)
                device.protocol_event("text", "SETTING MODE")
                device.protocol_event("text", "LOG CLEAR ERROR")
                self.assertEqual(device.serial.commands.count("log clear"), 1)
                self.assertEqual(device.phase, "")
                device.start_config("rtc", "2026-11-04 09:30:00")
                device.protocol_event("text", "SETTING MODE")
                device.protocol_event("text", "RTC SET OK: 2026-11-04 09:30:00 VL=1")
                results = []
                while not events.empty():
                    kind, value = events.get_nowait()
                    if kind == "config_result":
                        results.append(value)
                self.assertEqual(len(results), 2)
                self.assertTrue(all(not result[0] for result in results))
            finally:
                device.capture.close()


if __name__ == "__main__":
    unittest.main(verbosity=2)
