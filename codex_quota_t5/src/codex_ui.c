/**
 * @file codex_ui.c
 * @brief Codex quota dashboard UI with multi-page touchscreen support
 *
 * Page 0: Quota display (dual arc cards)
 * Page 1: Diagnostics (WiFi, MQTT, memory, uptime)
 *
 * Swipe left/right to switch pages.
 */

#include "codex_ui.h"
#include "codex_http.h"
#include "lvgl.h"
#include "tal_log.h"
#include <stdio.h>
#include <string.h>

/* ── 颜色定义 ─────────────────────────────────────── */
#define COLOR_BG        lv_color_hex(0x05070A)
#define COLOR_PANEL     lv_color_hex(0x111821)
#define COLOR_PANEL_2   lv_color_hex(0x0B1118)
#define COLOR_GREEN     lv_color_hex(0x3FB950)
#define COLOR_YELLOW    lv_color_hex(0xD29922)
#define COLOR_RED       lv_color_hex(0xF85149)
#define COLOR_BLUE      lv_color_hex(0x58A6FF)
#define COLOR_TEXT      lv_color_hex(0xE6EDF3)
#define COLOR_DIM       lv_color_hex(0x8B949E)
#define COLOR_RING_BG   lv_color_hex(0x263241)
#define COLOR_CYAN      lv_color_hex(0x39D2C0)

/* ── Quota Card 结构体 ──────────────────────────────── */
typedef struct {
    lv_obj_t *panel;
    lv_obj_t *arc;
    lv_obj_t *percent;
    lv_obj_t *title;
    lv_obj_t *used;
    lv_obj_t *reset;
} quota_card_t;

/* ── 全局 UI 对象 ──────────────────────────────────── */
static lv_obj_t *g_tileview = NULL;
static lv_obj_t *g_tile_quota = NULL;
static lv_obj_t *g_tile_diag = NULL;

/* Page 0: Quota */
static lv_obj_t *g_title;
static lv_obj_t *g_plan_badge;
static lv_obj_t *g_status_label;
static lv_obj_t *g_live_dot;
static quota_card_t g_primary;
static quota_card_t g_secondary;

/* Page 1: Diagnostics */
static lv_obj_t *g_diag_title;
static lv_obj_t *g_wifi_status;
static lv_obj_t *g_wifi_ssid_label;
static lv_obj_t *g_wifi_ip_label;
static lv_obj_t *g_wifi_rssi_label;
static lv_obj_t *g_mqtt_status;
static lv_obj_t *g_mqtt_host_label;
static lv_obj_t *g_mqtt_msgs_label;
static lv_obj_t *g_bridge_label;
static lv_obj_t *g_mem_heap_label;
static lv_obj_t *g_mem_psram_label;
static lv_obj_t *g_uptime_label;
static lv_obj_t *g_reconnect_label;

/* Page indicator */
static lv_obj_t *g_page_dots[CODEX_PAGE_COUNT];
static int g_current_page = 0;

/* ── 工具函数 ──────────────────────────────────────── */

static int clamp_percent(double value)
{
    if (value < 0.0) return 0;
    if (value > 100.0) return 100;
    return (int)(value + 0.5);
}

static lv_color_t color_for_remaining(double remaining)
{
    if (remaining > 50.0) return COLOR_GREEN;
    if (remaining > 20.0) return COLOR_YELLOW;
    return COLOR_RED;
}

static void arc_anim_cb(void *obj, int32_t value)
{
    lv_arc_set_value((lv_obj_t *)obj, value);
}

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

static const lv_font_t *font_for_percent(lv_coord_t screen_w)
{
    return screen_w >= 440 ? &lv_font_montserrat_34 : &lv_font_montserrat_24;
}

static const lv_font_t *font_for_cjk_text(void)
{
    return &lv_font_simsun_16_cjk;
}

static int is_ascii_text(const char *text)
{
    if (text == NULL || text[0] == '\0') return 0;
    while (*text) {
        if ((unsigned char)*text >= 0x80) return 0;
        text++;
    }
    return 1;
}

static int starts_with(const char *text, const char *prefix)
{
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

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

/* ── 格式化运行时间 ─────────────────────────────────── */
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

/* ── 格式化内存大小 ─────────────────────────────────── */
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

static void create_quota_card(quota_card_t *card, lv_obj_t *parent,
                              lv_coord_t x, lv_coord_t y,
                              lv_coord_t w, lv_coord_t h,
                              const char *title, lv_coord_t screen_w)
{
    lv_coord_t arc_size = (h < w ? h : w) - 56;
    if (arc_size > 118) arc_size = 118;
    if (arc_size < 86) arc_size = 86;

    card->panel = lv_obj_create(parent);
    lv_obj_set_size(card->panel, w, h);
    lv_obj_set_pos(card->panel, x, y);
    set_card_common_style(card->panel);

    card->title = lv_label_create(card->panel);
    lv_label_set_text(card->title, title);
    lv_obj_set_style_text_font(card->title, font_for_cjk_text(), 0);
    lv_obj_set_style_text_color(card->title, COLOR_TEXT, 0);
    lv_obj_align(card->title, LV_ALIGN_TOP_MID, 0, 10);

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

    card->percent = lv_label_create(card->panel);
    lv_label_set_text(card->percent, "--%");
    lv_obj_set_style_text_font(card->percent, font_for_percent(screen_w), 0);
    lv_obj_set_style_text_color(card->percent, COLOR_GREEN, 0);
    lv_obj_align_to(card->percent, card->arc, LV_ALIGN_CENTER, 0, -2);

    card->used = lv_label_create(card->panel);
    lv_label_set_text(card->used, "Used --%");
    lv_obj_set_style_text_font(card->used, font_for_cjk_text(), 0);
    lv_obj_set_style_text_color(card->used, COLOR_DIM, 0);
    lv_obj_align(card->used, LV_ALIGN_BOTTOM_MID, 0, -28);

    card->reset = lv_label_create(card->panel);
    lv_label_set_text(card->reset, "Reset --");
    lv_obj_set_style_text_font(card->reset, font_for_cjk_text(), 0);
    lv_obj_set_style_text_color(card->reset, COLOR_DIM, 0);
    lv_obj_align(card->reset, LV_ALIGN_BOTTOM_MID, 0, -10);
}

static void update_card(quota_card_t *card, const char *fallback_title,
                        const codex_window_t *window)
{
    char buf[64];
    char ascii[32];
    int remaining = clamp_percent(window->remaining);
    lv_color_t color = color_for_remaining(window->remaining);

    lv_obj_set_style_arc_color(card->arc, color, LV_PART_INDICATOR);
    animate_arc_to(card->arc, remaining);

    snprintf(buf, sizeof(buf), "%d%%", remaining);
    lv_label_set_text(card->percent, buf);
    lv_obj_set_style_text_color(card->percent, color, 0);

    lv_label_set_text(card->title,
                      window->label[0] ? window->label : fallback_title);

    snprintf(buf, sizeof(buf), "Used %.0f%%", window->used);
    lv_label_set_text(card->used, buf);

    format_duration_ascii(window->resets_in, ascii, sizeof(ascii));
    snprintf(buf, sizeof(buf), "Reset %s", ascii);
    lv_label_set_text(card->reset, buf);
}

/* ── 诊断页面创建 ───────────────────────────────────── */

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

    lv_obj_t *lbl = lv_label_create(panel);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, font_for_cjk_text(), 0);
    lv_obj_set_style_text_color(lbl, COLOR_CYAN, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    return panel;
}

static lv_obj_t *create_diag_item(lv_obj_t *parent, const char *label,
                                  lv_obj_t *align_obj, lv_coord_t y_offset)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_font(lbl, font_for_cjk_text(), 0);
    lv_obj_set_style_text_color(lbl, COLOR_DIM, 0);
    lv_obj_align_to(lbl, align_obj, LV_ALIGN_TOP_LEFT, 0, y_offset);

    lv_obj_t *val = lv_label_create(parent);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_font(val, font_for_cjk_text(), 0);
    lv_obj_set_style_text_color(val, COLOR_TEXT, 0);
    lv_obj_set_width(val, lv_pct(58));
    lv_label_set_long_mode(val, LV_LABEL_LONG_DOT);
    lv_obj_align_to(val, lbl, LV_ALIGN_OUT_RIGHT_MID, 8, 0);

    return val;
}

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

static void update_page_dots(void)
{
    for (int i = 0; i < CODEX_PAGE_COUNT; i++) {
        lv_obj_set_style_bg_color(g_page_dots[i],
                                  i == g_current_page ? COLOR_BLUE : COLOR_DIM, 0);
    }
}

/* ── Tileview 事件回调 ──────────────────────────────── */

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

void codex_ui_create(void)
{
    lv_disp_t *disp = lv_disp_get_default();
    if (disp != NULL && lv_disp_get_hor_res(disp) < lv_disp_get_ver_res(disp)) {
        lv_disp_set_rotation(disp, LV_DISP_ROT_90);
    }

    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_coord_t sw = disp != NULL ? lv_disp_get_hor_res(disp) : LV_HOR_RES;
    lv_coord_t sh = disp != NULL ? lv_disp_get_ver_res(disp) : LV_VER_RES;
    lv_coord_t margin = 10;
    lv_coord_t header_h = 44;
    lv_coord_t status_h = 28;
    lv_coord_t gap = 10;
    lv_coord_t content_y = header_h + margin;
    lv_coord_t content_h = sh - header_h - status_h - (margin * 3) - 20; /* 20 for dots */

    /* ── Tileview ────────────────────────────────── */
    g_tileview = lv_tileview_create(scr);
    lv_obj_set_size(g_tileview, sw, sh);
    lv_obj_set_style_bg_color(g_tileview, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(g_tileview, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(g_tileview, tileview_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* ── Page 0: Quota ───────────────────────────── */
    g_tile_quota = lv_tileview_add_tile(g_tileview, 0, 0, LV_DIR_RIGHT);

    lv_coord_t card_w = (sw - margin * 2 - gap) / 2;
    lv_coord_t card_h = content_h;

    g_title = lv_label_create(g_tile_quota);
    lv_label_set_text(g_title, "Codex Quota");
    lv_obj_set_style_text_font(g_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(g_title, COLOR_TEXT, 0);
    lv_obj_align(g_title, LV_ALIGN_TOP_LEFT, margin, 10);

    g_plan_badge = lv_label_create(g_tile_quota);
    lv_label_set_text(g_plan_badge, "...");
    lv_obj_set_style_text_font(g_plan_badge, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(g_plan_badge, COLOR_BLUE, 0);
    lv_obj_align(g_plan_badge, LV_ALIGN_TOP_RIGHT, -margin, 14);

    create_quota_card(&g_primary, g_tile_quota, margin, content_y, card_w, card_h, "Primary", sw);
    create_quota_card(&g_secondary, g_tile_quota, margin + card_w + gap, content_y, card_w, card_h, "Secondary", sw);

    g_live_dot = lv_obj_create(g_tile_quota);
    lv_obj_set_size(g_live_dot, 8, 8);
    lv_obj_set_style_radius(g_live_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(g_live_dot, COLOR_YELLOW, 0);
    lv_obj_set_style_bg_opa(g_live_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_live_dot, 0, 0);
    lv_obj_align(g_live_dot, LV_ALIGN_BOTTOM_LEFT, margin, -28);

    g_status_label = lv_label_create(g_tile_quota);
    lv_label_set_text(g_status_label, "Starting...");
    lv_obj_set_style_text_font(g_status_label, font_for_cjk_text(), 0);
    lv_obj_set_style_text_color(g_status_label, COLOR_DIM, 0);
    lv_obj_set_width(g_status_label, sw - margin * 2 - 20);
    lv_label_set_long_mode(g_status_label, LV_LABEL_LONG_DOT);
    lv_obj_align(g_status_label, LV_ALIGN_BOTTOM_LEFT, margin + 14, -25);

    /* ── Page 1: Diagnostics ─────────────────────── */
    g_tile_diag = lv_tileview_add_tile(g_tileview, 1, 0, LV_DIR_LEFT);

    g_diag_title = lv_label_create(g_tile_diag);
    lv_label_set_text(g_diag_title, "Diagnostics");
    lv_obj_set_style_text_font(g_diag_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(g_diag_title, COLOR_TEXT, 0);
    lv_obj_align(g_diag_title, LV_ALIGN_TOP_LEFT, margin, 10);

    /* WiFi section */
    lv_coord_t section_w = (sw - margin * 2 - gap) / 2;
    lv_coord_t section_h = (content_h - gap) / 2;

    lv_obj_t *wifi_panel = create_diag_section(g_tile_diag, "WiFi",
                                                margin, content_y,
                                                section_w, section_h);
    g_wifi_ssid_label = create_diag_item(wifi_panel, "SSID:", wifi_panel, 28);
    g_wifi_ip_label = create_diag_item(wifi_panel, "IP:", wifi_panel, 50);
    g_wifi_rssi_label = create_diag_item(wifi_panel, "RSSI:", wifi_panel, 72);
    g_wifi_status = create_diag_item(wifi_panel, "Status:", wifi_panel, 94);

    /* MQTT section */
    lv_obj_t *mqtt_panel = create_diag_section(g_tile_diag, "MQTT",
                                                margin + section_w + gap, content_y,
                                                section_w, section_h);
    g_mqtt_host_label = create_diag_item(mqtt_panel, "Broker:", mqtt_panel, 28);
    g_mqtt_msgs_label = create_diag_item(mqtt_panel, "Messages:", mqtt_panel, 50);
    g_mqtt_status = create_diag_item(mqtt_panel, "Status:", mqtt_panel, 72);

    /* System section */
    lv_obj_t *sys_panel = create_diag_section(g_tile_diag, "System",
                                               margin, content_y + section_h + gap,
                                               section_w, section_h);
    g_mem_heap_label = create_diag_item(sys_panel, "Heap:", sys_panel, 28);
    g_mem_psram_label = create_diag_item(sys_panel, "PSRAM:", sys_panel, 50);
    g_uptime_label = create_diag_item(sys_panel, "Uptime:", sys_panel, 72);

    /* Bridge & Stats section */
    lv_obj_t *bridge_panel = create_diag_section(g_tile_diag, "Bridge & Stats",
                                                  margin + section_w + gap, content_y + section_h + gap,
                                                  section_w, section_h);
    g_bridge_label = create_diag_item(bridge_panel, "Server:", bridge_panel, 28);
    g_reconnect_label = create_diag_item(bridge_panel, "Reconnects:", bridge_panel, 50);

    /* ── Page dots ───────────────────────────────── */
    create_page_dots(scr, sw, sh);

    PR_NOTICE("[ui] Multi-page UI created, res=%dx%d, pages=%d",
              (int)sw, (int)sh, CODEX_PAGE_COUNT);
}

void codex_ui_update(const codex_quota_t *quota)
{
    char buf[64];

    snprintf(buf, sizeof(buf), "%s", quota->plan_type[0] ? quota->plan_type : "plan");
    for (char *p = buf; *p; p++) {
        if (*p >= 'a' && *p <= 'z') *p -= 32;
    }
    lv_label_set_text(g_plan_badge, buf);

    update_card(&g_primary, "Primary", &quota->primary);

    if (quota->has_secondary) {
        lv_obj_clear_flag(g_secondary.panel, LV_OBJ_FLAG_HIDDEN);
        update_card(&g_secondary, "Secondary", &quota->secondary);
    } else {
        lv_obj_clear_flag(g_secondary.panel, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(g_secondary.title, "Secondary");
        lv_label_set_text(g_secondary.percent, "--%");
        lv_label_set_text(g_secondary.used, "Not available");
        lv_label_set_text(g_secondary.reset, "");
        animate_arc_to(g_secondary.arc, 0);
        lv_obj_set_style_arc_color(g_secondary.arc, COLOR_RING_BG, LV_PART_INDICATOR);
    }

    snprintf(buf, sizeof(buf), "Live data updated %s", quota->updated_time[0] ? quota->updated_time : "--:--");
    lv_label_set_text(g_status_label, buf);
    lv_obj_set_style_text_color(g_status_label, COLOR_DIM, 0);
    lv_obj_set_style_bg_color(g_live_dot, COLOR_GREEN, 0);
}

void codex_ui_set_status(const char *message)
{
    if (g_status_label == NULL) return;
    lv_label_set_text(g_status_label, message ? message : "Working...");
    lv_obj_set_style_text_color(g_status_label, COLOR_DIM, 0);
    if (g_live_dot != NULL) {
        lv_obj_set_style_bg_color(g_live_dot, COLOR_YELLOW, 0);
    }
}

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

void codex_ui_update_diag(const codex_diag_info_t *info)
{
    char buf[64];

    if (info == NULL) return;

    /* WiFi */
    lv_label_set_text(g_wifi_ssid_label, info->wifi_ssid[0] ? info->wifi_ssid : "N/A");
    lv_label_set_text(g_wifi_ip_label, info->wifi_ip[0] ? info->wifi_ip : "N/A");
    snprintf(buf, sizeof(buf), "%d dBm", info->wifi_rssi);
    lv_label_set_text(g_wifi_rssi_label, buf);
    lv_label_set_text(g_wifi_status, info->wifi_connected ? "Connected" : "Disconnected");
    lv_obj_set_style_text_color(g_wifi_status,
                                info->wifi_connected ? COLOR_GREEN : COLOR_RED, 0);

    /* MQTT */
    snprintf(buf, sizeof(buf), "%s:%u", info->mqtt_host, (unsigned)info->mqtt_port);
    lv_label_set_text(g_mqtt_host_label, buf);
    snprintf(buf, sizeof(buf), "%d", info->mqtt_msg_count);
    lv_label_set_text(g_mqtt_msgs_label, buf);
    lv_label_set_text(g_mqtt_status, info->mqtt_connected ? "Connected" : "Disconnected");
    lv_obj_set_style_text_color(g_mqtt_status,
                                info->mqtt_connected ? COLOR_GREEN : COLOR_RED, 0);

    /* System */
    format_memory(info->heap_total_bytes - info->heap_free_bytes,
                  info->heap_total_bytes, buf, sizeof(buf));
    lv_label_set_text(g_mem_heap_label, buf);

    if (info->psram_total_bytes > 0) {
        format_memory(info->psram_total_bytes - info->psram_free_bytes,
                      info->psram_total_bytes, buf, sizeof(buf));
        lv_label_set_text(g_mem_psram_label, buf);
    } else {
        lv_label_set_text(g_mem_psram_label, "N/A");
    }

    format_uptime(info->uptime_seconds, buf, sizeof(buf));
    lv_label_set_text(g_uptime_label, buf);

    /* Bridge */
    snprintf(buf, sizeof(buf), "%s:%u", info->bridge_host, (unsigned)info->bridge_port);
    lv_label_set_text(g_bridge_label, buf);

    snprintf(buf, sizeof(buf), "WiFi:%d MQTT:%d",
             info->wifi_reconnect_count, info->mqtt_reconnect_count);
    lv_label_set_text(g_reconnect_label, buf);
}

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

int codex_ui_get_current_page(void)
{
    return g_current_page;
}
