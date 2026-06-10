/**
 * @file codex_serial.c
 * @brief 串口配置命令解析器实现
 *
 * 通过 debug UART 接收配置命令，非阻塞解析。
 * 命令以 \n 或 \r\n 结尾。
 */

#include "codex_serial.h"
#include "tkl_uart.h"
#include "tal_log.h"
#include "tkl_system.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* ── 内部状态 ─────────────────────────────────────── */
static uint8_t g_uart_id = 1;          /* 默认 COM11 = UART1 */
static int g_initialized = 0;

/* 接收缓冲区 */
#define RX_BUF_SIZE 256
static char g_rx_buf[RX_BUF_SIZE];
static uint16_t g_rx_pos = 0;

/* ── 内部函数 ─────────────────────────────────────── */

/**
 * 解析单行命令
 */
static serial_cmd_type_t parse_command(const char *line, serial_cmd_t *cmd)
{
    char upper[16] = {0};
    int i = 0;

    /* 提取第一个单词并转大写 */
    while (line[i] && line[i] != ' ' && i < (int)sizeof(upper) - 1) {
        upper[i] = (line[i] >= 'a' && line[i] <= 'z') ? line[i] - 32 : line[i];
        i++;
    }
    upper[i] = '\0';

    /* 跳过空格 */
    const char *args = line + i;
    while (*args == ' ') args++;

    if (strcmp(upper, "SET") == 0) {
        /* SET <subcmd> <arg1> [arg2] */
        char subcmd[16] = {0};
        i = 0;
        while (args[i] && args[i] != ' ' && i < (int)sizeof(subcmd) - 1) {
            subcmd[i] = (args[i] >= 'a' && args[i] <= 'z') ? args[i] - 32 : args[i];
            i++;
        }
        subcmd[i] = '\0';
        args += i;
        while (*args == ' ') args++;

        if (strcmp(subcmd, "WIFI") == 0) {
            cmd->type = CMD_SET_WIFI;
            /* 解析 ssid（到下一个空格）和 password（剩余部分） */
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
            return CMD_SET_WIFI;
        }
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
        else {
            cmd->type = CMD_UNKNOWN;
            return CMD_UNKNOWN;
        }
    }
    else if (strcmp(upper, "GET") == 0) {
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
    else if (strcmp(upper, "SAVE") == 0) {
        cmd->type = CMD_SAVE;
        return CMD_SAVE;
    }
    else if (strcmp(upper, "REBOOT") == 0) {
        cmd->type = CMD_REBOOT;
        return CMD_REBOOT;
    }
    else if (strcmp(upper, "HELP") == 0 || strcmp(upper, "?") == 0) {
        /* HELP 也返回 UNKNOWN，让调用者打印帮助 */
        cmd->type = CMD_UNKNOWN;
        return CMD_UNKNOWN;
    }

    cmd->type = CMD_UNKNOWN;
    return CMD_UNKNOWN;
}

/* ── 公开接口 ─────────────────────────────────────── */

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

int codex_serial_poll(serial_cmd_t *cmd)
{
    if (!g_initialized || cmd == NULL) {
        return -1;
    }

    /* 读取可用数据 */
    uint8_t tmp[64];
    int32_t n = tkl_uart_read(g_uart_id, tmp, sizeof(tmp));
    if (n <= 0) {
        return 0;  /* 无数据 */
    }

    /* 逐字节处理，查找命令结尾 */
    for (int32_t i = 0; i < n; i++) {
        char ch = (char)tmp[i];

        /* 忽略 \r，\n 作为命令结尾 */
        if (ch == '\r') continue;

        if (ch == '\n') {
            if (g_rx_pos == 0) continue;  /* 空行 */

            g_rx_buf[g_rx_pos] = '\0';
            PR_NOTICE("[serial] 收到命令: \"%s\"", g_rx_buf);

            /* 解析命令 */
            memset(cmd, 0, sizeof(serial_cmd_t));
            parse_command(g_rx_buf, cmd);

            /* 重置缓冲区 */
            g_rx_pos = 0;
            memset(g_rx_buf, 0, sizeof(g_rx_buf));

            return 1;  /* 收到命令 */
        }

        /* 普通字符，追加到缓冲区 */
        if (g_rx_pos < RX_BUF_SIZE - 1) {
            g_rx_buf[g_rx_pos++] = ch;
        } else {
            /* 缓冲区溢出，丢弃当前命令 */
            PR_ERR("[serial] 命令过长，已丢弃");
            g_rx_pos = 0;
            memset(g_rx_buf, 0, sizeof(g_rx_buf));
        }
    }

    return 0;  /* 数据已读取但命令未完成 */
}

void codex_serial_respond(const char *fmt, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (len > 0) {
        /* 添加 \r\n 结尾 */
        if (len < (int)sizeof(buf) - 2) {
            buf[len++] = '\r';
            buf[len++] = '\n';
            buf[len] = '\0';
        }
        tkl_uart_write(g_uart_id, (const uint8_t *)buf, len);
    }
}

void codex_serial_deinit(void)
{
    g_initialized = 0;
    g_rx_pos = 0;
    PR_NOTICE("[serial] 串口命令解析器已关闭");
}
