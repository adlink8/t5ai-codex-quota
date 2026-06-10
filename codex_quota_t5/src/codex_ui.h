/**
 * @file codex_ui.h
 * @brief Codex 额度显示 UI 接口（多页面触摸切换）
 *
 * 页面布局：
 *   Page 0: 额度主页面（左右双卡片 + 环形进度条）
 *   Page 1: 诊断页面（WiFi、MQTT、内存、运行时间）
 *
 * 触摸屏左右滑动切换页面。
 */
#ifndef CODEX_UI_H
#define CODEX_UI_H

#include "codex_http.h"
#include <stdint.h>

/* ── 页面索引 ─────────────────────────────────────── */
#define CODEX_PAGE_QUOTA    0
#define CODEX_PAGE_DIAG     1
#define CODEX_PAGE_COUNT    2

/* ── 诊断信息结构体 ────────────────────────────────── */
typedef struct {
    /* WiFi */
    char wifi_ssid[32];
    char wifi_ip[16];
    int8_t wifi_rssi;
    int wifi_connected;

    /* MQTT */
    char mqtt_host[48];
    uint16_t mqtt_port;
    int mqtt_connected;
    int mqtt_msg_count;         /* 已接收消息数 */

    /* Bridge */
    char bridge_host[48];
    uint16_t bridge_port;

    /* 系统 */
    uint32_t uptime_seconds;
    uint32_t heap_free_bytes;
    uint32_t heap_total_bytes;
    uint32_t psram_free_bytes;
    uint32_t psram_total_bytes;

    /* 连接统计 */
    int wifi_reconnect_count;
    int mqtt_reconnect_count;
} codex_diag_info_t;

/* ── 基础 UI 接口 ──────────────────────────────────── */

/** 创建完整 UI 界面（启动时调用一次） */
void codex_ui_create(void);

/** 用最新数据刷新 UI */
void codex_ui_update(const codex_quota_t *quota);

/** 设置启动/连接/拉取中的状态文本 */
void codex_ui_set_status(const char *message);

/** 设置具体错误状态 */
void codex_ui_set_error(const char *message);

/** 设置离线状态 */
void codex_ui_set_offline(void);

/* ── 诊断页面接口 ──────────────────────────────────── */

/** 更新诊断页面信息（周期调用） */
void codex_ui_update_diag(const codex_diag_info_t *info);

/** 切换到指定页面 */
void codex_ui_switch_page(int page_index);

/** 获取当前页面索引 */
int codex_ui_get_current_page(void);

#endif /* CODEX_UI_H */
