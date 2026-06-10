/**
 * @file codex_http.h
 * @brief HTTP 客户端 + JSON 解析接口
 *
 * 对接局域网桥接服务器的 /quota 接口，获取 Codex 额度数据。
 * 功能：
 *   1. codex_http_get()  —— 发起 HTTP GET 请求，获取 JSON 响应体
 *   2. codex_parse_json() —— 解析 JSON 响应，填充 codex_quota_t 结构体
 *
 * 额度数据结构（与桥接服务器 JSON 对应）：
 *   {
 *     "plan_type": "plus",
 *     "primary":   { "label", "used_percent", "remaining_percent", "resets_in" },
 *     "secondary": { ... },    // Plus/Pro 才有
 *     "updated_at": "ISO8601"
 *   }
 */

#ifndef CODEX_HTTP_H
#define CODEX_HTTP_H

#include <stdint.h>
#include <stddef.h>

/* ── 额度数据结构（与桥接服务器 JSON 对应）────────── */

/**
 * @brief 单个额度窗口（5小时窗口 / 周窗口）
 *
 * 对应 JSON 中的 primary 或 secondary 对象。
 * 用于 LCD 屏幕上的环形进度条显示。
 */
typedef struct {
    char label[16];         /**< 窗口标签，如 "5小时" / "周额度" */
    double used;            /**< 已用百分比（0.0 ~ 100.0） */
    double remaining;       /**< 剩余百分比（0.0 ~ 100.0） */
    char resets_in[32];     /**< 重置倒计时，如 "4小时52分钟" / "6天21小时" */
} codex_window_t;

/**
 * @brief Codex 额度总览
 *
 * 包含计划类型、主/副窗口数据和更新时间。
 * primary 窗口始终存在；secondary 仅 Plus/Pro 计划才有。
 */
typedef struct {
    char plan_type[16];         /**< 计划类型："plus" / "pro" / "free" */
    codex_window_t primary;     /**< 主窗口（必有，5小时滑动窗口） */
    codex_window_t secondary;   /**< 副窗口（Plus/Pro 才有，周/月额度） */
    int has_secondary;          /**< 是否有副窗口：1=有, 0=无 */
    char updated_time[8];       /**< 数据更新时间，格式 "HH:MM"（从 ISO8601 截取） */
} codex_quota_t;

/* ── 接口函数 ─────────────────────────────────────── */

/**
 * @brief 发起 HTTP GET 请求并获取响应体
 *
 * 使用 TuyaOpen 的 http_client_request() API。
 * 纯 HTTP（非 HTTPS），超时 10 秒。
 *
 * @param[in]  host      服务器地址（如 "192.168.1.109"）
 * @param[in]  port      端口号（如 5678）
 * @param[in]  path      请求路径（如 "/quota"）
 * @param[out] out_buf   输出缓冲区，用于存放响应体
 * @param[in]  buf_size  输出缓冲区大小（字节）
 * @return  0 成功
 *         -1 请求失败（网络错误或 HTTP 状态码非 200）
 *         -2 响应体过大，无法放入输出缓冲区
 */
int codex_http_get(const char *host, uint16_t port, const char *path,
                   char *out_buf, size_t buf_size);

/**
 * @brief 解析桥接服务器返回的 JSON 额度数据
 *
 * 使用 cJSON 库解析。必填字段 "primary" 和 "primary.remaining_percent"，
 * 缺失则返回失败。"secondary" 为可选字段。
 * "updated_at" 为 ISO8601 格式，仅提取 "HH:MM" 部分。
 *
 * @param[in]  json_str  以 '\\0' 结尾的 JSON 字符串
 * @param[out] quota     输出结构体，成功时被完整填充
 * @return  0 成功, -1 失败（JSON 格式错误、必填字段缺失、或服务器返回 error）
 */
int codex_parse_json(const char *json_str, codex_quota_t *quota);

#endif /* CODEX_HTTP_H */
