/**
 * @file codex_serial.c
 * @brief 串口配置命令解析器实现
 *
 * 命令解析逻辑概述：
 *   ┌─────────────────────────────────────────────────────────┐
 *   │  tkl_uart_read() ──→ g_rx_buf[] 逐字节积累             │
 *   │       │                                                 │
 *   │       ▼                                                 │
 *   │  遇到 '\\n' ──→ parse_command() ──→ serial_cmd_t 输出  │
 *   │       │                                                 │
 *   │       ▼                                                 │
 *   │  重置缓冲区，返回 1                                     │
 *   └─────────────────────────────────────────────────────────┘
 *
 * parse_command() 解析流程：
 *   1. 提取第一个单词并转大写 → 匹配 SET / GET / SAVE / REBOOT
 *   2. SET 模式：再提取子命令（WIFI/BRIDGE/MQTT），然后解析 arg1/arg2
 *   3. GET 模式：提取子命令（CONFIG）
 *   4. 无法匹配 → CMD_UNKNOWN
 *
 * 所有字符串比较忽略大小写（内部转大写后比较）。
 */

#include "codex_serial.h"
#include "tkl_uart.h"
#include "tal_log.h"
#include "tkl_system.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* ── 内部状态 ─────────────────────────────────────── */

/** @brief 当前使用的 UART ID，默认 1（COM11 debug port） */
static uint8_t g_uart_id = 1;

/** @brief 解析器初始化标记，0=未初始化，1=已初始化 */
static int g_initialized = 0;

/** @brief 接收缓冲区大小（字节） */
#define RX_BUF_SIZE 256

/** @brief 接收缓冲区，逐字节积累串口数据直到遇到换行符 */
static char g_rx_buf[RX_BUF_SIZE];

/** @brief 当前缓冲区写入位置（下一个空闲字节的索引） */
static uint16_t g_rx_pos = 0;

/* ── 内部函数 ─────────────────────────────────────── */

/**
 * @brief 解析单行命令字符串，填充命令结构体
 *
 * 解析逻辑（大小写不敏感）：
 *   - 第 1 个单词匹配主命令：SET / GET / SAVE / REBOOT / HELP
 *   - SET 需要第 2 个单词匹配子命令：WIFI / BRIDGE / MQTT
 *   - SET 的子命令后，剩余文本按空格拆分为 arg1 和 arg2
 *   - arg2 取 arg1 之后的所有剩余字符（包含空格）
 *
 * @param[in]  line  以 '\\0' 结尾的命令行（不含 '\\n'）
 * @param[out] cmd   输出结构体，填充 type / arg1 / arg2
 * @return 命令类型（与 cmd->type 一致）
 */
static serial_cmd_type_t parse_command(const char *line, serial_cmd_t *cmd)
{
    char upper[16] = {0};
    int i = 0;

    /*
     * 第一步：提取第一个单词并转大写
     * 遍历 line 直到遇到空格或字符串结尾，同时将小写字母转为大写
     * 结果存入 upper[] 用于后续 strcmp 匹配
     */
    while (line[i] && line[i] != ' ' && i < (int)sizeof(upper) - 1) {
        upper[i] = (line[i] >= 'a' && line[i] <= 'z') ? line[i] - 32 : line[i];
        i++;
    }
    upper[i] = '\0';

    /* 跳过主命令与子命令/参数之间的空格 */
    const char *args = line + i;
    while (*args == ' ') args++;

    /* ── 主命令：SET ─────────────────────────────── */
    if (strcmp(upper, "SET") == 0) {
        /*
         * SET 格式：SET <subcmd> <arg1> [arg2]
         * 提取子命令（第二个单词），同样转大写后比较
         */
        char subcmd[16] = {0};
        i = 0;
        while (args[i] && args[i] != ' ' && i < (int)sizeof(subcmd) - 1) {
            subcmd[i] = (args[i] >= 'a' && args[i] <= 'z') ? args[i] - 32 : args[i];
            i++;
        }
        subcmd[i] = '\0';
        args += i;
        while (*args == ' ') args++;

        /* 子命令：WIFI → 解析 ssid(arg1) 和 password(arg2) */
        if (strcmp(subcmd, "WIFI") == 0) {
            cmd->type = CMD_SET_WIFI;
            /* 提取 ssid：到下一个空格为止 */
            i = 0;
            while (args[i] && args[i] != ' ' && i < (int)sizeof(cmd->arg1) - 1) {
                cmd->arg1[i] = args[i];
                i++;
            }
            cmd->arg1[i] = '\0';
            args += i;
            while (*args == ' ') args++;
            /* 剩余部分作为 password（可能包含空格） */
            strncpy(cmd->arg2, args, sizeof(cmd->arg2) - 1);
            cmd->arg2[sizeof(cmd->arg2) - 1] = '\0';
            return CMD_SET_WIFI;
        }
        /* 子命令：BRIDGE → 解析 host(arg1) 和 port_str(arg2) */
        else if (strcmp(subcmd, "BRIDGE") == 0) {
            cmd->type = CMD_SET_BRIDGE;
            i = 0;
            while (args[i] && args[i] != ' ' && i < (int)sizeof(cmd->arg1) - 1) {
                cmd->arg1[i] = args[i];
                i++;
            }
            cmd->arg1[i] = '\0';
            args += i;
            while (*args == ' ') args++;
            strncpy(cmd->arg2, args, sizeof(cmd->arg2) - 1);
            cmd->arg2[sizeof(cmd->arg2) - 1] = '\0';
            return CMD_SET_BRIDGE;
        }
        /* 子命令：MQTT → 解析 host(arg1) 和 port_str(arg2) */
        else if (strcmp(subcmd, "MQTT") == 0) {
            cmd->type = CMD_SET_MQTT;
            i = 0;
            while (args[i] && args[i] != ' ' && i < (int)sizeof(cmd->arg1) - 1) {
                cmd->arg1[i] = args[i];
                i++;
            }
            cmd->arg1[i] = '\0';
            args += i;
            while (*args == ' ') args++;
            strncpy(cmd->arg2, args, sizeof(cmd->arg2) - 1);
            cmd->arg2[sizeof(cmd->arg2) - 1] = '\0';
            return CMD_SET_MQTT;
        }
        /* 未知子命令 */
        else {
            cmd->type = CMD_UNKNOWN;
            return CMD_UNKNOWN;
        }
    }
    /* ── 主命令：GET ─────────────────────────────── */
    else if (strcmp(upper, "GET") == 0) {
        /*
         * GET 格式：GET <subcmd>
         * 目前仅支持 GET CONFIG
         */
        char subcmd[16] = {0};
        i = 0;
        while (args[i] && args[i] != ' ' && args[i] != '\n' && args[i] != '\r' &&
               i < (int)sizeof(subcmd) - 1) {
            subcmd[i] = (args[i] >= 'a' && args[i] <= 'z') ? args[i] - 32 : args[i];
            i++;
        }
        subcmd[i] = '\0';

        if (strcmp(subcmd, "CONFIG") == 0) {
            cmd->type = CMD_GET_CONFIG;
            return CMD_GET_CONFIG;
        }
        cmd->type = CMD_UNKNOWN;
        return CMD_UNKNOWN;
    }
    /* ── 主命令：SAVE（无参数）────────────────── */
    else if (strcmp(upper, "SAVE") == 0) {
        cmd->type = CMD_SAVE;
        return CMD_SAVE;
    }
    /* ── 主命令：REBOOT（无参数）──────────────── */
    else if (strcmp(upper, "REBOOT") == 0) {
        cmd->type = CMD_REBOOT;
        return CMD_REBOOT;
    }
    /* ── HELP / ?（返回 UNKNOWN，由调用者打印帮助）── */
    else if (strcmp(upper, "HELP") == 0 || strcmp(upper, "?") == 0) {
        cmd->type = CMD_UNKNOWN;
        return CMD_UNKNOWN;
    }

    /* ── 无法识别的命令 ─────────────────────────── */
    cmd->type = CMD_UNKNOWN;
    return CMD_UNKNOWN;
}

/* ── 公开接口 ─────────────────────────────────────── */

/**
 * @brief 初始化串口命令解析器
 *
 * @param[in] uart_id  UART ID（T5AI debug UART 通常为 1）
 * @return 0 成功, -1 失败
 */
int codex_serial_init(uint8_t uart_id)
{
    g_uart_id = uart_id;
    g_rx_pos = 0;
    memset(g_rx_buf, 0, sizeof(g_rx_buf));

    /* TuyaOpen UART 初始化（如果需要） */
    /* tkl_uart_init 通常在 board_init 中已完成，这里只做标记 */
    g_initialized = 1;
    PR_NOTICE("[serial] 串口命令解析器已初始化 (UART%d)", g_uart_id);
    return 0;
}

/**
 * @brief 串口命令非阻塞轮询
 *
 * 核心工作流程：
 *   1. 调用 tkl_uart_read() 尝试读取最多 64 字节到 tmp[] 临时缓冲区
 *   2. 逐字节处理：
 *      - '\\r' → 忽略（兼容 Windows "\\r\\n" 换行）
 *      - '\\n' → 命令行结束，调用 parse_command() 解析，重置缓冲区
 *      - 其他字符 → 追加到 g_rx_buf[]（256 字节满时丢弃整行）
 *   3. 如果读取到数据但命令未完成（未遇到 '\\n'），返回 0
 *
 * @param[out] cmd  输出参数，收到完整命令时填充此结构体
 * @return 1 收到完整命令, 0 无数据或命令未完成, -1 错误
 */
int codex_serial_poll(serial_cmd_t *cmd)
{
    if (!g_initialized || cmd == NULL) {
        return -1;
    }

    /* 尝试从 UART 读取数据（非阻塞，最多读 64 字节） */
    uint8_t tmp[64];
    int32_t n = tkl_uart_read(g_uart_id, tmp, sizeof(tmp));
    if (n <= 0) {
        return 0;  /* 无数据 */
    }

    /* 逐字节处理，查找命令结尾 */
    for (int32_t i = 0; i < n; i++) {
        char ch = (char)tmp[i];

        /* 忽略 \\r，兼容 Windows 风格的 \\r\\n 换行 */
        if (ch == '\r') continue;

        /* \\n 标志一行命令结束 */
        if (ch == '\n') {
            if (g_rx_pos == 0) continue;  /* 空行，忽略 */

            g_rx_buf[g_rx_pos] = '\0';
            PR_NOTICE("[serial] 收到命令: \"%s\"", g_rx_buf);

            /* 解析命令并填充输出结构体 */
            memset(cmd, 0, sizeof(serial_cmd_t));
            parse_command(g_rx_buf, cmd);

            /* 重置缓冲区，准备接收下一条命令 */
            g_rx_pos = 0;
            memset(g_rx_buf, 0, sizeof(g_rx_buf));

            return 1;  /* 收到完整命令 */
        }

        /* 普通字符，追加到接收缓冲区 */
        if (g_rx_pos < RX_BUF_SIZE - 1) {
            g_rx_buf[g_rx_pos++] = ch;
        } else {
            /* 缓冲区溢出：命令过长，丢弃当前积累的数据 */
            PR_ERR("[serial] 命令过长，已丢弃");
            g_rx_pos = 0;
            memset(g_rx_buf, 0, sizeof(g_rx_buf));
        }
    }

    return 0;  /* 数据已读取但命令行尚未完成 */
}

/**
 * @brief 通过串口发送响应文本
 *
 * 使用 vsnprintf 格式化文本，自动追加 "\\r\\n" 换行。
 * 最大输出 256 字节（含结尾换行符和 '\\0'）。
 *
 * @param[in] fmt  printf 格式字符串
 * @param[in] ...  可变参数
 */
void codex_serial_respond(const char *fmt, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (len > 0) {
        /* 追加 \\r\\n 结尾，确保终端正确换行 */
        if (len < (int)sizeof(buf) - 2) {
            buf[len++] = '\r';
            buf[len++] = '\n';
            buf[len] = '\0';
        }
        tkl_uart_write(g_uart_id, (void *)buf, len);
    }
}

/**
 * @brief 关闭串口命令解析器
 *
 * 清除初始化标记和接收缓冲区，不关闭底层 UART 硬件。
 */
void codex_serial_deinit(void)
{
    g_initialized = 0;
    g_rx_pos = 0;
    PR_NOTICE("[serial] 串口命令解析器已关闭");
}
