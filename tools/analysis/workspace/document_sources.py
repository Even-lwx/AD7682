"""本次注释整理工具：保留 C 词法单元和 Python 可执行 AST，单独添加函数文档。"""
import ast
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
C_RE = re.compile(r'^(?P<ret>(?:(?:static|extern|const|unsigned)\s+)*(?:void|int|float|char|uint\d+_t|int\d+_t|HAL_StatusTypeDef|AD7682_Status|GPIO_PinState)\s*\*?\s*)(?P<name>\w+)\s*\((?P<args>[^;{}]*?)\)\s*(?P<end>[{;])', re.M)
LEX = re.compile(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
def mask(text):
    return LEX.sub(lambda m: re.sub(r'[^\n]', ' ', m[0]) if m[0].startswith(('/', '"', "'")) else m[0], text)

# 说明依据实际实现编写；公开接口与定义共用同一用途说明。
DESC = {}
def entries(text):
    for line in text.strip().splitlines():
        name, description = line.split('|', 1)
        DESC[name] = description

entries('''
main|初始化时钟和外设，运行采样、记录、界面与串口命令调度；导出期间进入独占处理分支。
SystemClock_Config|配置系统时钟及总线分频；初始化失败时进入 Error_Handler。
SystemInit|在 C 运行环境建立前初始化芯片系统寄存器，供启动汇编调用。
SystemCoreClockUpdate|根据 RCC 当前配置重新计算 SystemCoreClock，不修改实际时钟配置。
Serial_Write|按当前链路模式发送文本；采样行与完成信息走 CRC 协议，其余文本直接发送。
RecordTimer_Init|配置 TIM3 的 ADC 采样节拍与 TIM14 的毫秒调度节拍并启用中断。
SensorSample_ConsumeRequest|在临界区取走并清除慢速传感器采样请求，避免主循环与中断竞争。
Serial_DumpPauseSampling|保存运行状态，停止 TIM3/TIM14 采样调度并暂停普通任务，进入独占导出。
Serial_DumpResumeSampling|退出独占导出，恢复调度、重建滤波窗口并请求新数据，不补采导出期间的数据。
AdcDiag_SetCoreClock|诊断时切换 MCU 时钟方案，并更新受影响的外设计时配置。
AdcDiag_SetLpsMode|按掩码暂停两颗 LPS22HH，保存或恢复控制寄存器用于诊断对比。
AdcDiag_SetEdgeSpeed|按掩码调整诊断所用 GPIO 的输出边沿速度。
AdcDiag_DumpPhase|将指定诊断阶段的采样缓存和统计数据发送至串口。
AdcDiag_StartPhase|设置当前诊断阶段的硬件条件并启动该阶段采集。
AdcDiag_StopPhase|停止诊断阶段采集，输出结果并恢复阶段修改的硬件状态。
AdcProbe_NextPeriod|计算单通道探测下一次转换的间隔，支持固定、伪随机及突发节奏。
AdcProbe_Finish|结束单通道探测，恢复正常采样状态并输出结束标记。
AdcProbe_ProcessRequest|在主循环处理单通道探测请求，分块采集原始 ADC 与 CNV 时间戳后输出。
AdcDiag_ProcessRequest|在主循环推进多阶段 ADC 诊断状态机，执行阶段切换与恢复。
Devices_Initialize|初始化各传感器、RTC、Flash 日志和 OLED，并报告设备状态。
BatteryLed_Update|根据电池电压更新状态指示灯及对应闪烁状态。
Serial_StartReceive|启动 USART1 循环 DMA 接收，将输入存入接收环形缓冲区。
HAL_UART_RxCpltCallback|处理 HAL 接收完成回调，核对串口实例并维护接收状态。
Serial_PollReceive|在主循环读取 DMA 新到字节，按换行组装命令并检测接收覆盖。
HAL_UART_ErrorCallback|在串口错误回调中记录错误和恢复请求，由主循环完成接收恢复。
Serial_RecoverReceive|在主循环清理 UART 错误、重启 DMA 接收并重置命令拼装状态。
Serial_IsSpace|识别空格、制表符、回车和换行。
Serial_ToLower|将 ASCII 大写字母转换为小写，其他字符保持原值。
Serial_Equals|忽略 ASCII 大小写比较两个以零结尾的字符串。
Serial_Trim|原地去掉首尾空白，返回有效子串起点；调用者必须传入可写字符串。
Serial_Digits|将指定数量的十进制字符解析到输出字节，遇非数字返回失败。
Serial_IsLeap|按公历规则判断指定年份是否为闰年。
Serial_DaysInMonth|计算月份天数并处理闰年二月；调用前须保证月份为 1～12。
Serial_Weekday|计算 2000 年起的公历星期，结果 0 为周日、6 为周六。
Serial_ParseSetTime|解析并校验 SET 命令的日期时间，将合法结果写入 RTC 时间结构。
Serial_DateTimeSeconds|将 RTC 日期时间转换为相对 2000 年的秒数，供历史时间范围比较。
Serial_ParseDumpTime|解析历史导出起止时间并转换为秒数，拒绝格式或日期不合法的输入。
Serial_PrintDumpResult|输出本次历史导出的匹配条数和读取错误数，供上位机完成校验。
Serial_StartLogDump|解析模式和可选时间范围，建立 Flash 序号快照并启动独占历史导出。
Serial_AdcPressureHpa|按厂家压力表将 ADC 码换算为 hPa：码值 × 5 / 65536 × 250 + 225。
Recording_ApplySavedConfig|将 Flash 保存的间隔和记录开关应用到运行时记录调度。
Application_KeyScan1ms|在 TIM14 毫秒中断中扫描按键、消抖并登记事件，不访问总线或打印。
Ui_Draw|根据菜单页面和当前测量值构建 OLED 显示内容，由主循环调用。
Ui_Process|在主循环消费按键事件、修改菜单设置并推进 OLED 分页刷新。
Serial_PrintSettingHelp|通过串口输出设置模式支持的命令和参数格式。
Serial_BatteryCommand|解析电池状态、单点校准及复位命令并返回执行结果。
Serial_ParseIntervalMs|将秒数字符串解析为毫秒，校验 0.5 秒步进及允许范围。
Serial_HandleSetTime|处理串口校时命令，写入 RTC 后回读并报告日期时间和 VL 状态。
Serial_ProcessCommand|分发完整串口命令，处理握手、ACK/NACK、日志、校时与诊断操作。
Serial_DumpLogs|在独占导出循环扫描一条 Flash 记录，筛选、换算和发送，维护批次检查点与重放状态。
Median3|返回三个浮点数的中位数，用于抑制孤立尖峰。
BatteryFilter_Update|依据 ADC 状态更新电池电压滤波与有效性，返回当前滤波结果。
ShtFilter_Update|依据读取状态更新温湿度滤波及有效性，抑制孤立异常读数。
Pressure_IsValid|检查气压值是否落在当前程序接受的有效范围内。
AdcFilter_Insert|将一个 ADC 样本插入时间滤波窗口，更新排序及累计值并返回滤波码值。
RecordCaptureDue|检查记录周期并保存待写快照；只在数据就绪时捕获，避免在中断中写 Flash。
RecordWritePending|在主循环将待写快照追加到 Flash，处理写入状态和记录调度。
Application_AdcSample100Hz|由 TIM3 中断执行四路 ADC 扫描和滤波，发布主循环可读取的采样快照。
Application_AdcGetSnapshot|在临界区复制滤波码和原始码，避免与 ADC 中断并发导致半更新数据。
PressureAverage_Update|更新气压滑动平均窗口，返回当前平均压力。
Samples_ProcessAndOutput|在主循环读取慢速传感器、更新有效性及滤波，并输出实时采样数据。
Error_Handler|关闭中断并停留在错误循环，供无法继续运行的初始化失败调用。
assert_failed|接收 HAL 参数断言失败的位置；当前实现预留给调试，不自动恢复。
SoftI2C_Delay|通过短忙等待控制软件 I2C 位时序，不提供精确微秒延时。
SoftI2C_WritePin|设置软件 I2C 引脚电平并等待一个位时序延迟。
SoftI2C_ReadSda|读取软件 I2C 数据线的当前电平。
SoftI2C_Start|产生软件 I2C 起始或重复起始条件。
SoftI2C_Stop|产生软件 I2C 停止条件并释放数据线。
SoftI2C_WriteByte|按高位优先发送一个字节，在第九个时钟读取从机 ACK。
SoftI2C_ReadByte|按高位优先读取一个字节，并根据参数向从机发送 ACK 或 NACK。
SoftI2C_MasterTransmit|发送设备写地址及连续数据，地址或数据未获 ACK 时停止并返回错误。
SoftI2C_MasterReceive|发送设备读地址并连续接收数据，末字节后发送 NACK。
SoftI2C_MemRead|写入寄存器地址后发重复起始，连续读取指定寄存器数据。
SoftI2C_MemWrite|发送寄存器地址及连续数据，任何写字节未获 ACK 时停止。
MX_I2C1_Init|将 PB11/PB12 配置为软件 I2C 开漏输出并释放总线。
HAL_MspInit|配置 HAL 公共底层资源及系统中断相关设置。
NMI_Handler|处理不可屏蔽异常，当前实现停留在异常循环等待调试。
HardFault_Handler|处理处理器硬件错误，停留在异常循环保留故障现场。
SVC_Handler|系统服务调用异常入口，当前工程未添加业务处理。
PendSV_Handler|可挂起系统服务异常入口，当前工程未添加业务处理。
SysTick_Handler|维护 HAL 毫秒计数，为超时和主循环调度提供时间基准。
USART1_IRQHandler|转交 HAL 处理 USART1 中断，触发串口错误等 HAL 回调。
DMA1_Channel1_IRQHandler|转交 HAL 处理 USART1 RX DMA 通道中断。
TIM3_IRQHandler|清除 TIM3 更新标志，在正常采样启用时执行 100 Hz ADC 采样。
TIM14_IRQHandler|维护毫秒节拍、扫描按键并设置慢速采样请求；不在此读传感器或写 Flash。
AD7682_TimebaseInit|配置专用 TIM2 为 1 MHz 自由运行计数器，供 CNV 时序和采样时间戳使用。
AD7682_TimeUs|读取 TIM2 微秒计数；32 位计数会回绕，时间差需用无符号减法。
AD7682_DelayUs|用 TIM2 忙等待指定微秒数，调用期间占用 CPU。
AD7682_SpiExchange|执行一次 ADC 的 16 位 SPI 寄存器交换，并检查超时及硬件错误。
AD7682_Transfer|控制 CNV、采集等待和配置字交换，取得一次流水线转换结果。
AD7682_Init|初始化驱动句柄、CNV 与微秒时基，保存 SPI 和参考电压配置。
AD7682_EnsureChannel|切换到指定 ADC 通道，丢弃流水线旧结果以建立有效通道配置。
AD7682_ReadChannel|读取一路有效 16 位 ADC 原始码；失败时不提交输出。
AD7682_ReadChannelSamples|读取同一通道五个有效样本，不进行中值滤波。
AD7682_ReadChannelTimedSamples|读取同一通道五个有效样本及 CNV 附近的微秒时间戳；失败时不提交输出数组。
AD7682_Median5|对五个原始码的副本排序并返回中位数，不修改输入数组。
AD7682_ReadFourChannels|扫描 IN0～IN3，每路五次有效转换取中位数作为输出。
AD7682_ReadFourChannelsSamples|扫描 IN0～IN3 并返回每路五个有效样本，供上层滤波或诊断。
AD7682_ReadRaw|兼容旧接口，读取 IN0 通道原始码。
AD7682_ReadVoltage|读取 IN0 原始码，按句柄中的参考电压换算为伏特。
BatteryAdc_ReadRaw|兼容旧接口读取电池 ADC；失败返回 0，需区分错误时使用 ReadRawChecked。
BatteryAdc_ReadRawChecked|启动内部 ADC、等待转换并停止 ADC，成功后提交原始码。
BatteryAdc_ReadVoltage|读取内部 ADC，按参考电压和电阻分压比换算电池端电压。
BatteryAdc_CalibratedVoltage|按 3.3 V 参考、12 位 ADC 和 100k/47k 分压电阻计算电池电压。
Logger_Crc16|计算 Flash 记录及配置使用的 CRC16/MODBUS，初值 FFFF、多项式 A001。
Logger_Put16|将 16 位整数按小端序写入字节缓冲区。
Logger_Put32|将 32 位整数按小端序写入字节缓冲区。
Logger_Get16|从字节缓冲区按小端序解码 16 位整数。
Logger_Get32|从字节缓冲区按小端序解码 32 位整数。
Logger_EncodeTemperature|将摄氏温度编码为 0.01 °C 整数，越界或无效值编码为 -32768。
Logger_EncodeHumidity|将相对湿度编码为 0.01 %RH 整数，无效值编码为 FFFF。
Logger_EncodePressure|将压力编码为 0.1 hPa 有符号整数，无效值编码为 -32768。
Logger_ConfigValid|校验内部 Flash 配置记录的魔数、版本及 CRC16。
Logger_ConfigErased|判断配置槽是否全部为 FF，即尚未编程。
Logger_LoadConfig|扫描内部 Flash 配置槽，恢复最新有效配置；无有效记录时保持默认值。
Logger_SaveConfig|追加保存带 CRC 的内部 Flash 配置，配置页满时擦除后重新写入。
Logger_PutPackedData|将版本专用的打包位域写入日志记录的分散存储字段。
Logger_GetPackedData|从日志记录的分散字段还原版本专用打包位域。
Logger_EncodeSample|把传感器快照编码为固定 32 字节日志，写入版本、序号及 CRC16。
Logger_DecodeSample|校验日志魔数、版本和 CRC16，兼容旧格式并解码采样字段。
Logger_WriteMetadata|向外部 Flash 元数据槽追加环形日志状态，槽满时擦除元数据扇区。
DataLogger_Init|绑定 Flash SPI，恢复元数据和配置，扫描有效记录以重建环形日志状态。
DataLogger_Append|编码并追加一条日志，按扇区擦除策略更新环形写指针和元数据。
DataLogger_Read|按当前导出顺序读取相对序号对应的记录，并校验记录 CRC。
DataLogger_Clear|清空日志元数据及运行时索引，保留内部 Flash 的设备配置。
DataLogger_SetIntervalMs|校验并持久化记录间隔；合法值为 500～86400000 ms 且为 500 的倍数。
DataLogger_SetDumpNewestFirst|设置并保存历史读取顺序，保存失败时恢复原设置。
DataLogger_Count|读取当前环形日志的有效记录数量。
DataLogger_SetRecordingEnabled|设置并持久化记录开关，保存失败时恢复原值。
DataLogger_GetRecordingEnabled|读取当前记录开关。
DataLogger_GetBatteryGain|读取配置中保存的电池校准增益。
DataLogger_GetDisplayAdcOnly|读取是否仅显示 ADC 的菜单配置。
DataLogger_SetMenuConfig|统一校验并保存显示模式、记录开关、导出顺序和间隔；失败回滚运行时配置。
DataLogger_SetBatteryGain|保存 0.5～1.5 范围的电池单点增益；失败时恢复旧值。
DataLogger_LastSequence|读取最近一条日志的绝对序号，用于建立导出快照。
DataLogger_ReadSequence|按绝对序号读取日志，核对存储序号及 CRC，拒绝已被覆盖的记录。
DataLogger_Capacity|返回外部 Flash 数据区可容纳的最大记录条数。
DataLogger_GetIntervalMs|读取当前记录间隔，单位毫秒。
DataLogger_GetDumpNewestFirst|读取当前导出顺序配置。
LPS22HH_ReadRegisters|从指定器件连续读取气压计寄存器。
LPS22HH_WriteRegister|向指定器件写入一个气压计寄存器。
LPS22HH_ReadIdAt|读取指定 I2C 地址的 WHO_AM_I 并将结果交给调用者。
LPS22HH_ReadIdAtAddress|提供指定地址的器件 ID 读取接口，便于诊断接线和地址。
LPS22HH_ReadId|兼容单器件接口，读取默认低地址气压计的 ID。
LPS22HH_InitDevice|检查指定气压计 ID，执行复位并配置 1 Hz 连续采样。
LPS22HH_Init|初始化默认全局气压计器件。
LPS22HH_SetPowerDown|保存 CTRL_REG1 后关闭 ODR 转换，供 ADC 干扰诊断使用。
LPS22HH_RestoreCtrlReg1|恢复先前保存的 CTRL_REG1，使气压计回到诊断前状态。
LPS22HH_ReadDevice|读取指定气压计的压力和温度原始寄存器，转换为 hPa 与 °C。
LPS22HH_ReadDiagnostic|读取 ID、控制、状态和原始测量寄存器，供串口诊断使用。
LPS22HH_Read|兼容单器件接口，读取默认气压计的压力和温度。
Send|设置 OLED D/C 和片选，通过 SPI 发送命令或显示数据。
WritePage|设置 OLED 页地址与列地址，写入一页 128 字节像素。
SSD1306_Init|绑定 SPI、复位 OLED 并发送显示控制器初始化命令。
SSD1306_BeginFrame|在刷新周期到达且旧帧已完成时开始普通显示帧。
SSD1306_SetLine|复制一行文本到显示任务的行缓存，供后续逐页绘制。
SSD1306_BeginUiFrame|立即开始用户操作触发的新帧，替换未完成的旧显示帧。
SSD1306_SetLargeText|设置当前帧是否采用二倍大字体。
SSD1306_SetDashboard|将当前帧切换为五行仪表盘显示布局。
SSD1306_Process|在主循环生成并发送至多一个显示页，避免长时间占用 SPI。
PCF8563_BcdToInt|将已屏蔽控制位的 BCD 数字解码为十进制整数。
PCF8563_IntToBcd|将十进制整数编码为 RTC 寄存器使用的 BCD 数字。
PCF8563_ReadRegisters|从 RTC 指定起始寄存器连续读取字节。
PCF8563_WriteRegisters|向 RTC 指定起始寄存器连续写入字节。
PCF8563_ReadDateTime|读取 RTC 日期时间，屏蔽控制位并将 BCD 字段转为整数。
PCF8563_WriteDateTime|校验时间字段范围，将日期时间及世纪位编码后写入 RTC。
PCF8563_SetRunning|读改写控制寄存器 STOP 位，启动或停止 RTC 计时。
PCF8563_GetVoltageLow|读取 RTC 秒寄存器 VL 标志，供上电和校时有效性检查。
SHT41_Crc8|按 SHT41 的 CRC8 算法校验测量字节，初值 FF、多项式 31。
SHT41_Reset|向 SHT41 发送软件复位命令并等待器件恢复。
SHT41_Read|启动温湿度测量，等待结果并校验两个 CRC8，成功后换算 °C 和 %RH。
Link_Crc32Update|用 16 项半字节表增量更新 CRC32/ISO-HDLC 内部状态，不在此执行最终异或。
Link_Crc16|用半字节查表计算串口控制帧的 CRC16/MODBUS。
Link_ParseHex|解析指定字符数的十六进制数字，成功后提交 32 位输出。
UartCrc_ParseHex8|严格解析八个十六进制字符，供串口会话号参数使用。
UartCrc_Enable|以给定会话号启用可靠链路，重置序号、重试及批次状态。
UartCrc_Disable|关闭可靠链路并清除待确认、失败及窗口状态。
UartCrc_IsEnabled|读取可靠链路是否启用。
UartCrc_IsBusy|读取是否有等待 ACK 的帧或批次，主循环据此暂停继续发送。
UartCrc_IsFailed|读取链路是否在一次重试后仍失败。
Link_Fail|标记链路失败、释放待确认状态并输出重新连接提示。
Link_Transmit|阻塞发送当前串口帧并记录发送结束时刻；本地完成仍需对端 ACK。
Link_Retry|执行最多一次重发；窗口模式通知主循环按 Flash 检查点整组重读。
UartCrc_Send|封装并发送可靠帧；整批模式的数据行直接发送，同时增量累计 CRC32 和行数。
UartCrc_Process|在主循环检查 500 ms ACK 超时，触发一次重发或链路失败。
UartCrc_HandleControl|校验 ACK/NACK 的 CRC、会话与序号，正确 ACK 释放发送状态，NACK 触发重发。
UartCrc_SetWindow|选择导出模式：0 逐帧、1 为 16 条窗口、2 为 100 条批次、3 为 1000 条批次。
UartCrc_WindowSize|返回当前模式的扫描上限：100、1000 或兼容窗口的 16 条。
UartCrc_WindowEnabled|返回窗口模式编码，0 表示关闭，1/2/3 表示对应窗口或批次模式。
UartCrc_WindowOpen|读取是否正在发送窗口或批次内部数据。
UartCrc_TakeReplay|取走并清除整组重放请求，由主循环恢复 Flash 导出检查点。
UartCrc_WindowBegin|保存窗口起始序号，重置增量 CRC32 和条数，并发送受 CRC16 保护的起始帧。
UartCrc_WindowEnd|发送窗口结束帧；整批模式附带行数和 CRC32，随后等待 ACK。
W25Q128_SetCs|控制 Flash 片选，低电平选中芯片。
W25Q128_SendCommand|在一次片选事务中发送不带地址的单字节 Flash 命令。
W25Q128_WaitReady|轮询状态寄存器 BUSY，等待编程或擦除结束，达到时限返回超时。
W25Q128_ReadJedec|读取 JEDEC 的厂商、器件类型和容量三个字节。
W25Q128_Read|以 24 位地址发送普通读取命令，将连续数据写入调用者缓冲区。
W25Q128_Write|先写使能再执行一次页编程并等待完成；调用者须确保不跨 256 字节页且目标已擦除。
W25Q128_EraseSector|擦除包含指定地址的 4 KB 扇区并等待 BUSY 清零，会破坏该扇区原数据。
''')

PARAM = dict(
    spi='SPI 外设句柄，须已完成初始化。', hspi='SPI 外设句柄，须已完成初始化。',
    handle='外设或软件总线句柄，须由调用者正确初始化。', dev='AD7682 驱动句柄。', device='目标气压计器件及其 I2C 地址。',
    hadc='内部 ADC 句柄。', hi2c='软件 I2C 总线句柄。', huart='触发回调的 UART 句柄。',
    timeout='底层事务超时，单位 ms；不是整个上层操作的总时间上限。', timeout_ms='单次事务超时，单位 ms。',
    channel='ADC 输入通道编号，IN0～IN3 对应 0～3。', code='ADC 码值；指针形式为输出，成功后写入。',
    samples='接收有效 ADC 样本的数组，容量按声明维度提供。', timestamps_us='接收 CNV 附近时间戳的数组，单位 µs。',
    codes='接收 IN0～IN3 ADC 码值的四元素数组。', values='参与中位数计算的五元素输入数组。',
    vref_volts='ADC 参考电压，单位 V。', vref='ADC 参考电压，单位 V。', divider_ratio='电池端电压与 ADC 引脚电压的比例。',
    voltage='电压值，单位 V；指针形式用于接收输出。', raw='ADC 原始码；指针形式用于接收输出。',
    pressure_hpa='接收压力结果的输出指针，单位 hPa。', temperature_c='接收温度结果的输出指针，单位 °C。',
    humidity_rh='接收相对湿度结果的输出指针，单位 %RH。', temperature='本次温度读数，单位 °C。', humidity='本次相对湿度读数，单位 %RH。',
    sample='传感器快照；const 指针为待写入输入，其他指针为读取输出。',
    sequence='日志或链路的绝对序号。', ordinal='从 0 开始的相对读取位置，顺序由导出设置决定。',
    milliseconds='记录间隔，单位 ms。', interval='记录间隔，单位 ms，按 500 ms 步进。',
    newest_first='0 从旧到新，非 0 从新到旧。', recording='记录开关，0 关闭、1 开启。', adc_only='0 显示全部，1 仅显示 ADC。',
    enabled='功能开关或模式编码，具体取值见函数用途。', gain='电池单点校准增益，允许 0.5～1.5。',
    address='目标地址；Flash 为字节地址，软件 I2C 为左移一位的器件地址。',
    buffer='数据缓冲区，容量须不小于 length；const 为待发送输入，否则用于接收。',
    length='连续处理的字节数，数组尺寸须与该长度匹配。', size='输入字节数。',
    data='待处理字节或数据区；指针的输入输出方向由 const 和函数用途确定。',
    p='小端字节缓冲区，须提供足够的 2 或 4 字节空间。', value='待编码、待解析或待写入的值；指针形式用于接收解析输出。',
    config='一条固定长度的内部 Flash 配置记录。', record='固定 32 字节 Flash 日志记录。',
    packed='版本专用的打包位域整数。', r='接收或提供打包字段的日志字节缓冲区。',
    reg='起始寄存器地址。', device_id='接收 WHO_AM_I 的输出指针。', diagnostic='接收诊断寄存器快照的输出结构。',
    saved_ctrl_reg1='接收暂停前 CTRL_REG1 的输出指针，恢复时使用。', ctrl_reg1='需要写回的 CTRL_REG1 原值。',
    datetime='RTC 日期时间结构；const 为输入，其他指针用于读取输出。', dt='RTC 日期时间结构，按函数用途作为输入或解析输出。',
    running='非 0 启动 RTC，0 停止。', voltage_low='VL 标志输出指针；允许 NULL，此时仅读取不写回。',
    now='当前 HAL 毫秒时间戳。', row='文本显示行编号，从 0 开始。', text='以零结尾的文本或命令字符串。',
    bytes='待发送的 OLED 命令或像素字节数组。', count='待发送的字节数。', page='OLED 页编号。', pixels='一页 128 字节的像素数据。',
    state='目标 GPIO 电平，RESET 为低、SET 为高。', port='GPIO 端口。', pin='GPIO 引脚掩码。',
    ack='非 0 发送 ACK，0 发送 NACK。', command='命令字或以零结尾的串口命令。',
    session='上位机协商的 32 位会话号。', payload='以零结尾的负载文本，可带 CRLF，长度受帧缓冲区限制。',
    line='以零结尾的 ACK/NACK 命令行。', crc='尚未最终异或的 CRC32 累计状态。', digits='需要解析的十六进制字符数。',
    us='等待时长，单位 µs。', wire='送入 SPI 的 16 位数据字。', cfg='ADC 配置字。',
    mode='硬件诊断模式编号。', phase='诊断阶段编号。', power_down_mask='气压计停机选择掩码。', high_mask='GPIO 高速边沿选择掩码。',
    c='待判断或转换的 ASCII 字符。', a='第一个输入值或字符串。', b='第二个输入值或字符串。',
    s='待处理字符串；原地修剪接口要求可写内存。', n='需要解析的十进制字符数。', year='公历年份。', month='月份，1～12。', day='日期，须已校验有效。',
    seconds='接收相对 2000 年秒数的输出指针。', result='接收记录间隔毫秒数的输出指针。',
    filtered='接收四路滤波 ADC 码值的数组。', filter='由本函数更新的滤波状态。',
    status='本次传感器或 ADC 读取状态，用于判断读数是否有效。',
    file='触发断言的源文件名。'
)

RET = {
    'SoftI2C_WriteByte':'1 收到从机 ACK，0 未收到 ACK。', 'SoftI2C_ReadByte':'从总线接收的一个字节。',
    'Serial_ToLower':'转换后的 ASCII 字符。', 'Serial_DaysInMonth':'该月份的天数。', 'Serial_Weekday':'星期编号，0 为周日、6 为周六。',
    'Serial_DateTimeSeconds':'相对 2000 年的秒数；无效日期返回实现中的无效哨兵值。',
    'AdcProbe_NextPeriod':'下一次转换的调度间隔，单位 µs。',
    'UartCrc_HandleControl':'1 表示已识别为控制帧（含损坏或过期控制帧），0 表示不是控制帧。',
    'UartCrc_WindowEnabled':'模式编码：0 关闭、1 为 16 条窗口、2 为 100 条、3 为 1000 条。',
    'UartCrc_WindowSize':'每组扫描的最大 Flash 记录条数。',
    'DataLogger_GetIntervalMs':'记录间隔，单位 ms。', 'DataLogger_GetBatteryGain':'电池校准增益。',
    'DataLogger_Count':'当前有效记录数。', 'DataLogger_Capacity':'可容纳的最大记录条数。', 'DataLogger_LastSequence':'最近记录的绝对序号。',
    'Link_Crc32Update':'更新后的内部 CRC32 状态；整批结束时再异或 FFFFFFFF。',
    'AD7682_TimeUs':'32 位微秒计数，会自然回绕。', 'Serial_Trim':'修剪后的子串首地址，仍指向原缓冲区。',
    'BatteryAdc_ReadRaw':'ADC 原始码；读取失败也返回 0。', 'Logger_EncodeTemperature':'0.01 °C 定点值或 -32768 无效哨兵。',
    'Logger_EncodeHumidity':'0.01 %RH 定点值或 FFFF 无效哨兵。', 'Logger_EncodePressure':'0.1 hPa 定点值或 -32768 无效哨兵。'
}

def c_description(name):
    if name in DESC:
        return DESC[name]
    if name.startswith('MX_') and name.endswith('_Init'):
        return '初始化 ' + name[3:-5] + ' 的工程参数、引脚及相关外设配置。'
    if '_MspDeInit' in name:
        return '释放对应外设的底层时钟、GPIO、DMA 或中断资源。'
    if '_MspInit' in name:
        return '为对应外设配置底层时钟、GPIO、DMA 或中断资源。'
    raise ValueError('缺少 C 用途说明: '+name)

def c_doc(m, path):
    name, ret = m['name'], m['ret'].strip()
    lines = ['/**', ' * @brief '+c_description(name)]
    for arg in m['args'].split(','):
        arg = arg.strip()
        if not arg or arg == 'void':
            continue
        ident_match = re.search(r'(\w+)\s*(?:\[[^\]]*\]\s*)*$', arg)
        if not ident_match:
            continue
        ident = ident_match[1]
        description = PARAM.get(ident)
        if ident == 'timeout' and ('SoftI2C' in name or path.name in ('pcf8563.c','pcf8563.h','sht41.c','sht41.h','lps22hh.c','lps22hh.h')):
            description = '保留的超时参数，单位 ms；当前软件 I2C 位操作不使用该值计时。'
        if ident == 'raw' and name == 'Application_AdcGetSnapshot':
            description = '接收四路 ADC 原始码的数组。'
        if ident == 'c' and name == 'Median3':
            description = '第三个待比较的浮点数。'
        if ident == 'data' and name == 'Send':
            description = 'D/C 选择，0 发送命令，非 0 发送像素数据。'
        if description is None:
            # 个别内部缓冲区名称只出现在驱动实现中；仍给出明确的方向和
            # 生命周期要求，避免留下没有用途说明的参数。
            direction = '接收输出' if '*' in arg and 'const' not in arg else '输入'
            description = f'{direction}参数 {ident}；缓冲区容量和有效期须满足函数实现要求。'
        lines.append(' * @param '+ident+' '+description)
    if name in RET:
        result = RET[name]
    elif ret.endswith('void'):
        result = '无返回值。'
    elif 'HAL_StatusTypeDef' in ret:
        result = 'HAL_OK 成功；其他 HAL 状态表示参数、外设或存储操作失败。'
        if name == 'UartCrc_Send':
            result = 'HAL_OK 已提交本次发送；HAL_BUSY 尚未确认上一帧；HAL_ERROR 链路不可用或负载超限。'
    elif 'AD7682_Status' in ret:
        result = 'AD7682_OK 成功；AD7682_ERROR 参数或外设错误；AD7682_TIMEOUT 转换通信超时。'
    elif 'GPIO_PinState' in ret:
        result = 'GPIO_PIN_SET 为高电平，GPIO_PIN_RESET 为低电平。'
    elif any(x in name for x in ('Crc16','Crc8')):
        result = '输入数据对应的校验值。'
    elif name.startswith(('Logger_Get','PCF8563_')):
        result = '解码或转换后的整数。'
    elif name == 'AD7682_Median5':
        result = '五个输入 ADC 码值的中位数。'
    elif 'float' in ret:
        result = '计算后的值，量纲与用途说明一致。'
    else:
        result = '非 0 表示条件成立或操作成功，0 表示条件不成立或失败。'
    lines.append(' * @return '+result)
    if path.suffix == '.c' and (path.parent.name in ('w25q128','lps22hh','pcf8563','sht41','oled','data_logger','uart_crc') or name.startswith('Serial_')):
        lines.append(' * @note 在主循环调用；涉及总线或 Flash 的操作可能阻塞，不用于中断内执行。')
    if name.startswith('AD7682_') and name not in ('AD7682_TimeUs','AD7682_Median5'):
        lines.append(' * @note 与 TIM3 采样共享 ADC 资源，主循环诊断调用前须暂停正常采样；禁止并发访问。')
    if name == 'AD7682_ReadChannel' or name == 'AD7682_ReadChannelTimedSamples':
        lines.append(' * @note timeout_ms 为单次 SPI 超时，上限 1000 ms；失败不提交输出。')
    if name.startswith('SoftI2C_'):
        lines.append(' * @note 当前实现为 GPIO 忙等待，不支持时钟拉伸检测；须由上层串行调用。')
    lines.append(' */\n')
    return '\n'.join(lines)

def c_tokens(s):
    return re.findall(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|\w+|[^\s]', re.sub(r'/\*[\s\S]*?\*/|//[^\n]*', '', s))

def strip_docs(tree):
    for node in ast.walk(tree):
        if isinstance(node, (ast.Module, ast.ClassDef, ast.FunctionDef, ast.AsyncFunctionDef)):
            if node.body and isinstance(node.body[0], ast.Expr) and isinstance(node.body[0].value, ast.Constant) and isinstance(node.body[0].value.value, str):
                node.body.pop(0)
    return ast.dump(tree, include_attributes=False)

if __name__ == '__main__':
    paths = sorted(list((ROOT/'code/Core').rglob('*.c')) + list((ROOT/'code/Core').rglob('*.h')) + list((ROOT/'code/code').rglob('*.c')) + list((ROOT/'code/code').rglob('*.h')))
    stats = {}
    for p in paths:
        original = p.read_bytes()
        text = original.decode('utf-8-sig')
        matches = [m for m in C_RE.finditer(mask(text)) if m['end']=='{' or p.suffix=='.h']
        edits = [(m.start(), c_doc(m, p)) for m in matches]
        new = text
        for pos, doc in reversed(edits):
            new = new[:pos]+doc+new[pos:]
        # 纠正与独占导出设计相冲突的旧注释。
        new = new.replace('每次只发送一条历史记录，避免导出过程长时间阻塞采样。', '每次扫描一条历史记录；由暂停采样后的独占主循环连续调用。')
        new = new.replace('每次主循环只发送一条，不阻塞采样、记录和命令处理。', '每次调用只扫描一条；独占主循环持续调用，并在记录之间处理命令和 ACK。')
        assert c_tokens(text) == c_tokens(new), str(p)
        if edits:
            newline = '\r\n' if b'\r\n' in original else '\n'
            p.write_bytes(new.replace('\r\n','\n').replace('\n',newline).encode('utf-8-sig' if original.startswith(b'\xef\xbb\xbf') else 'utf-8'))
            stats[str(p.relative_to(ROOT))] = len(edits)
    (ROOT/'tools/analysis/workspace/comment_stats.json').write_text(json.dumps(stats, ensure_ascii=False, indent=2), encoding='utf-8')
    print('C 函数定义/声明注释: ', sum(stats.values()), '文件: ', len(stats))
