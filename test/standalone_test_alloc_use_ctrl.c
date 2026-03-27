/*
 * Sigma.Core
 * Copyright (c) 2026 David Boarman (BadKraft) and contributors
 * QuantumOverride [Q|]
 * ----------------------------------------------
 * File: standalone_test_alloc_use_ctrl.c
 * Description: Standalone test for sc_alloc_use_t.ctrl field (FR-005)
 *
 * Tests ctrl field offset, cast compatibility, and NULL validity
 * without depending on sigma.test framework.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sigma.core/allocator.h"

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
 * TC1: ctrl field is at offset 0
 */
void test_ctrl_offset_zero(void) {
    TEST_START("ctrl field at offset 0");

    size_t ctrl_offset = offsetof(sc_alloc_use_t, ctrl);

    ASSERT_TRUE(ctrl_offset == 0, "ctrl must be first member (offset 0)");

    printf("  INFO: ctrl offset = %zu bytes\n", ctrl_offset);
}

/**
 * TC2: Construct hook with ctrl, verify via cast
 */
void test_hook_construction_with_ctrl(void) {
    TEST_START("Hook construction with ctrl pointer");

    // Create a mock controller pointer (we don't need actual ctrl_base_s, just a pointer)
    void *mock_ctrl = (void *)0xDEADBEEF;  // Fake pointer for testing

    // Create hook with ctrl
    sc_alloc_use_t hook = {.ctrl = (sc_ctrl_base_s *)mock_ctrl,
                           .alloc = malloc,
                           .release = free,
                           .resize = realloc,
                           .frame_begin = NULL,
                           .frame_end = NULL};

    ASSERT_TRUE(hook.ctrl == (sc_ctrl_base_s *)mock_ctrl,
                "ctrl field should match assigned pointer");
    ASSERT_TRUE((void *)hook.ctrl == mock_ctrl, "ctrl pointer value should preserve address");
}

/**
 * TC3: Cast sc_alloc_use_t* → sc_ctrl_base_s*
 */
void test_cast_use_to_ctrl(void) {
    TEST_START("Cast sc_alloc_use_t* to sc_ctrl_base_s*");

    // Create hook with ctrl
    void *mock_ctrl = (void *)0xCAFEBABE;

    sc_alloc_use_t hook = {.ctrl = (sc_ctrl_base_s *)mock_ctrl,
                           .alloc = malloc,
                           .release = free,
                           .resize = realloc,
                           .frame_begin = NULL,
                           .frame_end = NULL};

    // Cast sc_alloc_use_t* → sc_ctrl_base_s* (reads first member)
    sc_ctrl_base_s *ctrl_via_cast = *(sc_ctrl_base_s **)&hook;

    ASSERT_TRUE(ctrl_via_cast == (sc_ctrl_base_s *)mock_ctrl, "Cast should yield ctrl pointer");
    ASSERT_TRUE((void *)ctrl_via_cast == mock_ctrl, "Cast should preserve address");
}

/**
 * TC4: Round-trip cast compatibility
 */
void test_cast_ctrl_to_use(void) {
    TEST_START("Round-trip cast compatibility");

    // Create hook with ctrl
    void *mock_ctrl = (void *)0x12345678;

    sc_alloc_use_t hook = {.ctrl = (sc_ctrl_base_s *)mock_ctrl,
                           .alloc = malloc,
                           .release = free,
                           .resize = realloc,
                           .frame_begin = NULL,
                           .frame_end = NULL};

    // Round trip: use → ctrl
    sc_alloc_use_t *use_ptr = &hook;
    sc_ctrl_base_s *ctrl_ptr = *(sc_ctrl_base_s **)use_ptr;

    ASSERT_TRUE(ctrl_ptr == (sc_ctrl_base_s *)mock_ctrl, "Should extract ctrl correctly");
    ASSERT_TRUE((void *)ctrl_ptr == mock_ctrl, "Should preserve address through cast");

    // For proper round-trip, test with a controller-like struct:
    typedef struct {
        sc_alloc_use_t base;  // Must be first member
        int custom_data;
    } test_controller_t;

    test_controller_t test_ctrl = {.base = hook, .custom_data = 456};

    // Cast controller → sc_alloc_use_t*
    sc_alloc_use_t *use_from_ctrl = (sc_alloc_use_t *)&test_ctrl;

    ASSERT_TRUE(use_from_ctrl->ctrl == (sc_ctrl_base_s *)mock_ctrl,
                "Cast from controller should preserve ctrl");
    ASSERT_TRUE(use_from_ctrl->alloc == malloc, "Cast should preserve function pointers");
}

/**
 * TC5: NULL ctrl is valid for non-controller hooks
 */
void test_null_ctrl_valid(void) {
    TEST_START("NULL ctrl is valid");

    // Create hook with NULL ctrl (non-controller hook)
    sc_alloc_use_t hook = {.ctrl = NULL,  // Valid for standalone hooks
                           .alloc = malloc,
                           .release = free,
                           .resize = realloc,
                           .frame_begin = NULL,
                           .frame_end = NULL};

    ASSERT_TRUE(hook.ctrl == NULL, "NULL ctrl should be accepted");
    ASSERT_TRUE(hook.alloc != NULL, "Hook should still be functional with NULL ctrl");

    // Test that hook works even with NULL ctrl
    void *ptr = hook.alloc(64);
    ASSERT_TRUE(ptr != NULL, "alloc should work with NULL ctrl");
    hook.release(ptr);
}

// ============================================================
// Main test runner
// ============================================================

int main(void) {
    printf("================================================================================\n");
    printf(" Standalone sc_alloc_use_t.ctrl Tests (sigma.core Phase 1, FR-005)\n");
    printf("================================================================================\n");

    test_ctrl_offset_zero();
    test_hook_construction_with_ctrl();
    test_cast_use_to_ctrl();
    test_cast_ctrl_to_use();
    test_null_ctrl_valid();

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
