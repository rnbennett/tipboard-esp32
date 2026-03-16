#include "ui_internal.h"

/* Mode transitions are instant — opacity animations on the hero zone
 * with 200+ geodesic line children are too expensive for smooth rendering.
 * Clean instant switches feel better than clunky animations. */

void ui_transition_slide(lv_obj_t *hero, bool slide_left)
{
    (void)hero;
    (void)slide_left;
    /* Instant — no animation */
}

/* Celebration — quick double pulse for Pomodoro phase transitions.
 * Only animates the mode label, not the whole hero (much cheaper). */
static void celebrate_cb(void *var, int32_t val)
{
    lv_obj_set_style_text_opa((lv_obj_t *)var, val, 0);
}

static void celebrate_done_cb(lv_anim_t *a)
{
    lv_obj_t *obj = (lv_obj_t *)a->var;
    lv_obj_set_style_text_opa(obj, LV_OPA_COVER, 0);
}

void ui_transition_celebrate(lv_obj_t *hero)
{
    /* hero is passed but we animate the mode label directly
     * since it's much cheaper than the whole container */
    if (!hero) return;

    /* Find the mode label (second-to-last visible label in hero) */
    /* For now, pulse the hero's opacity briefly — just the stripe glow */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, hero);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_60);
    lv_anim_set_time(&a, 100);
    lv_anim_set_playback_time(&a, 100);
    lv_anim_set_repeat_count(&a, 1);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
    lv_anim_set_completed_cb(&a, celebrate_done_cb);
    lv_anim_start(&a);
}
