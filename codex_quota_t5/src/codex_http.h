/**
 * @file codex_http.h
 * @brief HTTP 客户端 + JSON 解析接口
 */
#ifndef CODEX_HTTP_H
#define CODEX_HTTP_H

#include <stdint.h>
#include <stddef.h>

/* ── 额度数据结构（与桥接服务器 JSON 对应）────────── */
typedef struct {
    char label[16];         /* "5小时" / "周额度" */
    double used;            /* 已用百分比 */
    double remaining;       /* 剩余百分比 */
    char resets_in[32];     /* "4小时52分钟" */
} codex_window_t;

typedef struct {
    char plan_type[16];     /* "plus" / "pro" / "free" */
    codex_window_t primary; /* 主窗口（必有） */
    codex_window_t secondary; /* 副窗口（Plus/Pro 才有） */
    int has_secondary;      /* 是否有副窗口 */
    char updated_time[8];   /* "10:00" */
} codex_quota_t;

/* ── 接口函数 ─────────────────────────────────────── */

/**
 * 发起 HTTP GET 请求
 * @param host      服务器地址（如 "192.168.1.109"）
 * @param port      端口（如 5678）
 * @param path      路径（如 "/quota"）
 * @param out_buf   输出缓冲区
 * @param buf_size  缓冲区大小
 * @return 0 成功, -1 失败
 */
int codex_http_get(const char *host, uint16_t port, const char *path,
                   char *out_buf, size_t buf_size);

/**
 * 解析桥接服务器返回的 JSON
 * @param json_str  JSON 字符串
 * @param quota     输出结构体
 * @return 0 成功, -1 失败
 */
int codex_parse_json(const char *json_str, codex_quota_t *quota);

#endif /* CODEX_HTTP_H */
