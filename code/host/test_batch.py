import unittest
import zlib
from protocol import ReliableReceiver, data_frame, control_frame
from test_host import SAMPLE


class BatchTests(unittest.TestCase):
    """本模块的 BatchTests 数据和行为封装。"""
    def setUp(self):
        """为每个测试建立独立接收器、数据列表和发送记录。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        self.rows,self.sent,self.events = [],[],[]
        self.rx = ReliableReceiver(77,self.sent.append,self.rows.append,lambda *e:self.events.append(e))
        self.rx.batch_size = 1000
        self.rx.window_mode = True

    def wire(self, count=100, start=1):
        """构造带批次边界、数据行和 CRC32 的测试字节序列。

        参数：
        count：数据行数。
        start：批次起始序号。

        返回：用于测试断言的帧、接收器或匹配事件。"""
        rows = [SAMPLE.encode()]*count
        checksum = zlib.crc32(b"".join(row+b"\n" for row in rows))
        return (data_frame(77,start,b"BATCH BEGIN"),
                b"".join(row+b"\r\n" for row in rows),
                data_frame(77,start+1,f"BATCH END count={count} crc={checksum:08X}".encode()))

    def test_100_and_1000_fragmented_batches_single_ack(self):
        """验证 100/1000 条批次分片接收且每批只确认一次。无参数；断言失败抛出测试异常，无返回值。"""
        for count in (100,1000):
            self.setUp()
            start,rows,end = self.wire(count)
            wire = start+rows
            for offset in range(0,len(wire),137):
                self.rx.feed(wire[offset:offset+137])
            self.assertFalse(self.rows)
            self.assertFalse(self.sent)
            self.rx.feed(end)
            self.assertEqual(len(self.rows),count)
            self.assertEqual(self.sent,[control_frame("A",77,2)])

    def test_corrupt_missing_extra_rows_and_lost_boundary(self):
        """拒绝损坏、缺行、多行及边界丢失的批次。无参数；断言失败抛出测试异常，无返回值。"""
        start,rows,end=self.wire()
        cases=[start+rows.replace(b"41771.30",b"41771.31",1)+end,
               start+rows.split(b"\r\n",1)[1]+end,
               start+rows+SAMPLE.encode()+b"\r\n"+end,
               rows+end,start+rows]
        for wire in cases:
            with self.subTest(size=len(wire)):
                self.setUp()
                self.rx.feed(wire)
                self.assertFalse(self.rows)
                self.rx.feed(start+rows+end)
                self.assertEqual(len(self.rows),100)
                self.assertEqual(self.sent[-1],control_frame("A",77,2))

    def test_ack_loss_deduplicates_and_final_short_batch(self):
        """验证 ACK 丢失去重、末尾短批及空批。无参数；断言失败抛出测试异常，无返回值。"""
        wire=b"".join(self.wire(1000))
        self.rx.feed(wire)
        self.rx.feed(wire)
        self.assertEqual(len(self.rows),1000)
        self.assertEqual(self.sent,[control_frame("A",77,2)]*2)
        self.rx.feed(b"".join(self.wire(7,3)))
        self.assertEqual(len(self.rows),1007)
        self.rx.feed(b"".join(self.wire(0,5)))
        self.assertEqual(self.rx.expected,7)

    def test_disk_error_no_ack(self):
        """确认批次写盘失败时不发送 ACK。无参数；断言失败抛出测试异常，无返回值。"""
        # 模拟磁盘写入失败；text 为待提交行，始终抛出 OSError，不应发送 ACK。
        def fail(text):raise OSError("disk full")
        self.rx.accept=fail
        with self.assertRaises(OSError):self.rx.feed(b"".join(self.wire()))
        self.assertFalse(self.sent)

    def test_limits_and_stale_batch(self):
        """核对批次容量限制及旧会话批次处理。无参数；断言失败抛出测试异常，无返回值。"""
        self.rx.batch_size=100
        self.rx.feed(b"".join(self.wire(101)))
        self.assertFalse(self.rows)
        self.assertEqual(self.sent[-1],control_frame("N",77,2))

    def test_reordered_rows_change_batch_crc(self):
        """确认行顺序变化会导致整批 CRC32 不一致。无参数；断言失败抛出测试异常，无返回值。"""
        a = SAMPLE.encode()
        b = a.replace(b"41771.30",b"41771.40")
        checksum = zlib.crc32(a+b"\n"+b+b"\n")
        wire = data_frame(77,1,b"BATCH BEGIN")+b+b"\r\n"+a+b"\r\n"
        wire += data_frame(77,2,f"BATCH END count=2 crc={checksum:08X}".encode())
        self.rx.feed(wire)
        self.assertFalse(self.rows)
        self.assertEqual(self.sent,[control_frame("N",77,2)])

    def test_crc32_standard_vector(self):
        """核对 CRC32 标准向量 123456789。无参数；断言失败抛出测试异常，无返回值。"""
        self.assertEqual(zlib.crc32(b"123456789"),0xCBF43926)


if __name__=="__main__":unittest.main()
