/*
 * Minimal RTC stub for FatFs on RP2350 (no hardware RTC).
 * Provides get_fattime() and time() with a fixed timestamp.
 */

#include <time.h>
#include "ff.h"

/* Return a fixed timestamp: 2025-01-01 00:00:00 */
DWORD get_fattime(void) {
    return ((DWORD)(2025 - 1980) << 25) |
           ((DWORD)1 << 21) |
           ((DWORD)1 << 16) |
           ((DWORD)0 << 11) |
           ((DWORD)0 << 5) |
           ((DWORD)0);
}

time_t time(time_t *pxTime) {
    /* Seconds since epoch for 2025-01-01 00:00:00 UTC */
    time_t t = 1735689600;
    if (pxTime) {
        *pxTime = t;
    }
    return t;
}

void time_init(void) {
    /* No-op: no hardware RTC on RP2350 */
}
