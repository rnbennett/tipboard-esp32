#pragma once

#include "lvgl.h"
#include "state.h"

/* Color scheme for a single mode */
typedef struct {
    lv_color_t primary;           /* Main accent (header bar, highlights) */
    lv_color_t primary_dim;       /* Dimmed variant for gradients */
    lv_color_t bg_gradient_start; /* Background gradient start */
    lv_color_t bg_gradient_end;   /* Background gradient end */
    lv_color_t text_primary;      /* Main text color */
    lv_color_t text_secondary;    /* Subtitle/secondary text */
    lv_color_t geo_tint;          /* Geodesic pattern tint */
    uint8_t geo_opacity;          /* Geodesic pattern opacity (0-255) */
} mode_color_scheme_t;

/* Get color scheme for a mode */
const mode_color_scheme_t *ui_theme_get_scheme(status_mode_t mode);

/* Get interpolated Pomodoro scheme (red for work, green for break) */
mode_color_scheme_t ui_theme_get_pomo_scheme(pomo_phase_t phase);

/* Deep cosmic blue-purple base background */
#define UI_COLOR_BG_BASE      lv_color_hex(0x0D0B1A)
#define UI_COLOR_BG_COSMIC    lv_color_hex(0x1A1035)
#define UI_COLOR_DIVIDER      lv_color_hex(0x2A2545)
#define UI_COLOR_TEXT_WHITE    lv_color_hex(0xF0F0F0)
#define UI_COLOR_TEXT_DIM      lv_color_hex(0x8888AA)
