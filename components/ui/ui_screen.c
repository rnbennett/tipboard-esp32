#include "ui.h"
#include "ui_theme.h"
#include "ui_internal.h"

/* Stub — implemented in Task 5 */

esp_err_t ui_init(void) { return ESP_OK; }
void ui_update(const status_state_t *state) { (void)state; }
void ui_update_timer(int32_t seconds, timer_type_t type) { (void)seconds; (void)type; }
lv_obj_t *ui_get_screen(void) { return NULL; }
