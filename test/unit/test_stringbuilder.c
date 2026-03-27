/*
 * Sigma.Text
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
 * File: test_stringbuilder.c
 * Description: Test suite for StringBuilder interface utilities
 */
#include <sigma.core/strings.h>
#include <sigma.test/sigtest.h>
#include <string.h>

void set_config_sb(FILE **log_stream) { *log_stream = fopen("logs/test_stringbuilder.log", "w"); }

void set_teardown_sb(void) {
    // No teardown needed
}

// Test StringBuilder creation
void test_stringbuilder_new(void) {
    string_builder sb = StringBuilder.new(64);
    Assert.isNotNull(sb, "StringBuilder creation failed");
    StringBuilder.dispose(sb);
}

// Test StringBuilder append
void test_stringbuilder_append(void) {
    string_builder sb = StringBuilder.new(32);
    Assert.isNotNull(sb, "StringBuilder creation failed");

    StringBuilder.append(sb, "Hello");
    StringBuilder.append(sb, ", ");
    StringBuilder.append(sb, "World!");

    string result = StringBuilder.toString(sb);
    Assert.areEqual(&(int){0}, &(int){String.compare(result, "Hello, World!")}, INT,
                    "StringBuilder append mismatch");

    String.dispose(result);
    StringBuilder.dispose(sb);
}

// Test StringBuilder appendf
void test_stringbuilder_appendf(void) {
    string_builder sb = StringBuilder.new(64);
    Assert.isNotNull(sb, "StringBuilder creation failed");

    StringBuilder.appendf(sb, "ID: %d", 42);
    StringBuilder.appendf(sb, ", Name: %s", "Test");

    string result = StringBuilder.toString(sb);
    Assert.areEqual(&(int){0}, &(int){String.compare(result, "ID: 42, Name: Test")}, INT,
                    "StringBuilder appendf mismatch");

    String.dispose(result);
    StringBuilder.dispose(sb);
}

// Test StringBuilder clear
void test_stringbuilder_clear(void) {
    string_builder sb = StringBuilder.new(32);
    StringBuilder.append(sb, "Initial content");
    StringBuilder.clear(sb);

    string result = StringBuilder.toString(sb);
    Assert.areEqual(&(size_t){0}, &(size_t){String.length(result)}, LONG,
                    "StringBuilder clear should result in empty string");

    String.dispose(result);
    StringBuilder.dispose(sb);
}

// Test StringBuilder capacity growth
void test_stringbuilder_capacity(void) {
    string_builder sb = StringBuilder.new(2);  // Very small initial capacity

    // Append enough data to force multiple expansions
    for (int i = 0; i < 100; i++) {
        StringBuilder.append(sb, "x");
    }

    string result = StringBuilder.toString(sb);
    Assert.areEqual(&(size_t){100}, &(size_t){String.length(result)}, LONG,
                    "StringBuilder should handle capacity growth");

    String.dispose(result);
    StringBuilder.dispose(sb);
}

// Test StringBuilder length
void test_stringbuilder_length(void) {
    string_builder sb = StringBuilder.new(32);
    StringBuilder.append(sb, "Test");

    Assert.areEqual(&(size_t){4}, &(size_t){StringBuilder.length(sb)}, LONG,
                    "StringBuilder length mismatch");

    StringBuilder.dispose(sb);
}

// Test StringBuilder capacity
void test_stringbuilder_get_capacity(void) {
    size_t initial_capacity = 64;
    string_builder sb = StringBuilder.new(initial_capacity);

    size_t capacity = StringBuilder.capacity(sb);
    Assert.isTrue(capacity >= initial_capacity,
                  "StringBuilder capacity should be at least initial capacity");

    StringBuilder.dispose(sb);
}

// Test StringBuilder snew (create from string)
void test_stringbuilder_snew(void) {
    string_builder sb = StringBuilder.snew("Initial content");
    Assert.isNotNull(sb, "StringBuilder snew failed");

    string result = StringBuilder.toString(sb);
    Assert.areEqual(&(int){0}, &(int){String.compare(result, "Initial content")}, INT,
                    "StringBuilder snew content mismatch");

    String.dispose(result);
    StringBuilder.dispose(sb);
}

// Test StringBuilder appendl (append with newline)
void test_stringbuilder_appendl(void) {
    string_builder sb = StringBuilder.new(64);
    StringBuilder.append(sb, "Line1");
    StringBuilder.appendl(sb, "Line2");
    StringBuilder.appendl(sb, NULL);  // Just newline

    string result = StringBuilder.toString(sb);
    Assert.areEqual(&(int){0}, &(int){String.compare(result, "Line1Line2\n\n")}, INT,
                    "StringBuilder appendl mismatch");

    String.dispose(result);
    StringBuilder.dispose(sb);
}

// Test StringBuilder setCapacity
void test_stringbuilder_set_capacity(void) {
    string_builder sb = StringBuilder.new(32);
    StringBuilder.append(sb, "Test");

    StringBuilder.setCapacity(sb, 128);
    size_t new_capacity = StringBuilder.capacity(sb);
    Assert.isTrue(new_capacity >= 128, "StringBuilder setCapacity should increase capacity");

    // Verify content preserved
    string result = StringBuilder.toString(sb);
    Assert.areEqual(&(int){0}, &(int){String.compare(result, "Test")}, INT,
                    "StringBuilder content should be preserved after setCapacity");

    String.dispose(result);
    StringBuilder.dispose(sb);
}

// Register tests
static void register_stringbuilder_tests(void) {
    testset("text_stringbuilder_set", set_config_sb, set_teardown_sb);

    testcase("StringBuilder new", test_stringbuilder_new);
    testcase("StringBuilder append", test_stringbuilder_append);
    testcase("StringBuilder appendf", test_stringbuilder_appendf);
    testcase("StringBuilder clear", test_stringbuilder_clear);
    testcase("StringBuilder capacity growth", test_stringbuilder_capacity);
    testcase("StringBuilder length", test_stringbuilder_length);
    testcase("StringBuilder get capacity", test_stringbuilder_get_capacity);
    testcase("StringBuilder snew", test_stringbuilder_snew);
    testcase("StringBuilder appendl", test_stringbuilder_appendl);
    testcase("StringBuilder set capacity", test_stringbuilder_set_capacity);
}

__attribute__((constructor)) static void enqueue_stringbuilder_tests(void) {
    Tests.enqueue(register_stringbuilder_tests);
}
