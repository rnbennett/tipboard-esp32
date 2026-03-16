#include "ui_internal.h"
#include "board.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "ui_geo";

/*
 * Spaceship Earth geodesic pattern — simplified for embedded.
 * Uses larger hex cells (200px radius) to keep LVGL object count low.
 * Each hex has 6 radial lines from center to vertices.
 * Hex edges are drawn as separate lines.
 *
 * Total: ~50 line objects — well within LVGL limits.
 */

/* Responsive hex size — smaller on CYD */
#if BOARD_DISP_H_RES >= 1024
#define HEX_RADIUS    140
#else
#define HEX_RADIUS    50
#endif
#define LINE_WIDTH    1

/* Pointy-top hex vertex offsets * 1000 for integer math */
/* cos(30,90,150,210,270,330) * 1000 */
static const int COS1K[6] = { 866,   0, -866, -866,    0,  866};
/* sin(30,90,150,210,270,330) * 1000 */
static const int SIN1K[6] = { 500, 1000, 500, -500, -1000, -500};

#define MAX_LINES 200

static lv_obj_t *s_lines[MAX_LINES];
static lv_point_precise_t s_points[MAX_LINES * 2];
static int s_line_count = 0;

static void add_line(lv_obj_t *parent,
                     int x0, int y0, int x1, int y1,
                     lv_color_t color, lv_opa_t opa)
{
    if (s_line_count >= MAX_LINES) return;
    int idx = s_line_count;

    lv_point_precise_t *pts = &s_points[idx * 2];
    pts[0].x = x0; pts[0].y = y0;
    pts[1].x = x1; pts[1].y = y1;

    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line, pts, 2);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_opa(line, opa, 0);
    lv_obj_set_style_line_width(line, LINE_WIDTH, 0);
    s_lines[idx] = line;
    s_line_count++;
}

void ui_geodesic_create(lv_obj_t *parent, int width, int height)
{
    int hex_w = (HEX_RADIUS * 1732) / 1000;  /* sqrt(3) * r */
    int row_h = (HEX_RADIUS * 3) / 2;

    int cols = (width / hex_w) + 2;
    int rows = (height / row_h) + 2;

    lv_color_t color = lv_color_hex(0x44CC00);
    lv_opa_t opa = 38;
    s_line_count = 0;

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            int cx = col * hex_w + ((row & 1) ? hex_w / 2 : 0);
            int cy = row * row_h;

            /* Vertex positions */
            int vx[6], vy[6];
            for (int i = 0; i < 6; i++) {
                vx[i] = cx + (HEX_RADIUS * COS1K[i]) / 1000;
                vy[i] = cy + (HEX_RADIUS * SIN1K[i]) / 1000;
            }

            /* 6 radial lines from center to each vertex */
            for (int i = 0; i < 6; i++) {
                add_line(parent, cx, cy, vx[i], vy[i], color, opa);
            }

            /* 3 top-half edge lines */
            for (int i = 0; i < 3; i++) {
                add_line(parent, vx[i], vy[i], vx[i + 1], vy[i + 1], color, opa);
            }
        }
    }

    ESP_LOGI(TAG, "Geodesic pattern: %d lines (%dx%d hex grid)", s_line_count, cols, rows);
}

void ui_geodesic_update(lv_color_t tint, lv_opa_t opacity)
{
    for (int i = 0; i < s_line_count; i++) {
        if (s_lines[i]) {
            lv_obj_set_style_line_color(s_lines[i], tint, 0);
            lv_obj_set_style_line_opa(s_lines[i], opacity, 0);
        }
    }
}
