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
 * File: test_alloc_use_ctrl.c
 * Description: Test suite for sc_alloc_use_t.ctrl field (FR-2603-sigma-core-005)
 *
 * TDD Phase 1A Step 1: Write tests FIRST (RED - will not compile yet)
 * - sc_alloc_use_t doesn't have .ctrl field yet
 * - Tests define expected behavior (offset 0, cast compatibility)
 * - Implementation in Step 3 will make tests pass (GREEN)
 */

#include <sigma.core/allocator.h>
#include <sigma.test/sigtest.h>
#include <stddef.h>
#include <stdlib.h>

// Test helper: mock controller base structure
typedef struct mock_ctrl_s {
    int id;
    char name[32];
} mock_ctrl_s;

// Test helper: mock allocator functions
static void *mock_alloc(usize size) { return malloc(size); }

static void mock_release(void *ptr) { free(ptr); }

static void *mock_realloc(void *ptr, usize size) { return realloc(ptr, size); }

// TC1: ctrl field is at offset 0 (first member guarantee)
void test_ctrl_offset_zero(void) {
    // Verify ctrl is first member at offset 0
    usize ctrl_offset = offsetof(sc_alloc_use_t, ctrl);
    Assert.areEqual(&(usize){0}, &ctrl_offset, LONG,
                    "ctrl field must be at offset 0 (first member)");
}

// TC2: Hook construction populates ctrl field
void test_hook_construction_with_ctrl(void) {
    // Create mock controller
    mock_ctrl_s mock_ctrl = {.id = 42, .name = "test_controller"};

    // Create hook with ctrl populated
    sc_alloc_use_t use = {.ctrl = (sc_ctrl_base_s *)&mock_ctrl,
                          .alloc = mock_alloc,
                          .release = mock_release,
                          .resize = mock_realloc,
                          .frame_begin = NULL,
                          .frame_end = NULL};

    // Verify ctrl is set correctly
    Assert.isNotNull(use.ctrl, "ctrl should be populated");
    Assert.isTrue(use.ctrl == (sc_ctrl_base_s *)&mock_ctrl, "ctrl should point to the controller");

    // Cast back and verify
    mock_ctrl_s *retrieved = (mock_ctrl_s *)use.ctrl;
    Assert.areEqual(&(int){42}, &retrieved->id, INT, "Controller ID should match");
}

// TC3: Cast compatibility (sc_alloc_use_t* → sc_ctrl_base_s*)
void test_cast_use_to_ctrl(void) {
    // Create mock controller
    mock_ctrl_s mock_ctrl = {.id = 123, .name = "cast_test"};

    // Create hook
    sc_alloc_use_t use = {.ctrl = (sc_ctrl_base_s *)&mock_ctrl,
                          .alloc = mock_alloc,
                          .release = mock_release,
                          .resize = mock_realloc,
                          .frame_begin = NULL,
                          .frame_end = NULL};

    // Cast use pointer to ctrl pointer
    sc_alloc_use_t *use_ptr = &use;

    // Because ctrl is first member at offset 0, dereferencing use_ptr as ctrl** gives ctrl value
    Assert.isTrue(*(sc_ctrl_base_s **)use_ptr == use.ctrl,
                  "Dereferencing sc_alloc_use_t* as sc_ctrl_base_s** should yield ctrl pointer");
}

// TC4: Cast compatibility (sc_ctrl_base_s* → sc_alloc_use_t*)
void test_cast_ctrl_to_use(void) {
    // Create hook with ctrl as first member
    mock_ctrl_s mock_ctrl = {.id = 456, .name = "reverse_cast"};

    sc_alloc_use_t use = {.ctrl = (sc_ctrl_base_s *)&mock_ctrl,
                          .alloc = mock_alloc,
                          .release = mock_release,
                          .resize = mock_realloc,
                          .frame_begin = NULL,
                          .frame_end = NULL};

    // Get ctrl pointer
    sc_ctrl_base_s *ctrl_ptr = use.ctrl;

    // Cast back to use pointer (round-trip)
    // NOTE: This only works if ctrl is first member (offset 0)
    sc_alloc_use_t *use_ptr = (sc_alloc_use_t *)&use;

    // Verify round-trip
    Assert.isTrue(use_ptr == &use, "Round-trip cast should preserve use pointer");
    Assert.isTrue(use_ptr->ctrl == ctrl_ptr, "Round-trip should preserve ctrl value");
}

// TC5: NULL ctrl for non-controller hooks (valid use case)
void test_null_ctrl_valid(void) {
    // Some hooks don't have controller state (e.g., malloc wrapper)
    sc_alloc_use_t use = {.ctrl = NULL,  // No controller
                          .alloc = mock_alloc,
                          .release = mock_release,
                          .resize = mock_realloc,
                          .frame_begin = NULL,
                          .frame_end = NULL};

    // NULL ctrl is valid
    Assert.isNull(use.ctrl, "ctrl can be NULL for non-controller hooks");

    // Hook should still work
    void *ptr = use.alloc(64);
    Assert.isNotNull(ptr, "Hook with NULL ctrl should still allocate");
    use.release(ptr);
}

// Register tests
static void register_alloc_use_ctrl_tests(void) {
    testset("alloc_use_ctrl_field", NULL, NULL);

    testcase("ctrl field at offset 0", test_ctrl_offset_zero);
    testcase("Hook construction populates ctrl", test_hook_construction_with_ctrl);
    testcase("Cast use* to ctrl*", test_cast_use_to_ctrl);
    testcase("Cast ctrl* to use* (round-trip)", test_cast_ctrl_to_use);
    testcase("NULL ctrl valid for non-controller hooks", test_null_ctrl_valid);
}

__attribute__((constructor)) static void enqueue_alloc_use_ctrl_tests(void) {
    Tests.enqueue(register_alloc_use_ctrl_tests);
}
