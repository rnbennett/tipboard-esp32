#include "ui_internal.h"

/* Two-phase crossfade: fade out (150ms) then fade in (250ms)
 * with ease-in-out for a smooth, polished feel. */
#define FADE_OUT_MS  150
#define FADE_IN_MS   250

static bool s_animating = false;

static void fade_in_cb(void *var, int32_t val)
{
    lv_obj_set_style_opa((lv_obj_t *)var, val, 0);
}

static void fade_in_done_cb(lv_anim_t *a)
{
    lv_obj_t *obj = (lv_obj_t *)a->var;
    lv_obj_set_style_opa(obj, LV_OPA_COVER, 0);
    s_animating = false;
}

static void fade_out_done_cb(lv_anim_t *a)
{
    /* Phase 2: fade back in */
    lv_obj_t *obj = (lv_obj_t *)a->var;

    lv_anim_t a2;
    lv_anim_init(&a2);
    lv_anim_set_var(&a2, obj);
    lv_anim_set_values(&a2, LV_OPA_0, LV_OPA_COVER);
    lv_anim_set_time(&a2, FADE_IN_MS);
    lv_anim_set_exec_cb(&a2, fade_in_cb);
    lv_anim_set_completed_cb(&a2, fade_in_done_cb);
    lv_anim_set_path_cb(&a2, lv_anim_path_ease_out);
    lv_anim_start(&a2);
}

/* Celebration flash — quick brightness pulse for Pomodoro phase transitions */
static void celebrate_cb(void *var, int32_t val)
{
    lv_obj_set_style_opa((lv_obj_t *)var, val, 0);
}

static void celebrate_done_cb(lv_anim_t *a)
{
    lv_obj_t *obj = (lv_obj_t *)a->var;
    lv_obj_set_style_opa(obj, LV_OPA_COVER, 0);
}

void ui_transition_celebrate(lv_obj_t *hero)
{
    if (!hero) return;

    /* Quick triple pulse: bright → dim → bright → dim → bright */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, hero);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_30);
    lv_anim_set_time(&a, 150);
    lv_anim_set_playback_time(&a, 150);
    lv_anim_set_repeat_count(&a, 2);
    lv_anim_set_exec_cb(&a, celebrate_cb);
    lv_anim_set_completed_cb(&a, celebrate_done_cb);
    lv_anim_start(&a);
}

void ui_transition_slide(lv_obj_t *hero, bool slide_left)
{
    (void)slide_left;
    if (s_animating || !hero) return;
    s_animating = true;

    /* Phase 1: fade out */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, hero);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_0);
    lv_anim_set_time(&a, FADE_OUT_MS);
    lv_anim_set_exec_cb(&a, fade_in_cb);
    lv_anim_set_completed_cb(&a, fade_out_done_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_start(&a);
}
