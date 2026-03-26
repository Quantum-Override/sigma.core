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

    string s1 = string_dupe("hello");
    assert(s1 != NULL);
    assert(strcmp(s1, "hello") == 0);
    string_dispose(s1);

    // NULL input
    string s2 = string_dupe(NULL);
    assert(s2 == NULL);

    // Empty string
    string s3 = string_dupe("");
    assert(s3 == NULL);  // FR spec: len == 0 returns NULL

    TEST_PASS();
}

void test_string_copy(void) {
    TEST_START("string_copy");

    string orig = string_dupe("world");
    assert(orig != NULL);

    string copy = string_copy(orig);
    assert(copy != NULL);
    assert(strcmp(orig, copy) == 0);
    assert(orig != copy);  // different pointers

    string_dispose(orig);
    string_dispose(copy);

    // NULL input
    string s2 = string_copy(NULL);
    assert(s2 == NULL);

    TEST_PASS();
}

void test_string_length(void) {
    TEST_START("string_length");

    string s = string_dupe("test");
    assert(string_length(s) == 4);
    string_dispose(s);

    // NULL input
    assert(string_length(NULL) == 0);

    TEST_PASS();
}

void test_string_concat(void) {
    TEST_START("string_concat");

    string s1 = string_dupe("hello");
    string s2 = string_dupe(" world");

    string result = string_concat(s1, s2);
    assert(result != NULL);
    assert(strcmp(result, "hello world") == 0);
    assert(string_length(result) == 11);

    string_dispose(s1);
    string_dispose(s2);
    string_dispose(result);

    // NULL inputs
    string s4 = string_dupe("x");
    assert(string_concat(NULL, s4) == NULL);
    string_dispose(s4);

    string s3 = string_dupe("y");
    assert(string_concat(s3, NULL) == NULL);
    string_dispose(s3);

    TEST_PASS();
}

void test_string_compare(void) {
    TEST_START("string_compare");

    string s1 = string_dupe("abc");
    string s2 = string_dupe("abc");
    string s3 = string_dupe("def");

    assert(string_compare(s1, s2) == 0);  // equal
    assert(string_compare(s1, s3) < 0);   // s1 < s3
    assert(string_compare(s3, s1) > 0);   // s3 > s1
    assert(string_compare(s1, s1) == 0);  // same pointer

    string_dispose(s1);
    string_dispose(s2);
    string_dispose(s3);

    // NULL inputs
    assert(string_compare(NULL, NULL) == 0);
    string s4 = string_dupe("x");
    assert(string_compare(s4, NULL) > 0);
    assert(string_compare(NULL, s4) < 0);
    string_dispose(s4);

    TEST_PASS();
}

void test_string_format(void) {
    TEST_START("string_format");

    string s1 = string_format("Number: %d", 42);
    assert(s1 != NULL);
    assert(strcmp(s1, "Number: 42") == 0);
    string_dispose(s1);

    string s2 = string_format("Hello %s, you are #%d", "world", 1);
    assert(s2 != NULL);
    assert(strcmp(s2, "Hello world, you are #1") == 0);
    string_dispose(s2);

    // NULL format
    assert(string_format(NULL) == NULL);

    TEST_PASS();
}

void test_string_to_array(void) {
    TEST_START("string_to_array");

    string s = string_dupe("array");
    char *arr = string_to_array(s);

    assert(arr != NULL);
    assert(strcmp(arr, "array") == 0);
    assert(arr != s);  // different pointers

    string_dispose(s);
    string_dispose(arr);  // both must be disposed separately

    // NULL input
    assert(string_to_array(NULL) == NULL);

    TEST_PASS();
}

void test_string_dispose_null(void) {
    TEST_START("string_dispose (NULL safe)");

    // Dispose NULL should not crash
    string_dispose(NULL);

    TEST_PASS();
}

void test_string_lifecycle(void) {
    TEST_START("string lifecycle (allocate → use → dispose)");

    string s1 = string_dupe("lifecycle");
    assert(s1 != NULL);

    usize len = string_length(s1);
    assert(len == 9);

    string s2 = string_copy(s1);
    assert(s2 != NULL);

    string s3 = string_concat(s1, s2);
    assert(s3 != NULL);
    assert(string_length(s3) == 18);

    string_dispose(s1);
    string_dispose(s2);
    string_dispose(s3);

    TEST_PASS();
}

/* ======================================================================== */
/* StringBuilder Test Cases                                                 */
/* ======================================================================== */

void test_stringbuilder_new(void) {
    TEST_START("stringbuilder_new");

    string_builder sb1 = stringbuilder_new(32);
    assert(sb1 != NULL);
    assert(stringbuilder_length(sb1) == 0);
    assert(stringbuilder_capacity(sb1) >= 32);
    stringbuilder_dispose(sb1);

    // Zero capacity defaults to 16
    string_builder sb2 = stringbuilder_new(0);
    assert(sb2 != NULL);
    assert(stringbuilder_capacity(sb2) >= 16);
    stringbuilder_dispose(sb2);

    TEST_PASS();
}

void test_stringbuilder_snew(void) {
    TEST_START("stringbuilder_snew");

    string s = string_dupe("initial");
    string_builder sb = stringbuilder_snew(s);
    assert(sb != NULL);
    assert(stringbuilder_length(sb) == 7);

    string result = stringbuilder_toString(sb);
    assert(strcmp(result, "initial") == 0);

    string_dispose(s);
    string_dispose(result);
    stringbuilder_dispose(sb);

    // NULL creates empty builder
    string_builder sb2 = stringbuilder_snew(NULL);
    assert(sb2 != NULL);
    assert(stringbuilder_length(sb2) == 0);
    stringbuilder_dispose(sb2);

    TEST_PASS();
}

void test_stringbuilder_append(void) {
    TEST_START("stringbuilder_append");

    string_builder sb = stringbuilder_new(0);

    stringbuilder_append(sb, "Hello");
    assert(stringbuilder_length(sb) == 5);

    stringbuilder_append(sb, " ");
    stringbuilder_append(sb, "World");
    assert(stringbuilder_length(sb) == 11);

    string result = stringbuilder_toString(sb);
    assert(strcmp(result, "Hello World") == 0);

    string_dispose(result);
    stringbuilder_dispose(sb);

    TEST_PASS();
}

void test_stringbuilder_appendf(void) {
    TEST_START("stringbuilder_appendf");

    string_builder sb = stringbuilder_new(0);

    stringbuilder_appendf(sb, "Number: %d", 42);
    assert(stringbuilder_length(sb) == 10);

    stringbuilder_appendf(sb, ", Float: %.2f", 3.14);

    string result = stringbuilder_toString(sb);
    assert(strcmp(result, "Number: 42, Float: 3.14") == 0);

    string_dispose(result);
    stringbuilder_dispose(sb);

    TEST_PASS();
}

void test_stringbuilder_appendl(void) {
    TEST_START("stringbuilder_appendl");

    string_builder sb = stringbuilder_new(0);

    stringbuilder_appendl(sb, "Line 1");
    stringbuilder_appendl(sb, "Line 2");
    stringbuilder_appendl(sb, NULL);  // just newline
    stringbuilder_appendl(sb, "Line 4");

    string result = stringbuilder_toString(sb);
    assert(strcmp(result, "Line 1\nLine 2\n\nLine 4\n") == 0);

    string_dispose(result);
    stringbuilder_dispose(sb);

    TEST_PASS();
}

void test_stringbuilder_lappends(void) {
    TEST_START("stringbuilder_lappends");

    string_builder sb = stringbuilder_new(0);

    stringbuilder_append(sb, "Start");
    stringbuilder_lappends(sb, "Line 1");
    stringbuilder_lappends(sb, "Line 2");

    string result = stringbuilder_toString(sb);
    assert(strcmp(result, "Start\nLine 1\nLine 2") == 0);

    string_dispose(result);
    stringbuilder_dispose(sb);

    TEST_PASS();
}

void test_stringbuilder_lappendf(void) {
    TEST_START("stringbuilder_lappendf");

    string_builder sb = stringbuilder_new(0);

    stringbuilder_append(sb, "Header");
    stringbuilder_lappendf(sb, "Value: %d", 100);
    stringbuilder_lappendf(sb, "Another: %s", "test");

    string result = stringbuilder_toString(sb);
    assert(strcmp(result, "Header\nValue: 100\nAnother: test") == 0);

    string_dispose(result);
    stringbuilder_dispose(sb);

    TEST_PASS();
}

void test_stringbuilder_clear(void) {
    TEST_START("stringbuilder_clear");

    string_builder sb = stringbuilder_new(0);

    stringbuilder_append(sb, "Some content");
    assert(stringbuilder_length(sb) > 0);

    stringbuilder_clear(sb);
    assert(stringbuilder_length(sb) == 0);

    string result = stringbuilder_toString(sb);
    assert(strcmp(result, "") == 0);

    string_dispose(result);
    stringbuilder_dispose(sb);

    TEST_PASS();
}

void test_stringbuilder_toString(void) {
    TEST_START("stringbuilder_toString");

    string_builder sb = stringbuilder_new(0);
    stringbuilder_append(sb, "convert");

    string s1 = stringbuilder_toString(sb);
    assert(s1 != NULL);
    assert(strcmp(s1, "convert") == 0);

    // Append more and convert again
    stringbuilder_append(sb, " more");
    string s2 = stringbuilder_toString(sb);
    assert(strcmp(s2, "convert more") == 0);

    string_dispose(s1);
    string_dispose(s2);
    stringbuilder_dispose(sb);

    TEST_PASS();
}

void test_stringbuilder_capacity(void) {
    TEST_START("stringbuilder_capacity");

    string_builder sb = stringbuilder_new(64);
    assert(stringbuilder_capacity(sb) >= 64);

    usize initial_capacity = stringbuilder_capacity(sb);

    // Set larger capacity
    stringbuilder_setCapacity(sb, 128);
    assert(stringbuilder_capacity(sb) >= 128);
    assert(stringbuilder_capacity(sb) > initial_capacity);

    // Set smaller capacity (should be ignored)
    stringbuilder_setCapacity(sb, 32);
    assert(stringbuilder_capacity(sb) >= 128);  // unchanged

    stringbuilder_dispose(sb);

    TEST_PASS();
}

void test_stringbuilder_dispose_null(void) {
    TEST_START("stringbuilder_dispose (NULL safe)");

    // Dispose NULL should not crash
    stringbuilder_dispose(NULL);

    TEST_PASS();
}

void test_stringbuilder_lifecycle(void) {
    TEST_START("stringbuilder lifecycle");

    // Create builder
    string_builder sb = stringbuilder_new(16);
    assert(sb != NULL);

    // Build up content
    stringbuilder_append(sb, "Part1");
    stringbuilder_appendf(sb, " %d", 2);
    stringbuilder_appendl(sb, " Part3");

    usize len = stringbuilder_length(sb);
    assert(len > 0);

    // Convert to string
    string final = stringbuilder_toString(sb);
    assert(strcmp(final, "Part1 2 Part3\n") == 0);

    // Cleanup
    string_dispose(final);
    stringbuilder_dispose(sb);

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
