#include "ui_internal.h"

#define SLIDE_DURATION_MS  300

static bool s_animating = false;

static void slide_anim_cb(void *var, int32_t val)
{
    lv_obj_set_style_translate_x((lv_obj_t *)var, val, 0);
}

static void slide_anim_done_cb(lv_anim_t *a)
{
    s_animating = false;
}

void ui_transition_slide(lv_obj_t *hero, bool slide_left)
{
    if (s_animating || !hero) return;
    s_animating = true;

    int start = slide_left ? -50 : 50;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, hero);
    lv_anim_set_values(&a, start, 0);
    lv_anim_set_time(&a, SLIDE_DURATION_MS);
    lv_anim_set_exec_cb(&a, slide_anim_cb);
    lv_anim_set_completed_cb(&a, slide_anim_done_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}
