# AD7682 上位机开发文档

本文面向需要修改、测试或重新打包上位机的开发人员。上位机源码位于 `code/host/`，界面使用 Python 自带的 Tkinter，串口使用 pyserial，协议和 CSV 处理不依赖 GUI，便于单元测试。

## 1. 开发环境

建议使用 Windows 10/11 64 位，并安装以下软件：

| 软件 | 版本/用途 |
| --- | --- |
| Python | 3.11 或更高版本；安装时勾选 **Add Python to PATH** |
| Tkinter | Windows Python 安装包通常自带，用于桌面界面 |
| CH343 驱动 | 连接真实 MCU 时安装厂家对应的 USB 转串口驱动 |
| Git | 获取源码和提交修改 |
| PyInstaller | 只在需要生成 EXE 时使用，版本由 requirements.txt 固定 |

工程不要求安装 Qt、PyQt、numpy、pandas 或 matplotlib。协议测试和上位机运行只使用标准库、pyserial 和 Tkinter。

检查 Python 和 Tkinter：

```powershell
python --version
python -c "import tkinter, sys; print(sys.version); print(tkinter.TkVersion)"
```

如果第二条命令提示找不到 tkinter，需要重新安装 python.org 的完整 Windows 安装包。

## 2. 创建虚拟环境

在工程根目录 `D:\lwx\lwx-project\AD7682` 执行：

```powershell
python -m venv .venv-host
.\.venv-host\Scripts\Activate.ps1
python -m pip install --upgrade pip
python -m pip install -r code\host\requirements.txt
```

如果 PowerShell 禁止激活脚本，可以直接使用虚拟环境中的解释器：

```powershell
.\.venv-host\Scripts\python.exe -m pip install -r code\host\requirements.txt
.\.venv-host\Scripts\python.exe code\host\app.py --demo
```

退出虚拟环境：

```powershell
deactivate
```

`requirements.txt` 当前固定为 `pyserial==3.5` 和 `pyinstaller==6.22.3`。前者负责串口通信，后者用于生成单文件 EXE。`.venv-host/` 是本机开发环境，不应提交到 Git。

## 3. 启动和连接

源码启动：

```powershell
python code\host\app.py
```

演示模式不访问真实串口，可以检查界面、时间选择器、CRC 重发、批量导出和 CSV：

```powershell
python code\host\app.py --demo
```

真实设备连接前确认：

1. MCU 已烧录配套固件。
2. CH343 驱动已正常安装，设备管理器中能看到 COM 号。
3. VOFA+ 或其他串口软件已关闭，避免占用同一个 COM 口。
4. 上位机使用 **921600 baud、8 数据位、Even 偶校验、1 停止位（8E1）**。
5. MCU TX 接 USB 转串口 RX，USB 转串口 TX 接 MCU RX，并共地。

## 4. 源码结构与线程约束

| 文件 | 作用 |
| --- | --- |
| `app.py` | Tk 主窗口、控件、曲线和界面事件分发 |
| `datetime_picker.py` | 日历、日期输入、时分秒 Spinbox、滚轮和上下键 |
| `device.py` | 后台串口线程、连接握手、设置、导出和文件生命周期 |
| `protocol.py` | CRC16/CRC32、帧解析、批量接收、CSV 写入 |
| `demo.py` | 模拟 MCU 串口，故意制造首发损坏帧以测试重发 |
| `test_*.py` | 协议、CSV、窗口/批次和设备线程回归测试 |
| `verify_*.py` | 编译并仿真实际 MCU 函数 |

串口读写、磁盘写入和协议状态机必须在 `Device.run()` 后台线程执行；Tk 控件只能由 `App.poll()` 主线程操作。后台线程通过 `events` 队列通知界面，不得直接调用 Tk API。

历史 CSV 先写入 `.part` 临时文件。只有记录数、Flash 读取错误数、CRC、序号和文件写入都通过后，才原子替换成目标 CSV。不要把 `accept()` 改成收到数据即 ACK，否则磁盘写失败时会出现“已确认但未保存”的记录。

## 5. 测试

```powershell
python -m unittest discover -s code\host -p "test_*.py" -v
python -m py_compile code\host\*.py
python code\host\verify_firmware.py
python code\host\verify_dump.py
python code\host\verify_exclusive.py
```

后三个验证脚本需要本机可用的 Keil ARMCC、pyelftools 和 Unicorn。脚本使用临时目录生成测试桩，不会连接实物板。修改协议或导出逻辑后还应重新编译 `code\MDK-ARM\code.uvprojx`。

## 6. 生成 EXE

```powershell
python -m PyInstaller --noconfirm --clean --windowed --onefile `
  --name AD7682Host `
  --distpath output\AD7682Host-dev `
  --workpath tmp\host-build `
  --specpath tmp `
  code\host\app.py
```

生成文件为 `output\AD7682Host-dev\AD7682Host.exe`。打包前先运行演示模式和全部测试；打包后应在没有 Python 开发环境的 Windows 电脑上验证界面、真实连接和 CSV 导出。

EXE 与 `AD7682_crc.hex` 必须来自同一协议版本。协议不匹配时，旧固件只能回退到逐条或 16 条窗口模式，不能宣称使用了最新批量功能。

## 7. 常见问题

**能看到 MCU 数据但发送命令无响应**：检查 USB 转串口 TX/RX 是否交叉、共地、串口是否为 921600/8E1，并发送真实 `CR LF`。

**导出后无法再次连接**：等待后台线程完成关闭，并确认 VOFA+ 没有占用同一个 COM 口。

**CSV 只有 `.part`**：表示导出未完成、计数不符、CRC 错误、Flash 读取错误或磁盘写入失败。它不能当作完整导出。

**批量导出速度不稳定**：批次大小可选 100 或 1000。1000 条减少 ACK 次数，但单批损坏时重传量更大；速度还受 Flash、USB 驱动、浮点格式化和磁盘影响。
