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
 * File: test_guid.c
 * Description: Test suite for Sigma.Core GUID interface
 */

#include <sigma.test/sigtest.h>
#include <string.h>
#include "guid.h"

static void set_config(FILE **log_stream) { *log_stream = fopen("logs/test_guid.log", "w"); }

static void set_teardown(void) {}

/* ---------------------------------------------------------------------- */
/* Tests                                                                   */
/* ---------------------------------------------------------------------- */

static void test_generate_not_zero(void) {
    sc_guid_t g = Guid.generate();
    Assert.isFalse(Guid.equal(g, Guid.zero), "generated GUID should not equal zero");
}

static void test_two_guids_differ(void) {
    sc_guid_t a = Guid.generate();
    sc_guid_t b = Guid.generate();
    Assert.isFalse(Guid.equal(a, b), "two GUIDs generated at different times should differ");
}

static void test_to_string_not_null(void) {
    char buf[37];
    sc_guid_t g = Guid.generate();
    string s = Guid.to_string(g, buf, 37);
    Assert.isNotNull(s, "Guid.to_string() should not return NULL");
}

static void test_to_string_length(void) {
    char buf[37];
    sc_guid_t g = Guid.generate();
    string s = Guid.to_string(g, buf, 37);
    Assert.isNotNull(s, "Guid.to_string() should not return NULL");
    /* "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" = 36 chars */
    Assert.isTrue(strlen(s) == 36, "GUID string length should be 36");
}

static void test_to_string_format(void) {
    char buf[37];
    sc_guid_t g = Guid.generate();
    string s = Guid.to_string(g, buf, 37);
    Assert.isNotNull(s, "Guid.to_string() should not return NULL");
    Assert.isTrue(s[8] == '-', "GUID string[8] should be '-'");
    Assert.isTrue(s[13] == '-', "GUID string[13] should be '-'");
    Assert.isTrue(s[18] == '-', "GUID string[18] should be '-'");
    Assert.isTrue(s[23] == '-', "GUID string[23] should be '-'");
}

static void test_epoch_embedded(void) {
    sc_guid_t g = Guid.generate();
    /* epoch nanoseconds stored big-endian in bytes[8..15] */
    uint64_t ns = 0;
    for (int i = 0; i < 8; i++) ns = (ns << 8) | (uint64_t)g.data[8 + i];
    Assert.isTrue(ns > 0, "epoch embedded in GUID bytes[8..15] should be non-zero");
}

/* ---------------------------------------------------------------------- */
/* Registration                                                            */
/* ---------------------------------------------------------------------- */

static void register_guid_tests(void) {
    testset("guid_set", set_config, set_teardown);

    testcase("generate_not_zero", test_generate_not_zero);
    testcase("two_guids_differ", test_two_guids_differ);
    testcase("to_string_not_null", test_to_string_not_null);
    testcase("to_string_length", test_to_string_length);
    testcase("to_string_format", test_to_string_format);
    testcase("epoch_embedded", test_epoch_embedded);
}

__attribute__((constructor)) static void enqueue_guid_tests(void) {
    Tests.enqueue(register_guid_tests);
}
