#include "cal_format.h"
#include <stdio.h>
#include <string.h>

void format_iso_time(const char *iso, char *out, size_t out_len)
{
    if (!out || out_len == 0) return;
    out[0] = '\0';
    /* Need at least "YYYY-MM-DDThh:mm" — 16 chars, time starts at offset 11. */
    if (!iso || strlen(iso) < 16) return;

    int hour = 0, min = 0;
    if (sscanf(iso + 11, "%d:%d", &hour, &min) != 2) return;
    if (hour < 0 || hour > 23 || min < 0 || min > 59) return;

    int h12 = hour % 12;
    if (h12 == 0) h12 = 12;
    const char *ampm = hour >= 12 ? "PM" : "AM";
    snprintf(out, out_len, "%d:%02d %s", h12, min, ampm);
}
