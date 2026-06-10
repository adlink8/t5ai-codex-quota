/**
 * @file codex_mqtt.c
 * @brief MQTT 客户端实现 - 订阅 codex/quota 主题
 *
 * 使用 TuyaOpen libmqtt API (mqtt_client_interface.h)。
 * 连接成功后自动订阅，收到消息后解析 JSON 并更新 UI。
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

/* ── 诊断计数 ────────────────────────────────────── */
static int g_disc_cb_count = 0;
static int g_conn_cb_count  = 0;
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
                  (unsigned)(now_ms - g_last_disc_ms),
                  g_mqtt_connected);
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

static void on_message(void *client, uint16_t msgid,
                       const mqtt_client_message_t *msg, void *userdata)
{
    (void)client;
    (void)userdata;

    PR_NOTICE("[mqtt] 收到消息: topic=%s len=%u msgid=%u",
              msg->topic, (unsigned)msg->length, msgid);

    /* 确保 payload 是有效的 JSON 字符串 */
    if (msg->payload == NULL || msg->length == 0) {
        PR_ERR("[mqtt] 空 payload");
        return;
    }

    /* 复制到本地缓冲区确保 null-terminated */
    char json_buf[2048];
    size_t copy_len = msg->length < sizeof(json_buf) - 1 ? msg->length : sizeof(json_buf) - 1;
    memcpy(json_buf, msg->payload, copy_len);
    json_buf[copy_len] = '\0';

    codex_quota_t quota;
    if (codex_parse_json(json_buf, &quota) == 0) {
        PR_NOTICE("[mqtt] 解析成功: %s | 主剩余 %.1f%% | 副剩余 %.1f%%",
                  quota.plan_type,
                  quota.primary.remaining,
                  quota.has_secondary ? quota.secondary.remaining : 0.0);

        lv_vendor_disp_lock();
        codex_ui_update(&quota);
        lv_vendor_disp_unlock();
    } else {
        PR_ERR("[mqtt] JSON 解析失败");
    }
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
