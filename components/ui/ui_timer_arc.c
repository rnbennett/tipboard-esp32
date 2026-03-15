#include "ui_internal.h"
#include "ui_theme.h"

static lv_obj_t *s_arc = NULL;

void ui_timer_arc_create(lv_obj_t *parent)
{
    s_arc = lv_arc_create(parent);
    lv_obj_set_size(s_arc, 250, 250);
    lv_arc_set_rotation(s_arc, 270);
    lv_arc_set_bg_angles(s_arc, 0, 360);
    lv_arc_set_range(s_arc, 0, 100);
    lv_arc_set_value(s_arc, 0);

    /* Background arc */
    lv_obj_set_style_arc_width(s_arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, UI_COLOR_DIVIDER, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s_arc, LV_OPA_30, LV_PART_MAIN);

    /* Progress arc */
    lv_obj_set_style_arc_width(s_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_arc, true, LV_PART_INDICATOR);

    /* Remove knob */
    lv_obj_set_style_pad_all(s_arc, 0, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(s_arc, LV_OPA_TRANSP, LV_PART_KNOB);

    /* Display only, not interactive */
    lv_obj_remove_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);

    /* Initially hidden */
    lv_obj_add_flag(s_arc, LV_OBJ_FLAG_HIDDEN);

    lv_obj_align(s_arc, LV_ALIGN_CENTER, 0, 60);
}

void ui_timer_arc_update(int32_t progress_pct, lv_color_t color, bool visible)
{
    if (!s_arc) return;

    if (!visible) {
        lv_obj_add_flag(s_arc, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_remove_flag(s_arc, LV_OBJ_FLAG_HIDDEN);
    lv_arc_set_value(s_arc, progress_pct);
    lv_obj_set_style_arc_color(s_arc, color, LV_PART_INDICATOR);
}
