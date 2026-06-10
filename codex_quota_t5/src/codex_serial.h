/**
 * @file codex_serial.h
 * @brief 串口配置命令解析器 —— 公共接口定义
 *
 * 通过 COM11 (debug UART) 接收配置命令，支持以下协议：
 *   SET WIFI <ssid> <password>     —— 配置 WiFi 热点
 *   SET BRIDGE <host> <port>       —— 配置桥接服务器地址
 *   SET MQTT <host> <port>         —— 配置 MQTT 代理地址
 *   GET CONFIG                     —— 查询当前运行时配置
 *   SAVE                           —— 将配置持久化到 NVS（TODO）
 *   REBOOT                         —— 立即重启设备
 *
 * 设计要点：
 *   - 非阻塞架构：codex_serial_poll() 在主循环中周期调用，不阻塞
 *   - 256 字节环形接收缓冲区，逐字节积累直到遇到 '\\n'
 *   - 通过 tkl_uart_read/write 操作底层硬件 UART
 */

#ifndef CODEX_SERIAL_H
#define CODEX_SERIAL_H

#include <stdint.h>

/**
 * @brief 串口命令类型枚举
 *
 * 每条接收到的命令会被解析为以下类型之一。
 * 解析后由调用者（tuya_main.c 主循环）根据 type 字段分发处理。
 */
typedef enum {
    CMD_SET_WIFI,       /**< SET WIFI <ssid> <password> —— arg1=ssid, arg2=password */
    CMD_SET_BRIDGE,     /**< SET BRIDGE <host> <port>   —— arg1=host, arg2=port_str */
    CMD_SET_MQTT,       /**< SET MQTT <host> <port>     —— arg1=host, arg2=port_str */
    CMD_GET_CONFIG,     /**< GET CONFIG                 —— 查询当前配置，无参数 */
    CMD_SAVE,           /**< SAVE                       —— 保存到 NVS，无参数 */
    CMD_REBOOT,         /**< REBOOT                     —— 重启设备，无参数 */
    CMD_UNKNOWN,        /**< 未知命令或 HELP            —— 调用者打印帮助信息 */
} serial_cmd_type_t;

/**
 * @brief 串口命令结构体
 *
 * 封装解析后的命令及其参数。arg1/arg2 的含义取决于 type 字段：
 *   CMD_SET_WIFI:   arg1 = SSID, arg2 = 密码
 *   CMD_SET_BRIDGE: arg1 = 主机地址, arg2 = 端口字符串
 *   CMD_SET_MQTT:   arg1 = 主机地址, arg2 = 端口字符串
 *   其他:           arg1/arg2 为空字符串
 */
typedef struct {
    serial_cmd_type_t type;     /**< 命令类型 */
    char arg1[64];              /**< 第一个参数（ssid / host / 空） */
    char arg2[128];             /**< 第二个参数（password / port_str / 空） */
} serial_cmd_t;

/**
 * @brief 初始化串口命令解析器
 *
 * 清空接收缓冲区，标记解析器为就绪状态。
 * 底层 UART 硬件初始化（tkl_uart_init）通常由 board_init 完成，
 * 此处仅设置内部标记。
 *
 * @param[in] uart_id  UART ID（T5AI debug UART 通常为 1，对应 COM11）
 * @return 0 成功, -1 失败
 */
int codex_serial_init(uint8_t uart_id);

/**
 * @brief 串口命令非阻塞轮询
 *
 * 在主循环中周期调用。内部流程：
 *   1. 调用 tkl_uart_read() 尝试读取最多 64 字节
 *   2. 逐字节追加到 256 字节内部缓冲区
 *   3. 遇到 '\\n' 时视为一行命令结束，调用 parse_command() 解析
 *   4. 解析结果写入 cmd 输出参数
 *
 * @param[out] cmd  输出参数，收到有效命令时填充此结构体
 * @return 1 收到命令（cmd 已填充）, 0 无命令（无数据或命令未完成）, -1 错误（未初始化或 cmd 为 NULL）
 */
int codex_serial_poll(serial_cmd_t *cmd);

/**
 * @brief 通过串口发送响应文本
 *
 * 内部使用 vsnprintf 格式化，自动追加 "\\r\\n" 换行符。
 * 最大输出 256 字节（含换行符）。
 *
 * @param[in] fmt  printf 格式字符串
 * @param[in] ...  可变参数
 */
void codex_serial_respond(const char *fmt, ...);

/**
 * @brief 关闭串口命令解析器
 *
 * 清除初始化标记和接收缓冲区。不关闭底层 UART 硬件。
 */
void codex_serial_deinit(void);

#endif /* CODEX_SERIAL_H */
