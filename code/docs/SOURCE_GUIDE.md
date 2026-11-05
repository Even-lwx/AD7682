# 源码中文索引

## 固件

| 路径 | 职责与调用位置 |
| --- | --- |
| Core/Src/main.c | 初始化、主循环、串口命令、记录调度、滤波、菜单与独占导出 |
| Core/Src/stm32g0xx_it.c | TIM3 ADC 采样、TIM14 毫秒请求及 UART/DMA 中断入口 |
| Core/Src/i2c.c | PB11/PB12 软件 I2C；GPIO 忙等待，不使用 timeout 参数计时 |
| Core/Src/adc.c、spi.c、usart.c、gpio.c | 工程外设、引脚、DMA 与中断底层初始化 |
| code/ad7682 | 四路 ADC 配置、流水线转换、微秒时基和原始采样 |
| code/battery | 内部 ADC 读取与电池电阻分压换算 |
| code/data_logger | 固定 32 字节 Flash 记录、CRC16、环形索引及内部 Flash 配置 |
| code/lps22hh、sht41、pcf8563 | 双气压计、温湿度和 RTC 驱动 |
| code/oled | 字模、文本布局和主循环分页显示 |
| code/w25q128 | SPI Flash 读取、页编程及扇区擦除 |
| code/uart_crc | 会话序号、CRC16 帧、100/1000 条增量 CRC32 和一次重发 |

TIM3 中断会直接读取 ADC；主循环诊断必须先暂停普通 ADC 采样。TIM14 只做计时、按键消抖和置位请求。其余总线访问及 Flash 导出在主循环执行。独占导出会暂停采样和新日志写入，结束后恢复，期间存在真实采样空档。

## 上位机

| 文件 | 职责 |
| --- | --- |
| app.py | Tk 界面、事件展示、曲线、导出选项和关闭生命周期 |
| datetime_picker.py | 日历、时间输入及鼠标滚轮/键盘步进 |
| device.py | 后台串口线程、连接握手、命令状态机及采集文件切换 |
| protocol.py | 帧校验、重发去重、窗口提交和 VOFA CSV 序列化 |
| demo.py | 无硬件演示串口，模拟数据及故意损坏的首发帧 |
| test_*.py | 协议、CSV、窗口/批次和设备线程的回归测试 |
| verify_*.py | 编译实际 MCU 函数并在 Cortex-M0 仿真器中验证 |

Tk 控件只在主线程更新；串口与磁盘操作在 Device 线程执行，通过事件队列通知界面。历史记录先暂存，经批次校验后写文件并 ACK；收到完成帧且计数正确、Flash 无读取错误才生成最终 CSV。

## 厂商库

| 目录 | 用途与维护方式 |
| --- | --- |
| Drivers/STM32G0xx_HAL_Driver | ST 的 GPIO、SPI、UART、DMA、ADC、RCC、Flash 等硬件抽象接口；工程配置在 Core 中调用，原文件保留厂商注释 |
| Drivers/CMSIS | ARM 内核寄存器、NVIC、核心指令及 STM32 器件定义；由 HAL、启动代码和应用引用 |
| MDK-ARM 启动文件 | 中断向量表、复位入口和运行时启动；交由编译器/芯片支持包维护 |

此处的中文索引用于说明依赖边界，厂商库内部的每个函数仍以官方接口文档为准。
