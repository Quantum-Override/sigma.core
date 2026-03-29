/*
 * Sigma.Core
 * Copyright (c) 2026 David Boarman (BadKraft) and contributors
 * QuantumOverride [Q|]
 * ----------------------------------------------
 * File: test/unit/test_module.c
 * Description: Unit tests for sigma.module registry and lifecycle.
 *
 *   No sigma.test linkage.  Uses mock_runner.h for assertions and reporting.
 *   Compiled with -DTSTDBG to enable sigma_module_reset().
 *
 *   Each test function calls sigma_module_reset() first, then exercises one
 *   scenario in isolation.  Stub module descriptors are defined at file scope.
 */

#include <sigma.core/allocator.h> /* sc_alloc_use_t full definition */
#include <sigma.core/module.h>
#include <stdio.h>
#include <string.h>
#include "utilities/mock_runner.h"

/* ── Minimal struct definitions for opaque types used in stubs ─────────── */
struct sc_trusted_cap_s {
    int _dummy;
};

/* ── Shared stub state (reset at top of each test) ─────────────────────── */
static int stub_a_init_count;
static int stub_b_init_count;
static int stub_c_init_count;
static int stub_d_init_count; /* reserved for future use */
static void *stub_a_ctx;
static void *stub_b_ctx;
static void *stub_c_ctx;
static int stub_a_shutdown_count;
static int stub_b_shutdown_count;
static int stub_c_shutdown_count;
static int stub_d_shutdown_count; /* reserved for future use */

static void reset_stub_state(void) {
    stub_a_init_count = stub_b_init_count = stub_c_init_count = stub_d_init_count = 0;
    stub_a_ctx = stub_b_ctx = stub_c_ctx = NULL;
    stub_a_shutdown_count = stub_b_shutdown_count = stub_c_shutdown_count = stub_d_shutdown_count =
        0;
}

/* ── Stub callbacks ─────────────────────────────────────────────────────── */
static int stub_a_init(void *ctx) {
    stub_a_init_count++;
    stub_a_ctx = ctx;
    return 0;
}
static int stub_b_init(void *ctx) {
    stub_b_init_count++;
    stub_b_ctx = ctx;
    return 0;
}
static int stub_c_init(void *ctx) {
    stub_c_init_count++;
    stub_c_ctx = ctx;
    return 0;
}
static void stub_a_shutdown(void) { stub_a_shutdown_count++; }
static void stub_b_shutdown(void) { stub_b_shutdown_count++; }
static void stub_c_shutdown(void) { stub_c_shutdown_count++; }

/* ── Shared dep arrays ──────────────────────────────────────────────────── */
static const char *no_deps[] = {NULL};
static const char *deps_a[] = {"mod.a", NULL};

/* ── Module descriptors ─────────────────────────────────────────────────── */
static const sigma_module_t mod_a = {.name = "mod.a",
                                     .version = "0.0.1",
                                     .role = SIGMA_ROLE_PERIPHERAL,
                                     .deps = no_deps,
                                     .init = stub_a_init,
                                     .shutdown = stub_a_shutdown};
static const sigma_module_t mod_b_dep_a = {.name = "mod.b",
                                           .version = "0.0.1",
                                           .role = SIGMA_ROLE_PERIPHERAL,
                                           .deps = deps_a,
                                           .init = stub_b_init,
                                           .shutdown = stub_b_shutdown};

static const sigma_module_t mod_b_nodep = {.name = "mod.b",
                                           .version = "0.0.1",
                                           .role = SIGMA_ROLE_PERIPHERAL,
                                           .deps = no_deps,
                                           .init = stub_b_init,
                                           .shutdown = stub_b_shutdown};

/* init_failure stub */
static int fail_init(void *ctx) {
    (void)ctx;
    return -1;
}
static const sigma_module_t mod_fail = {.name = "mod.fail",
                                        .version = "0.0.1",
                                        .role = SIGMA_ROLE_PERIPHERAL,
                                        .deps = no_deps,
                                        .init = fail_init,
                                        .shutdown = NULL};
static int after_fail_called = 0;
static int after_fail_init(void *ctx) {
    (void)ctx;
    after_fail_called = 1;
    return 0;
}
static const sigma_module_t mod_after_fail = {.name = "mod.after",
                                              .version = "0.0.1",
                                              .role = SIGMA_ROLE_PERIPHERAL,
                                              .deps = no_deps,
                                              .init = after_fail_init,
                                              .shutdown = NULL};

/* role stubs */
static const sigma_module_t mod_system = {.name = "mod.system",
                                          .version = "0.0.1",
                                          .role = SIGMA_ROLE_SYSTEM,
                                          .deps = no_deps,
                                          .init = stub_a_init,
                                          .shutdown = NULL};
static const sigma_module_t mod_trusted = {.name = "mod.trusted",
                                           .version = "0.0.1",
                                           .role = SIGMA_ROLE_TRUSTED,
                                           .deps = no_deps,
                                           .init = stub_b_init,
                                           .shutdown = NULL};
static const sigma_module_t mod_isolated = {.name = "mod.isolated",
                                            .version = "0.0.1",
                                            .role = SIGMA_ROLE_PERIPHERAL,
                                            .alloc = SIGMA_ALLOC_ARENA,
                                            .deps = no_deps,
                                            .init = stub_c_init,
                                            .shutdown = stub_c_shutdown};

/* alloc stubs */
static void *alloc_sys_ctx = NULL;
static void *alloc_custom_ctx = NULL;
static int alloc_sys_init(void *ctx) {
    alloc_sys_ctx = ctx;
    return 0;
}
static int alloc_custom_init(void *ctx) {
    alloc_custom_ctx = ctx;
    return 0;
}

static const sigma_module_t mod_alloc_system = {.name = "mod.alloc_sys",
                                                .version = "0.0.1",
                                                .role = SIGMA_ROLE_PERIPHERAL,
                                                .alloc = SIGMA_ALLOC_SYSTEM,
                                                .deps = no_deps,
                                                .init = alloc_sys_init,
                                                .shutdown = NULL};

static void *custom_alloc_fn(usize sz) {
    (void)sz;
    return NULL;
}
static void custom_release_fn(void *p) { (void)p; }
static void *custom_resize_fn(void *p, usize sz) {
    (void)p;
    (void)sz;
    return NULL;
}
static sc_alloc_use_t custom_hooks = {
    .alloc = custom_alloc_fn,
    .release = custom_release_fn,
    .resize = custom_resize_fn,
};
static const sigma_module_t mod_alloc_custom = {.name = "mod.alloc_custom",
                                                .version = "0.0.1",
                                                .role = SIGMA_ROLE_PERIPHERAL,
                                                .alloc = SIGMA_ALLOC_CUSTOM,
                                                .alloc_hooks = &custom_hooks,
                                                .deps = no_deps,
                                                .init = alloc_custom_init,
                                                .shutdown = NULL};

/* cycle stubs — a depends on b, b depends on a */
static const char *cycle_deps_a[] = {"cycle.b", NULL};
static const char *cycle_deps_b[] = {"cycle.a", NULL};
static const sigma_module_t mod_cycle_a = {.name = "cycle.a",
                                           .version = "0.0.1",
                                           .role = SIGMA_ROLE_PERIPHERAL,
                                           .deps = cycle_deps_a,
                                           .init = stub_a_init,
                                           .shutdown = NULL};
static const sigma_module_t mod_cycle_b = {.name = "cycle.b",
                                           .version = "0.0.1",
                                           .role = SIGMA_ROLE_PERIPHERAL,
                                           .deps = cycle_deps_b,
                                           .init = stub_b_init,
                                           .shutdown = NULL};

/* missing dep stub */
static const char *missing_dep[] = {"does.not.exist", NULL};
static const sigma_module_t mod_missing = {.name = "mod.missing",
                                           .version = "0.0.1",
                                           .role = SIGMA_ROLE_PERIPHERAL,
                                           .deps = missing_dep,
                                           .init = stub_a_init,
                                           .shutdown = NULL};

/* ── Mock panic handler ──────────────────────────────────────────────────── */
static bool mock_panic_fired = false;
static const char *mock_panic_module = NULL;
static const char *mock_panic_reason = NULL;

static void mock_panic(const char *mod, const char *reason) {
    mock_panic_fired = true;
    mock_panic_module = mod;
    mock_panic_reason = reason;
    /* Do NOT abort — test must continue to verify the panic was fired. */
}

/* ── Mock provider hooks ─────────────────────────────────────────────────── */
static sc_trusted_cap_t mock_cap = {0};
static sc_alloc_use_t mock_arena_use = {NULL, NULL, NULL};
static bool mock_grant_called = false;
static bool mock_arena_called = false;

static sc_trusted_cap_t *mock_grant_fn(const char *name) {
    (void)name;
    mock_grant_called = true;
    return &mock_cap;
}
static sc_alloc_use_t *mock_arena_fn(const char *name) {
    (void)name;
    mock_arena_called = true;
    return &mock_arena_use;
}

/* ── max_modules helpers ─────────────────────────────────────────────────── */
#define MAX_EXTRA 34
static char extra_name_buf[MAX_EXTRA][8];
static sigma_module_t extra_mods[MAX_EXTRA];

/* =========================================================================
 * Test functions
 * ========================================================================= */

/* 1 ─ register one module, init fires exactly once */
static void test_single_register_init(void) {
    sigma_module_reset();
    reset_stub_state();

    int r = sigma_module_register(&mod_a);
    MOCK_ASSERT(r == 0, "register should succeed");

    r = sigma_module_init_all();
    MOCK_ASSERT(r == 0, "init_all should return 0");
    MOCK_ASSERT(stub_a_init_count == 1, "init should be called exactly once");

    sigma_module_shutdown_all();
}

/* 2 ─ dep chain A→B: A inits before B */
static void test_dep_chain(void) {
    sigma_module_reset();
    reset_stub_state();

    /* register in reverse order to confirm topo-sort fixes it */
    sigma_module_register(&mod_b_dep_a);
    sigma_module_register(&mod_a);

    int r = sigma_module_init_all();
    MOCK_ASSERT(r == 0, "init_all should succeed");
    MOCK_ASSERT(stub_a_init_count == 1, "mod.a init should be called once");
    MOCK_ASSERT(stub_b_init_count == 1, "mod.b init should be called once");
    /* Verify order: a before b — use a sentinel counter approach */
    /* Re-run with ordering check via a shared sequence counter */
    sigma_module_shutdown_all();
}

/* 2b ─ dep chain ordering verified via sequence index */
static int seq_a_order = 0;
static int seq_b_order = 0;
static int seq_counter = 0;
static int seq_a_init(void *ctx) {
    (void)ctx;
    seq_a_order = ++seq_counter;
    return 0;
}
static int seq_b_init(void *ctx) {
    (void)ctx;
    seq_b_order = ++seq_counter;
    return 0;
}
static const sigma_module_t mod_seq_a = {.name = "seq.a",
                                         .version = "0.0.1",
                                         .role = SIGMA_ROLE_PERIPHERAL,
                                         .deps = no_deps,
                                         .init = seq_a_init,
                                         .shutdown = NULL};
static const char *seq_b_deps[] = {"seq.a", NULL};
static const sigma_module_t mod_seq_b = {.name = "seq.b",
                                         .version = "0.0.1",
                                         .role = SIGMA_ROLE_PERIPHERAL,
                                         .deps = seq_b_deps,
                                         .init = seq_b_init,
                                         .shutdown = NULL};

static void test_dep_chain_order(void) {
    sigma_module_reset();
    seq_a_order = seq_b_order = seq_counter = 0;

    sigma_module_register(&mod_seq_b); /* intentionally reversed */
    sigma_module_register(&mod_seq_a);

    int r = sigma_module_init_all();
    MOCK_ASSERT(r == 0, "init_all should succeed");
    MOCK_ASSERT(seq_a_order < seq_b_order, "seq.a must init before seq.b");

    sigma_module_shutdown_all();
}

/* 3 ─ diamond dep: B and C both dep A; D deps B and C — A init only once */
static int diamond_a_order = 0, diamond_b_order = 0;
static int diamond_c_order = 0, diamond_d_order = 0;
static int diamond_counter = 0;
static int diamond_a_cnt = 0;

static int da_init(void *ctx) {
    (void)ctx;
    diamond_a_cnt++;
    diamond_a_order = ++diamond_counter;
    return 0;
}
static int db_init(void *ctx) {
    (void)ctx;
    diamond_b_order = ++diamond_counter;
    return 0;
}
static int dc_init(void *ctx) {
    (void)ctx;
    diamond_c_order = ++diamond_counter;
    return 0;
}
static int dd_init(void *ctx) {
    (void)ctx;
    diamond_d_order = ++diamond_counter;
    return 0;
}

static const sigma_module_t dm_a = {.name = "dm.a",
                                    .version = "0.0.1",
                                    .role = SIGMA_ROLE_PERIPHERAL,
                                    .deps = no_deps,
                                    .init = da_init,
                                    .shutdown = NULL};
static const char *dm_b_deps[] = {"dm.a", NULL};
static const sigma_module_t dm_b = {.name = "dm.b",
                                    .version = "0.0.1",
                                    .role = SIGMA_ROLE_PERIPHERAL,
                                    .deps = dm_b_deps,
                                    .init = db_init,
                                    .shutdown = NULL};
static const char *dm_c_deps[] = {"dm.a", NULL};
static const sigma_module_t dm_c = {.name = "dm.c",
                                    .version = "0.0.1",
                                    .role = SIGMA_ROLE_PERIPHERAL,
                                    .deps = dm_c_deps,
                                    .init = dc_init,
                                    .shutdown = NULL};
static const char *dm_d_deps[] = {"dm.b", "dm.c", NULL};
static const sigma_module_t dm_d = {.name = "dm.d",
                                    .version = "0.0.1",
                                    .role = SIGMA_ROLE_PERIPHERAL,
                                    .deps = dm_d_deps,
                                    .init = dd_init,
                                    .shutdown = NULL};

static void test_diamond_dep(void) {
    sigma_module_reset();
    diamond_a_order = diamond_b_order = diamond_c_order = diamond_d_order = 0;
    diamond_counter = diamond_a_cnt = 0;

    sigma_module_register(&dm_d);
    sigma_module_register(&dm_c);
    sigma_module_register(&dm_b);
    sigma_module_register(&dm_a);

    int r = sigma_module_init_all();
    MOCK_ASSERT(r == 0, "init_all should succeed");
    MOCK_ASSERT(diamond_a_cnt == 1, "dm.a init should fire exactly once");
    MOCK_ASSERT(diamond_a_order < diamond_b_order, "dm.a before dm.b");
    MOCK_ASSERT(diamond_a_order < diamond_c_order, "dm.a before dm.c");
    MOCK_ASSERT(diamond_b_order < diamond_d_order, "dm.b before dm.d");
    MOCK_ASSERT(diamond_c_order < diamond_d_order, "dm.c before dm.d");

    sigma_module_shutdown_all();
}

/* 4 ─ shutdown fires in reverse init order */
static int sd_a_order = 0, sd_b_order = 0, sd_counter = 0;
static void sd_a_shutdown(void) { sd_a_order = ++sd_counter; }
static void sd_b_shutdown(void) { sd_b_order = ++sd_counter; }
static int sd_a_init(void *ctx) {
    (void)ctx;
    return 0;
}
static int sd_b_init(void *ctx) {
    (void)ctx;
    return 0;
}

static const sigma_module_t sd_a = {.name = "sd.a",
                                    .version = "0.0.1",
                                    .role = SIGMA_ROLE_PERIPHERAL,
                                    .deps = no_deps,
                                    .init = sd_a_init,
                                    .shutdown = sd_a_shutdown};
static const char *sd_b_deps[] = {"sd.a", NULL};
static const sigma_module_t sd_b = {.name = "sd.b",
                                    .version = "0.0.1",
                                    .role = SIGMA_ROLE_PERIPHERAL,
                                    .deps = sd_b_deps,
                                    .init = sd_b_init,
                                    .shutdown = sd_b_shutdown};

static void test_shutdown_reverse(void) {
    sigma_module_reset();
    sd_a_order = sd_b_order = sd_counter = 0;

    sigma_module_register(&sd_a);
    sigma_module_register(&sd_b);
    sigma_module_init_all();     /* sd.a inits first, sd.b second */
    sigma_module_shutdown_all(); /* sd.b should shut down first */

    MOCK_ASSERT(sd_b_order < sd_a_order, "sd.b must shut down before sd.a");
}

/* 5 ─ SYSTEM role receives NULL ctx */
static void test_role_system_null_ctx(void) {
    sigma_module_reset();
    reset_stub_state();

    void *sentinel = (void *)0xdeadbeef;
    stub_a_ctx = sentinel;

    sigma_module_register(&mod_system);
    sigma_module_init_all();

    MOCK_ASSERT(stub_a_ctx == NULL, "SYSTEM role init should receive NULL ctx");

    sigma_module_shutdown_all();
}

/* 6 ─ TRUSTED role receives non-NULL ctx from grant fn */
static void test_role_trusted_ctx(void) {
    sigma_module_reset();
    reset_stub_state();
    mock_grant_called = false;

    sigma_module_set_trusted_grant(mock_grant_fn);
    sigma_module_register(&mod_trusted);
    sigma_module_init_all();

    MOCK_ASSERT(mock_grant_called, "trusted grant fn should be called");
    MOCK_ASSERT(stub_b_ctx == &mock_cap, "TRUSTED init should receive mock_cap ptr");

    sigma_module_set_trusted_grant(NULL);
    sigma_module_shutdown_all();
}

/* 7 ─ ARENA alloc receives non-NULL sc_alloc_use_t* from arena provider */
static void test_alloc_arena_ctx(void) {
    sigma_module_reset();
    reset_stub_state();
    mock_arena_called = false;

    sigma_module_set_arena_provider(mock_arena_fn);
    sigma_module_register(&mod_isolated);
    sigma_module_init_all();

    MOCK_ASSERT(mock_arena_called, "arena provider fn should be called");
    MOCK_ASSERT(stub_c_ctx == &mock_arena_use, "ARENA alloc init should receive arena use ptr");

    sigma_module_set_arena_provider(NULL);
    sigma_module_shutdown_all();
}

/* 7a ─ SYSTEM alloc receives sc_alloc_use_t* with stdlib hooks (no provider needed) */
static void test_alloc_system_ctx(void) {
    sigma_module_reset();
    alloc_sys_ctx = NULL;

    sigma_module_register(&mod_alloc_system);
    sigma_module_init_all();

    MOCK_ASSERT(alloc_sys_ctx != NULL, "SYSTEM alloc should pass non-NULL ctx");
    sc_alloc_use_t *use = (sc_alloc_use_t *)alloc_sys_ctx;
    MOCK_ASSERT(use->alloc != NULL, "system alloc fn should be non-NULL (malloc)");
    MOCK_ASSERT(use->release != NULL, "system release fn should be non-NULL (free)");
    MOCK_ASSERT(use->resize != NULL, "system resize fn should be non-NULL (realloc)");

    sigma_module_shutdown_all();
}

/* 7b ─ CUSTOM alloc passes alloc_hooks pointer as ctx */
static void test_alloc_custom_ctx(void) {
    sigma_module_reset();
    alloc_custom_ctx = NULL;

    sigma_module_register(&mod_alloc_custom);
    sigma_module_init_all();

    MOCK_ASSERT(alloc_custom_ctx == &custom_hooks, "CUSTOM alloc should pass alloc_hooks as ctx");

    sigma_module_shutdown_all();
}

/* 8 ─ init() non-zero aborts init_all; later modules NOT called */
static void test_init_failure_aborts(void) {
    sigma_module_reset();
    after_fail_called = 0;

    sigma_module_register(&mod_fail);
    sigma_module_register(&mod_after_fail);

    int r = sigma_module_init_all();
    MOCK_ASSERT(r != 0, "init_all should return non-zero on init failure");
    MOCK_ASSERT(!after_fail_called, "module registered after failing module should NOT init");

    sigma_module_shutdown_all();
}

/* 9 ─ sigma_module_find returns correct descriptor pointer */
static void test_find_by_name(void) {
    sigma_module_reset();

    sigma_module_register(&mod_a);
    sigma_module_register(&mod_b_nodep);

    const sigma_module_t *found_a = sigma_module_find("mod.a");
    const sigma_module_t *found_b = sigma_module_find("mod.b");
    const sigma_module_t *not_found = sigma_module_find("mod.nope");

    MOCK_ASSERT(found_a == &mod_a, "sigma_module_find should return mod_a descriptor");
    MOCK_ASSERT(found_b == &mod_b_nodep, "sigma_module_find should return mod_b descriptor");
    MOCK_ASSERT(not_found == NULL, "sigma_module_find should return NULL for unknown name");
}

/* 10 ─ register 32 modules succeeds; 33rd returns -1 */
static void test_max_modules(void) {
    sigma_module_reset();

    for (int i = 0; i < 32; i++) {
        snprintf(extra_name_buf[i], sizeof(extra_name_buf[i]), "m%02d", i);
        extra_mods[i].name = extra_name_buf[i];
        extra_mods[i].version = "0.0.1";
        extra_mods[i].role = SIGMA_ROLE_PERIPHERAL;
        extra_mods[i].deps = no_deps;
        extra_mods[i].init = NULL;
        extra_mods[i].shutdown = NULL;
        int r = sigma_module_register(&extra_mods[i]);
        MOCK_ASSERT(r == 0, "each of the first 32 registers should succeed");
    }

    snprintf(extra_name_buf[32], sizeof(extra_name_buf[32]), "m32");
    extra_mods[32].name = extra_name_buf[32];
    extra_mods[32].version = "0.0.1";
    extra_mods[32].role = SIGMA_ROLE_PERIPHERAL;
    extra_mods[32].deps = no_deps;
    extra_mods[32].init = NULL;
    extra_mods[32].shutdown = NULL;
    int r = sigma_module_register(&extra_mods[32]);
    MOCK_ASSERT(r == -1, "33rd register should return -1 (registry full)");
}

/* 11 ─ missing dep triggers panic hook */
static void test_missing_dep_panic(void) {
    sigma_module_reset();
    mock_panic_fired = false;
    mock_panic_module = NULL;

    sigma_module_set_panic_fn(mock_panic);
    sigma_module_register(&mod_missing);
    sigma_module_init_all(); /* panic_fn fires instead of abort */

    MOCK_ASSERT(mock_panic_fired, "panic fn should fire when a dependency is not registered");
    MOCK_ASSERT(mock_panic_module != NULL && strcmp(mock_panic_module, "mod.missing") == 0,
                "panic should identify the module with the missing dep");

    sigma_module_set_panic_fn(NULL); /* restore default */
    sigma_module_shutdown_all();
}

/* 12 ─ A↔B cycle triggers panic hook */
static void test_cycle_panic(void) {
    sigma_module_reset();
    mock_panic_fired = false;

    sigma_module_set_panic_fn(mock_panic);
    sigma_module_register(&mod_cycle_a);
    sigma_module_register(&mod_cycle_b);
    sigma_module_init_all();

    MOCK_ASSERT(mock_panic_fired, "panic fn should fire on circular dependency");

    sigma_module_set_panic_fn(NULL);
    sigma_module_shutdown_all();
}

/* ── Bootstrap tests ────────────────────────────────────────────────────── */
/* Bootstrap hook should run AFTER SYSTEM, BEFORE other modules */

static int bootstrap_call_count = 0;
static int bootstrap_stub_a_count_at_call = 0;
static int bootstrap_stub_b_count_at_call = 0;

static void bootstrap_hook(void) {
    bootstrap_call_count++;
    /* Capture counts when bootstrap fires */
    bootstrap_stub_a_count_at_call = stub_a_init_count;
    bootstrap_stub_b_count_at_call = stub_b_init_count;
}

static void reset_bootstrap_state(void) {
    bootstrap_call_count = 0;
    bootstrap_stub_a_count_at_call = 0;
    bootstrap_stub_b_count_at_call = 0;
}

/* 13 ─ Bootstrap runs after SYSTEM, before USER */
static void test_bootstrap_timing(void) {
    sigma_module_reset();
    reset_stub_state();
    reset_bootstrap_state();

    /* mod_system is SIGMA_ROLE_SYSTEM using stub_a */
    /* mod_b_nodep is SIGMA_ROLE_PERIPHERAL using stub_b */
    sigma_module_register(&mod_system);
    sigma_module_register(&mod_b_nodep);

    Module.set_bootstrap(bootstrap_hook);
    Module.init_all();

    /* Bootstrap should have been called exactly once */
    MOCK_ASSERT(bootstrap_call_count == 1, "bootstrap hook should run exactly once");

    /* At the time bootstrap ran: */
    /* - SYSTEM module (stub_a) should have initialized */
    MOCK_ASSERT(bootstrap_stub_a_count_at_call == 1,
                "SYSTEM module should initialize BEFORE bootstrap");

    /* - USER module (stub_b) should NOT have initialized yet */
    MOCK_ASSERT(bootstrap_stub_b_count_at_call == 0,
                "USER module should NOT initialize before bootstrap");

    /* After init_all completes, both should be initialized */
    MOCK_ASSERT(stub_a_init_count == 1, "SYSTEM module should be initialized");
    MOCK_ASSERT(stub_b_init_count == 1, "USER module should be initialized after bootstrap");

    sigma_module_shutdown_all();
}

/* 14 ─ Multiple set_bootstrap calls (last wins) */
static int first_bootstrap_called = 0;
static int second_bootstrap_called = 0;

static void first_bootstrap(void) { first_bootstrap_called = 1; }
static void second_bootstrap(void) { second_bootstrap_called = 1; }

static void test_bootstrap_multiple_set(void) {
    sigma_module_reset();
    reset_stub_state();
    first_bootstrap_called = 0;
    second_bootstrap_called = 0;

    sigma_module_register(&mod_system);
    sigma_module_register(&mod_b_nodep);

    /* Set first hook, then overwrite with second */
    Module.set_bootstrap(first_bootstrap);
    Module.set_bootstrap(second_bootstrap);

    Module.init_all();

    /* Only second should have been called (last wins) */
    MOCK_ASSERT(first_bootstrap_called == 0, "first bootstrap should NOT run (overwritten)");
    MOCK_ASSERT(second_bootstrap_called == 1, "second bootstrap should run (last set)");

    sigma_module_shutdown_all();
}

/* 15 ─ NULL bootstrap allowed */
static void test_bootstrap_null(void) {
    sigma_module_reset();
    reset_stub_state();
    reset_bootstrap_state();

    sigma_module_register(&mod_system);
    sigma_module_register(&mod_b_nodep);

    /* Set bootstrap to NULL (clear) */
    Module.set_bootstrap(NULL);

    int r = Module.init_all();

    /* Init should succeed without bootstrap */
    MOCK_ASSERT(r == 0, "init_all should succeed with NULL bootstrap");
    MOCK_ASSERT(bootstrap_call_count == 0, "NULL bootstrap should not be called");
    MOCK_ASSERT(stub_a_init_count == 1, "SYSTEM module should still initialize");
    MOCK_ASSERT(stub_b_init_count == 1, "USER module should still initialize");

    sigma_module_shutdown_all();
}

/* 16 ─ Reset clears bootstrap */
static void test_bootstrap_reset_clears(void) {
    sigma_module_reset();
    reset_stub_state();
    reset_bootstrap_state();

    /* Set bootstrap, then reset */
    Module.set_bootstrap(bootstrap_hook);
    sigma_module_reset();

    sigma_module_register(&mod_system);
    sigma_module_register(&mod_b_nodep);

    Module.init_all();

    /* Bootstrap should NOT run (cleared by reset) */
    MOCK_ASSERT(bootstrap_call_count == 0, "reset should clear bootstrap hook");
    MOCK_ASSERT(stub_a_init_count == 1, "SYSTEM module should still initialize");
    MOCK_ASSERT(stub_b_init_count == 1, "USER module should still initialize");

    sigma_module_shutdown_all();
}

/* =========================================================================
 * main
 * ========================================================================= */
int main(void) {
    printf("test_module\n\n");

    mock_run("single_register_init", test_single_register_init);
    mock_run("dep_chain", test_dep_chain);
    mock_run("dep_chain_order", test_dep_chain_order);
    mock_run("diamond_dep", test_diamond_dep);
    mock_run("shutdown_reverse", test_shutdown_reverse);
    mock_run("role_system_null_ctx", test_role_system_null_ctx);
    mock_run("role_trusted_ctx", test_role_trusted_ctx);
    mock_run("alloc_arena_ctx", test_alloc_arena_ctx);
    mock_run("alloc_system_ctx", test_alloc_system_ctx);
    mock_run("alloc_custom_ctx", test_alloc_custom_ctx);
    mock_run("init_failure_aborts", test_init_failure_aborts);
    mock_run("find_by_name", test_find_by_name);
    mock_run("max_modules", test_max_modules);
    mock_run("missing_dep_panic", test_missing_dep_panic);
    mock_run("cycle_panic", test_cycle_panic);
    mock_run("bootstrap_timing", test_bootstrap_timing);
    mock_run("bootstrap_multiple_set", test_bootstrap_multiple_set);
    mock_run("bootstrap_null", test_bootstrap_null);
    mock_run("bootstrap_reset_clears", test_bootstrap_reset_clears);

    return mock_summary("test_module");
}
