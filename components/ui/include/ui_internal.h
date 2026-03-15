#pragma once

#include "lvgl.h"
#include "ui_theme.h"
#include <stdbool.h>

/* ui_geodesic.c */
void ui_geodesic_create(lv_obj_t *parent, int width, int height);
void ui_geodesic_update(lv_color_t tint, lv_opa_t opacity);

/* ui_timer_arc.c */
void ui_timer_arc_create(lv_obj_t *parent);
void ui_timer_arc_update(int32_t progress_pct, lv_color_t color, bool visible);

/* ui_transitions.c */
void ui_transition_slide(lv_obj_t *hero, bool slide_left);
void ui_transition_celebrate(lv_obj_t *hero);
