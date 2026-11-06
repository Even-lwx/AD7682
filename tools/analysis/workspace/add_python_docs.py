"""为项目 Python 源码补齐中文函数和类 docstring，不改变可执行语句。"""
import ast
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
FILES = list((ROOT/'code/host').glob('*.py')) + [p for p in ROOT.glob('*.py') if p.name not in ('tmp',)]

SPECIAL = {
    'crc16':'计算 CRC-16/MODBUS 校验值，供串口控制帧使用。',
    'data_frame':'组装一条带会话号、序号、长度和 CRC16 的数据帧。',
    'control_frame':'组装 ACK 或 NACK 控制帧，保护会话号和序号。',
    'unpack_frame':'解析并校验数据帧，返回会话号、序号和负载。',
    'sample_values':'校验采样行的列数、数值和日期字段，并按 VOFA 规则格式化。',
    'interval_seconds':'校验并规范化记录间隔，限制为 0.5 秒步进。',
    'rtc_datetime':'解析并校验 MCU 支持范围内的日期时间。',
    'log_info':'解析 MCU 返回的日志状态行并转换为结构化字典。',
    'c_literal':'把字节串转换为 C 十六进制字符串字面量。',
    'fmt':'将统计表中的数值格式化为可读文本。',
}

def signature(node):
    args = [a.arg for a in node.args.args if a.arg not in ('self','cls')]
    return ('参数：' + '、'.join(args) + '。') if args else '无参数。'

def function_doc(node):
    base = SPECIAL.get(node.name)
    if base is None:
        if node.name == '__init__':
            base = '初始化对象状态、缓存和线程控制资源。'
        elif node.name.startswith('test_'):
            base = '验证 ' + node.name[5:].replace('_',' ') + ' 场景。'
        elif node.name.startswith('_'):
            base = '执行本模块内部的辅助处理。'
        else:
            base = '执行 ' + node.name.replace('_',' ') + ' 相关处理。'
    return base + '\n\n' + signature(node) + ' 返回值：按调用场景返回处理结果或状态。'

def class_doc(node):
    if node.name == 'Device':
        return '串口设备工作线程：负责连接、可靠协议、配置、历史导出和实时采集。'
    if node.name == 'ReliableReceiver':
        return '可靠帧接收器：校验 CRC、维护序号、批次提交和错误重发。'
    if node.name == 'Capture':
        return '磁盘采集文件：流式写入 CSV，并在完成时原子替换目标文件。'
    if node.name == 'DemoSerial':
        return '纯软件演示串口：模拟 MCU 命令、历史数据和损坏帧。'
    return '本模块的 ' + node.name + ' 数据和行为封装。'

def insert_for(path):
    raw = path.read_bytes(); text = raw.decode('utf-8-sig'); lines = text.splitlines(keepends=True)
    try: tree = ast.parse(text)
    except SyntaxError: return 0
    inserts = []
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            if node.body and isinstance(node.body[0], ast.Expr) and isinstance(node.body[0].value, ast.Constant) and isinstance(node.body[0].value.value, str):
                continue
            first = node.body[0] if node.body else node
            line_no = first.lineno - 1
            # 文档字符串必须与函数/类的第一条语句同级；不能使用嵌套
            # 函数第一条语句的缩进，否则会把 docstring 放到错误层级。
            indent = ' ' * (node.col_offset + 4)
            doc = class_doc(node) if isinstance(node, ast.ClassDef) else function_doc(node)
            block = indent + '"""' + doc.replace('"""','\\"\\"\\"').replace('\n','\n'+indent) + '"""\n'
            if first.lineno == node.lineno:
                # 内联函数不插入独立语句，使用函数前注释保留原有语法。
                comment = ' ' * node.col_offset + '# ' + doc.replace('\n', ' ') + '\n'
                inserts.append((node.lineno - 1, comment))
            else:
                inserts.append((line_no, block))
    for line_no, block in sorted(inserts, reverse=True): lines.insert(line_no, block)
    new = ''.join(lines)
    if new != text:
        newline = '\r\n' if b'\r\n' in raw else '\n'
        path.write_bytes(new.replace('\r\n','\n').replace('\n',newline).encode('utf-8-sig' if raw.startswith(b'\xef\xbb\xbf') else 'utf-8'))
    return len(inserts)

total = 0
for path in FILES: total += insert_for(path)
print('Python docstring additions:', total)
