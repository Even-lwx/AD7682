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
    def receiver(self, accept=None):
        self.sent, self.rows, self.events = [], [], []
        return ReliableReceiver(0x1234, self.sent.append, accept or self.rows.append,
                                lambda *event: self.events.append(event))

    def test_crc_standard_vector(self):
        self.assertEqual(crc16(b"123456789"), 0x4B37)

    def test_fragmented_stream_and_duplicate_ack(self):
        rx = self.receiver()
        frame = data_frame(0x1234, 1, SAMPLE.encode())
        for b in frame:
            rx.feed(bytes([b]))
        rx.feed(frame)
        self.assertEqual(self.rows, [SAMPLE])
        self.assertEqual(rx.duplicates, 1)
        self.assertEqual(self.sent, [control_frame("A", 0x1234, 1)] * 2)

    def test_each_header_payload_and_crc_bit_error(self):
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
        rx = self.receiver()
        rx.feed(b"@broken\r\n@broken\r\n")
        self.assertEqual(rx.nacks, 1)
        self.assertEqual(rx.bad, 2)

    def test_stale_session_and_sequence_gap(self):
        rx = self.receiver()
        rx.feed(data_frame(99, 1, SAMPLE.encode()))
        self.assertFalse(self.sent)
        rx.feed(data_frame(0x1234, 3, SAMPLE.encode()))
        self.assertTrue(rx.failed)
        self.assertFalse(self.rows)

    def test_length_error_and_resync(self):
        rx = self.receiver()
        rx.feed(b"partial\x00" + data_frame(0x1234, 1, SAMPLE.encode()))
        self.assertEqual(self.rows, [SAMPLE])
        frame = data_frame(0x1234, 2, b"abc").strip()
        with self.assertRaises(ValueError):
            unpack_frame(frame.replace(b"0003", b"0004"))

    def test_wrap(self):
        rx = self.receiver()
        rx.expected = 0xFFFFFFFF
        rx.feed(data_frame(0x1234, 0xFFFFFFFF, SAMPLE.encode()))
        rx.feed(data_frame(0x1234, 0, SAMPLE.encode()))
        self.assertEqual(rx.expected, 1)
        self.assertEqual(len(self.rows), 2)

    def test_disk_failure_must_not_ack(self):
        def fail(text):
            raise OSError("disk full")
        rx = self.receiver(fail)
        with self.assertRaises(OSError):
            rx.feed(data_frame(0x1234, 1, SAMPLE.encode()))
        self.assertFalse(self.sent)


class CsvTests(unittest.TestCase):
    def test_exact_vofa_bytes_and_same_second_preserved(self):
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
        with tempfile.TemporaryDirectory() as folder:
            cap = Capture(Path(folder)/"a.part", True, Path(folder)/"a.csv")
            cap.finish(0, 0)
            self.assertEqual(len(cap.path.read_bytes().splitlines()), 1)

    def test_mixed_live_history_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            cap = Capture(Path(folder)/"a.part", True)
            try:
                with self.assertRaises(ValueError):
                    cap.append("samples: 1,2,3")
            finally:
                cap.close()


class IntegrationTests(unittest.TestCase):
    def test_dump_disconnect_reconnect_releases_exclusive_port(self):
        from demo import DemoSerial

        class ExclusiveSerial(DemoSerial):
            occupied = False

            def __init__(self):
                if type(self).occupied:
                    raise PermissionError("port still owned")
                type(self).occupied = True
                super().__init__()

            def close(self):
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
                self.assertGreater(device.rx.nacks, 10)
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

    def test_missing_clear_reply_is_unknown_and_not_retried(self):
        from demo import DemoSerial

        class MissingClearReply(DemoSerial):
            def text(self, text):
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
    def test_interval_and_date_validation(self):
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
