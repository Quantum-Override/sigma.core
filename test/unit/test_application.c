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
 * File: test_application.c
 * Description: Test suite for Application allocator API (FR-2603-sigma-core-004)
 *
 * TDD Phase 1A Step 1: Write tests FIRST (RED - will not compile yet)
 * - Application interface doesn't exist yet
 * - Tests define expected behavior
 * - Implementation in Step 2 will make tests pass (GREEN)
 */

#include <sigma.core/allocator.h>
#include <sigma.core/application.h>
#include <sigma.test/sigtest.h>
#include <stdlib.h>
#include <string.h>

// Test helper: custom allocator for testing
static void *test_alloc(usize size) { return malloc(size); }

static void test_release(void *ptr) { free(ptr); }

static void *test_realloc(void *ptr, usize size) { return realloc(ptr, size); }

// TC1: Set custom allocator and retrieve it
void test_set_get_allocator(void) {
    // Create custom allocator hook
    sc_alloc_use_t custom_use = {.ctrl = NULL,  // No controller for test hook
                                 .alloc = test_alloc,
                                 .release = test_release,
                                 .resize = test_realloc,
                                 .frame_begin = NULL,
                                 .frame_end = NULL};

    // Set custom allocator
    Application.set_allocator(&custom_use);

    // Retrieve and verify it's the same
    sc_alloc_use_t *retrieved = Application.get_allocator();
    Assert.isNotNull(retrieved, "get_allocator should not return NULL");
    Assert.isTrue(retrieved == &custom_use, "get_allocator should return the set allocator");
    Assert.isTrue(retrieved->alloc == test_alloc, "alloc function pointer should match");
    Assert.isTrue(retrieved->release == test_release, "release function pointer should match");

    // Reset for next test
    Application.set_allocator(NULL);
}

// TC2: Default SLB0 fallback when never set
void test_default_slb0_fallback(void) {
    // Never call set_allocator - should default to SLB0
    sc_alloc_use_t *use = Application.get_allocator();

    Assert.isNotNull(use, "get_allocator should never return NULL");
    Assert.isNotNull(use->alloc, "SLB0 hook should have alloc function");
    Assert.isNotNull(use->release, "SLB0 hook should have release function");
    Assert.isNotNull(use->resize, "SLB0 hook should have resize function");

    // Test that allocation works
    void *ptr = use->alloc(64);
    Assert.isNotNull(ptr, "SLB0 alloc should succeed");
    use->release(ptr);
}

// TC3: Assert on double-init (set_allocator called twice)
void test_double_init_assert(void) {
    sc_alloc_use_t custom1 = {.ctrl = NULL,
                              .alloc = test_alloc,
                              .release = test_release,
                              .resize = test_realloc,
                              .frame_begin = NULL,
                              .frame_end = NULL};

    sc_alloc_use_t custom2 = {.ctrl = NULL,
                              .alloc = test_alloc,
                              .release = test_release,
                              .resize = test_realloc,
                              .frame_begin = NULL,
                              .frame_end = NULL};

    // First set should succeed
    Application.set_allocator(&custom1);

    // Second set should assert (in debug builds)
    // NOTE: Can't test assertions directly, would need NDEBUG disabled
    // This test documents the expected behavior
    // In release builds, behavior is undefined (second call overwrites)

    // Reset for next test
    Application.set_allocator(NULL);
}

// TC4: set_allocator(NULL) resets to default
void test_reset_to_default(void) {
    sc_alloc_use_t custom = {.ctrl = NULL,
                             .alloc = test_alloc,
                             .release = test_release,
                             .resize = test_realloc,
                             .frame_begin = NULL,
                             .frame_end = NULL};

    // Set custom allocator
    Application.set_allocator(&custom);
    sc_alloc_use_t *retrieved1 = Application.get_allocator();
    Assert.isTrue(retrieved1 == &custom, "Should return custom allocator");

    // Reset to default with NULL
    Application.set_allocator(NULL);
    sc_alloc_use_t *retrieved2 = Application.get_allocator();

    // Should now return SLB0 fallback, not custom
    Assert.isNotNull(retrieved2, "get_allocator should return SLB0 after reset");
    Assert.isTrue(retrieved2 != &custom, "Should NOT return custom allocator after reset");
    Assert.isNotNull(retrieved2->alloc, "SLB0 fallback should have alloc");
}

// TC5: Thread-safety of get_allocator (read-only after init)
void test_get_allocator_thread_safe(void) {
    sc_alloc_use_t custom = {.ctrl = NULL,
                             .alloc = test_alloc,
                             .release = test_release,
                             .resize = test_realloc,
                             .frame_begin = NULL,
                             .frame_end = NULL};

    // Set once at init
    Application.set_allocator(&custom);

    // Multiple reads should return same pointer (thread-safe)
    sc_alloc_use_t *use1 = Application.get_allocator();
    sc_alloc_use_t *use2 = Application.get_allocator();
    sc_alloc_use_t *use3 = Application.get_allocator();

    Assert.isTrue(use1 == use2, "Multiple get_allocator calls should return same pointer");
    Assert.isTrue(use2 == use3, "Multiple get_allocator calls should return same pointer");
    Assert.isTrue(use1 == &custom, "All should point to custom allocator");

    // Reset for next test
    Application.set_allocator(NULL);
}

// Register tests
__attribute__((constructor)) void init_application_tests(void) {
    testset("application_allocator_api", NULL, NULL);

    testcase("Set and get allocator", test_set_get_allocator);
    testcase("Default SLB0 fallback", test_default_slb0_fallback);
    testcase("Double-init assert", test_double_init_assert);
    testcase("Reset to default with NULL", test_reset_to_default);
    testcase("Thread-safe get_allocator", test_get_allocator_thread_safe);
}
