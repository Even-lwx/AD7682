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

| 目录 | 用途 |
| --- | --- |
| `code/` | 固件与上位机源码；`code/log/` 保留原始实测数据 |
| `pcb/` | 各版本原理图、PCB、元件库与生产资料 |
| `doc/` | 正式器件数据手册 |
| `tools/analysis/` | 历史测量分析和报告生成脚本 |
| `tools/analysis/workspace/` | 从原 `tmp/` 整理的历史分析工作区，保留测试源码与报告依赖 |
| `data/measurements/` | 原始测量工作簿 |
| `output/reports/` | 实验报告、图表及统计结果 |
| `output/releases/` | 上位机各版本发布目录和压缩包 |
| `archive/source_snapshots/` | 旧版源码压缩快照 |
| `archive/reference_downloads/` | 历史下载文件，使用前核对完整性；不替代正式手册 |
| `archive/extracted_reference/` | 已有版本记录的手册提取文本与截图，保留供历史复核 |

历史分析脚本包含固定测试日期和数据列假设，应在本工程根目录运行，例如 `python tools/analysis/analyze.py`。`append_pressure.py` 会改写所选 CSV，使用前保留原始数据。历史工作区中更新源码注释、修改报告或生成代码的脚本不是日常入口，请先阅读脚本再运行。

根目录 `README.md`、`.gitignore` 是工程说明和忽略配置，应保留。Python 缓存、Altium 预览及运行日志可再生成；Altium `History/` 备份和 `Project Outputs*/` 生产输出保留。后续打包或验证可能重新生成 `tmp/`，该目录只存本次运行的临时结果。

## 注释约定

自有 C 函数的定义和公开声明使用中文 `@brief`、`@param`、`@return`；涉及中断、共享总线、超时和破坏性操作时补充限制。Python 函数使用中文 docstring 或短辅助函数前置注释。参数单位和错误处理以函数说明及实现为准。

`code/Drivers/` 是 STM32 HAL/CMSIS 厂商依赖，保留原始版权和官方接口说明，中文用途见源码索引。更新依赖时按厂商版本整体维护。CubeMX 的 `USER CODE` 标记保留，重新生成工程后应检查自定义代码与注释是否被覆盖。
