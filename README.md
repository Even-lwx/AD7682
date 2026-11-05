# AD7682 采集记录工程

STM32G031C8 固件负责四路 AD7682、温湿度、双气压计、电池、RTC、OLED 和 W25Q128 日志；Windows 上位机负责可靠串口连接与 VOFA 格式 CSV 导出。

## 从哪里开始

- 固件工程：`code/MDK-ARM/code.uvprojx`；CubeMX 配置：`code/code.ioc`。
- 应用入口与调度：`code/Core/Src/main.c`；中断入口：`code/Core/Src/stm32g0xx_it.c`。
- 自有驱动：`code/code/`；模块用途见[源码索引](code/docs/SOURCE_GUIDE.md)。
- 上位机操作、协议、CSV 列定义和验证命令：[上位机说明](code/host/README.md)。
- 上位机开发环境、依赖安装、测试和打包：[上位机开发文档](code/host/DEVELOPMENT.md)。
- ADC 当前滤波：[滤波说明](code/code/ad7682/FILTER_CURRENT.md)。
- 电池换算：[电池说明](code/code/battery/CALIBRATION.md)。
- 旧 ADC 实验：[历史目录](code/docs/archive/README.md)，仅用于回溯实验。

## 目录约定

`doc/` 保存器件资料，`pcb/` 保存硬件设计，`code/` 保存固件与上位机源码。`code/log/` 为实测数据，`output/` 为导出结果与发布包，`tmp/` 为本机临时构建和验证产物。根目录分析脚本用于既有测量报告，包含固定输入路径，运行前须核对目标数据。

## 注释约定

自有 C 函数的定义和公开声明使用中文 `@brief`、`@param`、`@return`；涉及中断、共享总线、超时和破坏性操作时补充限制。Python 函数使用中文 docstring 或短辅助函数前置注释。参数单位和错误处理以函数说明及实现为准。

`code/Drivers/` 是 STM32 HAL/CMSIS 厂商依赖，保留原始版权和官方接口说明，中文用途见源码索引。更新依赖时按厂商版本整体维护。CubeMX 的 `USER CODE` 标记保留，重新生成工程后应检查自定义代码与注释是否被覆盖。
