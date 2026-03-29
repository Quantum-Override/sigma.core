/*
 * SigmaCore
 * Copyright (c) 2026 David Boarman (BadKraft) and contributors
 * QuantumOverride [Q|]
 * ----------------------------------------------
 * File: test_string_malloc.c
 * Description: Standalone test suite for string_malloc.c
 *
 * NO sigma.test dependency — uses stdio + assert only.
 * Tests all 8 String functions for correctness and memory safety.
 * Run with valgrind to verify zero leaks.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "string_malloc.h"

/* Test counter */
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_START(name)                  \
    do {                                  \
        tests_run++;                      \
        printf("Running: %s ... ", name); \
    } while (0)

#define TEST_PASS()     \
    do {                \
        tests_passed++; \
        printf("✓\n");  \
    } while (0)

/* ======================================================================== */
/* Test Cases                                                               */
/* ======================================================================== */

void test_string_dupe(void) {
    TEST_START("string_dupe");

    string s1 = String.dupe("hello");
    assert(s1 != NULL);
    assert(strcmp(s1, "hello") == 0);
    String.dispose(s1);

    // NULL input
    string s2 = String.dupe(NULL);
    assert(s2 == NULL);

    // Empty string
    string s3 = String.dupe("");
    assert(s3 == NULL);  // FR spec: len == 0 returns NULL

    TEST_PASS();
}

void test_string_copy(void) {
    TEST_START("string_copy");

    string orig = String.dupe("world");
    assert(orig != NULL);

    string copy = String.copy(orig);
    assert(copy != NULL);
    assert(strcmp(orig, copy) == 0);
    assert(orig != copy);  // different pointers

    String.dispose(orig);
    String.dispose(copy);

    // NULL input
    string s2 = String.copy(NULL);
    assert(s2 == NULL);

    TEST_PASS();
}

void test_string_length(void) {
    TEST_START("string_length");

    string s = String.dupe("test");
    assert(String.length(s) == 4);
    String.dispose(s);

    // NULL input
    assert(String.length(NULL) == 0);

    TEST_PASS();
}

void test_string_concat(void) {
    TEST_START("string_concat");

    string s1 = String.dupe("hello");
    string s2 = String.dupe(" world");

    string result = String.concat(s1, s2);
    assert(result != NULL);
    assert(strcmp(result, "hello world") == 0);
    assert(String.length(result) == 11);

    String.dispose(s1);
    String.dispose(s2);
    String.dispose(result);

    // NULL inputs
    string s4 = String.dupe("x");
    assert(String.concat(NULL, s4) == NULL);
    String.dispose(s4);

    string s3 = String.dupe("y");
    assert(String.concat(s3, NULL) == NULL);
    String.dispose(s3);

    TEST_PASS();
}

void test_string_compare(void) {
    TEST_START("string_compare");

    string s1 = String.dupe("abc");
    string s2 = String.dupe("abc");
    string s3 = String.dupe("def");

    assert(String.compare(s1, s2) == 0);  // equal
    assert(String.compare(s1, s3) < 0);   // s1 < s3
    assert(String.compare(s3, s1) > 0);   // s3 > s1
    assert(String.compare(s1, s1) == 0);  // same pointer

    String.dispose(s1);
    String.dispose(s2);
    String.dispose(s3);

    // NULL inputs
    assert(String.compare(NULL, NULL) == 0);
    string s4 = String.dupe("x");
    assert(String.compare(s4, NULL) > 0);
    assert(String.compare(NULL, s4) < 0);
    String.dispose(s4);

    TEST_PASS();
}

void test_string_format(void) {
    TEST_START("string_format");

    string s1 = String.format("Number: %d", 42);
    assert(s1 != NULL);
    assert(strcmp(s1, "Number: 42") == 0);
    String.dispose(s1);

    string s2 = String.format("Hello %s, you are #%d", "world", 1);
    assert(s2 != NULL);
    assert(strcmp(s2, "Hello world, you are #1") == 0);
    String.dispose(s2);

    // NULL format
    assert(String.format(NULL) == NULL);

    TEST_PASS();
}

void test_string_to_array(void) {
    TEST_START("string_to_array");

    string s = String.dupe("array");
    char *arr = String.to_array(s);

    assert(arr != NULL);
    assert(strcmp(arr, "array") == 0);
    assert(arr != s);  // different pointers

    String.dispose(s);
    String.dispose(arr);  // both must be disposed separately

    // NULL input
    assert(String.to_array(NULL) == NULL);

    TEST_PASS();
}

void test_string_dispose_null(void) {
    TEST_START("string_dispose (NULL safe)");

    // Dispose NULL should not crash
    String.dispose(NULL);

    TEST_PASS();
}

void test_string_lifecycle(void) {
    TEST_START("string lifecycle (allocate → use → dispose)");

    string s1 = String.dupe("lifecycle");
    assert(s1 != NULL);

    usize len = String.length(s1);
    assert(len == 9);

    string s2 = String.copy(s1);
    assert(s2 != NULL);

    string s3 = String.concat(s1, s2);
    assert(s3 != NULL);
    assert(String.length(s3) == 18);

    String.dispose(s1);
    String.dispose(s2);
    String.dispose(s3);

    TEST_PASS();
}

/* ======================================================================== */
/* StringBuilder Test Cases                                                 */
/* ======================================================================== */

void test_stringbuilder_new(void) {
    TEST_START("stringbuilder_new");

    string_builder sb1 = StringBuilder.new(32);
    assert(sb1 != NULL);
    assert(StringBuilder.length(sb1) == 0);
    assert(StringBuilder.capacity(sb1) >= 32);
    StringBuilder.dispose(sb1);

    // Zero capacity defaults to 16
    string_builder sb2 = StringBuilder.new(0);
    assert(sb2 != NULL);
    assert(StringBuilder.capacity(sb2) >= 16);
    StringBuilder.dispose(sb2);

    TEST_PASS();
}

void test_stringbuilder_snew(void) {
    TEST_START("stringbuilder_snew");

    string s = String.dupe("initial");
    string_builder sb = StringBuilder.snew(s);
    assert(sb != NULL);
    assert(StringBuilder.length(sb) == 7);

    string result = StringBuilder.toString(sb);
    assert(strcmp(result, "initial") == 0);

    String.dispose(s);
    String.dispose(result);
    StringBuilder.dispose(sb);

    // NULL creates empty builder
    string_builder sb2 = StringBuilder.snew(NULL);
    assert(sb2 != NULL);
    assert(StringBuilder.length(sb2) == 0);
    StringBuilder.dispose(sb2);

    TEST_PASS();
}

void test_stringbuilder_append(void) {
    TEST_START("stringbuilder_append");

    string_builder sb = StringBuilder.new(0);

    StringBuilder.append(sb, "Hello");
    assert(StringBuilder.length(sb) == 5);

    StringBuilder.append(sb, " ");
    StringBuilder.append(sb, "World");
    assert(StringBuilder.length(sb) == 11);

    string result = StringBuilder.toString(sb);
    assert(strcmp(result, "Hello World") == 0);

    String.dispose(result);
    StringBuilder.dispose(sb);

    TEST_PASS();
}

void test_stringbuilder_appendf(void) {
    TEST_START("stringbuilder_appendf");

    string_builder sb = StringBuilder.new(0);

    StringBuilder.appendf(sb, "Number: %d", 42);
    assert(StringBuilder.length(sb) == 10);

    StringBuilder.appendf(sb, ", Float: %.2f", 3.14);

    string result = StringBuilder.toString(sb);
    assert(strcmp(result, "Number: 42, Float: 3.14") == 0);

    String.dispose(result);
    StringBuilder.dispose(sb);

    TEST_PASS();
}

void test_stringbuilder_appendl(void) {
    TEST_START("stringbuilder_appendl");

    string_builder sb = StringBuilder.new(0);

    StringBuilder.appendl(sb, "Line 1");
    StringBuilder.appendl(sb, "Line 2");
    StringBuilder.appendl(sb, NULL);  // just newline
    StringBuilder.appendl(sb, "Line 4");

    string result = StringBuilder.toString(sb);
    assert(strcmp(result, "Line 1\nLine 2\n\nLine 4\n") == 0);

    String.dispose(result);
    StringBuilder.dispose(sb);

    TEST_PASS();
}

void test_stringbuilder_lappends(void) {
    TEST_START("stringbuilder_lappends");

    string_builder sb = StringBuilder.new(0);

    StringBuilder.append(sb, "Start");
    StringBuilder.lappends(sb, "Line 1");
    StringBuilder.lappends(sb, "Line 2");

    string result = StringBuilder.toString(sb);
    assert(strcmp(result, "Start\nLine 1\nLine 2") == 0);

    String.dispose(result);
    StringBuilder.dispose(sb);

    TEST_PASS();
}

void test_stringbuilder_lappendf(void) {
    TEST_START("stringbuilder_lappendf");

    string_builder sb = StringBuilder.new(0);

    StringBuilder.append(sb, "Header");
    StringBuilder.lappendf(sb, "Value: %d", 100);
    StringBuilder.lappendf(sb, "Another: %s", "test");

    string result = StringBuilder.toString(sb);
    assert(strcmp(result, "Header\nValue: 100\nAnother: test") == 0);

    String.dispose(result);
    StringBuilder.dispose(sb);

    TEST_PASS();
}

void test_stringbuilder_clear(void) {
    TEST_START("stringbuilder_clear");

    string_builder sb = StringBuilder.new(0);

    StringBuilder.append(sb, "Some content");
    assert(StringBuilder.length(sb) > 0);

    StringBuilder.clear(sb);
    assert(StringBuilder.length(sb) == 0);

    string result = StringBuilder.toString(sb);
    assert(strcmp(result, "") == 0);

    String.dispose(result);
    StringBuilder.dispose(sb);

    TEST_PASS();
}

void test_stringbuilder_toString(void) {
    TEST_START("stringbuilder_toString");

    string_builder sb = StringBuilder.new(0);
    StringBuilder.append(sb, "convert");

    string s1 = StringBuilder.toString(sb);
    assert(s1 != NULL);
    assert(strcmp(s1, "convert") == 0);

    // Append more and convert again
    StringBuilder.append(sb, " more");
    string s2 = StringBuilder.toString(sb);
    assert(strcmp(s2, "convert more") == 0);

    String.dispose(s1);
    String.dispose(s2);
    StringBuilder.dispose(sb);

    TEST_PASS();
}

void test_stringbuilder_capacity(void) {
    TEST_START("stringbuilder_capacity");

    string_builder sb = StringBuilder.new(64);
    assert(StringBuilder.capacity(sb) >= 64);

    usize initial_capacity = StringBuilder.capacity(sb);

    // Set larger capacity
    StringBuilder.setCapacity(sb, 128);
    assert(StringBuilder.capacity(sb) >= 128);
    assert(StringBuilder.capacity(sb) > initial_capacity);

    // Set smaller capacity (should be ignored)
    StringBuilder.setCapacity(sb, 32);
    assert(StringBuilder.capacity(sb) >= 128);  // unchanged

    StringBuilder.dispose(sb);

    TEST_PASS();
}

void test_stringbuilder_dispose_null(void) {
    TEST_START("stringbuilder_dispose (NULL safe)");

    // Dispose NULL should not crash
    StringBuilder.dispose(NULL);

    TEST_PASS();
}

void test_stringbuilder_lifecycle(void) {
    TEST_START("stringbuilder lifecycle");

    // Create builder
    string_builder sb = StringBuilder.new(16);
    assert(sb != NULL);

    // Build up content
    StringBuilder.append(sb, "Part1");
    StringBuilder.appendf(sb, " %d", 2);
    StringBuilder.appendl(sb, " Part3");

    usize len = StringBuilder.length(sb);
    assert(len > 0);

    // Convert to string
    string final = StringBuilder.toString(sb);
    assert(strcmp(final, "Part1 2 Part3\n") == 0);

    // Cleanup
    String.dispose(final);
    StringBuilder.dispose(sb);

    TEST_PASS();
}

/* ======================================================================== */
/* Main                                                                     */
/* ======================================================================== */

int main(void) {
    printf("==============================================\n");
    printf("sigma.string.a Standalone Test Suite\n");
    printf("==============================================\n\n");

    printf("--- String Tests ---\n");
    test_string_dupe();
    test_string_copy();
    test_string_length();
    test_string_concat();
    test_string_compare();
    test_string_format();
    test_string_to_array();
    test_string_dispose_null();
    test_string_lifecycle();

    printf("\n--- StringBuilder Tests ---\n");
    test_stringbuilder_new();
    test_stringbuilder_snew();
    test_stringbuilder_append();
    test_stringbuilder_appendf();
    test_stringbuilder_appendl();
    test_stringbuilder_lappends();
    test_stringbuilder_lappendf();
    test_stringbuilder_clear();
    test_stringbuilder_toString();
    test_stringbuilder_capacity();
    test_stringbuilder_dispose_null();
    test_stringbuilder_lifecycle();

    printf("\n==============================================\n");
    printf("Tests: %d/%d passed\n", tests_passed, tests_run);
    printf("==============================================\n");

    if (tests_passed == tests_run) {
        printf("✓ All tests passed!\n");
        return 0;
    } else {
        printf("✗ %d test(s) failed\n", tests_run - tests_passed);
        return 1;
    }
}
