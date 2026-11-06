"""上位机界面与事件调度。

AD7682 desktop capture application. Run app.py or packaged AD7682Host.exe."""
import argparse
import queue
import time
import tkinter as tk
from collections import deque
from datetime import datetime
from tkinter import ttk, filedialog, messagebox

from device import Device, interval_seconds, rtc_datetime
from datetime_picker import DateTimePicker


class App:
    """本模块的 App 数据和行为封装。"""
    def __init__(self, root, demo=False):
        """初始化本对象的状态、缓存及所需资源；参数含义见下方。

        参数：
        root：Tk 主窗口。
        demo：是否启用纯软件演示。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        self.root = root
        self.events = queue.Queue()
        self.device = None
        self.points = deque(maxlen=350)
        self.busy = False
        root.title("AD7682 数据采集 v1.4 · 流式 CRC32 批量导出")
        root.geometry("1120x800")
        root.minsize(950, 710)
        root.configure(bg="#eef2f6")
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TFrame", background="#eef2f6")
        style.configure("TLabel", background="#eef2f6", font=("Microsoft YaHei UI", 10))
        style.configure("TButton", font=("Microsoft YaHei UI", 10), padding=(12, 7))
        style.configure("Title.TLabel", font=("Microsoft YaHei UI", 21, "bold"), foreground="#102a43")
        style.configure("Stat.TLabel", font=("Microsoft YaHei UI", 14, "bold"), foreground="#126782")
        frame = ttk.Frame(root, padding=20)
        frame.pack(fill="both", expand=True)
        ttk.Label(frame, text="AD7682 数据采集", style="Title.TLabel").pack(anchor="w")
        ttk.Label(frame, text="CRC-16 校验  /  错误自动请求重发一次  /  VOFA 格式 CSV").pack(anchor="w", pady=(3, 15))
        ports = ttk.Frame(frame)
        ports.pack(fill="x")
        ttk.Label(ports, text="串口").pack(side="left")
        self.port = tk.StringVar(value="DEMO" if demo else "")
        self.port_box = ttk.Combobox(ports, textvariable=self.port, width=19, state="readonly")
        self.port_box.pack(side="left", padx=8)
        self.refresh_button = ttk.Button(ports, text="刷新", command=self.refresh)
        self.refresh_button.pack(side="left")
        self.baud = tk.StringVar(value="921600")
        self.baud_box = ttk.Combobox(ports, textvariable=self.baud, values=["921600", "1152000", "115200"], width=10, state="readonly")
        self.baud_box.pack(side="left", padx=(18, 8))
        ttk.Label(ports, text="8 数据位 · Even · 1 停止位").pack(side="left")
        self.connect_button = ttk.Button(ports, text="连接", command=self.connect)
        self.connect_button.pack(side="right")
        self.state = tk.StringVar(value="未连接；DEMO 可试用界面与重传流程")
        ttk.Label(frame, textvariable=self.state).pack(anchor="w", pady=(12, 8))
        stats = ttk.Frame(frame)
        stats.pack(fill="x", pady=(0, 12))
        self.stats = tk.StringVar(value="有效数据 0     校验异常 0     重发请求 0     重复帧 0")
        ttk.Label(stats, textvariable=self.stats, style="Stat.TLabel").pack(anchor="w")
        tabs = ttk.Notebook(frame)
        tabs.pack(fill="x")
        dump = ttk.Frame(tabs, padding=12)
        settings = ttk.Frame(tabs, padding=12)
        tabs.add(dump, text="  Flash 历史导出  ")
        tabs.add(settings, text="  设备设置  ")
        self.tabs = tabs
        self.mode = tk.StringVar(value="ADC 数值")
        ttk.Combobox(dump, textvariable=self.mode, values=["ADC 数值", "压强 hPa"], state="readonly", width=13).grid(row=0, column=0, padx=(0, 12))
        self.order = tk.StringVar(value="从旧到新")
        ttk.Combobox(dump, textvariable=self.order, values=["从旧到新", "从新到旧"], state="readonly", width=13).grid(row=0, column=1, padx=(0, 12))
        self.range_enabled = tk.BooleanVar()
        self.batch_size = tk.StringVar(value="100")
        ttk.Checkbutton(dump, text="指定时间段（包含起止时刻）", variable=self.range_enabled).grid(row=0, column=2, columnspan=2, sticky="w")
        now = datetime.now().replace(microsecond=0)
        ttk.Label(dump, text="开始").grid(row=1, column=0, sticky="w", pady=(8, 0))
        self.start_time = DateTimePicker(dump, now.replace(hour=0, minute=0, second=0))
        self.start_time.grid(row=1, column=1, columnspan=3, pady=(8, 0), sticky="w")
        ttk.Label(dump, text="结束").grid(row=2, column=0, sticky="w", pady=(8, 0))
        self.end_time = DateTimePicker(dump, now)
        self.end_time.grid(row=2, column=1, columnspan=3, pady=(8, 0), sticky="w")
        ttk.Button(dump, text="今天 00:00 → 现在", command=self.reset_export_time).grid(row=2, column=4, padx=(12, 0), pady=(8, 0))
        batch = ttk.Frame(dump)
        batch.grid(row=3,column=0,columnspan=5,sticky="w",pady=(8,0))
        ttk.Label(batch,text="整批校验条数").pack(side="left")
        ttk.Combobox(batch,textvariable=self.batch_size,values=["100","1000"],state="readonly",width=6).pack(side="left",padx=8)
        ttk.Label(batch,text="每批一次 CRC32；错误整批重发，1000 条适合稳定链路").pack(side="left")
        self.dump_button = ttk.Button(dump, text="导出历史 CSV…", command=self.dump)
        self.dump_button.grid(row=0, column=4, rowspan=2, padx=(20, 0))
        dump.columnconfigure(4, weight=1)
        ttk.Label(settings, text="记录间隔（秒）").grid(row=0, column=0, sticky="w")
        self.interval = tk.StringVar(value="0.5")
        interval_entry = ttk.Entry(settings, textvariable=self.interval, width=12)
        interval_entry.grid(row=0, column=1, padx=8, sticky="w")
        interval_button = ttk.Button(settings, text="保存间隔", command=self.set_interval)
        interval_button.grid(row=0, column=2, sticky="w")
        ttk.Label(settings, text="历史输出顺序").grid(row=0, column=3, padx=(20, 8))
        order_box = ttk.Combobox(settings, textvariable=self.order, values=["从旧到新", "从新到旧"], state="readonly", width=12)
        order_box.grid(row=0, column=4, sticky="w")
        order_button = ttk.Button(settings, text="保存顺序", command=self.set_order)
        order_button.grid(row=0, column=5, padx=(8, 0))
        ttk.Label(settings, text="日期时间").grid(row=1, column=0, sticky="w", pady=8)
        self.rtc = tk.StringVar(value=datetime.now().strftime("%Y-%m-%d %H:%M:%S"))
        rtc_entry = ttk.Entry(settings, textvariable=self.rtc, width=22)
        rtc_entry.grid(row=1, column=1, columnspan=2, padx=8, sticky="w")
        rtc_button = ttk.Button(settings, text="写入日期时间", command=self.set_rtc)
        rtc_button.grid(row=1, column=3, padx=(20, 8), sticky="w")
        sync_button = ttk.Button(settings, text="同步电脑时间", command=lambda: self.configure_device("rtc_now"))
        sync_button.grid(row=1, column=4, sticky="w")
        clear_button = ttk.Button(settings, text="清空 Flash 日志…", command=self.clear_logs)
        clear_button.grid(row=2, column=0, columnspan=2, sticky="w")
        ttk.Label(settings, text="间隔：0.5～86400 秒，步进 0.5 秒；仅修改记录频率。").grid(row=2, column=2, columnspan=4, sticky="w", padx=(8, 0))
        self.device_status = tk.StringVar(value="点击“查看 Flash 状态”读取当前设备配置；设置成功会显示 MCU 确认结果。")
        ttk.Label(settings, textvariable=self.device_status, wraplength=1000).grid(row=3, column=0, columnspan=6, sticky="w", pady=(8, 0))
        self.config_buttons = (interval_button, order_button, rtc_button, sync_button, clear_button)
        self.config_entries = (interval_entry, rtc_entry)
        self.config_order_box = order_box
        toolbar = ttk.Frame(frame)
        toolbar.pack(fill="x", pady=12)
        self.live_button = ttk.Button(toolbar, text="实时采集 / 新建记录", command=self.live)
        self.live_button.pack(side="left")
        self.save_button = ttk.Button(toolbar, text="另存当前数据 CSV…", command=self.save)
        self.save_button.pack(side="left", padx=8)
        self.info_button = ttk.Button(toolbar, text="查看 Flash 状态", command=self.info)
        self.info_button.pack(side="left")
        output_tabs = ttk.Notebook(frame)
        output_tabs.pack(fill="both", expand=True)
        chart_frame = ttk.Frame(output_tabs)
        log_frame = ttk.Frame(output_tabs)
        output_tabs.add(chart_frame, text="  曲线预览  ")
        output_tabs.add(log_frame, text="  通信日志  ")
        self.canvas = tk.Canvas(chart_frame, height=125, bg="#102a43", highlightthickness=0)
        self.canvas.pack(fill="both", expand=True)
        self.values = tk.StringVar(value="等待通过 CRC 校验的数据…")
        ttk.Label(frame, textvariable=self.values, wraplength=1040).pack(anchor="w", pady=(8, 4))
        self.path_text = tk.StringVar(value="每次采集自动写入恢复文件；历史导出完整核对后才生成最终 CSV")
        ttk.Label(frame, textvariable=self.path_text, wraplength=1040).pack(anchor="w", pady=(0, 8))
        self.log = tk.Text(log_frame, height=7, bg="#ffffff", fg="#334e68", font=("Consolas", 9), relief="flat", state="disabled", wrap="word")
        self.log.pack(fill="both", expand=True)
        self.refresh()
        self.enable(False)
        root.protocol("WM_DELETE_WINDOW", self.close)
        root.after(100, self.poll)
        if demo:
            root.after(200, self.connect)

    def refresh(self):
        """刷新可用串口列表，并保留仍有效的当前选择。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        from serial.tools import list_ports
        devices = list(list_ports.comports())
        devices.sort(key=lambda item: (0 if "CH343" in (item.description or "").upper() else 1,
                                       item.device))
        ports = [item.device for item in devices]
        self.port_box["values"] = ports + ["DEMO"]
        if not self.port.get():
            self.port.set(ports[0] if ports else "DEMO")

    def enable(self, ready):
        """根据连接状态更新界面控件是否可操作。

        参数：
        ready：设备是否已就绪。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        for button in (self.dump_button, self.live_button, self.info_button) + self.config_buttons:
            button.configure(state="normal" if ready and not self.busy else "disabled")
        for entry in self.config_entries:
            entry.configure(state="normal" if ready and not self.busy else "disabled")
        self.config_order_box.configure(state="readonly" if ready and not self.busy else "disabled")
        self.save_button.configure(state="normal" if ready else "disabled")

    def connect(self):
        """切换连接状态；连接时启动设备线程，断开时等待资源释放。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        if self.device and not self.device.finished:
            self.device.command("disconnect")
            self.connect_button.configure(state="disabled")
            self.state.set("正在关闭上一次串口连接，请稍候…")
            return
        # A prior worker may have queued final stats/error events. A new
        # connection owns its event queue so stale events cannot change it.
        self.events = queue.Queue()
        self.device = Device(self.events)
        self.device.start(self.port.get(), int(self.baud.get()))
        self.state.set("连接中，等待 CRC 协议握手…" + ("【演示数据】" if self.port.get() == "DEMO" else ""))
        self.connect_button.configure(text="断开")
        self.port_box.configure(state="disabled")
        self.baud_box.configure(state="disabled")
        self.refresh_button.configure(state="disabled")

    def dump(self):
        """读取导出选项与时间范围，校验后把历史导出任务交给设备线程。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        interval = ""
        if self.range_enabled.get():
            try:
                start, end = [v.get() for v in (self.start_time, self.end_time)]
                if not 2000 <= start.year <= end.year <= 2099 or start > end:
                    raise ValueError()
                interval = start.strftime("%Y-%m-%d %H:%M:%S ") + end.strftime("%Y-%m-%d %H:%M:%S")
            except ValueError:
                messagebox.showerror("时间段错误", "格式为 YYYY-MM-DD HH:MM:SS，年份 2000～2099，开始不能晚于结束。")
                return
        target = filedialog.asksaveasfilename(defaultextension=".csv", filetypes=[("CSV", "*.csv")], initialfile="AD7682-history.csv")
        if target:
            self.busy = True
            self.enable(True)
            self.state.set("历史导出中；仅接收通过 CRC 的数据。短暂断线会自动重连并重试。")
            self.points.clear()
            self.device.command("dump", (target, "raw" if self.mode.get() == "ADC 数值" else "hpa", "old" if self.order.get() == "从旧到新" else "new", interval, int(self.batch_size.get())))

    def reset_export_time(self):
        """把历史时间范围设为电脑当天零点至当前时刻。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        now = datetime.now().replace(microsecond=0)
        self.start_time.set(now.replace(hour=0, minute=0, second=0))
        self.end_time.set(now)

    def save(self):
        """选择 CSV 保存位置，并把当前采集文件快照导出任务交给设备线程。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        target = filedialog.asksaveasfilename(defaultextension=".csv", filetypes=[("CSV", "*.csv")], initialfile="AD7682-capture.csv")
        if target:
            if self.busy and not messagebox.askyesno("导出仍在进行", "这会保存截至当前收到的数据快照，不能代表完整历史。是否继续？"):
                return
            self.device.command("export", target)

    def live(self):
        """请求开始新的实时数据采集文件。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        self.points.clear()
        self.device.command("live")
        self.state.set("实时采集中" + ("【演示数据】" if self.port.get() == "DEMO" else ""))

    def info(self):
        """请求读取设备 Flash 状态。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        self.busy = True
        self.enable(True)
        self.device.command("info")

    def configure_device(self, kind, value=None):
        """将设备设置类型与值提交给后台线程。

        参数：
        kind：配置或事件类型。
        value：配置值或事件负载。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        self.busy = True
        self.enable(True)
        self.state.set("正在等待 MCU 执行并确认设备设置…")
        self.device.command("configure", (kind, value))

    def set_interval(self):
        """校验界面记录间隔并提交保存任务。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        try:
            value = interval_seconds(self.interval.get())
        except ValueError as exc:
            messagebox.showerror("间隔无效", str(exc))
            return
        self.configure_device("interval", value)

    def set_order(self):
        """提交界面选定的历史输出顺序。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        self.configure_device("order", "old" if self.order.get() == "从旧到新" else "new")

    def set_rtc(self):
        """校验输入日期时间并提交 RTC 校时任务。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        try:
            value = rtc_datetime(self.rtc.get()).strftime("%Y-%m-%d %H:%M:%S")
        except ValueError as exc:
            messagebox.showerror("日期时间无效", str(exc))
            return
        self.configure_device("rtc", value)

    def clear_logs(self):
        """显示清空确认框，用户确认后提交 Flash 日志清空任务。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        if messagebox.askyesno("确认清空 MCU 日志", "将清空 MCU Flash 中的全部历史记录，无法恢复。\n已保存到电脑的 CSV 文件不受影响。\n如果设备记录开启，清空后会继续写入新记录。\n\n确定清空吗？", icon="warning", default="no"):
            self.configure_device("clear")

    def show_device_info(self, info):
        """把回读的设备日志状态填入界面。

        参数：
        info：设备回读状态字典。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        order = "从旧到新" if info["order"] == "oldest-first" else "从新到旧"
        self.interval.set(info["interval"])
        self.order.set(order)
        self.device_status.set(f"MCU：{info['count']:,} / {info['capacity']:,} 条；记录间隔 {info['interval']} 秒；"
                               f"顺序 {order}；记录{'开启' if info['recording'] == 'on' else '关闭'}")

    def add_log(self, text):
        """追加通信提示，并限制界面日志长度。

        参数：
        text：命令、数据或提示文本。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        self.log.configure(state="normal")
        self.log.insert("end", time.strftime("%H:%M:%S ") + text + "\n")
        if int(self.log.index("end-1c").split(".")[0]) > 400:
            self.log.delete("1.0", "100.0")
        self.log.see("end")
        self.log.configure(state="disabled")

    def plot(self, values):
        """更新有限长度的实时曲线缓存并重绘画布。

        参数：
        values：当前采样值列表。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        if len(values) not in (15, 18):
            return
        offset = 10 if len(values) == 15 else 7
        self.points.append([float(v) for v in values[offset:offset+4]])
        self.canvas.delete("all")
        w, h = self.canvas.winfo_width(), self.canvas.winfo_height()
        colors = ["#5ed7e8", "#ffd166", "#c4a2ff", "#7fe0a5"]
        flat = [v for row in self.points for v in row]
        low, high = min(flat), max(flat)
        span = max(high-low, 1)
        self.canvas.create_text(12, 12, anchor="nw", fill="#c6d6e4", text=f"ADC 四通道预览   {low:.3f} ～ {high:.3f}   （最近显示点；CSV 保存全部已确认记录）")
        for j, color in enumerate(colors):
            coordinates = []
            for i, row in enumerate(self.points):
                coordinates.extend((15+i*(w-30)/max(len(self.points)-1, 1), h-15-(row[j]-low)*(h-50)/span))
            if len(coordinates) >= 4:
                self.canvas.create_line(*coordinates, fill=color, width=2)

    def poll(self):
        # 限制每次队列处理量：长时间导出时，生产者填充队列的速度可能超过 Tk
        # 的渲染速度，因此必须始终让出执行权给窗口事件循环。
        """在 Tk 主线程消费后台事件，更新连接、统计、图表和错误提示。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        deadline = time.monotonic() + 0.015
        try:
            for _ in range(80):
                if time.monotonic() >= deadline:
                    break
                kind, value = self.events.get_nowait()
                if kind == "ready":
                    self.busy = False
                    self.enable(True)
                    self.state.set("CRC 可靠连接已建立" + ("【DEMO 演示数据，不是实际测量】" if self.port.get() == "DEMO" else ""))
                elif kind == "stats":
                    rows, bad, nacks, dup, values, rate = value
                    self.stats.set(f"有效数据 {rows:,}     校验异常 {bad}     重发请求 {nacks}     重复帧 {dup}"
                                   + (f"     导出 {rate:.0f} 条/秒" if rate is not None else ""))
                    if values:
                        if len(values) in (15, 18):
                            offset = 10 if len(values) == 15 else 7
                            battery = values[14] if len(values) == 15 else values[0]
                            self.values.set("最新记录  " + "  |  ".join(f"ADC{i}: {values[offset+i]}" for i in range(4))
                                            + f"  |  电池: {battery} V（完整各列保存在 CSV）")
                        self.plot(values)
                elif kind == "capture":
                    self.path_text.set("当前文件：" + value)
                elif kind == "done":
                    complete, path, rows = value
                    self.busy = False
                    self.enable(True)
                    self.path_text.set("文件：" + path)
                    self.state.set(f"历史导出{'完成' if complete else '不完整（Flash 读取错误）'}：{rows:,} 条；连接保持，可继续导出或修改设备设置")
                    self.add_log(self.state.get())
                elif kind == "saved":
                    self.add_log("CSV 已保存：" + value)
                elif kind == "sampling":
                    if not value:
                        self.state.set("独占导出中：所有传感器采样及新记录已暂停，完成后自动恢复")
                    elif self.busy:
                        self.state.set("MCU 已恢复采样，正在完成导出收尾…")
                elif kind == "reconnecting":
                    attempt, maximum, reason = value
                    self.state.set(f"导出串口暂时断开，正在自动重连（第 {attempt}/{maximum} 次）…")
                    self.add_log(f"串口暂时断开，自动重连 {attempt}/{maximum}：{reason}")
                elif kind == "device_info":
                    self.show_device_info(value)
                elif kind == "config_result":
                    success, message, info = value
                    self.busy = bool(self.device.phase)
                    self.enable(self.device.ready)
                    if info:
                        self.show_device_info(info)
                    self.state.set(message)
                    self.add_log(message)
                    if not success:
                        messagebox.showerror("设备设置未完成", message)
                elif kind == "error":
                    self.state.set("传输中止：" + value)
                    self.add_log(value)
                    messagebox.showerror("采集停止", value + "\n已接收的数据保留在当前文件中。")
                elif kind == "disconnected":
                    self.busy = False
                    self.enable(False)
                    self.connect_button.configure(text="连接")
                    self.connect_button.configure(state="normal")
                    self.port_box.configure(state="readonly")
                    self.baud_box.configure(state="readonly")
                    self.refresh_button.configure(state="normal")
                    self.add_log("串口已关闭")
                elif kind == "log":
                    self.add_log(value)
        except queue.Empty:
            pass
        self.root.after(100, self.poll)

    def close(self):
        """请求停止后台线程，等待串口释放后关闭界面。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        if self.device and not self.device.finished:
            if self.busy and not messagebox.askyesno("操作进行中", "正在导出或修改设备设置。退出后当前操作的结果可能需要重新连接确认，是否退出？"):
                return
            self.device.command("disconnect")
            self.root.after(100, self.wait_close)
        else:
            self.root.destroy()

    def wait_close(self):
        """轮询后台线程是否已释放串口，完成后销毁主窗口。

        返回：无返回值；结果通过对象状态、文件或事件通知。"""
        if not self.device.finished:
            self.root.after(100, self.wait_close)
        else:
            self.root.destroy()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--demo", action="store_true")
    parser.add_argument("--smoke-test", action="store_true")
    args = parser.parse_args()
    root = tk.Tk()
    app = App(root, args.demo or args.smoke_test)
    if args.smoke_test:
        root.after(2500, app.close)
    root.mainloop()
