#include "ui.h"
#include "ui_theme.h"
#include "ui_internal.h"
#include "esp_log.h"
#include <stdio.h>

/* Prototype font — the classic 1982 Epcot geometric typeface */
LV_FONT_DECLARE(font_prototype_96);
LV_FONT_DECLARE(font_prototype_72);
LV_FONT_DECLARE(font_prototype_48);
LV_FONT_DECLARE(font_prototype_28);
LV_FONT_DECLARE(font_prototype_20);

static const char *TAG = "ui_screen";

/* Screen dimensions */
#define SCREEN_W  1024
#define SCREEN_H  600

/* Zone heights */
#define TOP_BAR_H      40
#define BOTTOM_BAR_H   40
#define STRIPE_H       20    /* Monorail-style color stripe */
#define HERO_H         (SCREEN_H - TOP_BAR_H - BOTTOM_BAR_H)

/* Screen objects */
static lv_obj_t *s_screen = NULL;

/* Top bar */
static lv_obj_t *s_top_bar = NULL;
static lv_obj_t *s_time_label = NULL;
static lv_obj_t *s_weather_label = NULL;

/* Hero zone */
static lv_obj_t *s_hero = NULL;
static lv_obj_t *s_stripe_top = NULL;     /* Monorail color stripe */
static lv_obj_t *s_stripe_bottom = NULL;  /* Matching bottom stripe */
static lv_obj_t *s_mode_label = NULL;
static lv_obj_t *s_subtitle_label = NULL;
static lv_obj_t *s_timer_label = NULL;

/* Bottom bar */
static lv_obj_t *s_bottom_bar = NULL;
static lv_obj_t *s_pomo_label = NULL;

/* Dividers */
static lv_obj_t *s_divider_top = NULL;
static lv_obj_t *s_divider_bottom = NULL;

/* Current mode for change detection */
static status_mode_t s_current_mode = MODE_COUNT;

static void create_top_bar(lv_obj_t *parent)
{
    s_top_bar = lv_obj_create(parent);
    lv_obj_set_size(s_top_bar, SCREEN_W, TOP_BAR_H);
    lv_obj_set_pos(s_top_bar, 0, 0);
    lv_obj_set_style_bg_color(s_top_bar, UI_COLOR_BG_BASE, 0);
    lv_obj_set_style_bg_opa(s_top_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_top_bar, 0, 0);
    lv_obj_set_style_radius(s_top_bar, 0, 0);
    lv_obj_set_style_pad_left(s_top_bar, 16, 0);
    lv_obj_set_style_pad_right(s_top_bar, 16, 0);
    lv_obj_set_scrollbar_mode(s_top_bar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(s_top_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* Time/date (left) — placeholder until NTP in Phase 2 */
    s_time_label = lv_label_create(s_top_bar);
    lv_label_set_text(s_time_label, "--:--");
    lv_obj_set_style_text_color(s_time_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_time_label, &font_prototype_20, 0);
    lv_obj_align(s_time_label, LV_ALIGN_LEFT_MID, 0, 0);

    /* Weather (right) — placeholder until Phase 2 */
    s_weather_label = lv_label_create(s_top_bar);
    lv_label_set_text(s_weather_label, "");
    lv_obj_set_style_text_color(s_weather_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_weather_label, &font_prototype_20, 0);
    lv_obj_align(s_weather_label, LV_ALIGN_RIGHT_MID, 0, 0);
}

static void create_hero_zone(lv_obj_t *parent)
{
    s_hero = lv_obj_create(parent);
    lv_obj_set_size(s_hero, SCREEN_W, HERO_H);
    lv_obj_set_pos(s_hero, 0, TOP_BAR_H);
    lv_obj_set_style_bg_color(s_hero, UI_COLOR_BG_COSMIC, 0);
    lv_obj_set_style_bg_opa(s_hero, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_hero, 0, 0);
    lv_obj_set_style_radius(s_hero, 0, 0);
    lv_obj_set_style_pad_all(s_hero, 0, 0);
    lv_obj_set_scrollbar_mode(s_hero, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_clip_corner(s_hero, true, 0);

    /* Geodesic pattern (created first so it's behind everything) */
    ui_geodesic_create(s_hero, SCREEN_W, HERO_H);

    /* Monorail color stripe — top (iconic WDW monorail identity band) */
    s_stripe_top = lv_obj_create(s_hero);
    lv_obj_set_size(s_stripe_top, SCREEN_W, STRIPE_H);
    lv_obj_set_pos(s_stripe_top, 0, 0);
    lv_obj_set_style_bg_color(s_stripe_top, lv_color_hex(0x44CC00), 0);
    lv_obj_set_style_bg_opa(s_stripe_top, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_stripe_top, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(s_stripe_top, 2, 0);
    lv_obj_set_style_border_side(s_stripe_top, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_radius(s_stripe_top, 0, 0);
    lv_obj_set_scrollbar_mode(s_stripe_top, LV_SCROLLBAR_MODE_OFF);

    /* Monorail color stripe — bottom (matching band) */
    s_stripe_bottom = lv_obj_create(s_hero);
    lv_obj_set_size(s_stripe_bottom, SCREEN_W, STRIPE_H);
    lv_obj_set_pos(s_stripe_bottom, 0, HERO_H - STRIPE_H);
    lv_obj_set_style_bg_color(s_stripe_bottom, lv_color_hex(0x44CC00), 0);
    lv_obj_set_style_bg_opa(s_stripe_bottom, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_stripe_bottom, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(s_stripe_bottom, 2, 0);
    lv_obj_set_style_border_side(s_stripe_bottom, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_radius(s_stripe_bottom, 0, 0);
    lv_obj_set_scrollbar_mode(s_stripe_bottom, LV_SCROLLBAR_MODE_OFF);

    /* Timer arc — created early so it sits behind text labels (z-order) */
    ui_timer_arc_create(s_hero);

    /* Mode name — Prototype 72px, the classic Epcot geometric typeface */
    s_mode_label = lv_label_create(s_hero);
    lv_label_set_text(s_mode_label, "AVAILABLE");
    lv_obj_set_style_text_font(s_mode_label, &font_prototype_96, 0);
    lv_obj_set_style_text_color(s_mode_label, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_letter_space(s_mode_label, 12, 0);
    lv_obj_align(s_mode_label, LV_ALIGN_CENTER, 0, -40);

    /* Subtitle — Prototype 28px */
    s_subtitle_label = lv_label_create(s_hero);
    lv_label_set_text(s_subtitle_label, "");
    lv_obj_set_style_text_font(s_subtitle_label, &font_prototype_48, 0);
    lv_obj_set_style_text_color(s_subtitle_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(s_subtitle_label, LV_ALIGN_CENTER, 0, 30);
    lv_obj_add_flag(s_subtitle_label, LV_OBJ_FLAG_HIDDEN);

    /* Timer display — Prototype 48, hidden by default (only shown during Pomodoro/countdown) */
    s_timer_label = lv_label_create(s_hero);
    lv_label_set_text(s_timer_label, "");
    lv_obj_set_style_text_font(s_timer_label, &font_prototype_48, 0);
    lv_obj_set_style_text_color(s_timer_label, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_opa(s_timer_label, LV_OPA_80, 0);
    lv_obj_align(s_timer_label, LV_ALIGN_CENTER, 0, 100);
    lv_obj_add_flag(s_timer_label, LV_OBJ_FLAG_HIDDEN);
}

static void create_bottom_bar(lv_obj_t *parent)
{
    s_bottom_bar = lv_obj_create(parent);
    lv_obj_set_size(s_bottom_bar, SCREEN_W, BOTTOM_BAR_H);
    lv_obj_set_pos(s_bottom_bar, 0, SCREEN_H - BOTTOM_BAR_H);
    lv_obj_set_style_bg_color(s_bottom_bar, UI_COLOR_BG_BASE, 0);
    lv_obj_set_style_bg_opa(s_bottom_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_bottom_bar, 0, 0);
    lv_obj_set_style_radius(s_bottom_bar, 0, 0);
    lv_obj_set_style_pad_left(s_bottom_bar, 16, 0);
    lv_obj_set_style_pad_right(s_bottom_bar, 16, 0);
    lv_obj_set_scrollbar_mode(s_bottom_bar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(s_bottom_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* Pomodoro trigger label (repurposed from calendar event until Phase 3) */
    s_pomo_label = lv_label_create(s_bottom_bar);
    lv_label_set_text(s_pomo_label, "Tap for Pomodoro (25:00)");
    lv_obj_set_style_text_color(s_pomo_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_pomo_label, &font_prototype_20, 0);
    lv_obj_align(s_pomo_label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_flag(s_bottom_bar, LV_OBJ_FLAG_CLICKABLE);
}

static void create_dividers(lv_obj_t *parent)
{
    s_divider_top = lv_obj_create(parent);
    lv_obj_set_size(s_divider_top, SCREEN_W, 1);
    lv_obj_set_pos(s_divider_top, 0, TOP_BAR_H);
    lv_obj_set_style_bg_color(s_divider_top, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_bg_opa(s_divider_top, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_divider_top, 0, 0);
    lv_obj_set_style_radius(s_divider_top, 0, 0);
    lv_obj_set_scrollbar_mode(s_divider_top, LV_SCROLLBAR_MODE_OFF);

    s_divider_bottom = lv_obj_create(parent);
    lv_obj_set_size(s_divider_bottom, SCREEN_W, 1);
    lv_obj_set_pos(s_divider_bottom, 0, SCREEN_H - BOTTOM_BAR_H);
    lv_obj_set_style_bg_color(s_divider_bottom, UI_COLOR_DIVIDER, 0);
    lv_obj_set_style_bg_opa(s_divider_bottom, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_divider_bottom, 0, 0);
    lv_obj_set_style_radius(s_divider_bottom, 0, 0);
    lv_obj_set_scrollbar_mode(s_divider_bottom, LV_SCROLLBAR_MODE_OFF);
}

/* ── Touch event handlers ── */

static void hero_tap_cb(lv_event_t *e)
{
    const status_state_t *state = state_get();
    status_mode_t next = (state->mode + 1) % MODE_COUNT;

    /* Skip POMODORO in tap cycle (it's started explicitly via bottom bar) */
    if (next == MODE_POMODORO) {
        next = (next + 1) % MODE_COUNT;
    }

    state_set_mode(next, SOURCE_MANUAL);
}

static void hero_gesture_cb(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    const status_state_t *state = state_get();
    status_mode_t next;

    if (dir == LV_DIR_LEFT) {
        next = (state->mode + 1) % MODE_COUNT;
        if (next == MODE_POMODORO) next = (next + 1) % MODE_COUNT;
    } else if (dir == LV_DIR_RIGHT) {
        next = (state->mode + MODE_COUNT - 1) % MODE_COUNT;
        if (next == MODE_POMODORO) next = (next + MODE_COUNT - 1) % MODE_COUNT;
    } else {
        return;
    }

    state_set_mode(next, SOURCE_MANUAL);
}

static void hero_longpress_cb(lv_event_t *e)
{
    ESP_LOGI(TAG, "Long press detected — settings overlay (Phase 2+)");
}

static void bottom_bar_tap_cb(lv_event_t *e)
{
    const status_state_t *state = state_get();
    if (state->mode == MODE_POMODORO) {
        state_pomodoro_cancel();
    } else {
        state_pomodoro_start(state->pomo_work_sec, state->pomo_break_sec);
    }
}

/* ── Public API ── */

esp_err_t ui_init(void)
{
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, UI_COLOR_BG_BASE, 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(s_screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    create_top_bar(s_screen);
    create_hero_zone(s_screen);
    create_bottom_bar(s_screen);
    create_dividers(s_screen);

    /* Touch handlers on hero zone */
    lv_obj_add_flag(s_hero, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(s_hero, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_hero, hero_tap_cb, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(s_hero, hero_gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(s_hero, hero_longpress_cb, LV_EVENT_LONG_PRESSED, NULL);

    /* Pomodoro trigger on bottom bar */
    lv_obj_add_event_cb(s_bottom_bar, bottom_bar_tap_cb, LV_EVENT_SHORT_CLICKED, NULL);

    lv_scr_load(s_screen);

    ESP_LOGI(TAG, "UI initialized: 3-zone layout on %dx%d", SCREEN_W, SCREEN_H);
    return ESP_OK;
}

void ui_update(const status_state_t *state)
{
    if (!state || !s_screen) return;

    /* Get color scheme — Pomodoro uses phase-dependent colors */
    const mode_color_scheme_t *scheme;
    mode_color_scheme_t pomo_scheme;
    if (state->mode == MODE_POMODORO) {
        pomo_scheme = ui_theme_get_pomo_scheme(state->pomo_phase);
        scheme = &pomo_scheme;
    } else {
        scheme = ui_theme_get_scheme(state->mode);
    }

    /* Mode label */
    lv_label_set_text(s_mode_label, state_mode_label(state->mode));

    /* Subtitle */
    if (state->subtitle[0] != '\0') {
        lv_label_set_text(s_subtitle_label, state->subtitle);
        lv_obj_remove_flag(s_subtitle_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_subtitle_label, LV_OBJ_FLAG_HIDDEN);
    }

    /* Monorail stripe colors + glow */
    lv_obj_set_style_bg_color(s_stripe_top, scheme->primary, 0);
    lv_obj_set_style_shadow_color(s_stripe_top, scheme->primary, 0);
    lv_obj_set_style_bg_color(s_stripe_bottom, scheme->primary, 0);
    lv_obj_set_style_shadow_color(s_stripe_bottom, scheme->primary, 0);

    /* Hero background gradient */
    lv_obj_set_style_bg_color(s_hero, scheme->bg_gradient_start, 0);
    lv_obj_set_style_bg_grad_color(s_hero, scheme->bg_gradient_end, 0);
    lv_obj_set_style_bg_grad_dir(s_hero, LV_GRAD_DIR_VER, 0);

    /* Text colors */
    lv_obj_set_style_text_color(s_mode_label, scheme->text_primary, 0);
    lv_obj_set_style_text_color(s_subtitle_label, scheme->text_secondary, 0);

    /* Divider colors */
    lv_obj_set_style_bg_color(s_divider_top, scheme->primary_dim, 0);
    lv_obj_set_style_bg_opa(s_divider_top, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(s_divider_bottom, scheme->primary_dim, 0);
    lv_obj_set_style_bg_opa(s_divider_bottom, LV_OPA_30, 0);

    /* Geodesic pattern tint (only on mode change) */
    if (state->mode != s_current_mode) {
        ui_geodesic_update(scheme->geo_tint, scheme->geo_opacity);
        ui_transition_slide(s_hero, state->mode > s_current_mode);
    }

    /* Bottom bar Pomodoro text */
    if (state->mode == MODE_POMODORO) {
        if (state->pomo_phase == POMO_WORK) {
            lv_label_set_text(s_pomo_label, "Working... (tap to cancel)");
        } else if (state->pomo_phase == POMO_BREAK) {
            lv_label_set_text(s_pomo_label, "Break time! (tap to cancel)");
        }
    } else {
        lv_label_set_text(s_pomo_label, "Tap for Pomodoro (25:00)");
    }

    s_current_mode = state->mode;
}

void ui_update_timer(int32_t seconds, timer_type_t type)
{
    if (!s_timer_label) return;

    /* Only show timer during countdown (Pomodoro, custom countdown).
     * No timer displayed for TIMER_NONE or TIMER_ELAPSED. */
    if (type != TIMER_COUNTDOWN || seconds <= 0) {
        lv_obj_add_flag(s_timer_label, LV_OBJ_FLAG_HIDDEN);
        ui_timer_arc_update(0, lv_color_black(), false);
        return;
    }

    /* Show countdown timer */
    lv_obj_remove_flag(s_timer_label, LV_OBJ_FLAG_HIDDEN);

    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;

    if (h > 0) {
        lv_label_set_text_fmt(s_timer_label, "%d:%02d:%02d", h, m, s);
    } else {
        lv_label_set_text_fmt(s_timer_label, "%d:%02d", m, s);
    }

    /* Timer arc for countdown */
    const status_state_t *state = state_get();
    if (state->timer_duration_sec > 0) {
        int32_t pct = 100 - ((seconds * 100) / state->timer_duration_sec);
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        const mode_color_scheme_t *scheme = ui_theme_get_scheme(state->mode);
        ui_timer_arc_update(pct, scheme->primary, true);
    }

    /* Position timer inside arc */
    lv_obj_align(s_timer_label, LV_ALIGN_CENTER, 0, 40);
}

lv_obj_t *ui_get_screen(void)
{
    return s_screen;
}
