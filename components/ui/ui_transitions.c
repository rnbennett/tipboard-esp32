#include "ui_internal.h"

/* Use opacity fade instead of translate_x — much cheaper on embedded
 * since translate forces full redraw of all children (126 geodesic lines). */
#define FADE_DURATION_MS  200

static bool s_animating = false;

static void fade_in_cb(void *var, int32_t val)
{
    lv_obj_set_style_opa((lv_obj_t *)var, val, 0);
}

static void fade_done_cb(lv_anim_t *a)
{
    lv_obj_t *obj = (lv_obj_t *)a->var;
    lv_obj_set_style_opa(obj, LV_OPA_COVER, 0);
    s_animating = false;
}

void ui_transition_slide(lv_obj_t *hero, bool slide_left)
{
    (void)slide_left; /* direction not relevant for fade */
    if (s_animating || !hero) return;
    s_animating = true;

    /* Quick fade: drop to 60% opacity then fade back to 100% */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, hero);
    lv_anim_set_values(&a, LV_OPA_60, LV_OPA_COVER);
    lv_anim_set_time(&a, FADE_DURATION_MS);
    lv_anim_set_exec_cb(&a, fade_in_cb);
    lv_anim_set_completed_cb(&a, fade_done_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}
