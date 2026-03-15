#include "ui_theme.h"

/* Runtime-initialized scheme table.
 * lv_color_hex() can't be used in static initializers, so we init on first use. */
static mode_color_scheme_t s_schemes[MODE_COUNT];
static bool s_initialized = false;

static void init_schemes(void)
{
    if (s_initialized) return;

    /* Available — Monorail Green */
    s_schemes[MODE_AVAILABLE] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0x44CC00),
        .primary_dim       = lv_color_hex(0x228800),
        .bg_gradient_start = lv_color_hex(0x0D201A),
        .bg_gradient_end   = lv_color_hex(0x0A3515),
        .text_primary      = lv_color_hex(0xF0F0F0),
        .text_secondary    = lv_color_hex(0x88CC88),
        .geo_tint          = lv_color_hex(0x44CC00),
        .geo_opacity       = 38,
    };

    /* Focused — Monorail Gold */
    s_schemes[MODE_FOCUSED] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0xFFA800),
        .primary_dim       = lv_color_hex(0xCC7000),
        .bg_gradient_start = lv_color_hex(0x1A1810),
        .bg_gradient_end   = lv_color_hex(0x352208),
        .text_primary      = lv_color_hex(0xF0F0F0),
        .text_secondary    = lv_color_hex(0xDDAA66),
        .geo_tint          = lv_color_hex(0xFFA800),
        .geo_opacity       = 25,
    };

    /* Meeting — Monorail Red */
    s_schemes[MODE_MEETING] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0xEE3322),
        .primary_dim       = lv_color_hex(0xAA2211),
        .bg_gradient_start = lv_color_hex(0x1A1012),
        .bg_gradient_end   = lv_color_hex(0x350A08),
        .text_primary      = lv_color_hex(0xF0F0F0),
        .text_secondary    = lv_color_hex(0xDD8888),
        .geo_tint          = lv_color_hex(0xEE3322),
        .geo_opacity       = 30,
    };

    /* Away — Monorail Silver */
    s_schemes[MODE_AWAY] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0x8899AA),
        .primary_dim       = lv_color_hex(0x556677),
        .bg_gradient_start = lv_color_hex(0x121518),
        .bg_gradient_end   = lv_color_hex(0x181E22),
        .text_primary      = lv_color_hex(0xDDDDDD),
        .text_secondary    = lv_color_hex(0x778899),
        .geo_tint          = lv_color_hex(0x8899AA),
        .geo_opacity       = 18,
    };

    /* Pomodoro — starts Red (work phase uses this, break phase uses Available scheme) */
    s_schemes[MODE_POMODORO] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0xEE3322),
        .primary_dim       = lv_color_hex(0xAA2211),
        .bg_gradient_start = lv_color_hex(0x1A1012),
        .bg_gradient_end   = lv_color_hex(0x350A08),
        .text_primary      = lv_color_hex(0xF0F0F0),
        .text_secondary    = lv_color_hex(0xDD8888),
        .geo_tint          = lv_color_hex(0xEE3322),
        .geo_opacity       = 25,
    };

    /* Custom — Monorail Coral */
    s_schemes[MODE_CUSTOM] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0xFF7777),
        .primary_dim       = lv_color_hex(0xCC5544),
        .bg_gradient_start = lv_color_hex(0x1A1015),
        .bg_gradient_end   = lv_color_hex(0x350C12),
        .text_primary      = lv_color_hex(0xF0F0F0),
        .text_secondary    = lv_color_hex(0xEE99AA),
        .geo_tint          = lv_color_hex(0xFF7777),
        .geo_opacity       = 30,
    };

    /* Streaming — Monorail Coral (brighter) */
    s_schemes[MODE_STREAMING] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0xFF3355),
        .primary_dim       = lv_color_hex(0xDD2233),
        .bg_gradient_start = lv_color_hex(0x1A0C18),
        .bg_gradient_end   = lv_color_hex(0x380812),
        .text_primary      = lv_color_hex(0xFFFFFF),
        .text_secondary    = lv_color_hex(0xFF7788),
        .geo_tint          = lv_color_hex(0xFF3355),
        .geo_opacity       = 38,
    };

    s_initialized = true;
}

const mode_color_scheme_t *ui_theme_get_scheme(status_mode_t mode)
{
    init_schemes();
    if (mode >= MODE_COUNT) mode = MODE_AVAILABLE;
    return &s_schemes[mode];
}

mode_color_scheme_t ui_theme_get_pomo_scheme(pomo_phase_t phase)
{
    init_schemes();
    if (phase == POMO_BREAK) {
        return s_schemes[MODE_AVAILABLE]; /* Green for break */
    }
    return s_schemes[MODE_POMODORO]; /* Red for work */
}
