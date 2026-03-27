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
 * File: application.h
 * Description: Application-level API for sigma.core (FR-2603-sigma-core-004)
 */

#pragma once

#include "sigma.core/allocator.h"
#include "sigma.core/types.h"

/**
 * @brief Application interface - global application-level configuration
 *
 * Provides application-wide allocator control and future app-level services.
 *
 * Phase 1 (FR-2603-sigma-core-004):
 * - set_allocator() - Configure application-wide allocator
 * - get_allocator() - Retrieve current allocator (defaults to SLB0)
 */
typedef struct sc_application_i {
    /**
     * @brief Set application-wide allocator
     * @param use Allocator hook to use for all allocations
     *
     * CONSTRAINTS:
     * - Must be called from module_init hook (before any allocations)
     * - Must be called at most once (asserts on second call)
     * - Pass NULL to reset to default SLB0
     *
     * THREAD SAFETY: Single-threaded during module init (no lock needed)
     */
    void (*set_allocator)(sc_alloc_use_t *use);

    /**
     * @brief Get current application-wide allocator
     * @return Allocator hook, or SLB0 default if never set
     *
     * GUARANTEES:
     * - Never returns NULL (safe fallback to SLB0)
     * - Callable from any context (read-only, thread-safe after init)
     */
    sc_alloc_use_t *(*get_allocator)(void);
} sc_application_i;

/**
 * @brief Global Application interface instance
 */
extern const sc_application_i Application;
