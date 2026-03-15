#include "ui_internal.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "ui_geo";

/* Geodesic triangle grid parameters */
#define GEO_CELL_W    80
#define GEO_CELL_H    70
#define GEO_LINE_W    1

/* Storage for line objects and their point arrays */
static lv_obj_t **s_lines = NULL;
static lv_point_precise_t *s_points = NULL;
static int s_line_count = 0;

void ui_geodesic_create(lv_obj_t *parent, int width, int height)
{
    int cols = (width / GEO_CELL_W) + 2;
    int rows = (height / GEO_CELL_H) + 2;
    s_line_count = rows * cols;

    /* Allocate arrays from PSRAM */
    s_lines = heap_caps_calloc(s_line_count, sizeof(lv_obj_t *), MALLOC_CAP_SPIRAM);
    /* Each triangle needs 4 points (3 vertices + closing point) */
    s_points = heap_caps_malloc(s_line_count * 4 * sizeof(lv_point_precise_t), MALLOC_CAP_SPIRAM);
    if (!s_lines || !s_points) {
        ESP_LOGE(TAG, "Failed to allocate geodesic buffers");
        s_line_count = 0;
        return;
    }

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            int idx = row * cols + col;
            int x = col * GEO_CELL_W;
            int y = row * GEO_CELL_H;

            /* Compute triangle vertices */
            lv_point_precise_t *pts = &s_points[idx * 4];
            pts[0].x = x;                  pts[0].y = y + GEO_CELL_H;
            pts[1].x = x + GEO_CELL_W / 2; pts[1].y = y;
            pts[2].x = x + GEO_CELL_W;     pts[2].y = y + GEO_CELL_H;
            pts[3].x = x;                  pts[3].y = y + GEO_CELL_H; /* close */

            lv_obj_t *line = lv_line_create(parent);
            lv_line_set_points(line, pts, 4);
            lv_obj_set_style_line_color(line, lv_color_hex(0x44CC00), 0);
            lv_obj_set_style_line_opa(line, 38, 0);
            lv_obj_set_style_line_width(line, GEO_LINE_W, 0);

            s_lines[idx] = line;
        }
    }

    ESP_LOGI(TAG, "Geodesic pattern created: %d triangles (%dx%d grid)", s_line_count, cols, rows);
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
