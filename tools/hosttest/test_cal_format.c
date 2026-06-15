/* Host unit test for the pure calendar-time formatter. Builds with native gcc
 * (no ESP-IDF), so the logic can be exercised off-target. See Makefile. */
#include "cal_format.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;

#define CHECK(iso, expect) do {                                              \
    char out[16];                                                            \
    format_iso_time((iso), out, sizeof(out));                                \
    if (strcmp(out, (expect)) != 0) {                                        \
        printf("FAIL: format_iso_time(\"%s\") = \"%s\", expected \"%s\"\n",  \
               (iso) ? (iso) : "(null)", out, (expect));                     \
        fails++;                                                             \
    } else {                                                                 \
        printf("ok:   \"%-22s\" -> \"%s\"\n", (iso) ? (iso) : "(null)", out);\
    }                                                                        \
} while (0)

int main(void)
{
    /* Happy path + the AM/PM boundary cases that are easy to get wrong. */
    CHECK("2026-03-15T14:00:00", "2:00 PM");
    CHECK("2026-03-15T00:00:00", "12:00 AM");  /* midnight */
    CHECK("2026-03-15T12:30:00", "12:30 PM");  /* noon */
    CHECK("2026-03-15T13:05:00", "1:05 PM");
    CHECK("2026-03-15T23:59:00", "11:59 PM");
    CHECK("2026-03-15T09:07:00", "9:07 AM");

    /* Defensive: short / empty / null / out-of-range / non-numeric -> "" */
    CHECK("short", "");
    CHECK("", "");
    CHECK(NULL, "");
    CHECK("2026-03-15T99:99:00", "");
    CHECK("2026-03-15Txx:yy:00", "");

    if (fails) {
        printf("\n%d test(s) FAILED\n", fails);
        return 1;
    }
    printf("\nAll cal_format tests passed.\n");
    return 0;
}
