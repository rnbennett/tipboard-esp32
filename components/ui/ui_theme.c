#include "ui_theme.h"

/* Runtime-initialized scheme table.
 * Colors sampled from actual WDW Monorail fleet reference image.
 * lv_color_hex() can't be used in static initializers, so we init on first use. */
static mode_color_scheme_t s_schemes[MODE_COUNT];
static bool s_initialized = false;

static void init_schemes(void)
{
    if (s_initialized) return;

    /* Available — Monorail Green (#00A84E) */
    s_schemes[MODE_AVAILABLE] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0x00A84E),
        .primary_dim       = lv_color_hex(0x007A38),
        .bg_gradient_start = lv_color_hex(0x143828),
        .bg_gradient_end   = lv_color_hex(0x1A4E35),
        .text_primary      = lv_color_hex(0xF0F0F0),
        .text_secondary    = lv_color_hex(0x80CC99),
        .geo_tint          = lv_color_hex(0x00A84E),
        .geo_opacity       = 38,
    };

    /* Focused — Monorail Gold (#C7A94E) */
    s_schemes[MODE_FOCUSED] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0xC7A94E),
        .primary_dim       = lv_color_hex(0x8E7A38),
        .bg_gradient_start = lv_color_hex(0x2A2414),
        .bg_gradient_end   = lv_color_hex(0x3D3518),
        .text_primary      = lv_color_hex(0xF0F0F0),
        .text_secondary    = lv_color_hex(0xD4BB70),
        .geo_tint          = lv_color_hex(0xC7A94E),
        .geo_opacity       = 25,
    };

    /* Meeting — Monorail Red (#E4272C) */
    s_schemes[MODE_MEETING] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0xE4272C),
        .primary_dim       = lv_color_hex(0xA01C20),
        .bg_gradient_start = lv_color_hex(0x2D1010),
        .bg_gradient_end   = lv_color_hex(0x451818),
        .text_primary      = lv_color_hex(0xF0F0F0),
        .text_secondary    = lv_color_hex(0xDD8888),
        .geo_tint          = lv_color_hex(0xE4272C),
        .geo_opacity       = 30,
    };

    /* Away — Monorail Blue (#0065C1) (MonorailBlue!) */
    s_schemes[MODE_AWAY] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0x0065C1),
        .primary_dim       = lv_color_hex(0x004A8E),
        .bg_gradient_start = lv_color_hex(0x101828),
        .bg_gradient_end   = lv_color_hex(0x152545),
        .text_primary      = lv_color_hex(0xF0F0F0),
        .text_secondary    = lv_color_hex(0x77AADD),
        .geo_tint          = lv_color_hex(0x0065C1),
        .geo_opacity       = 25,
    };

    /* Pomodoro — Red (work) → Green (break) */
    s_schemes[MODE_POMODORO] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0xE4272C),
        .primary_dim       = lv_color_hex(0xA01C20),
        .bg_gradient_start = lv_color_hex(0x2D1010),
        .bg_gradient_end   = lv_color_hex(0x451818),
        .text_primary      = lv_color_hex(0xF0F0F0),
        .text_secondary    = lv_color_hex(0xDD8888),
        .geo_tint          = lv_color_hex(0xE4272C),
        .geo_opacity       = 25,
    };

    /* Custom — Monorail Teal (#00B5B8) (MonorailTeal!) */
    s_schemes[MODE_CUSTOM] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0x00B5B8),
        .primary_dim       = lv_color_hex(0x008285),
        .bg_gradient_start = lv_color_hex(0x102D2D),
        .bg_gradient_end   = lv_color_hex(0x184545),
        .text_primary      = lv_color_hex(0xF0F0F0),
        .text_secondary    = lv_color_hex(0x77DDDD),
        .geo_tint          = lv_color_hex(0x00B5B8),
        .geo_opacity       = 30,
    };

    /* Streaming / On Air — Monorail Silver (#A8A9AD) */
    s_schemes[MODE_STREAMING] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0xA8A9AD),
        .primary_dim       = lv_color_hex(0x78797D),
        .bg_gradient_start = lv_color_hex(0x202224),
        .bg_gradient_end   = lv_color_hex(0x303335),
        .text_primary      = lv_color_hex(0xFFFFFF),
        .text_secondary    = lv_color_hex(0xBBBBCC),
        .geo_tint          = lv_color_hex(0xA8A9AD),
        .geo_opacity       = 22,
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
