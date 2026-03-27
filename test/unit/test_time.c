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
 * File: test_time.c
 * Description: Test suite for Sigma.Core Time interface
 */

#include <sigma.test/sigtest.h>
#include <sigma.memory/internal/memory.h>
#include <string.h>
#include "timing.h"

static void set_config(FILE **log_stream) { *log_stream = fopen("logs/test_time.log", "w"); }

static void set_teardown(void) {}

/* ---------------------------------------------------------------------- */
/* Tests                                                                   */
/* ---------------------------------------------------------------------- */

static void test_epoch_nonzero(void) {
    uint64_t ns = Time.epoch();
    Assert.isTrue(ns > 0, "Time.epoch() should return a non-zero value");
}

static void test_utc_valid_fields(void) {
    sc_time_t t = Time.current_utc();
    Assert.isTrue(t.year >= 1970, "year should be >= 1970");
    Assert.isTrue(t.month >= 1 && t.month <= 12, "month should be 1-12");
    Assert.isTrue(t.day >= 1 && t.day <= 31, "day should be 1-31");
    Assert.isTrue(t.hour <= 23, "hour should be 0-23");
    Assert.isTrue(t.minute <= 59, "minute should be 0-59");
    Assert.isTrue(t.second <= 59, "second should be 0-59");
    Assert.isTrue(t.nanosecond < 1000000000U, "nanosecond should be < 1e9");
    Assert.isTrue(t.utc_offset_min == 0, "utc_offset_min should be 0 (v1)");
}

static void test_current_equals_utc(void) {
    sc_time_t local = Time.current();
    sc_time_t utc = Time.current_utc();
    /* v1: current() is always UTC */
    Assert.isTrue(local.year == utc.year, "year mismatch: current vs current_utc");
    Assert.isTrue(local.month == utc.month, "month mismatch: current vs current_utc");
    Assert.isTrue(local.day == utc.day, "day mismatch: current vs current_utc");
}

static void test_timestamp_not_null(void) {
    char buf[31];
    string ts = Time.timestamp(buf, 31);
    Assert.isNotNull(ts, "Time.timestamp() should not return NULL");
}

static void test_timestamp_length(void) {
    char buf[31];
    string ts = Time.timestamp(buf, 31);
    Assert.isNotNull(ts, "Time.timestamp() should not return NULL");
    /* "YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ" = 30 chars */
    Assert.isTrue(strlen(ts) == 30, "timestamp length should be 30");
}

static void test_timestamp_format(void) {
    char buf[31];
    string ts = Time.timestamp(buf, 31);
    Assert.isNotNull(ts, "Time.timestamp() should not return NULL");
    Assert.isTrue(ts[4] == '-', "timestamp[4] should be '-'");
    Assert.isTrue(ts[7] == '-', "timestamp[7] should be '-'");
    Assert.isTrue(ts[10] == 'T', "timestamp[10] should be 'T'");
    Assert.isTrue(ts[13] == ':', "timestamp[13] should be ':'");
    Assert.isTrue(ts[16] == ':', "timestamp[16] should be ':'");
    Assert.isTrue(ts[19] == '.', "timestamp[19] should be '.'");
    Assert.isTrue(ts[29] == 'Z', "timestamp[29] should be 'Z'");
}

/* ---------------------------------------------------------------------- */
/* Registration                                                            */
/* ---------------------------------------------------------------------- */

__attribute__((constructor)) void init_time_tests(void) {
    init_memory_system();
    testset("time_set", set_config, set_teardown);

    testcase("epoch_nonzero", test_epoch_nonzero);
    testcase("utc_valid_fields", test_utc_valid_fields);
    testcase("current_equals_utc", test_current_equals_utc);
    testcase("timestamp_not_null", test_timestamp_not_null);
    testcase("timestamp_length", test_timestamp_length);
    testcase("timestamp_format", test_timestamp_format);
}
