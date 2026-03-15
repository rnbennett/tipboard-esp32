#include "ui_theme.h"

/* Runtime-initialized scheme table.
 * lv_color_hex() can't be used in static initializers, so we init on first use. */
static mode_color_scheme_t s_schemes[MODE_COUNT];
static bool s_initialized = false;

static void init_schemes(void)
{
    if (s_initialized) return;

    /* Available — Monorail Green
     * Bright welcoming green, lighter background */
    s_schemes[MODE_AVAILABLE] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0x44CC00),
        .primary_dim       = lv_color_hex(0x228800),
        .bg_gradient_start = lv_color_hex(0x0A2A12),
        .bg_gradient_end   = lv_color_hex(0x0F3D1A),
        .text_primary      = lv_color_hex(0xF0F0F0),
        .text_secondary    = lv_color_hex(0x88CC88),
        .geo_tint          = lv_color_hex(0x44CC00),
        .geo_opacity       = 38,
    };

    /* Focused — Monorail Gold
     * Rich gold from actual monorail (deeper than pure yellow) */
    s_schemes[MODE_FOCUSED] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0xD4A017),
        .primary_dim       = lv_color_hex(0x9A7510),
        .bg_gradient_start = lv_color_hex(0x1A1608),
        .bg_gradient_end   = lv_color_hex(0x2A220C),
        .text_primary      = lv_color_hex(0xF0F0F0),
        .text_secondary    = lv_color_hex(0xDDB855),
        .geo_tint          = lv_color_hex(0xD4A017),
        .geo_opacity       = 25,
    };

    /* Meeting — Monorail Red
     * Bold red with dark angular delta feel */
    s_schemes[MODE_MEETING] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0xEE3322),
        .primary_dim       = lv_color_hex(0xAA2211),
        .bg_gradient_start = lv_color_hex(0x1A0808),
        .bg_gradient_end   = lv_color_hex(0x2D0E0A),
        .text_primary      = lv_color_hex(0xF0F0F0),
        .text_secondary    = lv_color_hex(0xDD8888),
        .geo_tint          = lv_color_hex(0xEE3322),
        .geo_opacity       = 30,
    };

    /* Away — Monorail Blue (MonorailBlue!)
     * Bold blue from the actual WDW blue monorail */
    s_schemes[MODE_AWAY] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0x0088DD),
        .primary_dim       = lv_color_hex(0x0066AA),
        .bg_gradient_start = lv_color_hex(0x081018),
        .bg_gradient_end   = lv_color_hex(0x0C1A2D),
        .text_primary      = lv_color_hex(0xFFFFFF),
        .text_secondary    = lv_color_hex(0x77BBEE),
        .geo_tint          = lv_color_hex(0x0088DD),
        .geo_opacity       = 25,
    };

    /* Pomodoro — Red (work) → Green (break) */
    s_schemes[MODE_POMODORO] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0xEE3322),
        .primary_dim       = lv_color_hex(0xAA2211),
        .bg_gradient_start = lv_color_hex(0x1A0808),
        .bg_gradient_end   = lv_color_hex(0x2D0E0A),
        .text_primary      = lv_color_hex(0xF0F0F0),
        .text_secondary    = lv_color_hex(0xDD8888),
        .geo_tint          = lv_color_hex(0xEE3322),
        .geo_opacity       = 25,
    };

    /* Custom — Monorail Teal (MonorailTeal!)
     * Vibrant teal from the actual WDW teal monorail */
    s_schemes[MODE_CUSTOM] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0x00BFA5),
        .primary_dim       = lv_color_hex(0x008C78),
        .bg_gradient_start = lv_color_hex(0x081A18),
        .bg_gradient_end   = lv_color_hex(0x0C2D28),
        .text_primary      = lv_color_hex(0xF0F0F0),
        .text_secondary    = lv_color_hex(0x77DDCC),
        .geo_tint          = lv_color_hex(0x00BFA5),
        .geo_opacity       = 30,
    };

    /* Streaming / On Air — Monorail Silver
     * Cool neutral silver — clean, professional on-air look */
    s_schemes[MODE_STREAMING] = (mode_color_scheme_t){
        .primary           = lv_color_hex(0x8899AA),
        .primary_dim       = lv_color_hex(0x556677),
        .bg_gradient_start = lv_color_hex(0x121518),
        .bg_gradient_end   = lv_color_hex(0x1A2028),
        .text_primary      = lv_color_hex(0xFFFFFF),
        .text_secondary    = lv_color_hex(0x99AABB),
        .geo_tint          = lv_color_hex(0x8899AA),
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
