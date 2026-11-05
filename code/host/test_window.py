"""Window protocol loss/corruption regressions independent of the demo MCU."""
import unittest
from protocol import ReliableReceiver, data_frame, control_frame
from test_host import SAMPLE


class WindowTests(unittest.TestCase):
    """本模块的 WindowTests 数据和行为封装。"""
    def setUp(self):
        """为每个测试建立独立接收器、数据列表和发送记录。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        self.sent, self.rows, self.events = [], [], []
        self.rx = ReliableReceiver(77, self.sent.append, self.rows.append, lambda *e: self.events.append(e))
        self.rx.window_mode = True

    def frames(self, start=1, count=16):
        """构造兼容窗口的起始、采样和结束测试帧。

        参数：
        start：批次起始序号。
        count：数据行数。

        返回：用于测试断言的帧、接收器或匹配事件。"""
        return [data_frame(77, start, b"WINDOW BEGIN")] + [
            data_frame(77, start+i+1, SAMPLE.encode()) for i in range(count)
        ] + [data_frame(77, start+count+1, b"WINDOW END")]

    def test_no_ack_or_rows_before_window_end(self):
        """确认窗口结束前既不提交行也不发送 ACK。无参数；断言失败抛出测试异常，无返回值。"""
        frames = self.frames()
        self.rx.feed(b"".join(frames[:-1]))
        self.assertEqual(self.sent, [])
        self.assertEqual(self.rows, [])
        self.rx.feed(frames[-1])
        self.assertEqual(len(self.rows), 16)
        self.assertEqual(self.sent, [control_frame("A", 77, 18)])

    def test_ack_loss_replays_without_duplicate_rows(self):
        """确认 ACK 丢失重放不会重复保存数据。无参数；断言失败抛出测试异常，无返回值。"""
        wire = b"".join(self.frames())
        self.rx.feed(wire)
        self.rx.feed(wire)
        self.assertEqual(len(self.rows), 16)
        self.assertEqual(self.sent, [control_frame("A", 77, 18)]*2)
        self.rx.feed(b"".join(self.frames(19, 3)))
        self.assertEqual(len(self.rows), 19)
        self.assertEqual(self.rx.expected, 24)

    def test_corrupt_or_missing_any_frame_recovered_as_whole_window(self):
        """验证窗口内任一帧损坏或缺失后整组重放恢复。无参数；断言失败抛出测试异常，无返回值。"""
        good = self.frames()
        for index in range(len(good)):
            for mode in ("missing", "corrupt"):
                with self.subTest(index=index, mode=mode):
                    self.setUp()
                    broken = good.copy()
                    if mode == "missing":
                        broken[index] = b""
                    else:
                        broken[index] = broken[index][:-5]+b"XXXX\n"
                    self.rx.feed(b"".join(broken))
                    self.assertFalse(self.rows)
                    self.rx.feed(b"".join(good))
                    self.assertEqual(len(self.rows), 16)
                    self.assertEqual(self.sent[-1], control_frame("A", 77, 18))

    def test_invalid_row_does_not_commit_other_rows(self):
        """确认一行内容非法时整组不提交。无参数；断言失败抛出测试异常，无返回值。"""
        frames = self.frames()
        frames[5] = data_frame(77, 6, b"samples: 1,2,3")
        self.rx.feed(b"".join(frames))
        self.assertFalse(self.rows)
        self.assertEqual(self.sent, [control_frame("N", 77, 18)])

    def test_missing_entire_window_is_not_silently_skipped(self):
        """确认整窗缺失会检测到序号缺口。无参数；断言失败抛出测试异常，无返回值。"""
        self.rx.feed(b"".join(self.frames(19)))
        self.assertTrue(self.rx.failed)
        self.assertFalse(self.rows)

    def test_empty_filtered_window_and_partial_final_window(self):
        """验证时间筛选后的空窗口及末尾不足 16 条窗口。无参数；断言失败抛出测试异常，无返回值。"""
        self.rx.feed(b"".join(self.frames(count=0)))
        self.assertEqual(self.sent[-1], control_frame("A", 77, 2))
        self.rx.feed(b"".join(self.frames(start=3, count=7)))
        self.assertEqual(len(self.rows), 7)
        self.assertEqual(self.sent[-1], control_frame("A", 77, 11))

    def test_write_failure_never_acknowledges_window(self):
        """确认窗口写盘失败时不发送 ACK。无参数；断言失败抛出测试异常，无返回值。"""
        def fail(text):
            """模拟文件写入失败，验证失败时不会确认未保存的数据。

            参数：
            text：命令、数据或提示文本。

            返回：无返回值；结果通过对象状态、文件或事件通知。"""
            raise OSError("disk full")
        self.rx.accept = fail
        with self.assertRaises(OSError):
            self.rx.feed(b"".join(self.frames()))
        self.assertFalse(self.sent)

    def test_stale_session_does_not_poison_window(self):
        """确认旧会话帧不会破坏当前有效窗口。无参数；断言失败抛出测试异常，无返回值。"""
        self.rx.feed(data_frame(99, 1, b"WINDOW BEGIN"))
        self.rx.feed(b"".join(self.frames(count=2)))
        self.assertEqual(len(self.rows), 2)


if __name__ == "__main__":
    unittest.main()
