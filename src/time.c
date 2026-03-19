/*
 * Sigma.Core
 * Copyright (c) 2026 David Boarman (BadKraft) and contributors
 * QuantumOverride [Q|]
 * ----------------------------------------------
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * ----------------------------------------------
 * File: time.c
 * Description: Implementation of the Time interface for Sigma.Core
 */

/* ====================================================================
 * Platform clock abstraction — no POSIX <time.h> used on Linux.
 * ==================================================================== */

/* Base types must come first — platform block uses int64_t, uint64_t. */
#include "types.h"

#if defined(__linux__)

#include <sys/syscall.h>
/* Declare syscall directly — avoids pulling in all of <unistd.h>. */
extern long syscall(long number, ...);

#define SC_CLOCK_REALTIME 0

struct _sc_timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

static int sc_clock_gettime(struct _sc_timespec *ts) {
    return (int)syscall(SYS_clock_gettime, SC_CLOCK_REALTIME, ts);
}

#elif defined(__APPLE__)

/* clock_gettime available on macOS 10.12+ */
#include <time.h>

struct _sc_timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

static int sc_clock_gettime(struct _sc_timespec *ts) {
    struct timespec _ts = {0};
    int r = clock_gettime(CLOCK_REALTIME, &_ts);
    ts->tv_sec = (int64_t)_ts.tv_sec;
    ts->tv_nsec = (int64_t)_ts.tv_nsec;
    return r;
}

#elif defined(_WIN32)

#include <windows.h>

/* Windows FILETIME epoch: 1601-01-01; Unix epoch: 1970-01-01.
 * Difference: 11644473600 seconds. */
#define SC_EPOCH_DIFF_SEC 11644473600ULL

struct _sc_timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

static int sc_clock_gettime(struct _sc_timespec *ts) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t t = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    t -= SC_EPOCH_DIFF_SEC * 10000000ULL;
    ts->tv_sec = (int64_t)(t / 10000000ULL);
    ts->tv_nsec = (int64_t)((t % 10000000ULL) * 100ULL);
    return 0;
}

#else
#error "Unsupported platform: time.c requires __linux__, __APPLE__, or _WIN32"
#endif

/* ====================================================================
 * Remaining includes
 * ==================================================================== */

#include <stdio.h>
#include "timing.h"

/* ====================================================================
 * Calendar math helpers
 * ==================================================================== */

// API functions
static sc_time_t time_current(void);
static sc_time_t time_current_utc(void);
static string time_timestamp(string buf, usize size);
static uint64_t time_epoch(void);

// Helper functions
static bool is_leap_year(int32_t y);
static uint8_t days_in_month(int32_t y, uint8_t m);
static sc_time_t epoch_to_time(uint64_t ns_epoch);

/* ====================================================================
 * API function definitions
 * ==================================================================== */

static uint64_t time_epoch(void) {
    struct _sc_timespec ts = {0};
    sc_clock_gettime(&ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static sc_time_t time_current_utc(void) { return epoch_to_time(time_epoch()); }

static sc_time_t time_current(void) {
    /* TZ support deferred — v1 returns UTC */
    return time_current_utc();
}

static string time_timestamp(string buf, usize size) {
    if (!buf) return NULL;
    sc_time_t t = time_current_utc();
    snprintf(buf, size, "%04d-%02u-%02uT%02u:%02u:%02u.%09uZ", t.year, t.month, t.day, t.hour,
             t.minute, t.second, t.nanosecond);
    return buf;
}

/* ====================================================================
 * Interface definition
 * ==================================================================== */

const sc_time_i Time = {
    .current = time_current,
    .current_utc = time_current_utc,
    .timestamp = time_timestamp,
    .epoch = time_epoch,
};

/* ====================================================================
 * Helper function definitions
 * ==================================================================== */

static bool is_leap_year(int32_t y) { return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0); }

static uint8_t days_in_month(int32_t y, uint8_t m) {
    static const uint8_t dom[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && is_leap_year(y)) return 29;
    return dom[m];
}

static sc_time_t epoch_to_time(uint64_t ns_epoch) {
    sc_time_t t = {0};
    uint64_t total_sec = ns_epoch / 1000000000ULL;
    t.nanosecond = (uint32_t)(ns_epoch % 1000000000ULL);

    uint64_t day_num = total_sec / 86400ULL;
    uint64_t time_sec = total_sec % 86400ULL;
    t.hour = (uint8_t)(time_sec / 3600ULL);
    t.minute = (uint8_t)((time_sec % 3600ULL) / 60ULL);
    t.second = (uint8_t)(time_sec % 60ULL);

    t.year = 1970;
    for (;;) {
        uint32_t diy = is_leap_year(t.year) ? 366U : 365U;
        if (day_num < (uint64_t)diy) break;
        day_num -= (uint64_t)diy;
        t.year++;
    }

    t.month = 1;
    for (;;) {
        uint8_t dim = days_in_month(t.year, t.month);
        if (day_num < (uint64_t)dim) break;
        day_num -= (uint64_t)dim;
        t.month++;
    }

    t.day = (uint8_t)(day_num + 1U);
    t.utc_offset_min = 0;
    return t;
}
