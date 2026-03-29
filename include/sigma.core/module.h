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
 * File: sigma.core/module.h
 * Description: Module registry and lifecycle API for Sigma.X projects.
 *
 *   Every Sigma.X component declares one static sigma_module_t and registers
 *   it from a constructor().  All init work happens in sigma_module_init_all(),
 *   called once from main().  This eliminates constructor-priority fragility.
 *
 *   Dependency ordering uses Kahn's BFS topo-sort; cycles and missing deps
 *   are handled by a user-replaceable panic hook (default: fprintf + abort).
 */
#pragma once

#include <sigma.core/allocator.h>
#include <sigma.core/types.h>

// ── Opaque forward declarations ────────────────────────────────────────────
// Full definitions live in their respective implementation headers.
// sigma.core/module.h intentionally does NOT pull in sigma.memory/memory.h.
typedef struct sc_trusted_cap_s sc_trusted_cap_t;  // full def in sigma.memory/memory.h
typedef struct sc_alloc_use_s sc_alloc_use_t;      // full def in sigma.core/allocator.h

// ── Module roles ───────────────────────────────────────────────────────────
/**
 * @brief Role determines what ctx is passed to init() and in what order modules
 *        are initialised relative to sigma.memory.
 *
 *   PERIPHERAL   — Supporting libraries; init(ctx) where ctx is determined by alloc field.
 *                  Used by sigma.text, sigma.collections, anvil, etc.
 *                  Zero-init default for library modules.
 *   TRUSTED      — Ring1 services; init(sc_trusted_cap_t*); sigma.memory grants a full
 *                  capability struct. Used by sigma.tasking (Ring1 system slots).
 *   APPLICATION  — Executables with main(); init(sc_trusted_cap_t*); sigma.memory grants
 *                  a capability from the app-tier pool. First-party modules we own and
 *                  audit (e.g. sigma.test, sigma.cli). Use Module.set_bootstrap() for
 *                  early setup.
 *   SYSTEM       — Core infrastructure; init(NULL); comes first in topo order.
 *                  Only sigma.memory uses this role. Must be explicitly set (value 99).
 */
typedef enum {
    SIGMA_ROLE_PERIPHERAL = 0,  /**< supporting libraries (zero-init default) */
    SIGMA_ROLE_TRUSTED = 1,     /**< Ring1 services */
    SIGMA_ROLE_APPLICATION = 2, /**< executables with main() */
    SIGMA_ROLE_SYSTEM = 99,     /**< core infrastructure (sigma.memory only) */
} sc_module_role;

// ── Allocator designation ──────────────────────────────────────────────────
/**
 * @brief Controls what ctx is passed to init() for PERIPHERAL modules.
 *        (Ignored for TRUSTED, APPLICATION, and SYSTEM roles.)
 *
 *   DEFAULT — ctx = NULL.  Module uses Application.get_allocator() (configured
 *             via bootstrap or defaults to SLB0). Zero-init default.
 *   SYSTEM  — ctx = sc_alloc_use_t* pointing to {malloc, free, realloc}.
 *             Module receives stdlib hooks with no slab involvement.
 *   ARENA   — ctx = sc_alloc_use_t* returned by the registered arena_provider.
 *             sigma.memory provides a dedicated arena (sigma.test archetype).
 *             NULL ctx if no arena_provider is registered — module falls back
 *             to malloc internally.
 *   CUSTOM  — ctx = mod->alloc_hooks.  Caller supplies all three fn pointers.
 */
typedef enum {
    SIGMA_ALLOC_DEFAULT = 0,
    SIGMA_ALLOC_SYSTEM = 1,
    SIGMA_ALLOC_ARENA = 2,
    SIGMA_ALLOC_CUSTOM = 3,
} sc_module_alloc;

// ── Module descriptor ──────────────────────────────────────────────────────
/**
 * @brief Static descriptor declaring a module's identity, role, and lifecycle.
 *
 * Declare one static instance per project in that project's module.c file.
 * The descriptor must remain valid for the process lifetime — use static storage.
 *
 * @field deps  NULL-terminated array of module name strings this module depends
 *              on.  Use `static const char *no_deps[] = { NULL }` for none.
 * @field init  Returns 0 on success; non-zero aborts sigma_module_init_all().
 * @field shutdown  May be NULL if no cleanup is needed.
 */
typedef struct sigma_module_s {
    const char *name;
    const char *version;
    sc_module_role role;
    sc_module_alloc alloc;       /**< ignored for SIGMA_ROLE_TRUSTED */
    sc_alloc_use_t *alloc_hooks; /**< non-NULL only for SIGMA_ALLOC_CUSTOM */
    const char **deps;
    int (*init)(void *ctx);
    void (*shutdown)(void);
    /**
     * @field arena_size  Requested dedicated slab size for SIGMA_ROLE_TRUSTED modules.
     *                    0 = use system default (256 KB).  Ignored for other roles.
     */
    usize arena_size;
    /**
     * @field arena_policy  Allocator type for the trusted module's dedicated slab.
     *                      POLICY_RECLAIM — first-fit free list; good for arbitrary
     *                      alloc/free (e.g. fiber stacks).
     *                      POLICY_BUMP — frame-based; fast but only releases whole frames.
     *                      POLICY_KERNEL is reserved for sigma.memory itself.
     *                      0 (unset) falls back to POLICY_RECLAIM.
     */
    sc_alloc_policy arena_policy;
} sigma_module_t;

// ── Panic function type ────────────────────────────────────────────────────
/**
 * @brief Called on unrecoverable module system errors (missing dep, cycle).
 *
 * The default implementation calls fprintf(stderr, ...) then abort().
 * A custom handler may longjmp or otherwise unwind — but it must not return.
 * Returning from a panic handler produces undefined behaviour.
 *
 * @param module_name  Name of the module that triggered the panic.
 * @param reason       Human-readable error description (e.g. dep name for
 *                     missing-dep, "circular dependency" for cycles).
 */
typedef void (*sc_module_panic_fn)(const char *module_name, const char *reason);

// ── Provider hook types ─────────────────────────────────────────────────────
// Registered by sigma.memory's init — sigma.core has no compile-time dep on it.

/** @brief Registered by sigma.memory to grant a capability to TRUSTED modules.
 *
 *  @param module_name  Name of the requesting module.
 *  @param arena_size   Requested slab size (bytes); 0 = system default (256 KB).
 *  @param arena_policy Requested controller policy (POLICY_RECLAIM or POLICY_BUMP);
 *                      POLICY_KERNEL is reserved and will cause the grant to fail.
 *  @return Heap-allocated sc_trusted_cap_t*, or NULL on failure.
 */
typedef sc_trusted_cap_t *(*sc_trusted_grant_fn)(const char *module_name, usize arena_size,
                                                 sc_alloc_policy arena_policy);

/** @brief Registered by sigma.memory to grant a capability to TRUSTED_APP modules.
 *
 *  Identical signature to sc_trusted_grant_fn; draws from the app-tier cap pool
 *  rather than the Ring1 system pool.
 *
 *  @param module_name  Name of the requesting module.
 *  @param arena_size   Requested slab size (bytes); 0 = system default (256 KB).
 *  @param arena_policy Requested controller policy (POLICY_RECLAIM or POLICY_BUMP);
 *                      POLICY_KERNEL is reserved and will cause the grant to fail.
 *  @return Heap-allocated sc_trusted_cap_t*, or NULL on failure.
 */
typedef sc_trusted_cap_t *(*sc_trusted_app_grant_fn)(const char *module_name, usize arena_size,
                                                     sc_alloc_policy arena_policy);

/** @brief Registered by sigma.memory to provide an arena-backed sc_alloc_use_t
 *         to SIGMA_ALLOC_ARENA modules.  Returns NULL if allocation fails. */
typedef sc_alloc_use_t *(*sc_arena_provider_fn)(const char *module_name);

/** @brief Application bootstrap hook type.
 *
 *  Called after SYSTEM modules initialize but before other modules initialize.
 *  Used by executable applications to configure global resources (e.g., allocators)
 *  after sigma.memory is ready but before ecosystem modules allocate.
 *
 *  Example: sigma.test registers a bootstrap hook that calls
 *           Application.set_allocator(Memory.get_allocator())
 */
typedef void (*sc_app_bootstrap_fn)(void);

// ── Module Interface ───────────────────────────────────────────────────────

/**
 * @brief Module system interface - all module lifecycle operations
 *
 * Provides unified access to module registration, initialization, and shutdown.
 * Applications use Module.set_bootstrap() to configure global resources after
 * SYSTEM modules (sigma.memory) initialize but before other modules run.
 */
typedef struct sc_module_i {
    /**
     * @brief Register a module descriptor
     * @param mod Module descriptor (must remain valid for process lifetime)
     * @return 0 on success; -1 if registry is full
     */
    int (*register_module)(const sigma_module_t *mod);

    /**
     * @brief Initialize all registered modules
     *
     * Performs 3-phase initialization:
     * 1. SYSTEM modules (sigma.memory, sigma.core)
     * 2. Application bootstrap hook (if registered)
     * 3. Remaining modules in topological order
     *
     * @return 0 if all inits succeed; first non-zero init return value otherwise
     */
    int (*init_all)(void);

    /**
     * @brief Shutdown all initialized modules in reverse order
     */
    void (*shutdown_all)(void);

    /**
     * @brief Register application bootstrap hook
     *
     * Hook runs after SYSTEM modules initialize but before other modules.
     * Allows applications to configure global resources (allocators, logging, etc.)
     * after sigma.memory is ready but before ecosystem modules allocate.
     *
     * @param fn Bootstrap function to call; NULL to clear
     *
     * Example usage (sigma.test/main.c):
     * @code
     * static void bootstrap_allocator(void) {
     *     Application.set_allocator(Memory.get_allocator());
     * }
     *
     * int main(void) {
     *     Module.set_bootstrap(bootstrap_allocator);
     *     Module.init_all();
     *     // ...
     * }
     * @endcode
     */
    void (*set_bootstrap)(sc_app_bootstrap_fn fn);
} sc_module_i;

/**
 * @brief Global Module interface instance
 */
extern const sc_module_i Module;

// ── Registry API (legacy direct functions) ────────────────────────────────
//
// These functions are provided for backward compatibility.
// New code should use the Module interface instead:
//   sigma_module_register(mod)  →  Module.register_module(mod)
//   sigma_module_init_all()     →  Module.init_all()
//   sigma_module_shutdown_all() →  Module.shutdown_all()
//

/**
 * @brief Register a module descriptor.
 *
 * Safe to call from __attribute__((constructor)) — does nothing but append a
 * pointer to a static array.  No memory allocation, no init dependencies.
 *
 * @return 0 on success; -1 if the registry is full (SC_MAX_MODULES exceeded).
 */
int sigma_module_register(const sigma_module_t *mod);

/**
 * @brief Topological-sort all registered modules by deps[], then call init()
 *        on each in order, passing role-appropriate ctx.
 *
 * Idempotent — returns 0 immediately if already initialised.
 *
 * @return 0 if all inits succeed; first non-zero init return value otherwise.
 *         Calls panic_fn (which must not return) on dep cycle or missing dep.
 */
int sigma_module_init_all(void);

/**
 * @brief Call shutdown() on every initialised module in reverse init order.
 *
 * Safe to call if sigma_module_init_all() was never called or failed.
 */
void sigma_module_shutdown_all(void);

/**
 * @brief Look up a registered module by name.
 *
 * Safe to call after sigma_module_register() and before or after init_all.
 *
 * @return Pointer to the descriptor, or NULL if not found.
 */
const sigma_module_t *sigma_module_find(const char *name);

// ── Configuration ──────────────────────────────────────────────────────────

/**
 * @brief Replace the panic handler.  Pass NULL to restore the default.
 */
void sigma_module_set_panic_fn(sc_module_panic_fn fn);

/**
 * @brief Register the TRUSTED capability provider (called by sigma.memory init).
 */
void sigma_module_set_trusted_grant(sc_trusted_grant_fn fn);

/**
 * @brief Register the TRUSTED_APP capability provider (called by sigma.memory init).
 */
void sigma_module_set_trusted_app_grant(sc_trusted_app_grant_fn fn);

/**
 * @brief Register the ARENA allocator provider (called by sigma.memory init).
 */
void sigma_module_set_arena_provider(sc_arena_provider_fn fn);

// ── Test support ───────────────────────────────────────────────────────────
#ifdef TSTDBG
/**
 * @brief Zero the registry, init_order, and all hook pointers.
 *
 * Intended for test harnesses only — allows multiple registration scenarios
 * within one process.  Never call in production code.
 */
void sigma_module_reset(void);
#endif
