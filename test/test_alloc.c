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
 * File: test_alloc.c
 * Description: Test suite for Sigma.Core allocation facade
 */

#include <sigtest/sigtest.h>
#include <string.h>
#include "alloc.h"

// Test structure to demonstrate allocation abstraction
typedef struct {
    int id;
    char name[32];
    double value;
} test_item_t;

// Custom allocator for testing (simple wrapper around malloc/free)
static object custom_alloc(usize size) { return malloc(size); }

static void custom_dispose(object ptr) { free(ptr); }

static object custom_calloc(usize nmemb, usize size) { return calloc(nmemb, size); }

static object custom_realloc(object ptr, usize size) { return realloc(ptr, size); }

static const sc_allocator_i custom_allocator = {.alloc = custom_alloc,
                                                .dispose = custom_dispose,
                                                .calloc = custom_calloc,
                                                .realloc = custom_realloc};

// Configure test set
static void set_config(FILE **log_stream) { *log_stream = fopen("logs/test_alloc.log", "w"); }

static void set_teardown(void) {
    // Reset allocator to default
    sc_set_allocator(NULL);
}

// Test default allocator
static void test_default_allocator(void) {
    test_item_t *item = (test_item_t *)sc_alloc(sizeof(test_item_t));
    Assert.isNotNull(item, "Default allocator failed to allocate");

    sc_free(item);
}

// Test custom allocator
static void test_custom_allocator(void) {
    sc_set_allocator(&custom_allocator);

    test_item_t *item = (test_item_t *)sc_alloc(sizeof(test_item_t));
    Assert.isNotNull(item, "Custom allocator failed to allocate");

    sc_free(item);
}

// Test reset to default
static void test_reset_to_default(void) {
    sc_set_allocator(NULL);

    test_item_t *item = (test_item_t *)sc_alloc(sizeof(test_item_t));
    Assert.isNotNull(item, "Reset to default allocator failed");

    sc_free(item);
}

// Test array allocation
static void test_array_allocation(void) {
    usize array_size = 5;
    int *array = (int *)sc_alloc(array_size * sizeof(int));
    Assert.isNotNull(array, "Array allocation failed");

    sc_free(array);
}

// Test sc_calloc
static void test_calloc(void) {
    int *array = (int *)sc_calloc(5, sizeof(int));
    Assert.isNotNull(array, "sc_calloc failed");

    sc_free(array);
}

// Test sc_realloc
static void test_realloc(void) {
    int *array = (int *)sc_alloc(5 * sizeof(int));
    Assert.isNotNull(array, "sc_alloc failed");

    array = (int *)sc_realloc(array, 10 * sizeof(int));
    Assert.isNotNull(array, "sc_realloc failed");

    sc_free(array);
}

// Register test cases
__attribute__((constructor)) void init_core_alloc_tests(void) {
    testset("core_alloc_set", set_config, set_teardown);

    testcase("default_allocator", test_default_allocator);
    testcase("custom_allocator", test_custom_allocator);
    testcase("reset_to_default", test_reset_to_default);
    testcase("array_allocation", test_array_allocation);
    testcase("sc_calloc", test_calloc);
    testcase("sc_realloc", test_realloc);
}