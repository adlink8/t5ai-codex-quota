/**
 * @file codex_mqtt.c
 * @brief MQTT 客户端实现 - 订阅 codex/quota 主题
 *
 * 使用 TuyaOpen libmqtt API (mqtt_client_interface.h)。
 * 连接成功后自动订阅，收到消息后解析 JSON 并更新 UI。
 *
 * 重要：on_message 回调可能在 MQTT SDK 内部线程运行，
 * 不直接调用 LVGL，而是通过消息队列延迟到主循环处理。
 */

#include "codex_mqtt.h"
#include "codex_http.h"     /* codex_parse_json, codex_quota_t */
#include "codex_ui.h"       /* codex_ui_update */

#include "mqtt_client_interface.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "tkl_system.h"     /* tkl_system_sleep */
#include "lv_vendor.h"      /* lv_vendor_disp_lock/unlock */

#include <string.h>

/* ── 外部运行时配置（来自 tuya_main.c）─────────────── */
extern char g_mqtt_host[48];
extern int  g_mqtt_port;

/* ── 配置 ─────────────────────────────────────────── */
#ifndef DEVICE_ID
#define DEVICE_ID           "t5ai-001"
#endif
#define MQTT_TOPIC          "codex/quota"
#define MQTT_QOS            0       /* QoS 0: Fire and Forget */
#define MQTT_KEEPALIVE      60      /* 秒 */
#define MQTT_TIMEOUT_MS     5000    /* 连接超时 */
#define MQTT_CLIENT_ID      "codex_t5ai"

/* ── 内部状态 ─────────────────────────────────────── */
static void *g_mqtt_client = NULL;
static int g_mqtt_connected = 0;

/* ── 消息队列（回调 → 主循环）────────────────────── */
#define MQTT_MSG_BUF_SIZE 2048
static char g_pending_json[MQTT_MSG_BUF_SIZE];
static volatile int g_new_message = 0;  /* 原子标志 */

/* ── 诊断计数 ────────────────────────────────────── */
static int g_disc_cb_count = 0;
static int g_conn_cb_count  = 0;
static int g_msg_rx_count   = 0;  /* 收到的 MQTT 消息总数 */
static uint32_t g_last_disc_ms = 0;  /* 上次断连回调时间戳 */

/* ── 回调函数 ─────────────────────────────────────── */

static void on_connected(void *client, void *userdata)
{
    (void)userdata;
    g_conn_cb_count++;
    g_mqtt_connected = 1;
    PR_NOTICE("[mqtt] on_connected #%d: 已连接 broker，订阅 %s",
              g_conn_cb_count, MQTT_TOPIC);

    uint16_t msgid = mqtt_client_subscribe(client, MQTT_TOPIC, MQTT_QOS);
    if (msgid > 0) {
        PR_NOTICE("[mqtt] 订阅成功 msgid=%u", msgid);
    } else {
        PR_ERR("[mqtt] 订阅失败 msgid=0");
    }
}

static void on_disconnected(void *client, void *userdata)
{
    (void)client;
    (void)userdata;

    g_disc_cb_count++;
    uint32_t now_ms = tkl_system_get_millisecond();

    /* 幂等检查 — 已断连状态则直接返回 */
    if (g_mqtt_connected == 0) {
        /* 每 50 次静默回调打一次诊断，避免 UART 洪泛 */
        if ((g_disc_cb_count % 50) == 0) {
            PR_NOTICE("[mqtt] disc_cb #%d silent (conn=0, dt=%u ms)",
                      g_disc_cb_count,
                      (unsigned)(now_ms - g_last_disc_ms));
        }
        return;
    }

    /* 硬性去抖: 2 秒内不重复处理 */
    if (g_last_disc_ms != 0 && (now_ms - g_last_disc_ms) < 2000) {
        PR_NOTICE("[mqtt] disc_cb #%d DEBOUNCE (dt=%u ms, conn was %d)",
                  g_disc_cb_count,
                  (unsigned)(now_ms - g_last_disc_ms), g_mqtt_connected);
        /* 仍然标记断连，但不重复执行逻辑 */
        g_mqtt_connected = 0;
        g_last_disc_ms = now_ms;
        return;
    }

    g_last_disc_ms = now_ms;
    g_mqtt_connected = 0;
    PR_NOTICE("[mqtt] on_disconnect #%d: 标记断连 (conn_cb=%d)",
              g_disc_cb_count, g_conn_cb_count);
    /* 注意: 不在这里销毁客户端。
     * 此回调可能从 SDK yield 内部触发，销毁会导致 use-after-free。
     * 由主循环的 codex_mqtt_reconnect() 负责完整销毁和重建。 */
}

/**
 * MQTT 消息回调 — 仅复制数据到共享缓冲区，不调用 LVGL
 *
 * 重要：此回调可能在 MQTT SDK 内部线程运行（非主线程），
 * 不能直接调用 LVGL 函数，否则会导致：
 *   1. 线程安全问题（LVGL 不是线程安全的）
 *   2. 栈溢出（MQTT 任务栈通常只有 2-4KB）
 */
static void on_message(void *client, uint16_t msgid,
                       const mqtt_client_message_t *msg, void *userdata)
{
    (void)client;
    (void)userdata;

    g_msg_rx_count++;

    PR_NOTICE("[mqtt] 收到消息 #%d: topic=%s len=%u msgid=%u",
              g_msg_rx_count, msg->topic, (unsigned)msg->length, msgid);

    /* 确保 payload 是有效的 JSON 字符串 */
    if (msg->payload == NULL || msg->length == 0) {
        PR_ERR("[mqtt] 空 payload");
        return;
    }

    /* 安全检查：payload 不能超过缓冲区大小 */
    if (msg->length >= MQTT_MSG_BUF_SIZE) {
        PR_ERR("[mqtt] payload 过大: %u >= %u, 丢弃消息",
               (unsigned)msg->length, (unsigned)MQTT_MSG_BUF_SIZE);
        return;
    }

    /* 仅复制数据到共享缓冲区（栈上操作很小） */
    memcpy(g_pending_json, msg->payload, msg->length);
    g_pending_json[msg->length] = '\0';
    g_new_message = 1;  /* 通知主循环有新消息 */

    PR_NOTICE("[mqtt] 消息已缓存，等待主循环处理 (len=%u)", (unsigned)msg->length);
}

static void on_subscribed(void *client, uint16_t msgid, void *userdata)
{
    (void)client;
    (void)userdata;
    PR_NOTICE("[mqtt] 订阅确认 msgid=%u", msgid);
}

/* ── 公开接口 ─────────────────────────────────────── */

int codex_mqtt_init_and_connect(const char *host, uint16_t port)
{
    if (host == NULL || host[0] == '\0' || port == 0) {
        PR_ERR("[mqtt] 无效参数: host=%s port=%u", host ? host : "NULL", port);
        return -1;
    }

    PR_NOTICE("[mqtt] 创建客户端 broker=%s:%u", host, port);

    g_mqtt_client = mqtt_client_new();
    if (g_mqtt_client == NULL) {
        PR_ERR("[mqtt] mqtt_client_new() 返回 NULL");
        return -1;
    }

    mqtt_client_config_t config = {
        .cacert = NULL,
        .cacert_len = 0,
        .host = host,
        .port = port,
        .keepalive = MQTT_KEEPALIVE,
        .timeout_ms = MQTT_TIMEOUT_MS,
        .clientid = MQTT_CLIENT_ID,
        .username = NULL,
        .password = NULL,
        .userdata = NULL,
        .on_connected = on_connected,
        .on_disconnected = on_disconnected,
        .on_message = on_message,
        .on_published = NULL,
        .on_subscribed = on_subscribed,
        .on_unsubscribed = NULL,
    };

    mqtt_client_status_t status = mqtt_client_init(g_mqtt_client, &config);
    if (status != MQTT_STATUS_SUCCESS) {
        PR_ERR("[mqtt] mqtt_client_init() 失败 status=%d", status);
        mqtt_client_free(g_mqtt_client);
        g_mqtt_client = NULL;
        return -1;
    }

    PR_NOTICE("[mqtt] 正在连接...");
    status = mqtt_client_connect(g_mqtt_client);
    if (status != MQTT_STATUS_SUCCESS) {
        PR_ERR("[mqtt] mqtt_client_connect() 失败 status=%d", status);
        mqtt_client_deinit(g_mqtt_client);
        mqtt_client_free(g_mqtt_client);
        g_mqtt_client = NULL;
        return -1;
    }

    /* 调用一次 yield 让连接回调生效 */
    mqtt_client_yield(g_mqtt_client);

    if (g_mqtt_connected) {
        PR_NOTICE("[mqtt] 连接 + 订阅完成");
        return 0;
    }

    /* 多试几次 yield */
    for (int i = 0; i < 10; i++) {
        mqtt_client_yield(g_mqtt_client);
        if (g_mqtt_connected) {
            PR_NOTICE("[mqtt] 连接 + 订阅完成 (yield %d)", i + 1);
            return 0;
        }
        tkl_system_sleep(200);
    }

    PR_ERR("[mqtt] 连接超时");
    mqtt_client_deinit(g_mqtt_client);
    mqtt_client_free(g_mqtt_client);
    g_mqtt_client = NULL;
    return -1;
}

void codex_mqtt_yield(void)
{
    /* 仅在已连接状态下调用 SDK yield，避免断连后反复 yield
     * 导致 MQTTRecvFailed/MQTTSendFailed 无限错误循环 */
    if (g_mqtt_client != NULL && g_mqtt_connected) {
        mqtt_client_yield(g_mqtt_client);
        /* yield 返回后检查是否被断连 */
        if (!g_mqtt_connected) {
            PR_NOTICE("[mqtt] yield 返回后检测到断连");
        }
    }
}

int codex_mqtt_reconnect(void)
{
    PR_NOTICE("[mqtt] reconnect: entry (disc_cb=%d conn_cb=%d was_conn=%d)",
              g_disc_cb_count, g_conn_cb_count, g_mqtt_connected);

    /* 完全销毁旧客户端 */
    if (g_mqtt_client != NULL) {
        mqtt_client_disconnect(g_mqtt_client);
        mqtt_client_deinit(g_mqtt_client);
        mqtt_client_free(g_mqtt_client);
        g_mqtt_client = NULL;
        g_mqtt_connected = 0;
    }

    /* 创建新客户端 */
    g_mqtt_client = mqtt_client_new();
    if (g_mqtt_client == NULL) {
        PR_ERR("[mqtt] mqtt_client_new() 返回 NULL");
        return -1;
    }

    mqtt_client_config_t config = {
        .cacert = NULL,
        .cacert_len = 0,
        .host = g_mqtt_host,
        .port = g_mqtt_port,
        .keepalive = MQTT_KEEPALIVE,
        .timeout_ms = MQTT_TIMEOUT_MS,
        .clientid = MQTT_CLIENT_ID,
        .username = NULL,
        .password = NULL,
        .userdata = NULL,
        .on_connected = on_connected,
        .on_disconnected = on_disconnected,
        .on_message = on_message,
        .on_published = NULL,
        .on_subscribed = on_subscribed,
        .on_unsubscribed = NULL,
    };

    mqtt_client_status_t status = mqtt_client_init(g_mqtt_client, &config);
    if (status != MQTT_STATUS_SUCCESS) {
        PR_ERR("[mqtt] init 失败 status=%d", status);
        mqtt_client_free(g_mqtt_client);
        g_mqtt_client = NULL;
        return -1;
    }

    status = mqtt_client_connect(g_mqtt_client);
    if (status != MQTT_STATUS_SUCCESS) {
        PR_ERR("[mqtt] reconnect connect() failed status=%d (disc_cb=%d)",
               status, g_disc_cb_count);
        mqtt_client_deinit(g_mqtt_client);
        mqtt_client_free(g_mqtt_client);
        g_mqtt_client = NULL;
        return -1;
    }

    PR_NOTICE("[mqtt] reconnect: TCP+MQTT connected, yielding... (disc_cb=%d)",
              g_disc_cb_count);

    /* yield 等待连接 + 订阅完成 */
    mqtt_client_yield(g_mqtt_client);
    if (g_mqtt_connected) {
        PR_NOTICE("[mqtt] 重连成功");
        return 0;
    }

    for (int i = 0; i < 10; i++) {
        mqtt_client_yield(g_mqtt_client);
        if (g_mqtt_connected) {
            PR_NOTICE("[mqtt] 重连成功 (yield %d)", i + 1);
            return 0;
        }
        tkl_system_sleep(200);
    }

    PR_ERR("[mqtt] reconnect: timeout (disc_cb=%d, conn=%d)",
            g_disc_cb_count, g_mqtt_connected);
    mqtt_client_deinit(g_mqtt_client);
    mqtt_client_free(g_mqtt_client);
    g_mqtt_client = NULL;
    return -1;
}

void codex_mqtt_disconnect(void)
{
    if (g_mqtt_client != NULL) {
        mqtt_client_disconnect(g_mqtt_client);
        mqtt_client_deinit(g_mqtt_client);
        mqtt_client_free(g_mqtt_client);
        g_mqtt_client = NULL;
        g_mqtt_connected = 0;
        PR_NOTICE("[mqtt] 已断开并释放");
    }
}

int codex_mqtt_is_connected(void)
{
    return g_mqtt_connected;
}

int codex_mqtt_has_pending_message(void)
{
    return g_new_message;
}

int codex_mqtt_get_msg_count(void)
{
    return g_msg_rx_count;
}

int codex_mqtt_process_pending_message(codex_quota_t *quota)
{
    if (!g_new_message) {
        return 0;  /* 无待处理消息 */
    }

    /* 清除标志（先清除再处理，避免竞争） */
    g_new_message = 0;

    PR_NOTICE("[mqtt] 主循环处理缓存消息 (len=%u)",
              (unsigned)strlen(g_pending_json));

    if (codex_parse_json(g_pending_json, quota) == 0) {
        PR_NOTICE("[mqtt] 解析成功: %s | 主剩余 %.1f%% | 副剩余 %.1f%%",
                  quota->plan_type,
                  quota->primary.remaining,
                  quota->has_secondary ? quota->secondary.remaining : 0.0);
        return 1;  /* 解析成功 */
    } else {
        PR_ERR("[mqtt] JSON 解析失败");
        return -1;  /* 解析失败 */
    }
}
