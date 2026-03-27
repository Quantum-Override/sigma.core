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
 * File: application.c
 * Description: Application implementation (FR-2603-sigma-core-004)
 *
 * TDD GREEN PHASE: Full implementation that makes tests PASS
 */

#include "sigma.core/application.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

// ============================================================
// Forward declarations
// ============================================================
static void application_set_allocator(sc_alloc_use_t *use);
static sc_alloc_use_t *application_get_allocator(void);

// External SLB0 functions (from sigma.memory)
// These will be linked when sigma.memory is present
extern void *slb0_alloc(usize size) __attribute__((weak));
extern void slb0_free(void *ptr) __attribute__((weak));
extern void *slb0_realloc(void *ptr, usize size) __attribute__((weak));

// ============================================================
// Module State
// ============================================================

// Application allocator storage
static sc_alloc_use_t *s_app_allocator = NULL;
static bool s_allocator_set = false;

// ============================================================
// API Interface
// ============================================================

const sc_application_i Application = {.set_allocator = application_set_allocator,
                                      .get_allocator = application_get_allocator};

// ============================================================
// Helper Functions
// ============================================================

/**
 * @brief Get SLB0 fallback hook (static storage)
 * @return Static SLB0 hook that never changes
 *
 * Used when application never calls set_allocator().
 * Falls back to malloc/free if sigma.memory not linked.
 */
static sc_alloc_use_t *get_slb0_hook(void) {
    static sc_alloc_use_t slb0_hook = {
        .ctrl = NULL,  // SLB0 controller not accessible from sigma.core
        .alloc = NULL,
        .release = NULL,
        .resize = NULL,
        .frame_begin = NULL,
        .frame_end = NULL};

    // Initialize function pointers on first call
    if (slb0_hook.alloc == NULL) {
        // Use SLB0 if available (weak linkage), otherwise fall back to malloc
        slb0_hook.alloc = slb0_alloc ? (void *(*)(usize))slb0_alloc : malloc;
        slb0_hook.release = slb0_free ? (void (*)(void *))slb0_free : free;
        slb0_hook.resize = slb0_realloc ? (void *(*)(void *, usize))slb0_realloc : realloc;
    }

    return &slb0_hook;
}

// ============================================================
// API Implementations
// ============================================================

/**
 * @brief Set application-wide allocator
 * @param use Allocator hook (NULL resets to default)
 *
 * CONSTRAINTS:
 * - Should be called from module_init (single-init pattern)
 * - Calling twice asserts in debug builds
 * - NULL resets to default SLB0
 */
static void application_set_allocator(sc_alloc_use_t *use) {
    if (use == NULL) {
        // NULL = reset to default
        s_app_allocator = NULL;
        s_allocator_set = false;
        return;
    }

    // Assert on double-init (debug builds only)
    assert(!s_allocator_set && "Application.set_allocator() called twice");

    s_app_allocator = use;
    s_allocator_set = true;
}

/**
 * @brief Get current application-wide allocator
 * @return Allocator hook (never NULL)
 *
 * Returns stored allocator if set, otherwise SLB0 fallback.
 * Thread-safe for reads after initialization.
 */
static sc_alloc_use_t *application_get_allocator(void) {
    if (s_app_allocator != NULL) {
        return s_app_allocator;
    }

    // Safe fallback to SLB0
    return get_slb0_hook();
}
