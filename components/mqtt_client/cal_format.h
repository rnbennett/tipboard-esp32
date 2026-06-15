#pragma once
#include <stddef.h>

/* Format an ISO-8601 datetime such as "2026-03-15T14:00:00" into a 12-hour
 * clock string such as "2:00 PM". Writes an empty string to `out` if the input
 * is too short, malformed, or out of range.
 *
 * Pure — no ESP-IDF dependencies, so it is unit-tested on the host
 * (see tools/hosttest/). The firmware calls it from the MQTT calendar handler. */
void format_iso_time(const char *iso, char *out, size_t out_len);
