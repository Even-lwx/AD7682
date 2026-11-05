#ifndef UART_CRC_H
#define UART_CRC_H

#include "stm32g0xx_hal.h"

/* 用于采样行和导出完成信息的 ASCII 停等传输。
 * 串口保持 921600 8E1，需显式启用；默认使用原有 VOFA 输出。 */
/**
 * @brief 以给定会话号启用可靠链路，重置序号、重试及批次状态。
 * @param session 上位机协商的 32 位会话号。
 * @return 无返回值。
 */
void UartCrc_Enable(uint32_t session);
/**
 * @brief 关闭可靠链路并清除待确认、失败及窗口状态。
 * @return 无返回值。
 */
void UartCrc_Disable(void);
/**
 * @brief 读取可靠链路是否启用。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 */
uint8_t UartCrc_IsEnabled(void);
/**
 * @brief 读取是否有等待 ACK 的帧或批次，主循环据此暂停继续发送。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 */
uint8_t UartCrc_IsBusy(void);
/**
 * @brief 读取链路是否在一次重试后仍失败。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 */
uint8_t UartCrc_IsFailed(void);
/**
 * @brief 封装并发送可靠帧；整批模式的数据行直接发送，同时增量累计 CRC32 和行数。
 * @param payload 以零结尾的负载文本，可带 CRLF，长度受帧缓冲区限制。
 * @return HAL_OK 已提交本次发送；HAL_BUSY 尚未确认上一帧；HAL_ERROR 链路不可用或负载超限。
 */
HAL_StatusTypeDef UartCrc_Send(const char *payload);
/**
 * @brief 在主循环检查 500 ms ACK 超时，触发一次重发或链路失败。
 * @return 无返回值。
 */
void UartCrc_Process(void);
/**
 * @brief 校验 ACK/NACK 的 CRC、会话与序号，正确 ACK 释放发送状态，NACK 触发重发。
 * @param line 以零结尾的 ACK/NACK 命令行。
 * @return 1 表示已识别为控制帧（含损坏或过期控制帧），0 表示不是控制帧。
 */
uint8_t UartCrc_HandleControl(const char *line);
/**
 * @brief 严格解析八个十六进制字符，供串口会话号参数使用。
 * @param text 以零结尾的文本或命令字符串。
 * @param value 待编码、待解析或待写入的值；指针形式用于接收解析输出。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 */
uint8_t UartCrc_ParseHex8(const char *text, uint32_t *value);
/**
 * @brief 选择导出模式：0 逐帧、1 为 16 条窗口、2 为 100 条批次、3 为 1000 条批次。
 * @param enabled 功能开关或模式编码，具体取值见函数用途。
 * @return 无返回值。
 */
void UartCrc_SetWindow(uint8_t enabled);
/**
 * @brief 返回窗口模式编码，0 表示关闭，1/2/3 表示对应窗口或批次模式。
 * @return 模式编码：0 关闭、1 为 16 条窗口、2 为 100 条、3 为 1000 条。
 */
uint8_t UartCrc_WindowEnabled(void);
/**
 * @brief 读取是否正在发送窗口或批次内部数据。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 */
uint8_t UartCrc_WindowOpen(void);
/**
 * @brief 取走并清除整组重放请求，由主循环恢复 Flash 导出检查点。
 * @return 非 0 表示条件成立或操作成功，0 表示条件不成立或失败。
 */
uint8_t UartCrc_TakeReplay(void);
/**
 * @brief 保存窗口起始序号，重置增量 CRC32 和条数，并发送受 CRC16 保护的起始帧。
 * @return 无返回值。
 */
void UartCrc_WindowBegin(void);
/**
 * @brief 发送窗口结束帧；整批模式附带行数和 CRC32，随后等待 ACK。
 * @return 无返回值。
 */
void UartCrc_WindowEnd(void);
/**
 * @brief 返回当前模式的扫描上限：100、1000 或兼容窗口的 16 条。
 * @return 每组扫描的最大 Flash 记录条数。
 */
uint16_t UartCrc_WindowSize(void);

#endif

