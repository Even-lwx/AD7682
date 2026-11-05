"""Calendar date selection and keyboard/wheel time editing using native Tk."""
import calendar
import tkinter as tk
from datetime import datetime
from tkinter import ttk


class DateTimePicker(ttk.Frame):
    def __init__(self, master, value):
        super().__init__(master)
        self.date = tk.StringVar()
        self.parts = [tk.StringVar() for _ in range(3)]
        self.popup = None
        ttk.Entry(self, textvariable=self.date, width=12).pack(side="left")
        ttk.Button(self, text="日历 ▾", command=self.open_calendar, padding=(5, 3)).pack(side="left", padx=(3, 10))
        self.spins = []
        for i, (part, maximum) in enumerate(zip(self.parts, (23, 59, 59))):
            spin = ttk.Spinbox(self, textvariable=part, from_=0, to=maximum,
                               wrap=True, format="%02.0f", width=3)
            spin.pack(side="left")
            self.spins.append(spin)
            spin.bind("<Up>", lambda e, n=i: self.step(n, 1))
            spin.bind("<Down>", lambda e, n=i: self.step(n, -1))
            spin.bind("<MouseWheel>", lambda e, n=i: self.step(n, 1 if e.delta > 0 else -1))
            spin.bind("<FocusOut>", lambda e, n=i: self.normalize(n))
            ttk.Label(self, text=("时", "分", "秒")[i]).pack(side="left", padx=(2, 5))
        self.set(value)

    def set(self, value):
        self.date.set(value.strftime("%Y-%m-%d"))
        for variable, number in zip(self.parts, (value.hour, value.minute, value.second)):
            variable.set(f"{number:02d}")

    def get(self):
        day = datetime.strptime(self.date.get().strip(), "%Y-%m-%d")
        if not 2000 <= day.year <= 2099:
            raise ValueError("年份须为 2000～2099")
        hour, minute, second = (int(part.get()) for part in self.parts)
        return day.replace(hour=hour, minute=minute, second=second)

    def step(self, index, direction):
        limit = (24, 60, 60)[index]
        try:
            number = int(self.parts[index].get())
        except ValueError:
            number = 0
        self.parts[index].set(f"{(number+direction) % limit:02d}")
        return "break"

    def normalize(self, index):
        try:
            number = int(self.parts[index].get())
            if 0 <= number < (24, 60, 60)[index]:
                self.parts[index].set(f"{number:02d}")
        except ValueError:
            pass  # 保留无效输入供用户查看，由导出校验给出原因。

    def open_calendar(self):
        if self.popup and self.popup.winfo_exists():
            self.popup.lift()
            return
        try:
            value = self.get()
        except ValueError:
            value = datetime.now()
        self.popup = tk.Toplevel(self)
        self.popup.title("选择日期")
        self.popup.transient(self.winfo_toplevel())
        self.popup.resizable(False, False)
        self.popup.bind("<Escape>", lambda e: self.popup.destroy())
        self.popup.geometry(f"+{self.winfo_rootx()}+{self.winfo_rooty()+self.winfo_height()+4}")
        content = ttk.Frame(self.popup, padding=10)
        content.pack()
        navigation = ttk.Frame(content)
        navigation.pack(fill="x")
        self.year = tk.StringVar(value=str(value.year))
        self.month = tk.StringVar(value=f"{value.month:02d}")
        ttk.Button(navigation, text="‹", width=2, command=lambda: self.move_month(-1)).pack(side="left")
        for variable, low, high, width in ((self.year, 2000, 2099, 5), (self.month, 1, 12, 3)):
            box = ttk.Spinbox(navigation, textvariable=variable, from_=low, to=high,
                              width=width, command=self.draw_calendar, state="readonly", wrap=True)
            box.pack(side="left", padx=4)
            box.bind("<MouseWheel>", lambda e: self.move_month(1 if e.delta < 0 else -1))
        ttk.Button(navigation, text="›", width=2, command=lambda: self.move_month(1)).pack(side="left")
        self.days = ttk.Frame(content)
        self.days.pack(pady=8)
        ttk.Button(content, text="今天", command=self.choose_today).pack(fill="x")
        self.draw_calendar()
        self.popup.update_idletasks()
        x = max(0, min(self.winfo_rootx(), self.winfo_screenwidth()-self.popup.winfo_reqwidth()-20))
        y = max(0, min(self.winfo_rooty()+self.winfo_height()+4,
                       self.winfo_screenheight()-self.popup.winfo_reqheight()-80))
        self.popup.geometry(f"+{x}+{y}")

    def move_month(self, direction):
        total = int(self.year.get())*12 + int(self.month.get())-1 + direction
        total = min(max(total, 2000*12), 2099*12+11)
        year, month = divmod(total, 12)
        self.year.set(str(year))
        self.month.set(f"{month+1:02d}")
        self.draw_calendar()
        return "break"

    def draw_calendar(self):
        for child in self.days.winfo_children():
            child.destroy()
        year, month = int(self.year.get()), int(self.month.get())
        for col, name in enumerate("一二三四五六日"):
            ttk.Label(self.days, text=name, anchor="center").grid(row=0, column=col, sticky="ew")
        for row, week in enumerate(calendar.monthcalendar(year, month), 1):
            for col, day in enumerate(week):
                if day:
                    selected = self.date.get() == f"{year:04d}-{month:02d}-{day:02d}"
                    ttk.Button(self.days, text=f"[{day}]" if selected else str(day), width=3,
                               padding=(3, 5), command=lambda d=day: self.choose(d)).grid(row=row, column=col, padx=1, pady=1)

    def choose(self, day):
        self.date.set(f"{int(self.year.get()):04d}-{int(self.month.get()):02d}-{day:02d}")
        self.popup.destroy()

    def choose_today(self):
        self.date.set(datetime.now().strftime("%Y-%m-%d"))
        self.popup.destroy()
