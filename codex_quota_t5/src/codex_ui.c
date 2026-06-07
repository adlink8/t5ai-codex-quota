/**
 * @file codex_ui.c
 * @brief Codex quota dashboard UI for T5AI-Board LVGL.
 */

#include "codex_ui.h"
#include "codex_http.h"
#include "lvgl.h"
#include "tal_log.h"
#include <stdio.h>
#include <string.h>

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

typedef struct {
    lv_obj_t *panel;
    lv_obj_t *arc;
    lv_obj_t *percent;
    lv_obj_t *title;
    lv_obj_t *used;
    lv_obj_t *reset;
} quota_card_t;

static lv_obj_t *g_root;
static lv_obj_t *g_title;
static lv_obj_t *g_plan_badge;
static lv_obj_t *g_status_label;
static lv_obj_t *g_live_dot;
static quota_card_t g_primary;
static quota_card_t g_secondary;

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

static const lv_font_t *font_for_title(lv_coord_t screen_w)
{
    return screen_w >= 440 ? &lv_font_montserrat_20 : &lv_font_montserrat_16;
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

static void format_label_ascii(const char *label, const char *fallback,
                               char *out, size_t out_size)
{
    int value = 0;

    if (label != NULL && sscanf(label, "%d", &value) == 1 && value > 0) {
        if (strstr(label, "小时") != NULL) {
            snprintf(out, out_size, "%dh", value);
            return;
        }
        if (strstr(label, "天") != NULL) {
            snprintf(out, out_size, "%dd", value);
            return;
        }
    }

    if (is_ascii_text(label)) {
        snprintf(out, out_size, "%s", label);
    } else {
        snprintf(out, out_size, "%s", fallback);
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
    lv_obj_set_style_text_font(card->title, font_for_title(screen_w), 0);
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
    lv_obj_set_style_text_font(card->used, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(card->used, COLOR_DIM, 0);
    lv_obj_align(card->used, LV_ALIGN_BOTTOM_MID, 0, -28);

    card->reset = lv_label_create(card->panel);
    lv_label_set_text(card->reset, "Reset --");
    lv_obj_set_style_text_font(card->reset, &lv_font_montserrat_14, 0);
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

    format_label_ascii(window->label, fallback_title, ascii, sizeof(ascii));
    lv_label_set_text(card->title, ascii);

    snprintf(buf, sizeof(buf), "Used %.0f%%", window->used);
    lv_label_set_text(card->used, buf);

    format_duration_ascii(window->resets_in, ascii, sizeof(ascii));
    snprintf(buf, sizeof(buf), "Reset %s", ascii);
    lv_label_set_text(card->reset, buf);
}

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
    lv_coord_t card_y = header_h + margin;
    lv_coord_t card_h = sh - header_h - status_h - (margin * 3);
    lv_coord_t card_w = (sw - margin * 2 - gap) / 2;

    g_root = lv_obj_create(scr);
    lv_obj_set_size(g_root, sw, sh);
    lv_obj_set_pos(g_root, 0, 0);
    lv_obj_set_style_bg_color(g_root, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(g_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_root, 0, 0);
    lv_obj_set_style_pad_all(g_root, 0, 0);
    lv_obj_clear_flag(g_root, LV_OBJ_FLAG_SCROLLABLE);

    g_title = lv_label_create(g_root);
    lv_label_set_text(g_title, "Codex Quota");
    lv_obj_set_style_text_font(g_title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(g_title, COLOR_TEXT, 0);
    lv_obj_align(g_title, LV_ALIGN_TOP_LEFT, margin, 10);

    g_plan_badge = lv_label_create(g_root);
    lv_label_set_text(g_plan_badge, "...");
    lv_obj_set_style_text_font(g_plan_badge, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(g_plan_badge, COLOR_BLUE, 0);
    lv_obj_align(g_plan_badge, LV_ALIGN_TOP_RIGHT, -margin, 14);

    create_quota_card(&g_primary, g_root, margin, card_y, card_w, card_h, "Primary", sw);
    create_quota_card(&g_secondary, g_root, margin + card_w + gap, card_y, card_w, card_h, "Secondary", sw);

    g_live_dot = lv_obj_create(g_root);
    lv_obj_set_size(g_live_dot, 8, 8);
    lv_obj_set_style_radius(g_live_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(g_live_dot, COLOR_YELLOW, 0);
    lv_obj_set_style_bg_opa(g_live_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_live_dot, 0, 0);
    lv_obj_align(g_live_dot, LV_ALIGN_BOTTOM_LEFT, margin, -10);

    g_status_label = lv_label_create(g_root);
    lv_label_set_text(g_status_label, "Starting...");
    lv_obj_set_style_text_font(g_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(g_status_label, COLOR_DIM, 0);
    lv_obj_align(g_status_label, LV_ALIGN_BOTTOM_LEFT, margin + 14, -7);

    PR_NOTICE("[ui] Codex landscape UI created, res=%dx%d",
              (int)sw, (int)sh);
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
