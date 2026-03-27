/*
 * Sigma.Core
 * Copyright (c) 2026 David Boarman (BadKraft) and contributors
 * QuantumOverride [Q|]
 * ----------------------------------------------
 * File: standalone_test_application.c
 * Description: Standalone test for Application API (no sigma.test dependency)
 *
 * Tests Application.set_allocator() and Application.get_allocator()
 * without depending on sigma.test framework (which has ABI conflicts
 * during FR-005 ctrl field migration).
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sigma.core/allocator.h"
#include "sigma.core/application.h"

// ============================================================
// Test helpers
// ============================================================

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_START(name) printf("\n[TEST] %s\n", name)
#define ASSERT_TRUE(cond, msg)             \
    do {                                   \
        if (!(cond)) {                     \
            printf("  ✗ FAIL: %s\n", msg); \
            tests_failed++;                \
            return;                        \
        } else {                           \
            printf("  ✓ PASS: %s\n", msg); \
            tests_passed++;                \
        }                                  \
    } while (0)

// ============================================================
// Test cases
// ============================================================

/**
 * TC1: Set custom allocator, retrieve it, verify same pointer
 */
void test_set_get_allocator(void) {
    TEST_START("Set and get allocator");

    // Create custom allocator hook
    static sc_alloc_use_t custom = {.ctrl = NULL,
                                    .alloc = malloc,
                                    .release = free,
                                    .resize = realloc,
                                    .frame_begin = NULL,
                                    .frame_end = NULL};

    Application.set_allocator(&custom);
    sc_alloc_use_t *retrieved = Application.get_allocator();

    ASSERT_TRUE(retrieved == &custom, "Should return same allocator pointer");

    // Reset for next test
    Application.set_allocator(NULL);
}

/**
 * TC2: Never set allocator → defaults to SLB0
 */
void test_default_slb0_fallback(void) {
    TEST_START("Default SLB0 fallback");

    // Ensure no allocator is set
    Application.set_allocator(NULL);

    sc_alloc_use_t *fallback = Application.get_allocator();

    ASSERT_TRUE(fallback != NULL, "Should return non-NULL fallback");
    ASSERT_TRUE(fallback->alloc != NULL, "Fallback should have alloc function");
    ASSERT_TRUE(fallback->release != NULL, "Fallback should have release function");

    // Test that fallback actually works
    void *ptr = fallback->alloc(64);
    ASSERT_TRUE(ptr != NULL, "Fallback alloc should work");
    fallback->release(ptr);
}

/**
 * TC4: set_allocator(NULL) resets to default SLB0
 */
void test_reset_to_default(void) {
    TEST_START("Reset to default with NULL");

    // Set custom allocator
    static sc_alloc_use_t custom = {.ctrl = NULL,
                                    .alloc = malloc,
                                    .release = free,
                                    .resize = realloc,
                                    .frame_begin = NULL,
                                    .frame_end = NULL};

    Application.set_allocator(&custom);
    sc_alloc_use_t *use1 = Application.get_allocator();
    ASSERT_TRUE(use1 == &custom, "Should return custom allocator");

    // Reset to NULL
    Application.set_allocator(NULL);
    sc_alloc_use_t *use2 = Application.get_allocator();

    ASSERT_TRUE(use2 != NULL, "Should return non-NULL after reset");
    ASSERT_TRUE(use2 != &custom, "Should NOT return custom allocator after reset");
}

/**
 * TC5: Multiple get_allocator calls return same pointer (thread-safe reads)
 */
void test_get_allocator_thread_safe(void) {
    TEST_START("Multiple get_allocator calls return same pointer");

    // Set custom allocator
    static sc_alloc_use_t custom = {.ctrl = NULL,
                                    .alloc = malloc,
                                    .release = free,
                                    .resize = realloc,
                                    .frame_begin = NULL,
                                    .frame_end = NULL};

    Application.set_allocator(&custom);

    sc_alloc_use_t *use1 = Application.get_allocator();
    sc_alloc_use_t *use2 = Application.get_allocator();
    sc_alloc_use_t *use3 = Application.get_allocator();

    ASSERT_TRUE(use1 == use2, "First and second calls should return same pointer");
    ASSERT_TRUE(use2 == use3, "Second and third calls should return same pointer");
    ASSERT_TRUE(use1 == &custom, "All should point to custom allocator");

    // Reset for next test
    Application.set_allocator(NULL);
}

// ============================================================
// Main test runner
// ============================================================

int main(void) {
    printf("================================================================================\n");
    printf(" Standalone Application API Tests (sigma.core Phase 1, FR-004)\n");
    printf("================================================================================\n");

    test_set_get_allocator();
    test_default_slb0_fallback();
    test_reset_to_default();
    test_get_allocator_thread_safe();

    // Note: TC3 (double-init assert) cannot be tested standalone
    // because it triggers an assertion which terminates the process

    printf("\n================================================================================\n");
    printf(" Test Summary: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("================================================================================\n");

    if (tests_failed > 0) {
        printf("\n❌ FAILED: Some tests did not pass\n");
        return 1;
    } else {
        printf("\n✅ SUCCESS: All tests passed!\n");
        return 0;
    }
}
