/**
 * @file codex_http.c
 * @brief HTTP 客户端 + JSON 解析（对接桥接服务器 /quota 接口）
 */

#include "codex_http.h"
#include "http_client_interface.h"
#include "cJSON.h"
#include "tal_log.h"
#include <string.h>

/* ── HTTP GET 请求 ────────────────────────────────── */
int codex_http_get(const char *host, uint16_t port, const char *path,
                   char *out_buf, size_t buf_size)
{
    http_client_request_t request = {
        .host = host,
        .port = port,
        .path = path,
        .method = "GET",
        .cacert = NULL,
        .cacert_len = 0,
        .tls_no_verify = false,  /* 纯 HTTP，必须 false 才走 TCP 通道 */
        .headers = NULL,
        .headers_count = 0,
        .body = NULL,
        .body_length = 0,
        .timeout_ms = 10000,
    };

    http_client_response_t response = {0};

    PR_NOTICE("[http] GET http://%s:%u%s timeout=%ums",
              host, port, path, (unsigned)request.timeout_ms);

    http_client_status_t status = http_client_request(&request, &response);

    if (status != HTTP_CLIENT_SUCCESS) {
        PR_ERR("[http] request failed: status=%d host=%s port=%u path=%s",
               status, host, port, path);
        http_client_free(&response);
        return -1;
    }

    PR_NOTICE("[http] response code=%u body_len=%u header_len=%u",
              response.status_code,
              (unsigned)response.body_length,
              (unsigned)response.headers_length);

    if (response.status_code != 200) {
        PR_ERR("[http] unexpected HTTP status=%u", response.status_code);
        http_client_free(&response);
        return -1;
    }

    if (response.body == NULL || response.body_length == 0) {
        PR_ERR("[http] empty response body");
        http_client_free(&response);
        return -1;
    }

    /* 检查响应体是否能放入输出缓冲区（不允许静默截断） */
    if (response.body_length >= buf_size) {
        PR_ERR("[http] response too large: %u >= %u",
               (unsigned)response.body_length, (unsigned)buf_size);
        http_client_free(&response);
        return -2;
    }
    memcpy(out_buf, response.body, response.body_length);
    out_buf[response.body_length] = '\0';

    http_client_free(&response);
    return 0;
}

/* ── JSON 解析（桥接服务器返回格式）───────────────── */
/*
 * 期望的 JSON 格式：
 * {
 *   "plan_type": "plus",
 *   "primary": {
 *     "label": "5小时",
 *     "used_percent": 6.0,
 *     "remaining_percent": 94.0,
 *     "resets_in": "4小时52分钟"
 *   },
 *   "secondary": {
 *     "label": "周额度",
 *     "used_percent": 1.0,
 *     "remaining_percent": 99.0,
 *     "resets_in": "6天21小时"
 *   },
 *   "credits": { ... },
 *   "updated_at": "2026-06-07T10:00:00+08:00"
 * }
 */
int codex_parse_json(const char *json_str, codex_quota_t *quota)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        const char *err = cJSON_GetErrorPtr();
        PR_ERR("[json] 解析错误: %s", err ? err : "unknown");
        return -1;
    }

    /* 检查是否有错误字段 */
    cJSON *error = cJSON_GetObjectItem(root, "error");
    if (error && cJSON_IsString(error)) {
        PR_ERR("[json] 服务器错误: %s", error->valuestring);
        cJSON_Delete(root);
        return -1;
    }

    memset(quota, 0, sizeof(codex_quota_t));

    /* plan_type */
    cJSON *plan = cJSON_GetObjectItem(root, "plan_type");
    if (plan && cJSON_IsString(plan)) {
        strncpy(quota->plan_type, plan->valuestring, sizeof(quota->plan_type) - 1);
    }

    /* ── primary 窗口（必须存在且为对象） ─────────── */
    cJSON *primary = cJSON_GetObjectItem(root, "primary");
    if (primary == NULL) {
        PR_ERR("[json] missing required field: \"primary\"");
        cJSON_Delete(root);
        return -1;
    }
    if (!cJSON_IsObject(primary)) {
        PR_ERR("[json] field \"primary\" must be an object");
        cJSON_Delete(root);
        return -1;
    }

    /* primary.remaining_percent（必须为数字） */
    cJSON *remain = cJSON_GetObjectItem(primary, "remaining_percent");
    if (remain == NULL) {
        PR_ERR("[json] missing required field: \"primary.remaining_percent\"");
        cJSON_Delete(root);
        return -1;
    }
    if (!cJSON_IsNumber(remain)) {
        PR_ERR("[json] field \"primary.remaining_percent\" must be a number");
        cJSON_Delete(root);
        return -1;
    }

    /* primary — 其余可选字段 */
    cJSON *label = cJSON_GetObjectItem(primary, "label");
    if (label && cJSON_IsString(label))
        strncpy(quota->primary.label, label->valuestring,
                sizeof(quota->primary.label) - 1);

    cJSON *used = cJSON_GetObjectItem(primary, "used_percent");
    if (used && cJSON_IsNumber(used))
        quota->primary.used = used->valuedouble;

    quota->primary.remaining = remain->valuedouble;

    cJSON *resets = cJSON_GetObjectItem(primary, "resets_in");
    if (resets && cJSON_IsString(resets))
        strncpy(quota->primary.resets_in, resets->valuestring,
                sizeof(quota->primary.resets_in) - 1);

    /* secondary window */
    cJSON *secondary = cJSON_GetObjectItem(root, "secondary");
    if (secondary && cJSON_IsObject(secondary)) {
        quota->has_secondary = 1;

        cJSON *label = cJSON_GetObjectItem(secondary, "label");
        if (label && cJSON_IsString(label))
            strncpy(quota->secondary.label, label->valuestring,
                    sizeof(quota->secondary.label) - 1);

        cJSON *used = cJSON_GetObjectItem(secondary, "used_percent");
        if (used && cJSON_IsNumber(used))
            quota->secondary.used = used->valuedouble;

        cJSON *remain = cJSON_GetObjectItem(secondary, "remaining_percent");
        if (remain && cJSON_IsNumber(remain))
            quota->secondary.remaining = remain->valuedouble;

        cJSON *resets = cJSON_GetObjectItem(secondary, "resets_in");
        if (resets && cJSON_IsString(resets))
            strncpy(quota->secondary.resets_in, resets->valuestring,
                    sizeof(quota->secondary.resets_in) - 1);
    }

    /* updated_at — 提取时间部分 */
    cJSON *updated = cJSON_GetObjectItem(root, "updated_at");
    if (updated && cJSON_IsString(updated)) {
        /* ISO 格式太长，只取 HH:MM */
        const char *t = updated->valuestring;
        const char *p = strchr(t, 'T');
        if (p) {
            p++; /* 跳过 T */
            strncpy(quota->updated_time, p, 5); /* "10:00" */
        }
    }

    cJSON_Delete(root);
    return 0;
}
