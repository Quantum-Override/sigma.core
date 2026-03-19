/*
 * Sigma.Core
 * Copyright (c) 2026 David Boarman (BadKraft) and contributors
 * QuantumOverride [Q|]
 * ----------------------------------------------
 * File: module.c
 * Description: Implementation of the sigma.module registry and lifecycle.
 *
 *   Static-only state — no heap allocations anywhere in this file.
 *   Topological ordering uses Kahn's BFS in-degree algorithm: O(V+E),
 *   no recursion, handles diamonds correctly, detects both missing deps
 *   and cycles.
 */

#include <sigma.core/allocator.h> /* sc_alloc_use_t full definition */
#include <sigma.core/module.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Forward declaration ─────────────────────────────────────────────────── */
static void default_panic(const char *module_name, const char *reason);
static int registry_index(const char *name);
static int compute_topo_order(void);
static int dispatch_init_all(void);

/* ── Static state ────────────────────────────────────────────────────────── */
#define SC_MAX_MODULES 32

static const sigma_module_t *registry[SC_MAX_MODULES];
static uint8_t registry_count = 0;

static const sigma_module_t *init_order[SC_MAX_MODULES];
static uint8_t init_count = 0;

static bool initialized = false;
static sc_module_panic_fn panic_fn = default_panic;
static sc_trusted_grant_fn trusted_grant = NULL;
static sc_arena_provider_fn arena_provider = NULL;

/* stdlib hooks — thin wrappers avoid --wrap=malloc data-relocation issues */
static void *system_alloc_wrap(usize sz) { return malloc(sz); }
static void system_release_wrap(void *p) { free(p); }
static void *system_resize_wrap(void *p, usize sz) { return realloc(p, sz); }

static sc_alloc_use_t system_alloc_use = {
    .alloc = system_alloc_wrap,
    .release = system_release_wrap,
    .resize = system_resize_wrap,
};

/* =========================================================================
 * Public API
 * ========================================================================= */

int sigma_module_register(const sigma_module_t *mod) {
    if (!mod || registry_count >= SC_MAX_MODULES) return -1;
    registry[registry_count++] = mod;
    return 0;
}

int sigma_module_init_all(void) {
    if (initialized) return 0;
    if (compute_topo_order() != 0) return -1;
    if (dispatch_init_all() != 0) return -1;
    initialized = true;
    return 0;
}

void sigma_module_shutdown_all(void) {
    if (!initialized) return;
    for (int i = (int)init_count - 1; i >= 0; i--) {
        if (init_order[i]->shutdown) init_order[i]->shutdown();
    }
    initialized = false;
    init_count = 0;
}

const sigma_module_t *sigma_module_find(const char *name) {
    int i = registry_index(name);
    return (i >= 0) ? registry[i] : NULL;
}

void sigma_module_set_panic_fn(sc_module_panic_fn fn) { panic_fn = fn ? fn : default_panic; }

void sigma_module_set_trusted_grant(sc_trusted_grant_fn fn) { trusted_grant = fn; }

void sigma_module_set_arena_provider(sc_arena_provider_fn fn) { arena_provider = fn; }

#ifdef TSTDBG
void sigma_module_reset(void) {
    registry_count = 0;
    init_count = 0;
    initialized = false;
    panic_fn = default_panic;
    trusted_grant = NULL;
    arena_provider = NULL;
    for (int i = 0; i < SC_MAX_MODULES; i++) {
        registry[i] = NULL;
        init_order[i] = NULL;
    }
}
#endif

/* =========================================================================
 * Helper / utility function definitions
 * ========================================================================= */

static void default_panic(const char *module_name, const char *reason) {
    fprintf(stderr, "sigma.module: PANIC in '%s': %s\n", module_name, reason);
    /* abort() intentionally omitted during unit tests when mock_panic replaces
     * this fn — the replacement must not return either, but we cannot call
     * abort() here because the real default handler is replaced in tests. */
    __builtin_unreachable();
}

static int registry_index(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < (int)registry_count; i++) {
        if (registry[i] && strcmp(registry[i]->name, name) == 0) return i;
    }
    return -1;
}

/*
 * compute_topo_order — Kahn's BFS algorithm
 *
 * in_degree[i] = number of unresolved deps for registry[i].
 * Seed queue with all in_degree==0 nodes.  Each time a node is appended to
 * init_order, decrement in_degree of every node that depended on it; enqueue
 * those that hit zero.  If init_count < registry_count at the end, there is
 * a cycle among the remaining nodes.
 *
 * Missing dep detection: if any dep name is absent from the registry during
 * the in_degree computation phase, call panic_fn immediately.
 */
static int compute_topo_order(void) {
    uint8_t in_degree[SC_MAX_MODULES] = {0};
    uint8_t queue[SC_MAX_MODULES];
    int qhead = 0, qtail = 0;

    /* Phase 1: compute in_degree, check for missing deps. */
    for (int i = 0; i < (int)registry_count; i++) {
        const char **dep = registry[i]->deps;
        if (!dep) continue;
        while (*dep) {
            int di = registry_index(*dep);
            if (di < 0) {
                panic_fn(registry[i]->name, *dep); /* passes missing dep name */
                return -1;
            }
            in_degree[i]++;
            dep++;
        }
    }

    /* Phase 2: seed with zero-in-degree nodes. */
    for (int i = 0; i < (int)registry_count; i++) {
        if (in_degree[i] == 0) queue[qtail++] = (uint8_t)i;
    }

    /* Phase 3: BFS. */
    init_count = 0;
    while (qhead < qtail) {
        int cur = (int)queue[qhead++];
        init_order[init_count++] = registry[cur];

        /* For each module that lists cur as a dep: decrement its in_degree. */
        for (int i = 0; i < (int)registry_count; i++) {
            if (i == cur) continue;
            const char **dep = registry[i]->deps;
            if (!dep) continue;
            while (*dep) {
                if (strcmp(*dep, registry[cur]->name) == 0) {
                    in_degree[i]--;
                    if (in_degree[i] == 0) queue[qtail++] = (uint8_t)i;
                }
                dep++;
            }
        }
    }

    /* Phase 4: cycle check. */
    if (init_count != registry_count) {
        for (int i = 0; i < (int)registry_count; i++) {
            if (in_degree[i] > 0) {
                panic_fn(registry[i]->name, "circular dependency detected");
                return -1;
            }
        }
    }

    return 0;
}

/*
 * dispatch_init_all — walk init_order, dispatch alloc-appropriate ctx.
 *
 * TRUSTED modules always receive sc_trusted_cap_t* from trusted_grant;
 * the alloc field is ignored for TRUSTED.
 *
 * All other roles: ctx is derived from the alloc field:
 *   DEFAULT — NULL (global Allocator / SLB0)
 *   SYSTEM  — &system_alloc_use (stdlib hooks; no sigma.memory dep)
 *   ARENA   — sc_alloc_use_t* from arena_provider (sigma.memory hook);
 *             NULL if not registered — module falls back to malloc
 *   CUSTOM  — mod->alloc_hooks (caller-supplied fn pointers)
 */
static int dispatch_init_all(void) {
    for (int i = 0; i < (int)init_count; i++) {
        const sigma_module_t *mod = init_order[i];
        void *ctx = NULL;

        if (mod->role == SIGMA_ROLE_TRUSTED) {
            ctx = trusted_grant ? trusted_grant(mod->name) : NULL;
        } else {
            switch (mod->alloc) {
                case SIGMA_ALLOC_SYSTEM:
                    ctx = &system_alloc_use;
                    break;
                case SIGMA_ALLOC_ARENA:
                    ctx = arena_provider ? arena_provider(mod->name) : NULL;
                    break;
                case SIGMA_ALLOC_CUSTOM:
                    ctx = mod->alloc_hooks;
                    break;
                case SIGMA_ALLOC_DEFAULT:
                default:
                    ctx = NULL;
                    break;
            }
        }

        if (!mod->init) continue;

        int result = mod->init(ctx);
        if (result != 0) {
            fprintf(stderr, "sigma.module: '%s' init() returned %d\n", mod->name, result);
            return -1;
        }
    }

    return 0;
}
