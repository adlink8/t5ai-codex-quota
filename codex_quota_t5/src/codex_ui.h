/**
 * @file codex_ui.h
 * @brief Codex 额度显示 UI 接口（多页面触摸切换）
 *
 * ============================================================================
 * 架构概述
 * ============================================================================
 *
 * 本模块基于 LVGL 的 lv_tileview 组件实现多页面 UI：
 *
 *   ┌──────────────────────────────────────────────────────────┐
 *   │  lv_tileview (全屏容器，管理多个 tile 子页面)             │
 *   │                                                          │
 *   │  ┌──────────────┐    ← 水平滑动 →    ┌──────────────┐    │
 *   │  │  Tile (0,0)   │                   │  Tile (1,0)   │    │
 *   │  │  额度主页面    │  ──── 左滑 ────→  │  诊断页面      │    │
 *   │  │  Page 0       │  ←─── 右滑 ─────  │  Page 1       │    │
 *   │  └──────────────┘                    └──────────────┘    │
 *   │                                                          │
 *   │  ● ○   ← 底部页面指示点（蓝色=当前页，灰色=非当前页）       │
 *   └──────────────────────────────────────────────────────────┘
 *
 * ============================================================================
 * 页面内容
 * ============================================================================
 *
 * Page 0 — 额度主页面 (CODEX_PAGE_QUOTA):
 *   - 顶部标题 "Codex Quota" + 计划类型标签（如 PRO/PLUS）
 *   - 左右两张额度卡片，各含环形进度条、百分比、已用额度、重置时间
 *   - 底部状态栏：绿色/黄色/红色指示点 + 实时状态文本
 *
 * Page 1 — 诊断页面 (CODEX_PAGE_DIAG):
 *   - 四个面板（2×2 网格布局）：
 *     [WiFi]     [MQTT]
 *     [System]   [Bridge & Stats]
 *   - 每面板包含多个诊断条目（SSID、IP、RSSI、Broker 等）
 *
 * ============================================================================
 * 页面切换机制
 * ============================================================================
 *
 *   用户触摸滑动 → lv_tileview 自动处理滑动手势
 *                → 触发 LV_EVENT_VALUE_CHANGED 事件
 *                → tileview_event_cb() 回调执行：
 *                   1. 通过 lv_tileview_get_tile_act() 获取当前活动 tile
 *                   2. 与 g_tile_quota / g_tile_diag 比较确定页码
 *                   3. 更新 g_current_page 和底部指示点颜色
 *
 *   程序切换 → codex_ui_switch_page(page_index) 调用：
 *              1. 验证页码有效性
 *              2. 通过 lv_obj_set_tile() 切换到目标 tile（带动画）
 *              3. 更新页码和指示点
 *
 * ============================================================================
 * 诊断信息更新
 * ============================================================================
 *
 *   外部模块（如主循环定时器）每 ~2 秒收集诊断信息后调用：
 *     codex_ui_update_diag(&diag_info);
 *
 *   内部流程：
 *     1. 更新 WiFi 面板：SSID、IP、RSSI、连接状态（绿色/红色）
 *     2. 更新 MQTT 面板：Broker 地址、消息计数、连接状态
 *     3. 更新 System 面板：Heap/PSRAM 内存使用、运行时间
 *     4. 更新 Bridge 面板：服务器地址、WiFi/MQTT 重连计数
 *
 * ============================================================================
 *
 * @note 本头文件仅声明公共接口，内部静态函数和 LVGL 对象定义在 codex_ui.c 中
 * @note 中文字体通过 lv_font_cn_16 外部声明引入
 */
#ifndef CODEX_UI_H
#define CODEX_UI_H

#include "codex_http.h"
#include <stdint.h>

/* ── 页面索引 ─────────────────────────────────────── */
/** @brief 额度主页面索引（Page 0），包含双卡片 + 环形进度条 */
#define CODEX_PAGE_QUOTA    0

/** @brief 诊断页面索引（Page 1），包含 WiFi/MQTT/内存/运行时间 */
#define CODEX_PAGE_DIAG     1

/** @brief 总页面数，用于页面指示点数组大小和循环边界检查 */
#define CODEX_PAGE_COUNT    2

/* ── 诊断信息结构体 ────────────────────────────────── */
/**
 * @brief 诊断信息数据结构，由外部模块填充后传递给 codex_ui_update_diag()
 *
 * 该结构体汇集了系统运行时的各类诊断数据：
 * - WiFi 连接状态和信号强度
 * - MQTT 连接状态和消息统计
 * - Bridge 服务器信息
 * - 系统内存使用情况（Heap + PSRAM）
 * - 运行时间和连接重连计数
 *
 * @note 建议每 ~2 秒填充一次并调用 codex_ui_update_diag()
 */
typedef struct {
    /* WiFi 诊断信息 */
    char wifi_ssid[32];         /**< @brief 当前连接的 WiFi 热点名称 */
    char wifi_ip[16];           /**< @brief 分配的 IPv4 地址字符串，如 "192.168.1.100" */
    int8_t wifi_rssi;           /**< @brief WiFi 信号强度（dBm），值越大信号越好，如 -30 优于 -70 */
    int wifi_connected;         /**< @brief WiFi 连接状态：非 0 表示已连接，0 表示断开 */

    /* MQTT 诊断信息 */
    char mqtt_host[48];         /**< @brief MQTT Broker 主机名或 IP 地址 */
    uint16_t mqtt_port;         /**< @brief MQTT Broker 端口号，通常为 1883（TCP）或 8883（TLS） */
    int mqtt_connected;         /**< @brief MQTT 连接状态：非 0 表示已连接，0 表示断开 */
    int mqtt_msg_count;         /**< @brief 已接收的 MQTT 消息总数（自启动起累计） */

    /* Bridge 服务器信息 */
    char bridge_host[48];       /**< @brief Bridge 代理服务器主机名或 IP 地址 */
    uint16_t bridge_port;       /**< @brief Bridge 代理服务器端口号 */

    /* 系统资源信息 */
    uint32_t uptime_seconds;    /**< @brief 系统运行时间（秒），用于诊断页面显示运行时长 */
    uint32_t heap_free_bytes;   /**< @brief 当前空闲堆内存（字节），用于计算 Heap 使用率 */
    uint32_t heap_total_bytes;  /**< @brief 堆内存总大小（字节） */
    uint32_t psram_free_bytes;  /**< @brief 当前空闲 PSRAM（字节），部分 ESP32 模组无 PSRAM 则为 0 */
    uint32_t psram_total_bytes; /**< @brief PSRAM 总大小（字节），无 PSRAM 时为 0，UI 显示 "N/A" */

    /* 连接重连统计 */
    int wifi_reconnect_count;   /**< @brief WiFi 自启动以来的重连次数，用于判断连接稳定性 */
    int mqtt_reconnect_count;   /**< @brief MQTT 自启动以来的重连次数，用于判断 Broker 连接质量 */
} codex_diag_info_t;

/* ── 基础 UI 接口 ──────────────────────────────────── */

/**
 * @brief 创建完整 UI 界面（启动时调用一次）
 *
 * 初始化工作：
 * 1. 检测屏幕方向，竖屏自动旋转 90° 为横屏
 * 2. 创建 lv_tileview 多页面容器
 * 3. 构建 Page 0（额度页面）：标题、双卡片、状态栏
 * 4. 构建 Page 1（诊断页面）：四个诊断面板
 * 5. 创建底部页面指示点
 *
 * @note 此函数应在 LVGL 初始化完成后且 lv_tick_inc() 已启动时调用
 * @note 仅需调用一次，重复调用会先清理旧 UI
 */
void codex_ui_create(void);

/**
 * @brief 用最新额度数据刷新 UI
 *
 * 更新内容：
 * - 将计划类型（plan_type）转为大写显示在标签上
 * - 更新主额度卡片（primary）的进度条、百分比、已用额度、重置时间
 * - 如果存在次额度（has_secondary），显示并更新次卡片
 * - 如果不存在次额度，显示 "Not available" 占位文本
 * - 更新底部状态栏的实时更新时间和绿色指示点
 *
 * @param[in] quota 指向 codex_quota_t 结构体的指针，包含最新额度数据
 *                  （由 codex_http 模块从 API 拉取后填充）
 *
 * @note quota 指针不能为 NULL，调用前请确保数据有效
 * @note 环形进度条采用 ease_out 动画过渡，动画时长 450ms
 */
void codex_ui_update(const codex_quota_t *quota);

/**
 * @brief 设置启动/连接/拉取中的状态文本
 *
 * 在数据尚未就绪时（如启动中、WiFi 连接中、API 拉取中），
 * 在底部状态栏显示临时状态提示文本，指示点变为黄色。
 *
 * @param[in] message 状态提示文本，如 "Starting..."、"Connecting..."、"Fetching..."
 *                    若为 NULL，则显示默认文本 "Working..."
 */
void codex_ui_set_status(const char *message);

/**
 * @brief 设置错误状态文本
 *
 * 当发生错误（如 API 请求失败、JSON 解析错误等）时调用，
 * 状态文本变为红色，指示点也变为红色。
 *
 * @param[in] message 错误描述文本，如 "HTTP Error 500"、"JSON Parse Failed"
 *                    若为 NULL，则显示默认文本 "Error"
 */
void codex_ui_set_error(const char *message);

/**
 * @brief 设置离线状态
 *
 * 当 Bridge 连接断开或无数据源时调用，
 * 显示 "Offline - waiting for bridge data"，文本和指示点均变为红色。
 *
 * @note 无参数，显示固定的离线提示文本
 */
void codex_ui_set_offline(void);

/* ── 诊断页面接口 ──────────────────────────────────── */

/**
 * @brief 更新诊断页面信息（周期调用）
 *
 * 将外部收集的诊断数据写入 Page 1 的各个诊断面板：
 * - WiFi 面板：SSID、IP 地址、RSSI 信号强度、连接状态
 * - MQTT 面板：Broker 地址（host:port）、消息计数、连接状态
 * - System 面板：Heap 内存使用率、PSRAM 使用率（无 PSRAM 显示 N/A）、运行时间
 * - Bridge & Stats 面板：服务器地址（host:port）、WiFi/MQTT 重连计数
 *
 * @param[in] info 指向 codex_diag_info_t 结构体的指针，包含所有诊断数据
 *                 若为 NULL，函数直接返回不做任何更新
 *
 * @note 建议由主循环定时器每 ~2 秒调用一次
 * @note 连接状态文字颜色自动设置：Connected=绿色，Disconnected=红色
 */
void codex_ui_update_diag(const codex_diag_info_t *info);

/**
 * @brief 切换到指定页面（带动画）
 *
 * 通过 lv_obj_set_tile() 切换 lv_tileview 的当前活动 tile，
 * 带滑动动画效果，同时更新底部指示点颜色。
 *
 * @param[in] page_index 目标页面索引：
 *                       - CODEX_PAGE_QUOTA (0): 额度主页面
 *                       - CODEX_PAGE_DIAG (1): 诊断页面
 *
 * @note 若 page_index 超出范围或 tileview 未初始化，函数静默返回
 * @note 也可通过触摸滑动由用户直接切换，此时由 tileview_event_cb 自动处理
 */
void codex_ui_switch_page(int page_index);

/**
 * @brief 获取当前页面索引
 *
 * @return 当前活动页面的索引值：
 *         - CODEX_PAGE_QUOTA (0): 额度主页面
 *         - CODEX_PAGE_DIAG (1): 诊断页面
 *
 * @note 返回值来自全局变量 g_current_page，在滑动或程序切换时自动更新
 */
int codex_ui_get_current_page(void);

#endif /* CODEX_UI_H */
