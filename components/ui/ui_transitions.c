#include "ui_internal.h"

/* Gentle opacity dip — smooth and subtle, no jarring black flash.
 * Dips to 40% over 200ms then recovers to 100% over 300ms. */
#define DIP_DOWN_MS  200
#define DIP_UP_MS    300
#define DIP_MIN_OPA  LV_OPA_40

static bool s_animating = false;

static void opa_cb(void *var, int32_t val)
{
    lv_obj_set_style_opa((lv_obj_t *)var, val, 0);
}

static void dip_up_done_cb(lv_anim_t *a)
{
    lv_obj_t *obj = (lv_obj_t *)a->var;
    lv_obj_set_style_opa(obj, LV_OPA_COVER, 0);
    s_animating = false;
}

static void dip_down_done_cb(lv_anim_t *a)
{
    lv_obj_t *obj = (lv_obj_t *)a->var;

    /* Recover back to full */
    lv_anim_t a2;
    lv_anim_init(&a2);
    lv_anim_set_var(&a2, obj);
    lv_anim_set_values(&a2, DIP_MIN_OPA, LV_OPA_COVER);
    lv_anim_set_time(&a2, DIP_UP_MS);
    lv_anim_set_exec_cb(&a2, opa_cb);
    lv_anim_set_completed_cb(&a2, dip_up_done_cb);
    lv_anim_set_path_cb(&a2, lv_anim_path_ease_out);
    lv_anim_start(&a2);
}

void ui_transition_slide(lv_obj_t *hero, bool slide_left)
{
    (void)slide_left;
    if (s_animating || !hero) return;
    s_animating = true;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, hero);
    lv_anim_set_values(&a, LV_OPA_COVER, DIP_MIN_OPA);
    lv_anim_set_time(&a, DIP_DOWN_MS);
    lv_anim_set_exec_cb(&a, opa_cb);
    lv_anim_set_completed_cb(&a, dip_down_done_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_start(&a);
}

/* Celebration — quick double pulse for Pomodoro phase transitions */
static void celebrate_done_cb(lv_anim_t *a)
{
    lv_obj_t *obj = (lv_obj_t *)a->var;
    lv_obj_set_style_opa(obj, LV_OPA_COVER, 0);
}

void ui_transition_celebrate(lv_obj_t *hero)
{
    if (!hero) return;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, hero);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_40);
    lv_anim_set_time(&a, 120);
    lv_anim_set_playback_time(&a, 120);
    lv_anim_set_repeat_count(&a, 2);
    lv_anim_set_exec_cb(&a, opa_cb);
    lv_anim_set_completed_cb(&a, celebrate_done_cb);
    lv_anim_start(&a);
}
