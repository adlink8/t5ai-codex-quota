/**
 * @file codex_ui.c
 * @brief Codex 额度显示 UI 实现（多页面触摸切换）
 *
 * ============================================================================
 * 模块架构
 * ============================================================================
 *
 * 本文件实现了基于 LVGL lv_tileview 的多页面触摸 UI 系统。
 *
 * 整体布局结构（从外到内）：
 *
 *   lv_scr_act()  ─── 全局屏幕对象
 *     └── lv_tileview  ─── 多页面容器（全屏大小，管理 tile 滑动切换）
 *           ├── tile (0,0)  ─── Page 0: 额度主页面
 *           │     ├── 标题 "Codex Quota" + 计划标签
 *           │     ├── 左卡片 (g_primary)  ── 主额度
 *           │     │     ├── 环形进度条 (lv_arc)
 *           │     │     ├── 百分比数字标签
 *           │     │     ├── 已用额度文本
 *           │     │     └── 重置时间文本
 *           │     ├── 右卡片 (g_secondary)  ── 次额度（可选）
 *           │     └── 底部状态栏
 *           │           ├── 彩色指示点 (g_live_dot)
 *           │           └── 状态文本 (g_status_label)
 *           │
 *           ├── tile (1,0)  ─── Page 1: 诊断页面
 *           │     ├── 标题 "Diagnostics"
 *           │     ├── [WiFi 面板]     [MQTT 面板]
 *           │     └── [System 面板]   [Bridge & Stats 面板]
 *           │
 *           └── 页面指示点 (g_page_dots[])  ─── 覆盖在最上层
 *
 * ============================================================================
 * 颜色方案
 * ============================================================================
 *
 *   背景色系：深蓝黑 (0x05070A) → 面板 (0x111821) → 次面板 (0x0B1118)
 *   状态色系：绿色=正常(0x3FB950)  黄色=警告(0xD29922)  红色=错误(0xF85149)
 *   强调色系：蓝色=高亮(0x58A6FF)  青色=小标题(0x39D2C0)
 *   文本色系：亮白=正文(0xE6EDF3)  灰色=辅助(0x8B949E)
 *
 * ============================================================================
 * 关键数据流
 * ============================================================================
 *
 *   API 数据刷新：
 *     codex_http 拉取 → codex_quota_t → codex_ui_update() → 更新卡片
 *
 *   诊断数据刷新（每 ~2 秒）：
 *     主循环定时器 → 收集系统信息 → codex_diag_info_t → codex_ui_update_diag() → 更新面板
 *
 *   用户交互：
 *     触摸滑动 → lv_tileview 自动处理 → LV_EVENT_VALUE_CHANGED
 *              → tileview_event_cb() → 更新 g_current_page + 指示点
 *
 * ============================================================================
 */

#include "codex_ui.h"
#include "codex_http.h"
#include "lvgl.h"
#include "tal_log.h"
#include <stdio.h>
#include <string.h>

/* ── 外部字体声明 ─────────────────────────────────── */
/** @brief 中文字体（16px），用于包含中文的标签文本 */
LV_FONT_DECLARE(lv_font_cn_16);

/* ── 颜色定义 ─────────────────────────────────────── */
/** @brief 屏幕背景色：深蓝黑 */
#define COLOR_BG        lv_color_hex(0x05070A)
/** @brief 主面板背景色：略亮的深蓝 */
#define COLOR_PANEL     lv_color_hex(0x111821)
/** @brief 次面板/状态栏背景色：更深的蓝黑 */
#define COLOR_PANEL_2   lv_color_hex(0x0B1118)
/** @brief 绿色：表示正常/已连接/高额度剩余（>50%） */
#define COLOR_GREEN     lv_color_hex(0x3FB950)
/** @brief 黄色：表示警告/中等额度剩余（20%-50%） */
#define COLOR_YELLOW    lv_color_hex(0xD29922)
/** @brief 红色：表示错误/断连/低额度剩余（<20%） */
#define COLOR_RED       lv_color_hex(0xF85149)
/** @brief 蓝色：用于页面指示点高亮和计划标签 */
#define COLOR_BLUE      lv_color_hex(0x58A6FF)
/** @brief 主文本色：明亮的浅白 */
#define COLOR_TEXT      lv_color_hex(0xE6EDF3)
/** @brief 辅助文本色：灰色，用于次要信息 */
#define COLOR_DIM       lv_color_hex(0x8B949E)
/** @brief 环形进度条背景轨道色：暗蓝 */
#define COLOR_RING_BG   lv_color_hex(0x263241)
/** @brief 青色：用于诊断面板小标题 */
#define COLOR_CYAN      lv_color_hex(0x39D2C0)

/* ── Quota Card 结构体 ──────────────────────────────── */
/**
 * @brief 额度卡片 UI 元素集合
 *
 * 每张卡片包含一个环形进度条和多个文本标签。
 * 额度页面有两张卡片：Primary（主额度）和 Secondary（次额度）。
 */
typedef struct {
    lv_obj_t *panel;    /**< @brief 卡片面板容器（圆角矩形背景） */
    lv_obj_t *arc;      /**< @brief 环形进度条控件（lv_arc），范围 0-100 */
    lv_obj_t *percent;  /**< @brief 百分比数字标签，如 "75%"，大字体 */
    lv_obj_t *title;    /**< @brief 卡片标题标签，如 "Primary"/"Secondary" */
    lv_obj_t *used;     /**< @brief 已用额度标签，如 "Used 25%" */
    lv_obj_t *reset;    /**< @brief 重置时间标签，如 "Reset 2d 5h" */
} quota_card_t;

/* ── 全局 UI 对象 ──────────────────────────────────── */

/* Tileview 相关对象 */
static lv_obj_t *g_tileview = NULL;     /**< @brief lv_tileview 多页面容器主对象 */
static lv_obj_t *g_tile_quota = NULL;   /**< @brief Page 0 tile：额度主页面 */
static lv_obj_t *g_tile_diag = NULL;    /**< @brief Page 1 tile：诊断页面 */

/* Page 0: 额度页面元素 */
static lv_obj_t *g_title;              /**< @brief 页面标题 "Codex Quota" */
static lv_obj_t *g_plan_badge;         /**< @brief 计划类型标签，如 "PRO"/"PLUS" */
static lv_obj_t *g_status_bar;         /**< @brief 底部状态栏面板 */
static lv_obj_t *g_status_label;       /**< @brief 状态文本标签，如 "Live data updated 14:30" */
static lv_obj_t *g_live_dot;           /**< @brief 状态指示点：绿色=实时/黄色=工作中/红色=错误 */
static quota_card_t g_primary;         /**< @brief 主额度卡片 */
static quota_card_t g_secondary;       /**< @brief 次额度卡片 */

/* Page 1: 诊断页面元素 */
static lv_obj_t *g_diag_title;         /**< @brief 诊断页面标题 "Diagnostics" */
static lv_obj_t *g_wifi_status;        /**< @brief WiFi 连接状态文本（Connected/Disconnected） */
static lv_obj_t *g_wifi_ssid_label;    /**< @brief WiFi SSID 值标签 */
static lv_obj_t *g_wifi_ip_label;      /**< @brief WiFi IP 地址值标签 */
static lv_obj_t *g_wifi_rssi_label;    /**< @brief WiFi RSSI 信号强度值标签 */
static lv_obj_t *g_mqtt_status;        /**< @brief MQTT 连接状态文本 */
static lv_obj_t *g_mqtt_host_label;    /**< @brief MQTT Broker 地址值标签 */
static lv_obj_t *g_mqtt_msgs_label;    /**< @brief MQTT 消息计数值标签 */
static lv_obj_t *g_bridge_label;       /**< @brief Bridge 服务器地址值标签 */
static lv_obj_t *g_mem_heap_label;     /**< @brief Heap 内存使用率值标签 */
static lv_obj_t *g_mem_psram_label;    /**< @brief PSRAM 使用率值标签 */
static lv_obj_t *g_uptime_label;       /**< @brief 系统运行时间值标签 */
static lv_obj_t *g_reconnect_label;    /**< @brief 重连计数值标签 */

/* Page 指示点 */
static lv_obj_t *g_page_dots[CODEX_PAGE_COUNT]; /**< @brief 底部页面指示点数组 */
static int g_current_page = 0;                  /**< @brief 当前活动页面索引（0 或 1） */

/* ── 工具函数 ──────────────────────────────────────── */

/**
 * @brief 将浮点百分比值限制在 [0, 100] 整数范围内
 *
 * @param[in] value 原始浮点百分比值（可能超出 0-100 范围）
 *
 * @return 限制后的整数百分比值，范围 [0, 100]
 *         - value < 0.0  → 返回 0
 *         - value > 100.0 → 返回 100
 *         - 其他 → 四舍五入取整
 */
static int clamp_percent(double value)
{
    if (value < 0.0) return 0;
    if (value > 100.0) return 100;
    return (int)(value + 0.5);
}

/**
 * @brief 根据剩余百分比返回对应的状态颜色
 *
 * 颜色映射规则：
 *   - remaining > 50% → 绿色（充足）
 *   - 20% < remaining ≤ 50% → 黄色（注意）
 *   - remaining ≤ 20% → 红色（告警）
 *
 * @param[in] remaining 剩余百分比（0.0 ~ 100.0）
 *
 * @return 对应的 lv_color_t 颜色值
 */
static lv_color_t color_for_remaining(double remaining)
{
    if (remaining > 50.0) return COLOR_GREEN;
    if (remaining > 20.0) return COLOR_YELLOW;
    return COLOR_RED;
}

/**
 * @brief 环形进度条动画的执行回调函数
 *
 * LVGL 动画系统每帧调用此函数，将当前动画帧的值设置到 lv_arc 控件上，
 * 实现环形进度条的平滑过渡效果。
 *
 * @param[in] obj   指向 lv_arc 控件对象的指针（通过 lv_anim_set_var 设置）
 * @param[in] value 当前动画帧的数值（范围与 arc 的 range 一致，即 0-100）
 */
static void arc_anim_cb(void *obj, int32_t value)
{
    lv_arc_set_value((lv_obj_t *)obj, value);
}

/**
 * @brief 启动环形进度条的平滑动画
 *
 * 使用 ease_out 缓动曲线，在 450ms 内将弧形进度条从当前值
 * 动画过渡到目标值，产生流畅的视觉效果。
 *
 * @param[in] arc    目标 lv_arc 控件对象
 * @param[in] target 目标百分比值（0-100）
 *
 * @note 动画使用 lv_anim_path_ease_out 缓动曲线，开头快结尾慢
 * @note 如果目标值与当前值相同，动画仍然会启动但无可见变化
 */
static void animate_arc_to(lv_obj_t *arc, int target)
{
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, arc);
    lv_anim_set_values(&anim, lv_arc_get_value(arc), target);
    lv_anim_set_time(&anim, 450);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&anim, arc_anim_cb);
    lv_anim_start(&anim);
}

/**
 * @brief 根据屏幕宽度选择百分比数字的字体大小
 *
 * 宽屏（≥440px）使用 34px 大字体，窄屏使用 24px 字体，
 * 确保百分比数字在不同分辨率下清晰可读。
 *
 * @param[in] screen_w 屏幕宽度（像素）
 *
 * @return 指向字体对象的指针
 *         - screen_w ≥ 440 → lv_font_montserrat_34
 *         - screen_w < 440 → lv_font_montserrat_24
 */
static const lv_font_t *font_for_percent(lv_coord_t screen_w)
{
    return screen_w >= 440 ? &lv_font_montserrat_34 : &lv_font_montserrat_24;
}

/**
 * @brief 获取中文文本使用的字体
 *
 * @return 指向 lv_font_cn_16 字体对象的指针（16px 中文字体）
 */
static const lv_font_t *font_for_cjk_text(void)
{
    return &lv_font_cn_16;
}

/**
 * @brief 检查字符串是否为纯 ASCII 文本
 *
 * 遍历字符串中的每个字节，若存在任何 ≥0x80 的字节则判定为非 ASCII
 * （即可能包含中文等多字节字符）。
 *
 * @param[in] text 待检查的字符串指针
 *
 * @return 1 表示纯 ASCII，0 表示包含非 ASCII 字符或为空/NULL
 */
static int is_ascii_text(const char *text)
{
    if (text == NULL || text[0] == '\0') return 0;
    while (*text) {
        if ((unsigned char)*text >= 0x80) return 0;
        text++;
    }
    return 1;
}

/**
 * @brief 检查字符串是否以指定前缀开头
 *
 * @param[in] text   待检查的完整字符串
 * @param[in] prefix 待匹配的前缀字符串
 *
 * @return 1 表示 text 以 prefix 开头，0 表示不匹配
 */
static int starts_with(const char *text, const char *prefix)
{
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

/**
 * @brief 向输出缓冲区追加一个带单位的数值片段
 *
 * 用于将中文时长（如 "3天5小时2分钟"）转写为 ASCII 缩写（如 "3d5h2m"）。
 * 此函数追加单个片段，如 append_unit(buf, size, &used, 3, "d") 追加 "3d"。
 *
 * @param[out]    out      输出缓冲区
 * @param[in]     out_size 输出缓冲区总大小
 * @param[in,out] used     当前已使用的字节数（会自动更新）
 * @param[in]     value    数值（≤0 时跳过不追加）
 * @param[in]     unit     单位后缀字符串，如 "d"/"h"/"m"
 */
static void append_unit(char *out, size_t out_size, size_t *used,
                        int value, const char *unit)
{
    if (value <= 0 || *used >= out_size) return;
    int written = snprintf(out + *used, out_size - *used, "%d%s", value, unit);
    if (written > 0) {
        *used += (size_t)written;
        if (*used >= out_size) *used = out_size - 1;
    }
}

/**
 * @brief 将中文时长字符串格式化为纯 ASCII 缩写
 *
 * 解析包含中文时长单位的字符串（如 "3天5小时2分钟"），
 * 转换为纯 ASCII 格式（如 "3d5h2m"），以便在小字体中正确显示。
 *
 * 转换规则：
 *   "X天"   → "Xd"
 *   "X小时" → "Xh"
 *   "X分钟" → "Xm"
 *
 * @param[in]  duration 中文时长字符串，如 "2天3小时"
 * @param[out] out      输出缓冲区，存放转换后的 ASCII 字符串
 * @param[in]  out_size 输出缓冲区大小
 *
 * @note 若输入为 NULL 或空字符串，输出 "--"
 * @note 若输入不含中文单位，尝试原样输出（仅当纯 ASCII 时）
 */
static void format_duration_ascii(const char *duration, char *out, size_t out_size)
{
    const char *p = duration;
    size_t used = 0;
    int number = 0;
    int has_number = 0;

    if (!duration || duration[0] == '\0') {
        snprintf(out, out_size, "--");
        return;
    }

    while (*p) {
        if (*p >= '0' && *p <= '9') {
            number = number * 10 + (*p - '0');
            has_number = 1;
            p++;
            continue;
        }

        if (has_number && starts_with(p, "天")) {
            append_unit(out, out_size, &used, number, "d");
            number = 0;
            has_number = 0;
        } else if (has_number && starts_with(p, "小时")) {
            append_unit(out, out_size, &used, number, "h");
            number = 0;
            has_number = 0;
        } else if (has_number && starts_with(p, "分钟")) {
            append_unit(out, out_size, &used, number, "m");
            number = 0;
            has_number = 0;
        }

        p++;
    }

    if (used == 0) {
        snprintf(out, out_size, "%s", is_ascii_text(duration) ? duration : "--");
    } else {
        out[used] = '\0';
    }
}

/**
 * @brief 将秒数格式化为人类可读的运行时间字符串
 *
 * 根据时间长度自动选择合适的显示格式：
 *   - ≥ 1天：显示 "Xd Xh Xm"（如 "3d 5h 20m"）
 *   - ≥ 1小时：显示 "Xh Xm Xs"（如 "2h 30m 15s"）
 *   - < 1小时：显示 "Xm Xs"（如 "5m 30s"）
 *
 * @param[in]  seconds  运行时间（秒）
 * @param[out] out      输出缓冲区
 * @param[in]  out_size 输出缓冲区大小
 */
static void format_uptime(uint32_t seconds, char *out, size_t out_size)
{
    uint32_t d = seconds / 86400;
    uint32_t h = (seconds % 86400) / 3600;
    uint32_t m = (seconds % 3600) / 60;
    uint32_t s = seconds % 60;

    if (d > 0) {
        snprintf(out, out_size, "%lud %luh %lum", (unsigned long)d, (unsigned long)h, (unsigned long)m);
    } else if (h > 0) {
        snprintf(out, out_size, "%luh %lum %lus", (unsigned long)h, (unsigned long)m, (unsigned long)s);
    } else {
        snprintf(out, out_size, "%lum %lus", (unsigned long)m, (unsigned long)s);
    }
}

/**
 * @brief 格式化内存使用情况为可读字符串
 *
 * 计算空闲内存和使用百分比，输出类似 "245KB / 320KB (23% free)" 的格式。
 *
 * @param[in]  used     已使用的内存（字节）
 * @param[in]  total    内存总量（字节）
 * @param[out] out      输出缓冲区
 * @param[in]  out_size 输出缓冲区大小
 *
 * @note 若 total 为 0，输出 "N/A"
 * @note 输出的 KB 值通过 /1024 计算，百分比显示的是空闲比例
 */
static void format_memory(uint32_t used, uint32_t total, char *out, size_t out_size)
{
    if (total == 0) {
        snprintf(out, out_size, "N/A");
        return;
    }
    uint32_t free = total - used;
    uint32_t pct = (used * 100) / total;
    snprintf(out, out_size, "%luKB / %luKB (%lu%% free)",
             (unsigned long)(free / 1024), (unsigned long)(total / 1024),
             (unsigned long)(100 - pct));
}

/* ── Quota Card 创建 ───────────────────────────────── */

/**
 * @brief 为卡片面板设置通用样式
 *
 * 统一的深色主题样式：
 *   - 背景色：深蓝 (0x111821)，完全不透明
 *   - 边框：1px，颜色 0x223044
 *   - 圆角：8px
 *   - 无内边距，不可滚动
 *
 * @param[in] card 目标 lv_obj_t 面板对象
 */
static void set_card_common_style(lv_obj_t *card)
{
    lv_obj_set_style_bg_color(card, COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x223044), 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
}

/**
 * @brief 创建一张完整的额度卡片
 *
 * 在指定的父容器中创建一张额度卡片，包含以下子元素：
 *   1. 标题标签（居中顶部）
 *   2. 环形进度条（270° 弧形，135°→45°，即 3/4 圆环）
 *   3. 百分比数字标签（覆盖在弧形中心）
 *   4. 已用额度标签（底部偏上）
 *   5. 重置时间标签（最底部）
 *
 * 弧形尺寸根据卡片宽高自适应（86px ~ 118px）。
 *
 * @param[out] card      输出的 quota_card_t 结构体，填充各 UI 元素指针
 * @param[in]  parent    父容器对象（通常是 tile）
 * @param[in]  x         卡片左上角 X 坐标（相对父容器）
 * @param[in]  y         卡片左上角 Y 坐标（相对父容器）
 * @param[in]  w         卡片宽度（像素）
 * @param[in]  h         卡片高度（像素）
 * @param[in]  title     卡片标题文本（如 "Primary"/"Secondary"）
 * @param[in]  screen_w  屏幕宽度，用于选择百分比字体大小
 */
static void create_quota_card(quota_card_t *card, lv_obj_t *parent,
                              lv_coord_t x, lv_coord_t y,
                              lv_coord_t w, lv_coord_t h,
                              const char *title, lv_coord_t screen_w)
{
    /* 根据卡片尺寸计算弧形大小，限制在 86-118px 范围内 */
    lv_coord_t arc_size = (h < w ? h : w) - 56;
    if (arc_size > 118) arc_size = 118;
    if (arc_size < 86) arc_size = 86;

    /* 创建卡片面板 */
    card->panel = lv_obj_create(parent);
    lv_obj_set_size(card->panel, w, h);
    lv_obj_set_pos(card->panel, x, y);
    set_card_common_style(card->panel);

    /* 标题标签 */
    card->title = lv_label_create(card->panel);
    lv_label_set_text(card->title, title);
    lv_obj_set_style_text_font(card->title, font_for_cjk_text(), 0);
    lv_obj_set_style_text_color(card->title, COLOR_TEXT, 0);
    lv_obj_align(card->title, LV_ALIGN_TOP_MID, 0, 10);

    /* 环形进度条（lv_arc）
     * bg_angles: 135°→45° 画 270° 弧（3/4 圆环，底部留空）
     * indicator 部分显示当前值对应的弧长
     * 不可点击，仅作显示用途 */
    card->arc = lv_arc_create(card->panel);
    lv_obj_set_size(card->arc, arc_size, arc_size);
    lv_obj_align(card->arc, LV_ALIGN_CENTER, 0, -2);
    lv_arc_set_bg_angles(card->arc, 135, 45);
    lv_arc_set_rotation(card->arc, 0);
    lv_arc_set_range(card->arc, 0, 100);
    lv_arc_set_value(card->arc, 0);
    lv_obj_set_style_arc_width(card->arc, 11, LV_PART_MAIN);
    lv_obj_set_style_arc_color(card->arc, COLOR_RING_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_width(card->arc, 11, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(card->arc, COLOR_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(card->arc, 0, LV_PART_INDICATOR);
    lv_obj_clear_flag(card->arc, LV_OBJ_FLAG_CLICKABLE);

    /* 百分比数字标签（覆盖在弧形中心） */
    card->percent = lv_label_create(card->panel);
    lv_label_set_text(card->percent, "--%");
    lv_obj_set_style_text_font(card->percent, font_for_percent(screen_w), 0);
    lv_obj_set_style_text_color(card->percent, COLOR_GREEN, 0);
    lv_obj_align_to(card->percent, card->arc, LV_ALIGN_CENTER, 0, -2);

    /* 已用额度标签 */
    card->used = lv_label_create(card->panel);
    lv_label_set_text(card->used, "Used --%");
    lv_obj_set_style_text_font(card->used, font_for_cjk_text(), 0);
    lv_obj_set_style_text_color(card->used, COLOR_DIM, 0);
    lv_obj_align(card->used, LV_ALIGN_BOTTOM_MID, 0, -28);

    /* 重置时间标签 */
    card->reset = lv_label_create(card->panel);
    lv_label_set_text(card->reset, "Reset --");
    lv_obj_set_style_text_font(card->reset, font_for_cjk_text(), 0);
    lv_obj_set_style_text_color(card->reset, COLOR_DIM, 0);
    lv_obj_align(card->reset, LV_ALIGN_BOTTOM_MID, 0, -10);
}

/**
 * @brief 更新一张额度卡片的数据
 *
 * 根据 window 数据刷新卡片的全部显示内容：
 *   1. 计算剩余百分比并限制在 [0, 100]
 *   2. 根据剩余百分比设置弧形颜色（绿/黄/红）
 *   3. 启动弧形动画过渡到新值（450ms ease_out）
 *   4. 更新百分比数字和已用额度文本
 *   5. 更新标题（优先使用 API 返回的 label）
 *   6. 将中文重置时间转为 ASCII 缩写后显示
 *
 * @param[in] card           目标卡片结构体指针
 * @param[in] fallback_title 备用标题（当 window->label 为空时使用）
 * @param[in] window         指向 codex_window_t 的数据源，包含剩余百分比、已用额度、重置时间等
 */
static void update_card(quota_card_t *card, const char *fallback_title,
                        const codex_window_t *window)
{
    char buf[64];
    char ascii[32];
    int remaining = clamp_percent(window->remaining);
    lv_color_t color = color_for_remaining(window->remaining);

    /* 更新弧形颜色和动画 */
    lv_obj_set_style_arc_color(card->arc, color, LV_PART_INDICATOR);
    animate_arc_to(card->arc, remaining);

    /* 更新百分比数字 */
    snprintf(buf, sizeof(buf), "%d%%", remaining);
    lv_label_set_text(card->percent, buf);
    lv_obj_set_style_text_color(card->percent, color, 0);

    /* 更新标题（API 返回的 label 优先） */
    lv_label_set_text(card->title,
                      window->label[0] ? window->label : fallback_title);

    /* 更新已用额度 */
    snprintf(buf, sizeof(buf), "Used %.0f%%", window->used);
    lv_label_set_text(card->used, buf);

    /* 更新重置时间（中文→ASCII 缩写） */
    format_duration_ascii(window->resets_in, ascii, sizeof(ascii));
    snprintf(buf, sizeof(buf), "Reset %s", ascii);
    lv_label_set_text(card->reset, buf);
}

/* ── 诊断页面创建 ───────────────────────────────────── */

/**
 * @brief 创建诊断页面的一个分区面板
 *
 * 创建一个带标题的诊断分区面板（如 "WiFi"/"MQTT"/"System"），
 * 使用青色标题文字和深色面板样式。面板用于容纳多个诊断条目。
 *
 * @param[in] parent 父容器对象（通常是诊断页面 tile）
 * @param[in] title  面板标题文本（如 "WiFi"/"MQTT"/"System"/"Bridge & Stats"）
 * @param[in] x      面板左上角 X 坐标
 * @param[in] y      面板左上角 Y 坐标
 * @param[in] w      面板宽度
 * @param[in] h      面板高度
 *
 * @return 创建好的面板对象指针，可继续向其中添加诊断条目
 */
static lv_obj_t *create_diag_section(lv_obj_t *parent, const char *title,
                                     lv_coord_t x, lv_coord_t y,
                                     lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_style_bg_color(panel, COLOR_PANEL, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x223044), 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_pad_all(panel, 8, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    /* 分区标题（青色，左对齐） */
    lv_obj_t *lbl = lv_label_create(panel);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, font_for_cjk_text(), 0);
    lv_obj_set_style_text_color(lbl, COLOR_CYAN, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    return panel;
}

/**
 * @brief 在诊断面板中创建一个诊断条目（标签 + 值）
 *
 * 每个条目由灰色标签和亮白值文本组成，采用左右布局：
 *
 *   [标签文本]  [值文本（自动截断加省略号）]
 *
 * 值文本宽度设为父容器 58%，超长时用 "..." 截断。
 *
 * @param[in] parent     父容器（诊断面板对象）
 * @param[in] label      条目标签文本（如 "SSID:"/"IP:"/"RSSI:"）
 * @param[in] align_obj  对齐参考对象（通常传入面板自身，从面板顶部偏移）
 * @param[in] y_offset   相对 align_obj 顶部的 Y 偏移量（像素）
 *
 * @return 创建好的值标签对象指针，后续通过 lv_label_set_text() 更新其内容
 */
static lv_obj_t *create_diag_item(lv_obj_t *parent, const char *label,
                                  lv_obj_t *align_obj, lv_coord_t y_offset)
{
    /* 标签文本（灰色，固定） */
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_font(lbl, font_for_cjk_text(), 0);
    lv_obj_set_style_text_color(lbl, COLOR_DIM, 0);
    lv_obj_align_to(lbl, align_obj, LV_ALIGN_TOP_LEFT, 0, y_offset);

    /* 值文本（亮白，动态更新） */
    lv_obj_t *val = lv_label_create(parent);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_font(val, font_for_cjk_text(), 0);
    lv_obj_set_style_text_color(val, COLOR_TEXT, 0);
    lv_obj_set_width(val, lv_pct(58));
    lv_label_set_long_mode(val, LV_LABEL_LONG_DOT);
    lv_obj_align_to(val, lbl, LV_ALIGN_OUT_RIGHT_MID, 8, 0);

    return val;
}

/**
 * @brief 创建底部页面指示点
 *
 * 在屏幕底部居中位置创建一组小圆点，每个页面对应一个。
 * 当前活动页面的圆点为蓝色，其余为灰色。
 *
 * 布局示例（2 个页面）：
 *   ──────────● ○──────────
 *
 * @param[in] parent    父容器（屏幕对象）
 * @param[in] screen_w  屏幕宽度，用于计算居中位置
 * @param[in] screen_h  屏幕高度，用于计算底部 Y 坐标（screen_h - 16px）
 */
static void create_page_dots(lv_obj_t *parent, lv_coord_t screen_w, lv_coord_t screen_h)
{
    lv_coord_t dot_size = 8;
    lv_coord_t gap = 12;
    lv_coord_t total_w = CODEX_PAGE_COUNT * dot_size + (CODEX_PAGE_COUNT - 1) * gap;
    lv_coord_t start_x = (screen_w - total_w) / 2;
    lv_coord_t y = screen_h - 16;

    for (int i = 0; i < CODEX_PAGE_COUNT; i++) {
        g_page_dots[i] = lv_obj_create(parent);
        lv_obj_set_size(g_page_dots[i], dot_size, dot_size);
        lv_obj_set_pos(g_page_dots[i], start_x + i * (dot_size + gap), y);
        lv_obj_set_style_radius(g_page_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(g_page_dots[i],
                                  i == 0 ? COLOR_BLUE : COLOR_DIM, 0);
        lv_obj_set_style_bg_opa(g_page_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(g_page_dots[i], 0, 0);
    }
}

/**
 * @brief 更新页面指示点颜色
 *
 * 根据 g_current_page 的值更新底部圆点颜色：
 *   - 当前页面 → 蓝色
 *   - 其他页面 → 灰色
 *
 * @note 在页面切换事件回调和程序切换中都会调用
 */
static void update_page_dots(void)
{
    for (int i = 0; i < CODEX_PAGE_COUNT; i++) {
        lv_obj_set_style_bg_color(g_page_dots[i],
                                  i == g_current_page ? COLOR_BLUE : COLOR_DIM, 0);
    }
}

/* ── Tileview 事件回调 ──────────────────────────────── */

/**
 * @brief lv_tileview 页面切换事件回调
 *
 * 当用户触摸滑动导致 tileview 的活动 tile 发生变化时，
 * LVGL 触发 LV_EVENT_VALUE_CHANGED 事件，此回调被调用。
 *
 * 处理流程：
 *   1. 通过 lv_tileview_get_tile_act() 获取当前活动的 tile 对象
 *   2. 与 g_tile_quota / g_tile_diag 指针比较，确定页面索引
 *   3. 更新全局变量 g_current_page
 *   4. 调用 update_page_dots() 刷新底部指示点颜色
 *
 * @param[in] e LVGL 事件对象（本回调未使用其属性）
 *
 * @note 此回调在 codex_ui_create() 中通过 lv_obj_add_event_cb() 注册
 * @note 仅处理水平方向的 tile 切换（纵向 tile 暂未使用）
 */
static void tileview_event_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_t *active_tile = lv_tileview_get_tile_act(g_tileview);

    if (active_tile == g_tile_quota) {
        g_current_page = CODEX_PAGE_QUOTA;
    } else if (active_tile == g_tile_diag) {
        g_current_page = CODEX_PAGE_DIAG;
    }
    update_page_dots();
}

/* ── 公开接口 ──────────────────────────────────────── */

/**
 * @brief 创建完整 UI 界面（启动时调用一次）
 *
 * 初始化步骤：
 *   1. 屏幕方向检测：竖屏时自动旋转 90° 为横屏
 *   2. 清理默认屏幕，设置深色背景
 *   3. 计算各区域尺寸（header/content/status/dots）
 *   4. 创建 lv_tileview 容器（全屏，注册切换事件）
 *   5. 创建 Page 0（额度页面）：
 *      - 标题 + 计划标签
 *      - 左右双卡片（Primary/Secondary）
 *      - 底部状态栏（指示点 + 状态文本）
 *   6. 创建 Page 1（诊断页面）：
 *      - 标题
 *      - WiFi/MQTT/System/Bridge 四个面板（2×2 网格）
 *   7. 创建底部页面指示点
 *   8. 输出日志（分辨率和页面数）
 *
 * @note 必须在 LVGL 初始化完成且 lv_tick_inc() 已启动后调用
 * @note 仅调用一次；重复调用会先清理旧 UI 再重建
 */
void codex_ui_create(void)
{
    /* 屏幕方向检测：竖屏自动旋转为横屏 */
    lv_disp_t *disp = lv_disp_get_default();
    if (disp != NULL && lv_disp_get_hor_res(disp) < lv_disp_get_ver_res(disp)) {
        lv_disp_set_rotation(disp, LV_DISP_ROT_90);
    }

    /* 清理屏幕并设置深色背景 */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* 获取屏幕尺寸 */
    lv_coord_t sw = disp != NULL ? lv_disp_get_hor_res(disp) : LV_HOR_RES;
    lv_coord_t sh = disp != NULL ? lv_disp_get_ver_res(disp) : LV_VER_RES;
    lv_coord_t margin = 10;
    lv_coord_t header_h = 44;       /* 标题区域高度 */
    lv_coord_t status_h = 34;       /* 底部状态栏高度 */
    lv_coord_t dots_h = 16;         /* 页面指示点高度 */
    lv_coord_t gap = 10;            /* 卡片/面板间距 */
    lv_coord_t content_y = header_h + margin;  /* 内容区起始 Y */
    /* Page 0 内容高度：扣除 header + status + dots + 间距 */
    lv_coord_t content_h = sh - header_h - status_h - dots_h - (margin * 3);
    /* Page 1 内容高度：扣除 header + dots + 间距（无 status 栏） */
    lv_coord_t diag_content_h = sh - header_h - dots_h - (margin * 2);

    /* ── Tileview 容器 ──────────────────────────────────
     * lv_tileview 是 LVGL 的多页面滑动容器组件：
     * - 每个 tile 是一个独立的子页面
     * - 通过 add_tile(col, row, direction) 添加 tile
     * - 用户触摸滑动时自动切换 tile
     * - LV_DIR_RIGHT 表示该 tile 可以向右滑动离开 */
    g_tileview = lv_tileview_create(scr);
    lv_obj_set_size(g_tileview, sw, sh);
    lv_obj_set_style_bg_color(g_tileview, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(g_tileview, LV_OPA_COVER, 0);
    /* 注册页面切换事件回调 */
    lv_obj_add_event_cb(g_tileview, tileview_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* ── Page 0: 额度主页面 ─────────────────────────────
     * tile 坐标 (0,0)，允许向右滑动（进入 Page 1） */
    g_tile_quota = lv_tileview_add_tile(g_tileview, 0, 0, LV_DIR_RIGHT);

    /* 计算双卡片宽度（等分可用空间） */
    lv_coord_t card_w = (sw - margin * 2 - gap) / 2;
    lv_coord_t card_h = content_h;

    /* 标题 "Codex Quota" */
    g_title = lv_label_create(g_tile_quota);
    lv_label_set_text(g_title, "Codex Quota");
    lv_obj_set_style_text_font(g_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(g_title, COLOR_TEXT, 0);
    lv_obj_align(g_title, LV_ALIGN_TOP_LEFT, margin, 10);

    /* 计划类型标签（右上角，如 "PRO"） */
    g_plan_badge = lv_label_create(g_tile_quota);
    lv_label_set_text(g_plan_badge, "...");
    lv_obj_set_style_text_font(g_plan_badge, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(g_plan_badge, COLOR_BLUE, 0);
    lv_obj_align(g_plan_badge, LV_ALIGN_TOP_RIGHT, -margin, 14);

    /* 左卡片（Primary）和右卡片（Secondary） */
    create_quota_card(&g_primary, g_tile_quota, margin, content_y, card_w, card_h, "Primary", sw);
    create_quota_card(&g_secondary, g_tile_quota, margin + card_w + gap, content_y, card_w, card_h, "Secondary", sw);

    /* 底部状态栏 */
    g_status_bar = lv_obj_create(g_tile_quota);
    lv_obj_set_size(g_status_bar, sw - margin * 2, status_h);
    lv_obj_align(g_status_bar, LV_ALIGN_BOTTOM_MID, 0, -dots_h - 2);
    lv_obj_set_style_bg_color(g_status_bar, COLOR_PANEL_2, 0);
    lv_obj_set_style_bg_opa(g_status_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_status_bar, 1, 0);
    lv_obj_set_style_border_color(g_status_bar, lv_color_hex(0x1D2A3A), 0);
    lv_obj_set_style_radius(g_status_bar, 8, 0);
    lv_obj_set_style_pad_all(g_status_bar, 0, 0);
    lv_obj_clear_flag(g_status_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* 状态指示点（初始黄色=工作中） */
    g_live_dot = lv_obj_create(g_status_bar);
    lv_obj_set_size(g_live_dot, 8, 8);
    lv_obj_set_style_radius(g_live_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(g_live_dot, COLOR_YELLOW, 0);
    lv_obj_set_style_bg_opa(g_live_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_live_dot, 0, 0);
    lv_obj_align(g_live_dot, LV_ALIGN_LEFT_MID, 10, 0);

    /* 状态文本标签 */
    g_status_label = lv_label_create(g_status_bar);
    lv_label_set_text(g_status_label, "Starting...");
    lv_obj_set_style_text_font(g_status_label, font_for_cjk_text(), 0);
    lv_obj_set_style_text_color(g_status_label, COLOR_DIM, 0);
    lv_obj_set_width(g_status_label, sw - margin * 2 - 32);
    lv_label_set_long_mode(g_status_label, LV_LABEL_LONG_DOT);
    lv_obj_align(g_status_label, LV_ALIGN_LEFT_MID, 24, 0);

    /* ── Page 1: 诊断页面 ───────────────────────────────
     * tile 坐标 (1,0)，允许向左滑动（返回 Page 0） */
    g_tile_diag = lv_tileview_add_tile(g_tileview, 1, 0, LV_DIR_LEFT);

    /* 诊断页面标题 */
    g_diag_title = lv_label_create(g_tile_diag);
    lv_label_set_text(g_diag_title, "Diagnostics");
    lv_obj_set_style_text_font(g_diag_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(g_diag_title, COLOR_TEXT, 0);
    lv_obj_align(g_diag_title, LV_ALIGN_TOP_LEFT, margin, 10);

    /* 计算 2×2 面板网格尺寸 */
    lv_coord_t section_w = (sw - margin * 2 - gap) / 2;
    lv_coord_t section_h = (diag_content_h - gap) / 2;

    /* 左上：WiFi 面板 */
    lv_obj_t *wifi_panel = create_diag_section(g_tile_diag, "WiFi",
                                                margin, content_y,
                                                section_w, section_h);
    g_wifi_ssid_label = create_diag_item(wifi_panel, "SSID:", wifi_panel, 28);
    g_wifi_ip_label = create_diag_item(wifi_panel, "IP:", wifi_panel, 50);
    g_wifi_rssi_label = create_diag_item(wifi_panel, "RSSI:", wifi_panel, 72);
    g_wifi_status = create_diag_item(wifi_panel, "Status:", wifi_panel, 94);

    /* 右上：MQTT 面板 */
    lv_obj_t *mqtt_panel = create_diag_section(g_tile_diag, "MQTT",
                                                margin + section_w + gap, content_y,
                                                section_w, section_h);
    g_mqtt_host_label = create_diag_item(mqtt_panel, "Broker:", mqtt_panel, 28);
    g_mqtt_msgs_label = create_diag_item(mqtt_panel, "Messages:", mqtt_panel, 50);
    g_mqtt_status = create_diag_item(mqtt_panel, "Status:", mqtt_panel, 72);

    /* 左下：System 面板 */
    lv_obj_t *sys_panel = create_diag_section(g_tile_diag, "System",
                                               margin, content_y + section_h + gap,
                                               section_w, section_h);
    g_mem_heap_label = create_diag_item(sys_panel, "Heap:", sys_panel, 28);
    g_mem_psram_label = create_diag_item(sys_panel, "PSRAM:", sys_panel, 50);
    g_uptime_label = create_diag_item(sys_panel, "Uptime:", sys_panel, 72);

    /* 右下：Bridge & Stats 面板 */
    lv_obj_t *bridge_panel = create_diag_section(g_tile_diag, "Bridge & Stats",
                                                  margin + section_w + gap, content_y + section_h + gap,
                                                  section_w, section_h);
    g_bridge_label = create_diag_item(bridge_panel, "Server:", bridge_panel, 28);
    g_reconnect_label = create_diag_item(bridge_panel, "Reconnects:", bridge_panel, 50);

    /* ── 底部页面指示点 ────────────────────────────────── */
    create_page_dots(scr, sw, sh);

    PR_NOTICE("[ui] Multi-page UI created, res=%dx%d, pages=%d",
              (int)sw, (int)sh, CODEX_PAGE_COUNT);
}

/**
 * @brief 用最新额度数据刷新 UI
 *
 * 更新内容：
 *   1. 计划标签：将 plan_type 转为大写显示（如 "pro" → "PRO"）
 *   2. 主卡片：更新进度条、百分比、已用额度、重置时间
 *   3. 次卡片：若 has_secondary 为真则更新数据，否则显示占位
 *   4. 状态栏：显示 "Live data updated HH:MM"，指示点变绿
 *
 * @param[in] quota 指向 codex_quota_t 的指针，由 codex_http 拉取后填充
 *
 * @note 环形进度条使用 450ms ease_out 动画平滑过渡
 * @note 次额度不可用时显示 "Not available"，弧形归零且颜色为背景色
 */
void codex_ui_update(const codex_quota_t *quota)
{
    char buf[64];

    /* 计划类型标签转大写 */
    snprintf(buf, sizeof(buf), "%s", quota->plan_type[0] ? quota->plan_type : "plan");
    for (char *p = buf; *p; p++) {
        if (*p >= 'a' && *p <= 'z') *p -= 32;
    }
    lv_label_set_text(g_plan_badge, buf);

    /* 更新主额度卡片 */
    update_card(&g_primary, "Primary", &quota->primary);

    /* 更新次额度卡片 */
    if (quota->has_secondary) {
        lv_obj_clear_flag(g_secondary.panel, LV_OBJ_FLAG_HIDDEN);
        update_card(&g_secondary, "Secondary", &quota->secondary);
    } else {
        /* 次额度不可用：显示占位信息 */
        lv_obj_clear_flag(g_secondary.panel, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_secondary.title, "Secondary");
        lv_label_set_text(g_secondary.percent, "--%");
        lv_label_set_text(g_secondary.used, "Not available");
        lv_label_set_text(g_secondary.reset, "");
        animate_arc_to(g_secondary.arc, 0);
        lv_obj_set_style_arc_color(g_secondary.arc, COLOR_RING_BG, LV_PART_INDICATOR);
    }

    /* 更新底部状态栏 */
    snprintf(buf, sizeof(buf), "Live data updated %s", quota->updated_time[0] ? quota->updated_time : "--:--");
    lv_label_set_text(g_status_label, buf);
    lv_obj_set_style_text_color(g_status_label, COLOR_DIM, 0);
    lv_obj_set_style_bg_color(g_live_dot, COLOR_GREEN, 0);
}

/**
 * @brief 设置启动/连接/拉取中的状态文本
 *
 * 底部状态栏显示临时状态提示，指示点变黄色（工作中）。
 *
 * @param[in] message 状态文本，如 "Starting..."/"Connecting..."/"Fetching..."
 *                    NULL 时显示 "Working..."
 */
void codex_ui_set_status(const char *message)
{
    if (g_status_label == NULL) return;
    lv_label_set_text(g_status_label, message ? message : "Working...");
    lv_obj_set_style_text_color(g_status_label, COLOR_DIM, 0);
    if (g_live_dot != NULL) {
        lv_obj_set_style_bg_color(g_live_dot, COLOR_YELLOW, 0);
    }
}

/**
 * @brief 设置错误状态文本
 *
 * 状态文本变红色，指示点也变红色，表示发生错误。
 *
 * @param[in] message 错误描述文本，如 "HTTP Error 500"/"JSON Parse Failed"
 *                    NULL 时显示 "Error"
 */
void codex_ui_set_error(const char *message)
{
    if (g_status_label != NULL) {
        lv_label_set_text(g_status_label, message ? message : "Error");
        lv_obj_set_style_text_color(g_status_label, COLOR_RED, 0);
    }
    if (g_live_dot != NULL) {
        lv_obj_set_style_bg_color(g_live_dot, COLOR_RED, 0);
    }
}

/**
 * @brief 设置离线状态
 *
 * 显示固定的离线提示文本，文本和指示点均为红色。
 *
 * @note 无参数，文本固定为 "Offline - waiting for bridge data"
 */
void codex_ui_set_offline(void)
{
    if (g_status_label != NULL) {
        lv_label_set_text(g_status_label, "Offline - waiting for bridge data");
        lv_obj_set_style_text_color(g_status_label, COLOR_RED, 0);
    }
    if (g_live_dot != NULL) {
        lv_obj_set_style_bg_color(g_live_dot, COLOR_RED, 0);
    }
}

/**
 * @brief 更新诊断页面信息（周期调用）
 *
 * 将外部收集的诊断数据写入 Page 1 各面板：
 *
 *   WiFi 面板：
 *     - SSID: 当前连接的热点名称
 *     - IP: 分配的 IP 地址
 *     - RSSI: 信号强度（dBm）
 *     - Status: Connected（绿色）/ Disconnected（红色）
 *
 *   MQTT 面板：
 *     - Broker: host:port 格式
 *     - Messages: 累计消息计数
 *     - Status: Connected（绿色）/ Disconnected（红色）
 *
 *   System 面板：
 *     - Heap: 已用/总量KB（空闲百分比）
 *     - PSRAM: 同上，无 PSRAM 显示 "N/A"
 *     - Uptime: 运行时间（Xd Xh Xm 或 Xh Xm Xs）
 *
 *   Bridge & Stats 面板：
 *     - Server: host:port 格式
 *     - Reconnects: "WiFi:X MQTT:Y" 格式
 *
 * @param[in] info 指向诊断信息结构体的指针，NULL 时直接返回
 *
 * @note 建议每 ~2 秒调用一次，由主循环定时器触发
 */
void codex_ui_update_diag(const codex_diag_info_t *info)
{
    char buf[64];

    if (info == NULL) return;

    /* ── WiFi 面板更新 ──────────────────────────────── */
    lv_label_set_text(g_wifi_ssid_label, info->wifi_ssid[0] ? info->wifi_ssid : "N/A");
    lv_label_set_text(g_wifi_ip_label, info->wifi_ip[0] ? info->wifi_ip : "N/A");
    snprintf(buf, sizeof(buf), "%d dBm", info->wifi_rssi);
    lv_label_set_text(g_wifi_rssi_label, buf);
    lv_label_set_text(g_wifi_status, info->wifi_connected ? "Connected" : "Disconnected");
    lv_obj_set_style_text_color(g_wifi_status,
                                info->wifi_connected ? COLOR_GREEN : COLOR_RED, 0);

    /* ── MQTT 面板更新 ──────────────────────────────── */
    snprintf(buf, sizeof(buf), "%s:%u", info->mqtt_host, (unsigned)info->mqtt_port);
    lv_label_set_text(g_mqtt_host_label, buf);
    snprintf(buf, sizeof(buf), "%d", info->mqtt_msg_count);
    lv_label_set_text(g_mqtt_msgs_label, buf);
    lv_label_set_text(g_mqtt_status, info->mqtt_connected ? "Connected" : "Disconnected");
    lv_obj_set_style_text_color(g_mqtt_status,
                                info->mqtt_connected ? COLOR_GREEN : COLOR_RED, 0);

    /* ── System 面板更新 ────────────────────────────── */
    /* Heap 内存 */
    format_memory(info->heap_total_bytes - info->heap_free_bytes,
                  info->heap_total_bytes, buf, sizeof(buf));
    lv_label_set_text(g_mem_heap_label, buf);

    /* PSRAM（无 PSRAM 时 total 为 0，显示 N/A） */
    if (info->psram_total_bytes > 0) {
        format_memory(info->psram_total_bytes - info->psram_free_bytes,
                      info->psram_total_bytes, buf, sizeof(buf));
        lv_label_set_text(g_mem_psram_label, buf);
    } else {
        lv_label_set_text(g_mem_psram_label, "N/A");
    }

    /* 运行时间 */
    format_uptime(info->uptime_seconds, buf, sizeof(buf));
    lv_label_set_text(g_uptime_label, buf);

    /* ── Bridge & Stats 面板更新 ────────────────────── */
    snprintf(buf, sizeof(buf), "%s:%u", info->bridge_host, (unsigned)info->bridge_port);
    lv_label_set_text(g_bridge_label, buf);

    snprintf(buf, sizeof(buf), "WiFi:%d MQTT:%d",
             info->wifi_reconnect_count, info->mqtt_reconnect_count);
    lv_label_set_text(g_reconnect_label, buf);
}

/**
 * @brief 切换到指定页面（带动画）
 *
 * 通过 lv_obj_set_tile() 将 tileview 切换到目标 tile，
 * 带滑动动画效果。同时更新 g_current_page 和底部指示点。
 *
 * @param[in] page_index 目标页面索引：
 *                       - CODEX_PAGE_QUOTA (0): 额度主页面
 *                       - CODEX_PAGE_DIAG (1): 诊断页面
 *
 * @note 若 page_index 超出 [0, CODEX_PAGE_COUNT) 范围，静默返回
 * @note 若 g_tileview 未初始化（NULL），静默返回
 */
void codex_ui_switch_page(int page_index)
{
    if (page_index < 0 || page_index >= CODEX_PAGE_COUNT) return;
    if (g_tileview == NULL) return;

    lv_obj_t *target_tile = NULL;
    switch (page_index) {
    case CODEX_PAGE_QUOTA: target_tile = g_tile_quota; break;
    case CODEX_PAGE_DIAG:  target_tile = g_tile_diag; break;
    default: return;
    }

    if (target_tile != NULL) {
        lv_obj_set_tile(g_tileview, target_tile, LV_ANIM_ON);
        g_current_page = page_index;
        update_page_dots();
    }
}

/**
 * @brief 获取当前页面索引
 *
 * @return 当前活动页面索引：
 *         - CODEX_PAGE_QUOTA (0): 额度主页面
 *         - CODEX_PAGE_DIAG (1): 诊断页面
 *
 * @note 返回值在触摸滑动事件回调或 codex_ui_switch_page() 中更新
 */
int codex_ui_get_current_page(void)
{
    return g_current_page;
}
