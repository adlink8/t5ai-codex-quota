/**
 * @file tuya_main.c
 * @brief Codex 额度监控 - T5AI-Board 主程序
 *
 * 通过 WiFi 连接局域网桥接服务器，获取 Codex 额度数据，
 * 在 LCD 屏幕上显示环形进度条。
 *
 * 架构：T5 → WiFi → PC 桥接服务器 → ChatGPT API
 */

#include "tal_wifi.h"
#include "tal_thread.h"
#include "tal_sw_timer.h"
#include "tal_system.h"
#include "tal_log.h"
#include "tal_memory.h"
#include "tkl_system.h"
#include "tkl_output.h"
#include "cJSON.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 板级 & LVGL 初始化 */
#include "board_com_api.h"
#include "lv_vendor.h"

#if ENABLE_TP
/* 前向声明触摸驱动函数，避免包含 drv_tp.h（其内部 beken_mutex_t 类型
 * 需要额外 Beken SDK 头文件，会引入不必要的依赖） */
extern int drv_tp_open(int hor_size, int ver_size, int tp_mirror);
#define TP_MIRROR_NONE  0
#endif

#include "codex_http.h"
#include "codex_ui.h"
#include "codex_mqtt.h"
#include "codex_serial.h"

/* ── 配置 ────────────────────────────────────────── */
/* WiFi、桥接服务器、MQTT 参数由 Kconfig 提供。 */
#ifndef WIFI_SSID
#define WIFI_SSID       ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD   ""
#endif

#ifndef BRIDGE_HOST
#define BRIDGE_HOST     ""
#endif

#ifndef BRIDGE_PORT
#define BRIDGE_PORT     5678
#endif

#ifndef BRIDGE_PATH
#define BRIDGE_PATH     "/quota"
#endif

#ifndef MQTT_HOST
#define MQTT_HOST       ""
#endif

#ifndef MQTT_PORT
#define MQTT_PORT       1883
#endif

/* 刷新间隔（毫秒） */
#define REFRESH_OK_MS   60000    /* 正常 60 秒 */
#define REFRESH_MAX_MS  300000   /* 退避上限 5 分钟 */

/* ── 全局状态 ─────────────────────────────────────── */
static codex_quota_t g_quota;            /* 额度数据 */
static THREAD_HANDLE g_refresh_thread;   /* 刷新线程句柄 */
static uint32_t g_refresh_ms = REFRESH_OK_MS;
static int g_wifi_ok = 0;
static int g_mqtt_ok = 0;               /* MQTT 连接状态 */

/* ── 运行时可配置副本（串口命令可修改，extern 访问）── */
char g_wifi_ssid[32] = {0};
char g_wifi_password[64] = {0};
char g_bridge_host[48] = {0};
int  g_bridge_port = BRIDGE_PORT;
char g_bridge_path[32] = {0};
char g_mqtt_host[48] = {0};
int  g_mqtt_port = MQTT_PORT;

static const char *wifi_stat_name(WF_STATION_STAT_E stat)
{
    switch (stat) {
    case WSS_IDLE: return "IDLE";
    case WSS_CONNECTING: return "CONNECTING";
    case WSS_PASSWD_WRONG: return "PASSWD_WRONG";
    case WSS_NO_AP_FOUND: return "NO_AP_FOUND";
    case WSS_CONN_FAIL: return "CONN_FAIL";
    case WSS_CONN_SUCCESS: return "CONN_SUCCESS";
    case WSS_GOT_IP: return "GOT_IP";
    case WSS_DHCP_FAIL: return "DHCP_FAIL";
    default: return "UNKNOWN";
    }
}

static const char *wifi_event_name(WF_EVENT_E event)
{
    switch (event) {
    case WFE_CONNECTED: return "CONNECTED";
    case WFE_CONNECT_FAILED: return "CONNECT_FAILED";
    case WFE_DISCONNECTED: return "DISCONNECTED";
    default: return "UNKNOWN";
    }
}

static const char *wifi_disc_reason_name(WF_DISCONN_REASON_E reason)
{
    switch (reason) {
    case TUYA_WLAN_REASON_NONE: return "NONE";
    case TUYA_WLAN_REASON_SSID_NOT_FOUND: return "SSID_NOT_FOUND";
    case TUYA_WLAN_REASON_WRONG_PASSWORD: return "WRONG_PASSWORD";
    case TUYA_WLAN_REASON_4WAYS_HANDSHAKE_TIMEOUT: return "HANDSHAKE_TIMEOUT";
    case TUYA_WLAN_REASON_DEAUTH_LEAVING: return "DEAUTH_LEAVING";
    case TUYA_WLAN_REASON_AP_UNABLE_TO_HANDLE_NEW_STA: return "AP_FULL";
    default: return "OTHER";
    }
}

static WF_DISCONN_REASON_E wifi_last_disc_reason(void)
{
    WF_DISCONN_REASON_E reason = TUYA_WLAN_REASON_NONE;
    tal_wifi_ioctl(WFI_GET_LAST_DISCONN_REASON, &reason);
    return reason;
}

static int ip_is_valid(const NW_IP_S *ip)
{
    return ip != NULL && ip->ip[0] != '\0' && strcmp(ip->ip, "0.0.0.0") != 0;
}

static void ui_statusf(const char *fmt, ...)
{
    char message[96];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);

    lv_vendor_disp_lock();
    codex_ui_set_status(message);
    lv_vendor_disp_unlock();
}

static void ui_errorf(const char *fmt, ...)
{
    char message[96];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);

    lv_vendor_disp_lock();
    codex_ui_set_error(message);
    lv_vendor_disp_unlock();
}

static void ssid_to_ascii(const uint8_t *ssid, uint8_t len,
                          char *out, size_t out_size)
{
    size_t pos = 0;
    if (out_size == 0) return;

    for (uint8_t i = 0; i < len && pos + 1 < out_size; i++) {
        unsigned char ch = ssid[i];
        if (ch >= 32 && ch < 127) {
            out[pos++] = (char)ch;
        } else {
            out[pos++] = '?';
        }
    }
    out[pos] = '\0';
}

static void log_visible_aps(void)
{
    AP_IF_S *aps = NULL;
    uint32_t num = 0;
    OPERATE_RET ret = tal_wifi_all_ap_scan(&aps, &num);
    int best = -1;
    char best_ssid[WIFI_SSID_LEN + 1] = {0};

    PR_NOTICE("[wifi] all_ap_scan ret=%d num=%u", ret, (unsigned)num);
    if (ret != OPRT_OK || aps == NULL || num == 0) {
        ui_errorf("NO_AP_FOUND scan=%d n=%u", ret, (unsigned)num);
        return;
    }

    for (uint32_t i = 0; i < num && i < 12; i++) {
        char ssid[WIFI_SSID_LEN + 1] = {0};
        ssid_to_ascii(aps[i].ssid, aps[i].s_len, ssid, sizeof(ssid));
        PR_NOTICE("[wifi] ap[%u] ssid=\"%s\" len=%u ch=%u rssi=%d sec=%u",
                  (unsigned)i, ssid, aps[i].s_len, aps[i].channel,
                  aps[i].rssi, aps[i].security);
        if (best < 0 || aps[i].rssi > aps[best].rssi) {
            best = (int)i;
        }
    }

    if (best >= 0) {
        ssid_to_ascii(aps[best].ssid, aps[best].s_len,
                      best_ssid, sizeof(best_ssid));
        ui_errorf("Target missing; seen %u: %s",
                  (unsigned)num, best_ssid[0] ? best_ssid : "hidden");
    }

    tal_wifi_release_ap(aps);
}

static void log_wifi_snapshot(const char *stage)
{
    WF_STATION_STAT_E stat = WSS_IDLE;
    WF_DISCONN_REASON_E reason = TUYA_WLAN_REASON_NONE;
    NW_IP_S ip = {0};
    int8_t rssi = 0;
    OPERATE_RET stat_ret = tal_wifi_station_get_status(&stat);
    OPERATE_RET ip_ret = tal_wifi_get_ip(WF_STATION, &ip);
    OPERATE_RET rssi_ret = tal_wifi_station_get_conn_ap_rssi(&rssi);
    OPERATE_RET reason_ret = tal_wifi_ioctl(WFI_GET_LAST_DISCONN_REASON, &reason);

    PR_NOTICE("[wifi][%s] status ret=%d stat=%s(%d)",
              stage, stat_ret, wifi_stat_name(stat), stat);
    PR_NOTICE("[wifi][%s] ip_ret=%d ip=%s mask=%s gw=%s dns=%s rssi_ret=%d rssi=%d",
              stage, ip_ret, ip.ip, ip.mask, ip.gw, ip.dns, rssi_ret, rssi);
    PR_NOTICE("[wifi][%s] disconn_reason ret=%d reason=%s(%d)",
              stage, reason_ret, wifi_disc_reason_name(reason), reason);
}

static void wifi_event_cb(WF_EVENT_E event, void *arg)
{
    WF_DISCONN_REASON_E reason = TUYA_WLAN_REASON_NONE;
    (void)arg;

    tal_wifi_ioctl(WFI_GET_LAST_DISCONN_REASON, &reason);
    PR_NOTICE("[wifi][event] %s(%d), reason=%s(%d)",
              wifi_event_name(event), event, wifi_disc_reason_name(reason), reason);
}

/* ── WiFi 连接 ────────────────────────────────────── */
static int wifi_connect(void)
{
    if (g_wifi_ssid[0] == '\0') {
        PR_ERR("[codex] WiFi SSID 未配置，请通过 Kconfig/app_default.config 设置 WIFI_SSID");
        return -1;
    }

    PR_NOTICE("[wifi] config ssid=\"%s\" password_len=%u bridge=http://%s:%d%s",
              g_wifi_ssid, (unsigned)strlen(g_wifi_password),
              g_bridge_host, g_bridge_port, g_bridge_path);

    OPERATE_RET ret = tal_wifi_init(wifi_event_cb);
    PR_NOTICE("[wifi] tal_wifi_init ret=%d", ret);
    if (ret != OPRT_OK) {
        ui_errorf("WiFi init failed %d", ret);
        return -1;
    }

    ret = tal_wifi_set_work_mode(WWM_STATION);
    PR_NOTICE("[wifi] set station mode ret=%d", ret);

    AP_IF_S *ap = NULL;
    ret = tal_wifi_assign_ap_scan((int8_t *)g_wifi_ssid, &ap);
    if (ret == OPRT_OK && ap != NULL) {
        PR_NOTICE("[wifi] scan hit ssid=\"%s\" channel=%u rssi=%d security=%u",
                  ap->ssid, ap->channel, ap->rssi, ap->security);
        tal_wifi_release_ap(ap);
    } else {
        PR_ERR("[wifi] scan failed/no ap: ret=%d ssid=\"%s\"", ret, g_wifi_ssid);
        ui_errorf("AP scan failed %d", ret);
        log_visible_aps();
    }

    log_wifi_snapshot("before_connect");

    PR_NOTICE("[wifi] station_connect start ssid=\"%s\"", g_wifi_ssid);
    ret = tal_wifi_station_connect((int8_t *)g_wifi_ssid, (int8_t *)g_wifi_password);

    if (ret != OPRT_OK) {
        PR_ERR("[wifi] station_connect returned error: %d", ret);
        log_wifi_snapshot("connect_return_error");
        ui_errorf("WiFi connect ret %d", ret);
        return -1;
    }

    for (int i = 0; i < 30; i++) {
        WF_STATION_STAT_E stat = WSS_IDLE;
        NW_IP_S ip = {0};

        tal_wifi_station_get_status(&stat);
        tal_wifi_get_ip(WF_STATION, &ip);

        PR_NOTICE("[wifi] wait %02ds stat=%s(%d) ip=%s gw=%s",
                  i + 1, wifi_stat_name(stat), stat, ip.ip, ip.gw);
        ui_statusf("WiFi %s %02ds", wifi_stat_name(stat), i + 1);

        if ((stat == WSS_GOT_IP || stat == WSS_CONN_SUCCESS) && ip_is_valid(&ip)) {
            int8_t rssi = 0;
            tal_wifi_station_get_conn_ap_rssi(&rssi);
        PR_NOTICE("[wifi] connected ip=%s mask=%s gw=%s dns=%s rssi=%d",
                  ip.ip, ip.mask, ip.gw, ip.dns, rssi);
        ui_statusf("WiFi OK %s", ip.ip);
        g_wifi_ok = 1;
        return 0;
        }

        if (stat == WSS_PASSWD_WRONG || stat == WSS_NO_AP_FOUND ||
            stat == WSS_CONN_FAIL || stat == WSS_DHCP_FAIL) {
            WF_DISCONN_REASON_E reason = wifi_last_disc_reason();
            PR_ERR("[wifi] terminal status=%s(%d) reason=%s(%d)",
                   wifi_stat_name(stat), stat, wifi_disc_reason_name(reason), reason);
            ui_errorf("WiFi fail %s", wifi_stat_name(stat));
            break;
        }

        tkl_system_sleep(1000);
    }

    WF_STATION_STAT_E final_stat = WSS_IDLE;
    WF_DISCONN_REASON_E reason = wifi_last_disc_reason();
    tal_wifi_station_get_status(&final_stat);
    log_wifi_snapshot("connect_failed_or_timeout");
    ui_errorf("WiFi fail %s/%s",
              wifi_stat_name(final_stat),
              wifi_disc_reason_name(reason));
    g_wifi_ok = 0;

    return -1;
}

/* ── 数据拉取 + 解析 ─────────────────────────────── */
static int fetch_quota(void)
{
    char json_buf[2048] = {0};

    if (g_bridge_host[0] == '\0' || g_bridge_path[0] == '\0' || g_bridge_port <= 0) {
        PR_ERR("[codex] 桥接服务器未配置: host=%s port=%d path=%s",
               g_bridge_host, g_bridge_port, g_bridge_path);
        return -1;
    }

    /* 诊断：打印当前网络状态 */
    {
        NW_IP_S ip = {0};
        tal_wifi_get_ip(WF_STATION, &ip);
        PR_NOTICE("[codex] 网络状态: ip=%s mask=%s gw=%s dns=%s",
                  ip.ip, ip.mask, ip.gw, ip.dns);
    }

    PR_NOTICE("[codex] 请求桥接服务器: http://%s:%d%s",
              g_bridge_host, g_bridge_port, g_bridge_path);

    int ret = codex_http_get(g_bridge_host, g_bridge_port, g_bridge_path,
                             json_buf, sizeof(json_buf));
    if (ret != 0) {
        PR_ERR("[codex] HTTP 请求失败");
        ui_errorf("HTTP fail ret %d", ret);
        return -1;
    }

    ret = codex_parse_json(json_buf, &g_quota);
    if (ret != 0) {
        PR_ERR("[codex] JSON 解析失败");
        ui_errorf("JSON parse fail %d", ret);
        return -1;
    }

    PR_NOTICE("[codex] %s | 主窗口剩余 %.1f%% | 副窗口剩余 %.1f%%",
              g_quota.plan_type,
              g_quota.primary.remaining,
              g_quota.has_secondary ? g_quota.secondary.remaining : 0.0);

    return 0;
}

/* ── 刷新线程（FreeRTOS 任务）────────────────────── */
static void refresh_task(void *args)
{
    while (1) {
        /* MQTT 已连接时不需要 HTTP 轮询 */
        if (g_mqtt_ok && codex_mqtt_is_connected()) {
            tkl_system_sleep(1000);
            continue;
        }

        if (!g_wifi_ok) {
            ui_statusf("WiFi reconnecting...");
            if (wifi_connect() != 0) {
                tkl_system_sleep(30000);
                continue;
            }
        }

        lv_vendor_disp_lock();
        codex_ui_set_status("Fetching quota...");
        lv_vendor_disp_unlock();

        if (fetch_quota() == 0) {
            /* 成功 → 恢复正常间隔 */
            g_refresh_ms = REFRESH_OK_MS;
            lv_vendor_disp_lock();
            codex_ui_update(&g_quota);
            lv_vendor_disp_unlock();
        } else {
            /* 失败 → 指数退避 */
            g_refresh_ms = g_refresh_ms * 2;
            if (g_refresh_ms > REFRESH_MAX_MS)
                g_refresh_ms = REFRESH_MAX_MS;
            lv_vendor_disp_lock();
            codex_ui_set_offline();
            lv_vendor_disp_unlock();
        }

        tkl_system_sleep(g_refresh_ms);
    }
}

/* ── 主入口（T5AI 平台入口函数名）───────────────── */
void tuya_app_main(void)
{
    /* 0. 子系统初始化（必须在一切之前） */
#if defined(ENABLE_EXT_RAM) && (ENABLE_EXT_RAM == 1)
    cJSON_InitHooks(&(cJSON_Hooks){.malloc_fn = tal_psram_malloc, .free_fn = tal_psram_free});
#else
    cJSON_InitHooks(&(cJSON_Hooks){.malloc_fn = tal_malloc, .free_fn = tal_free});
#endif

    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    /* 初始化运行时配置副本 */
    strncpy(g_wifi_ssid, WIFI_SSID, sizeof(g_wifi_ssid) - 1);
    strncpy(g_wifi_password, WIFI_PASSWORD, sizeof(g_wifi_password) - 1);
    strncpy(g_bridge_host, BRIDGE_HOST, sizeof(g_bridge_host) - 1);
    g_bridge_port = BRIDGE_PORT;
    strncpy(g_bridge_path, BRIDGE_PATH, sizeof(g_bridge_path) - 1);
    strncpy(g_mqtt_host, MQTT_HOST, sizeof(g_mqtt_host) - 1);
    g_mqtt_port = MQTT_PORT;

    PR_NOTICE("[codex] Codex Quota Monitor for T5AI-Board");
    PR_NOTICE("[codex] bridge=http://%s:%d%s mqtt=%s:%d",
              g_bridge_host, g_bridge_port, g_bridge_path, g_mqtt_host, g_mqtt_port);

    tal_sw_timer_init();

    /* 1. 初始化板级硬件 */
    board_register_hardware();

    /* 2. 初始化 LVGL 显示 */
    lv_vendor_init(DISPLAY_NAME);

    /* 2.5 初始化触摸屏驱动 */
#if ENABLE_TP
    {
        lv_disp_t *disp = lv_disp_get_default();
        lv_coord_t tp_w = disp ? lv_disp_get_hor_res(disp) : 480;
        lv_coord_t tp_h = disp ? lv_disp_get_ver_res(disp) : 320;
        /* 如果显示是竖屏但会被 UI 旋转为横屏，交换宽高 */
        if (tp_w < tp_h) {
            lv_coord_t tmp = tp_w;
            tp_w = tp_h;
            tp_h = tmp;
        }
        int tp_ret = drv_tp_open(tp_w, tp_h, TP_MIRROR_NONE);
        PR_NOTICE("[codex] drv_tp_open(%d, %d) ret=%d", (int)tp_w, (int)tp_h, tp_ret);
    }
#endif

    /* 3. 启动 LVGL 渲染循环 (priority=5, stack=8KB)
     *    必须先启动 LVGL 任务，让显示驱动就绪后，才能创建 UI 对象 */
    lv_vendor_start(5, 1024 * 8);

    /* 等待 LVGL 任务和显示驱动完全就绪 */
    tkl_system_sleep(500);

    /* 4. 创建 UI（LVGL 显示已就绪，可以安全操作屏幕对象） */
    lv_vendor_disp_lock();
    codex_ui_create();
    lv_vendor_disp_unlock();

    /* 5. 连接 WiFi */
    lv_vendor_disp_lock();
    codex_ui_set_status("WiFi connecting...");
    lv_vendor_disp_unlock();

    g_wifi_ok = (wifi_connect() == 0);
    if (!g_wifi_ok) {
        /* WiFi 失败也继续运行，用户可以重启 */
    } else {
        lv_vendor_disp_lock();
        codex_ui_set_status("WiFi connected - fetching quota...");
        lv_vendor_disp_unlock();
    }

    /* 6. 尝试 MQTT 连接（优先使用推送模式） */
    if (g_wifi_ok && g_mqtt_host[0] != '\0') {
        /* 等待网络栈完全就绪 */
        PR_NOTICE("[codex] 等待网络栈就绪...");
        tkl_system_sleep(2000);

        lv_vendor_disp_lock();
        codex_ui_set_status("Connecting MQTT...");
        lv_vendor_disp_unlock();

        if (codex_mqtt_init_and_connect(g_mqtt_host, g_mqtt_port) == 0) {
            g_mqtt_ok = 1;
            PR_NOTICE("[codex] MQTT 模式已启用，HTTP 轮询已停用");
            lv_vendor_disp_lock();
            codex_ui_set_status("MQTT connected");
            lv_vendor_disp_unlock();
        } else {
            PR_NOTICE("[codex] MQTT 连接失败，回退到 HTTP 轮询");
            g_mqtt_ok = 0;
        }
    }

    /* 7. 首次拉取数据（HTTP，如果 MQTT 未连接） */
    if (!g_mqtt_ok) {
        lv_vendor_disp_lock();
        codex_ui_set_status("Fetching quota...");
        lv_vendor_disp_unlock();
    }

    if (!g_mqtt_ok && g_wifi_ok && fetch_quota() == 0) {
        lv_vendor_disp_lock();
        codex_ui_update(&g_quota);
        lv_vendor_disp_unlock();
    } else if (g_wifi_ok) {
        lv_vendor_disp_lock();
        codex_ui_set_offline();
        lv_vendor_disp_unlock();
    }

    /* 8. 启动周期刷新线程（MQTT 模式下仅做兜底） */
    THREAD_CFG_T thread_cfg = {
        .stackDepth = 8192,
        .priority = THREAD_PRIO_3,
        .thrdname = "codex_ref",
    };
    tal_thread_create_and_start(
        &g_refresh_thread, NULL, NULL,
        refresh_task, NULL, &thread_cfg
    );

    /* 9. 初始化串口命令解析器 */
    codex_serial_init(1);  /* UART1 = COM11 debug port */
    codex_serial_respond("=== Codex Quota Monitor ===");
    codex_serial_respond("Type HELP for available commands");

    /* 10. 主循环保持运行 — MQTT yield + 断线指数退避重连 + 串口命令 */
    uint32_t mqtt_reconnect_at_ms  = 0;
    uint32_t mqtt_backoff_ms       = 1000;   /* 初始退避 1 s */
    #define MQTT_BACKOFF_MAX_MS    60000     /* 退避上限 60 s */

    uint32_t diag_update_at_ms = 0;
    #define DIAG_UPDATE_INTERVAL_MS  2000   /* 诊断页面刷新间隔 2 秒 */

    while (1) {
        /* ── 串口命令处理 ─────────────────────────── */
        serial_cmd_t cmd;
        if (codex_serial_poll(&cmd) == 1) {
            switch (cmd.type) {
            case CMD_SET_WIFI:
                /* SET WIFI <ssid> <password> */
                if (cmd.arg1[0] == '\0') {
                    codex_serial_respond("ERR: SET WIFI <ssid> <password>");
                } else {
                    strncpy(g_wifi_ssid, cmd.arg1, sizeof(g_wifi_ssid) - 1);
                    g_wifi_ssid[sizeof(g_wifi_ssid) - 1] = '\0';
                    if (cmd.arg2[0] != '\0') {
                        strncpy(g_wifi_password, cmd.arg2, sizeof(g_wifi_password) - 1);
                        g_wifi_password[sizeof(g_wifi_password) - 1] = '\0';
                    }
                    codex_serial_respond("OK: WiFi SSID=%s (restart to apply)", g_wifi_ssid);
                    PR_NOTICE("[serial] WiFi config updated: ssid=%s", g_wifi_ssid);
                }
                break;

            case CMD_SET_BRIDGE:
                /* SET BRIDGE <host> <port> */
                if (cmd.arg1[0] == '\0') {
                    codex_serial_respond("ERR: SET BRIDGE <host> <port>");
                } else {
                    strncpy(g_bridge_host, cmd.arg1, sizeof(g_bridge_host) - 1);
                    g_bridge_host[sizeof(g_bridge_host) - 1] = '\0';
                    if (cmd.arg2[0] != '\0') {
                        int port = atoi(cmd.arg2);
                        if (port > 0 && port < 65536) {
                            g_bridge_port = port;
                        }
                    }
                    codex_serial_respond("OK: Bridge=%s:%d (restart to apply)",
                                         g_bridge_host, g_bridge_port);
                    PR_NOTICE("[serial] Bridge config updated: %s:%d",
                              g_bridge_host, g_bridge_port);
                }
                break;

            case CMD_SET_MQTT:
                /* SET MQTT <host> <port> */
                if (cmd.arg1[0] == '\0') {
                    codex_serial_respond("ERR: SET MQTT <host> <port>");
                } else {
                    strncpy(g_mqtt_host, cmd.arg1, sizeof(g_mqtt_host) - 1);
                    g_mqtt_host[sizeof(g_mqtt_host) - 1] = '\0';
                    if (cmd.arg2[0] != '\0') {
                        int port = atoi(cmd.arg2);
                        if (port > 0 && port < 65536) {
                            g_mqtt_port = port;
                        }
                    }
                    codex_serial_respond("OK: MQTT=%s:%d (restart to apply)",
                                         g_mqtt_host, g_mqtt_port);
                    PR_NOTICE("[serial] MQTT config updated: %s:%d",
                              g_mqtt_host, g_mqtt_port);
                }
                break;

            case CMD_GET_CONFIG:
                codex_serial_respond("=== Current Config ===");
                codex_serial_respond("WiFi SSID: %s", g_wifi_ssid);
                codex_serial_respond("Bridge: %s:%d%s",
                                     g_bridge_host, g_bridge_port, g_bridge_path);
                codex_serial_respond("MQTT: %s:%d", g_mqtt_host, g_mqtt_port);
                codex_serial_respond("Device ID: t5ai-001");
                codex_serial_respond("MQTT %s, WiFi %s",
                                     codex_mqtt_is_connected() ? "connected" : "disconnected",
                                     g_wifi_ok ? "connected" : "disconnected");
                break;

            case CMD_SAVE:
                codex_serial_respond("TODO: NVS save not yet implemented");
                PR_NOTICE("[serial] SAVE requested but NVS not implemented");
                break;

            case CMD_REBOOT:
                codex_serial_respond("Rebooting in 1 second...");
                tkl_system_sleep(1000);
                tkl_system_reset();
                break;

            case CMD_UNKNOWN:
            default:
                codex_serial_respond("=== Available Commands ===");
                codex_serial_respond("  SET WIFI <ssid> <password>");
                codex_serial_respond("  SET BRIDGE <host> <port>");
                codex_serial_respond("  SET MQTT <host> <port>");
                codex_serial_respond("  GET CONFIG");
                codex_serial_respond("  SAVE");
                codex_serial_respond("  REBOOT");
                codex_serial_respond("  HELP");
                break;
            }
        }

        /* ── 诊断页面更新 ─────────────────────────── */
        {
            uint32_t now = tkl_system_get_millisecond();
            if (now >= diag_update_at_ms) {
                diag_update_at_ms = now + DIAG_UPDATE_INTERVAL_MS;

                codex_diag_info_t diag;
                memset(&diag, 0, sizeof(diag));

                /* WiFi */
                strncpy(diag.wifi_ssid, g_wifi_ssid, sizeof(diag.wifi_ssid) - 1);
                diag.wifi_connected = g_wifi_ok;
                if (g_wifi_ok) {
                    NW_IP_S ip = {0};
                    tal_wifi_get_ip(WF_STATION, &ip);
                    strncpy(diag.wifi_ip, ip.ip, sizeof(diag.wifi_ip) - 1);
                    int8_t rssi = 0;
                    tal_wifi_station_get_conn_ap_rssi(&rssi);
                    diag.wifi_rssi = rssi;
                }

                /* MQTT */
                strncpy(diag.mqtt_host, g_mqtt_host, sizeof(diag.mqtt_host) - 1);
                diag.mqtt_port = g_mqtt_port;
                diag.mqtt_connected = codex_mqtt_is_connected();
                diag.mqtt_msg_count = codex_mqtt_get_msg_count();

                /* Bridge */
                strncpy(diag.bridge_host, g_bridge_host, sizeof(diag.bridge_host) - 1);
                diag.bridge_port = g_bridge_port;

                /* System */
                diag.uptime_seconds = tkl_system_get_millisecond() / 1000;
                /* TODO: get real heap/PSRAM stats from tal_memory API */

                lv_vendor_disp_lock();
                codex_ui_update_diag(&diag);
                lv_vendor_disp_unlock();
            }
        }
        if (g_mqtt_ok) {
            codex_mqtt_yield();

            /* 处理待处理的 MQTT 消息（在主循环中安全调用 LVGL） */
            {
                codex_quota_t mqtt_quota;
                int result = codex_mqtt_process_pending_message(&mqtt_quota);
                if (result == 1) {
                    /* 解析成功，更新 UI */
                    lv_vendor_disp_lock();
                    codex_ui_update(&mqtt_quota);
                    lv_vendor_disp_unlock();
                    g_quota = mqtt_quota;
                } else if (result == -1) {
                    PR_ERR("[codex] MQTT 消息解析失败");
                }
            }

            /* yield 可能检测到断连 → on_disconnected 已置 g_mqtt_connected=0 */
            if (!codex_mqtt_is_connected()) {
                uint32_t now = tkl_system_get_millisecond();
                if (now >= mqtt_reconnect_at_ms) {
                    PR_NOTICE("[codex] MQTT 断连，尝试重连 (backoff=%u ms)",
                              (unsigned)mqtt_backoff_ms);
                    ui_statusf("MQTT reconnecting...");

                    if (codex_mqtt_reconnect() == 0) {
                        PR_NOTICE("[codex] MQTT 重连成功，恢复推送模式");
                        ui_statusf("MQTT reconnected");
                        mqtt_backoff_ms = 1000;
                        mqtt_reconnect_at_ms = 0;
                    } else {
                        PR_NOTICE("[codex] MQTT 重连失败，%u ms 后重试",
                                  (unsigned)mqtt_backoff_ms);
                        ui_statusf("MQTT fail, retry %us",
                                   (unsigned)(mqtt_backoff_ms / 1000));
                        uint32_t jitter = tkl_system_get_random(1000); /* 0–1000 ms */
                        PR_NOTICE("[codex] backoff=%u ms + jitter=%u ms",
                                  (unsigned)mqtt_backoff_ms, (unsigned)jitter);
                        mqtt_reconnect_at_ms = now + mqtt_backoff_ms + jitter;
                        mqtt_backoff_ms *= 2;
                        if (mqtt_backoff_ms > MQTT_BACKOFF_MAX_MS)
                            mqtt_backoff_ms = MQTT_BACKOFF_MAX_MS;
                    }
                }
            } else {
                /* 连接正常 → 重置退避 */
                mqtt_backoff_ms = 1000;
            }
        }
        tkl_system_sleep(100);
    }
}
