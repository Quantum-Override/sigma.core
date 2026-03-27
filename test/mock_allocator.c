/*
 * Sigma.Core
 * Copyright (c) 2026 David Boarman (BadKraft) and contributors
 * QuantumOverride [Q|]
 * ----------------------------------------------
 * File: mock_allocator.c
 * Description: Minimal mock Allocator for sigma.core unit tests
 *
 * Provides minimal Allocator facade implementation for testing sigma.core
 * without sigma.memory dependency. Uses malloc/free directly.
 *
 * NOTE: This is ONLY for sigma.core unit tests. Real applications should
 * use sigma.memory package which provides full allocator implementation.
 */

#include <assert.h>
#include <stdlib.h>
#include "sigma.core/allocator.h"

// ============================================================
// Mock implementations (minimal, uses malloc/free)
// ============================================================

static slab mock_acquire(usize size) {
    // Not implemented for mock - tests shouldn't call this
    (void)size;
    assert(0 && "mock_acquire not implemented - use Application.set_allocator() instead");
    return NULL;
}

static void mock_release(sc_ctrl_base_s *ctrl) {
    // Not implemented for mock
    (void)ctrl;
    assert(0 && "mock_release not implemented");
}

static bump_allocator mock_create_bump(usize size) {
    // Not implemented for mock
    (void)size;
    assert(0 && "mock_create_bump not implemented");
    return NULL;
}

static reclaim_allocator mock_create_reclaim(usize size) {
    // Not implemented for mock
    (void)size;
    assert(0 && "mock_create_reclaim not implemented");
    return NULL;
}

// Simple malloc wrapper
static object mock_alloc(usize size) { return malloc(size); }

// Simple free wrapper
static void mock_dispose(object ptr) { free(ptr); }

// Simple realloc wrapper
static object mock_realloc(object ptr, usize new_size) { return realloc(ptr, new_size); }

static sc_ctrl_base_s *mock_create_custom(usize size, ctrl_factory_fn factory) {
    // Not implemented for mock
    (void)size;
    (void)factory;
    assert(0 && "mock_create_custom not implemented");
    return NULL;
}

static void mock_register_ctrl(sc_ctrl_base_s *ctrl) {
    // Not implemented for mock
    (void)ctrl;
    assert(0 && "mock_register_ctrl not implemented");
}

static bool mock_is_ready(void) {
    // Mock is always "ready" (malloc available)
    return true;
}

// ============================================================
// Global Allocator facade instance
// ============================================================

const sc_allocator_i Allocator = {.acquire = mock_acquire,
                                  .release = mock_release,
                                  .create_bump = mock_create_bump,
                                  .create_reclaim = mock_create_reclaim,
                                  .alloc = mock_alloc,
                                  .dispose = mock_dispose,
                                  .realloc = mock_realloc,
                                  .create_custom = mock_create_custom,
                                  .register_ctrl = mock_register_ctrl,
                                  .is_ready = mock_is_ready};
