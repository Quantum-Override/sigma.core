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
 * File: test_text_memory_compat.c
 * Description: Sanity check test verifying sigma.text works with sigma.memory
 *
 * This test uses BBS v0.3.0+ per-test overrides (TEST_REQUIRES/TEST_INCLUDES)
 * to link with sigma.memory.o instead of the default sigma.core.alloc.o.
 *
 * Compatibility is guaranteed by the Allocator abstraction:
 * - sigma.text uses: Allocator.alloc/dispose throughout
 * - sigma.core.alloc: minimal malloc wrapper
 * - sigma.memory: full memory management system
 * Both implement the same Allocator interface, so code works with either.
 *
 * Run with: ctest text_memory_compat
 */
#include <sigma.core/strings.h>
#include <sigma.test/sigtest.h>
#include <string.h>

void set_config_compat(FILE **log_stream) {
    *log_stream = fopen("logs/test_text_memory_compat.log", "w");
}

void set_teardown_compat(void) {
    // No teardown needed
}

// Test basic String operations with sigma.memory allocator
void test_string_basic_ops(void) {
    string str1 = "Hello";
    string str2 = " World";
    string result = String.concat(str1, str2);

    Assert.isNotNull(result, "String concat should succeed");
    Assert.areEqual(&(int){0}, &(int){String.compare(result, "Hello World")}, INT,
                    "String concat result should match");

    String.dispose(result);
}

// Test StringBuilder with sigma.memory allocator
void test_stringbuilder_ops(void) {
    string_builder sb = StringBuilder.new(32);
    Assert.isNotNull(sb, "StringBuilder creation should succeed");

    StringBuilder.append(sb, "Test");
    StringBuilder.append(sb, " String");

    string result = StringBuilder.toString(sb);
    Assert.isNotNull(result, "StringBuilder.toString should succeed");
    Assert.areEqual(&(int){0}, &(int){String.compare(result, "Test String")}, INT,
                    "StringBuilder result should match");

    String.dispose(result);
    StringBuilder.dispose(sb);
}

// Test String.dupe with sigma.memory
void test_string_dupe(void) {
    string original = "Test String";
    string dup = String.dupe(original);

    Assert.isNotNull(dup, "String.dupe should succeed");
    Assert.areEqual(&(int){0}, &(int){String.compare(dup, original)}, INT,
                    "Duplicated string should match original");
    Assert.isTrue(original != dup, "Pointers should be different");

    String.dispose(dup);
}

// Test format operations with sigma.memory
void test_string_format(void) {
    string formatted = String.format("Value: %d", 42);
    Assert.isNotNull(formatted, "String.format should succeed");
    Assert.areEqual(&(int){0}, &(int){String.compare(formatted, "Value: 42")}, INT,
                    "Formatted string should match");

    String.dispose(formatted);
}

// Test multiple allocations and disposals
void test_multiple_allocs(void) {
    const int count = 10;
    string strs[count];

    // Allocate multiple strings
    for (int i = 0; i < count; i++) {
        strs[i] = String.format("String %d", i);
        Assert.isNotNull(strs[i], "Allocation %d should succeed", i);
    }

    // Verify and dispose
    for (int i = 0; i < count; i++) {
        char expected[32];
        snprintf(expected, sizeof(expected), "String %d", i);
        Assert.areEqual(&(int){0}, &(int){String.compare(strs[i], expected)}, INT,
                        "String %d content mismatch", i);
        String.dispose(strs[i]);
    }
}

// Register tests
static void register_memory_compat_tests(void) {
    testset("text_memory_compat", set_config_compat, set_teardown_compat);

    testcase("Basic string operations", test_string_basic_ops);
    testcase("StringBuilder operations", test_stringbuilder_ops);
    testcase("String duplication", test_string_dupe);
    testcase("String formatting", test_string_format);
    testcase("Multiple allocations", test_multiple_allocs);
}

__attribute__((constructor)) static void enqueue_memory_compat_tests(void) {
    Tests.enqueue(register_memory_compat_tests);
}
