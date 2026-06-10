/**
 * @file codex_serial.h
 * @brief 串口配置命令解析器
 *
 * 通过 COM11 (debug UART) 接收配置命令，支持：
 *   SET WIFI <ssid> <password>
 *   SET BRIDGE <host> <port>
 *   SET MQTT <host> <port>
 *   GET CONFIG
 *   SAVE
 *   REBOOT
 */

#ifndef CODEX_SERIAL_H
#define CODEX_SERIAL_H

#include <stdint.h>

/**
 * 串口配置命令回调
 * 收到有效命令后调用，由调用者实现具体逻辑
 */
typedef enum {
    CMD_SET_WIFI,       /* ssid, password */
    CMD_SET_BRIDGE,     /* host, port */
    CMD_SET_MQTT,       /* host, port */
    CMD_GET_CONFIG,     /* 查询当前配置 */
    CMD_SAVE,           /* 保存到 NVS */
    CMD_REBOOT,         /* 重启设备 */
    CMD_UNKNOWN,        /* 未知命令 */
} serial_cmd_type_t;

typedef struct {
    serial_cmd_type_t type;
    char arg1[64];      /* ssid / host / 空 */
    char arg2[128];     /* password / port_str / 空 */
} serial_cmd_t;

/**
 * 初始化串口命令解析器
 * @param uart_id UART ID (T5AI debug UART 通常为 1)
 * @return 0 成功, -1 失败
 */
int codex_serial_init(uint8_t uart_id);

/**
 * 串口命令处理（非阻塞）
 * 在主循环中周期调用，检查是否有新命令
 * @param cmd 输出参数，收到命令时填充
 * @return 1 收到命令, 0 无命令, -1 错误
 */
int codex_serial_poll(serial_cmd_t *cmd);

/**
 * 串口发送响应
 * @param fmt printf 格式字符串
 */
void codex_serial_respond(const char *fmt, ...);

/**
 * 关闭串口
 */
void codex_serial_deinit(void);

#endif /* CODEX_SERIAL_H */
