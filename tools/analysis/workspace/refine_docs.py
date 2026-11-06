"""核对后细化函数说明与参数含义。"""
import ast
import re
from pathlib import Path

descriptions = {}
descriptions.update({
 'crc16':'计算 CRC16/MODBUS，初值 FFFF、多项式 A001，无最终异或。',
 'data_frame':'组装带 CRC16 的数据帧，负载长度、会话号和序号一并参与校验。',
 'control_frame':'组装 ACK/NACK 控制帧，kind 必须是 A 或 N。',
 'unpack_frame':'校验数据帧格式、长度和 CRC16；失败抛出 ValueError。',
 'sample_values':'校验采样行并按 VOFA 的 float32 与三位小数规则转换，历史行须为 15 列。',
 'interval_seconds':'校验 0.5～86400 秒的记录间隔，须为 0.5 秒整数倍。',
 'rtc_datetime':'解析 YYYY-MM-DD HH:MM:SS，限制为 2000～2099 年合法日期。',
 'log_info':'解析日志状态文本，错误格式抛出 ValueError。',
 'c_literal':'将输入字节逐个转为 C 字符串中的十六进制转义。',
})
for line in '''
refresh|刷新可用串口列表，并保留仍有效的当前选择。
enable|根据连接状态更新界面控件是否可操作。
connect|切换连接状态；连接时启动设备线程，断开时等待资源释放。
dump|读取导出选项与时间范围，校验后把历史导出任务交给设备线程。
reset_export_time|把历史时间范围设为电脑当天零点至当前时刻。
save|选择 CSV 保存位置，并把当前采集文件快照导出任务交给设备线程。
live|请求开始新的实时数据采集文件。
info|请求读取设备 Flash 状态。
configure_device|将设备设置类型与值提交给后台线程。
set_interval|校验界面记录间隔并提交保存任务。
set_order|提交界面选定的历史输出顺序。
set_rtc|校验输入日期时间并提交 RTC 校时任务。
clear_logs|显示清空确认框，用户确认后提交 Flash 日志清空任务。
show_device_info|把回读的设备日志状态填入界面。
add_log|追加通信提示，并限制界面日志长度。
plot|更新有限长度的实时曲线缓存并重绘画布。
poll|在 Tk 主线程消费后台事件，更新连接、统计、图表和错误提示。
wait_close|轮询后台线程是否已释放串口，完成后销毁主窗口。
set|把日期时间写入日期框与时分秒控件。
get|校验控件输入并返回 datetime，日期或时间非法时抛出 ValueError。
step|按方向递增或递减选定时间字段，越界时在字段范围内循环。
normalize|将合法时间字段补齐为两位数字，保留无效输入供导出校验提示。
open_calendar|创建或前置日期选择弹窗，并显示当前月份。
move_month|按方向切换日历月份，限制在 2000～2099 年。
draw_calendar|按选定年月重建日历日期按钮，并标示当前选择。
choose|保存所选日期并关闭日历弹窗。
choose_today|把日期设为电脑今天并关闭日历弹窗。
start_config|校验配置参数，进入设置握手阶段并发送 setting。
apply_config|生成设置命令并发送一次，进入等待设备响应阶段。
finish_config|结束设置状态并上报结果，必要时退出设备设置模式。
config_response|推进设置命令与回读校验状态机，返回是否已处理该文本。
event|向线程安全事件队列发布一条通知，供界面主线程消费。
start|启动串口后台线程并标记连接生命周期状态。
command|把界面请求放入任务队列，供串口线程按顺序处理。
send|向串口完整写入字节串；短写时抛出 OSError。
line|把文本编码为 ASCII 并附加真实 CRLF 后发送。
spool|建立新的磁盘采集临时文件，并重置采集计时与状态。
accept|接收已验证负载；保存采样行或核对历史完成计数并切回实时采集。
protocol_event|处理协议事件和普通文本，推进握手、导出及配置状态机。
run|在后台线程打开串口、读写协议与任务，退出时关闭串口和采集文件。
batch_row|累计一行原始数据的 CRC32，校验内容后暂存至当前批次。
batch_frame|处理批次起止帧，整批校验通过后提交 CSV 并确认；损坏则请求重发。
window_frame|处理兼容的 16 条窗口，整组有效才提交，重复窗口只补 ACK。
reject|登记接收错误；逐帧模式最多请求一次重发，窗口模式延迟到组尾处理。
feed|接收任意分片字节流，按行重同步、验帧并分发数据或状态事件。
append|校验采样列并写入 CSV；可选择延迟 flush 到批次提交时执行。
flush|在文件锁内刷新尚未关闭的采集文件，供确认发送前调用。
finish|核对历史总行数和读取错误，只有完整成功才把临时文件替换成最终 CSV。
export|按当前文件长度复制采集快照到临时文件，再原子替换目标 CSV。
text|把一行模拟 MCU 文本追加到串口接收缓冲区。
write|解析电脑发送的命令和 ACK/NACK，推进模拟 MCU 状态并生成响应。
in_waiting|返回模拟接收缓冲区当前可读取的字节数。
read|按指定上限返回模拟串口数据，必要时生成实时帧或历史批次。
setUp|为每个测试建立独立接收器、数据列表和发送记录。
wire|构造带批次边界、数据行和 CRC32 的测试字节序列。
frames|构造兼容窗口的起始、采样和结束测试帧。
receiver|建立测试接收器及用于断言的数据、发送和事件缓存。
wait_event|在截止时间内等待指定设备事件，超时使测试失败。
fail|模拟文件写入失败，验证失败时不会确认未保存的数据。
function|从 MCU 源文件提取指定函数定义，供 Cortex-M0 验证程序使用。
struct|从头文件提取结构声明，供导出函数仿真编译使用。
savefig|保存统计图并关闭图对象，返回图片的 Base64 文本。
table|把统计数据转换为报告中的 HTML 表格。
'''.strip().splitlines():
    key, val = line.split('|',1); descriptions[key]=val

params = dict(root='Tk 主窗口', demo='是否启用纯软件演示', ready='设备是否已就绪', kind='配置或事件类型', value='配置值或事件负载', values='当前采样值列表', info='设备回读状态字典', text='命令、数据或提示文本', index='时分秒字段索引，0/1/2', direction='步进方向，正数递增、负数递减', day='选中月份中的日期', events='线程安全的界面事件队列', success='设置是否成功', message='结果提示文字', port='串口名称或 DEMO', baud='串口波特率', name='任务名或待提取函数名', data='待发送字节串', historical='是否为 15 列历史数据', target='历史完成后的最终文件路径', path='采集临时文件或源文件路径', session='32 位链路会话号', send='发送控制帧的回调', accept='提交已验证负载的回调', event='上报协议事件的回调', line='不含 CRLF 的原始字节行', seq='控制帧序号', payload='数据负载', reason='拒绝接收的原因', chunk='本次接收的任意分片字节', flush='本次写行后是否立即刷新文件', expected='MCU 报告的总记录数', read_errors='MCU 报告的 Flash 读取错误数', destination='CSV 快照目标路径', count='数据行数', start='批次起始序号', timeout='等待上限，单位秒', ending='待提取结构的结尾标记', fig='待保存的 matplotlib 图对象', v='待格式化的单元格值', df='待生成表格的数据', title='表格标题')

for p in list(Path('code/host').glob('*.py'))+list(Path('.').glob('*.py')):
    s=p.read_text(encoding='utf-8-sig'); tree=ast.parse(s); lines=s.splitlines(keepends=True); edits=[]
    for n in ast.walk(tree):
        if not isinstance(n,(ast.FunctionDef,ast.AsyncFunctionDef)): continue
        doc=ast.get_docstring(n,clean=False)
        if doc is None: continue  # 单行辅助函数已有前置中文注释。
        desc=descriptions.get(n.name)
        if n.name=='close':
            desc={'app.py':'请求停止后台线程，等待串口释放后关闭界面。','protocol.py':'在文件锁内 flush、fsync 并关闭采集文件，允许重复调用。','demo.py':'关闭模拟串口，保留与 pyserial 一致的调用接口。'}.get(p.name,'关闭测试串口并释放模拟占用状态。')
        if n.name=='__init__': desc='初始化本对象的状态、缓存及所需资源；参数含义见下方。'
        if n.name.startswith('test_'):
            old=doc.strip().splitlines()[0]
            desc=old if '验证' in old else '验证 '+n.name[5:].replace('_',' ')+'，异常行为由断言检查。'
        if desc is None:
            if re.search('[\u4e00-\u9fff]',doc) and '按调用场景' not in doc: continue
            continue
        args=[a.arg for a in n.args.posonlyargs+n.args.args+n.args.kwonlyargs if a.arg not in ('self','cls')]
        details=[desc]
        if args:
            details+=['','参数：']+[a+'：'+params.get(a, '测试输入，约束见调用处及断言')+'。' for a in args]
        result='无返回值；结果通过对象状态、文件或事件通知。'
        if n.name in ('in_waiting',): result='当前缓冲区可读字节数。'
        elif n.name=='read': result='最多指定长度的字节串。'
        elif n.name=='write': result='已消费的字节数。'
        elif n.name=='config_response': result='True 已处理配置响应，False 交给其他状态处理。'
        elif n.name=='get': result='经过校验的 datetime；非法输入抛出 ValueError。'
        elif n.name in ('function','struct','savefig','table','fmt','c_literal','interval_seconds'): result='提取或格式化后的文本。'
        elif n.name in ('data_frame','control_frame'): result='可直接发送的完整帧字节串。'
        elif n.name=='crc16': result='16 位 CRC 整数。'
        elif n.name=='unpack_frame': result='三元组 (会话号, 序号, 负载字节)。'
        elif n.name=='sample_values': result='格式化后的数字字符串列表。'
        elif n.name=='rtc_datetime': result='校验通过的 datetime。'
        elif n.name=='log_info': result='包含条数、容量、间隔、顺序和开关的字典。'
        elif n.name=='step' or n.name=='move_month': result='事件处理终止标志 break。'
        elif n.name=='append': result='按 VOFA 规则格式化的采样列列表。'
        elif n.name.startswith('test_'): result='无返回值；断言失败抛出测试异常。'
        elif n.name in ('wire','frames','receiver','wait_event'): result='用于测试断言的帧、接收器或匹配事件。'
        details+=['','返回：'+result]
        # 保留原有英文实现细节，替换本次生成的笼统说明。
        if '按调用场景' not in doc and not re.search('[\u4e00-\u9fff]',doc): details+=['','实现说明：'+doc.strip()]
        indent=' '*n.body[0].col_offset; block=indent+'"""'+('\n'+indent).join(details)+'"""\n'
        d=n.body[0]; edits.append((d.lineno-1,d.end_lineno,block))
    for start,end,block in sorted(edits,reverse=True): lines[start:end]=[block]
    p.write_text(''.join(lines),encoding='utf-8')

# 补充 C 内部少数专用参数的精确含义。
replacements={
'id 输入参数 id；缓冲区容量和有效期须满足函数实现要求。':'id 接收 JEDEC 厂商、类型和容量的三字节输出数组。',
'rx 输入参数 rx；缓冲区容量和有效期须满足函数实现要求。':'rx 接收本次 SPI 交换结果的两字节数组。',
'update_cfg 输入参数 update_cfg；缓冲区容量和有效期须满足函数实现要求。':'update_cfg 非 0 时发送新配置，0 时保持当前配置。',
'readback 接收输出参数 readback；缓冲区容量和有效期须满足函数实现要求。':'readback 接收转换结果的输出指针。',
}
for key,what in [('adcHandle','ADC'),('spiHandle','SPI'),('uartHandle','UART')]:
    replacements[key+' 接收输出参数 '+key+'；缓冲区容量和有效期须满足函数实现要求。']=key+' 需要初始化或释放底层资源的 '+what+' 外设句柄。'
for p in list(Path('code/Core').rglob('*.[ch]'))+list(Path('code/code').rglob('*.[ch]')):
    s=p.read_text(encoding='utf-8-sig')
    for a,b in replacements.items(): s=s.replace(a,b)
    p.write_text(s,encoding='utf-8')
