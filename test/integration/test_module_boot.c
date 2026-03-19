/*
 * Sigma.Core
 * Copyright (c) 2026 David Boarman (BadKraft) and contributors
 * QuantumOverride [Q|]
 * ----------------------------------------------
 * File: test/integration/test_module_boot.c
 * Description: Integration test for sigma.module full lifecycle.
 *
 *   Standalone binary — no sigma.test, no sigma.memory linkage.
 *   Links only src/module.o (compiled locally by rtest).
 *
 *   Verifies: registration, topo ordering, role dispatch, shutdown reversal,
 *   and panic hook customisation across a realistic three-module dep chain.
 */

#include <sigma.core/allocator.h>
#include <sigma.core/module.h>
#include <stdio.h>
#include <string.h>
#include "utilities/mock_runner.h"

/* ── Opaque stubs ────────────────────────────────────────────────────────── */
struct sc_trusted_cap_s {
    int _dummy;
};

/* ── Order tracking ──────────────────────────────────────────────────────── */
static int seq[4];
static int seq_n = 0;
static int shut[4];
static int shut_n = 0;

static void reset_tracking(void) { seq_n = shut_n = 0; }

/* ── Three-module chain: core → service → app ───────────────────────────── */
static int core_init(void *ctx) {
    (void)ctx;
    seq[seq_n++] = 0;
    return 0;
}
static void core_shutdown(void) { shut[shut_n++] = 0; }

static int service_init(void *ctx) {
    (void)ctx;
    seq[seq_n++] = 1;
    return 0;
}
static void service_shutdown(void) { shut[shut_n++] = 1; }

static int app_init(void *ctx) {
    (void)ctx;
    seq[seq_n++] = 2;
    return 0;
}
static void app_shutdown(void) { shut[shut_n++] = 2; }

static const char *no_deps[] = {NULL};
static const char *svc_deps[] = {"boot.core", NULL};
static const char *app_deps[] = {"boot.service", NULL};

static const sigma_module_t core_mod = {.name = "boot.core",
                                        .version = "1.0.0",
                                        .role = SIGMA_ROLE_SYSTEM,
                                        .deps = no_deps,
                                        .init = core_init,
                                        .shutdown = core_shutdown};
static const sigma_module_t service_mod = {.name = "boot.service",
                                           .version = "1.0.0",
                                           .role = SIGMA_ROLE_USER,
                                           .deps = svc_deps,
                                           .init = service_init,
                                           .shutdown = service_shutdown};
static const sigma_module_t app_mod = {.name = "boot.app",
                                       .version = "1.0.0",
                                       .role = SIGMA_ROLE_USER,
                                       .deps = app_deps,
                                       .init = app_init,
                                       .shutdown = app_shutdown};

/* =========================================================================
 * Scenario 1 — Full lifecycle
 * ========================================================================= */
static void test_full_lifecycle(void) {
    sigma_module_reset();
    reset_tracking();

    /* Register in undefined order — topo-sort must fix it. */
    sigma_module_register(&app_mod);
    sigma_module_register(&core_mod);
    sigma_module_register(&service_mod);

    int r = sigma_module_init_all();
    MOCK_ASSERT(r == 0, "init_all should return 0");
    MOCK_ASSERT(seq_n == 3, "all three modules should have initialised");
    MOCK_ASSERT(seq[0] == 0, "core must init first");
    MOCK_ASSERT(seq[1] == 1, "service must init second");
    MOCK_ASSERT(seq[2] == 2, "app must init third");

    sigma_module_shutdown_all();
    MOCK_ASSERT(shut_n == 3, "all three modules should have shut down");
    MOCK_ASSERT(shut[0] == 2, "app must shut down first");
    MOCK_ASSERT(shut[1] == 1, "service must shut down second");
    MOCK_ASSERT(shut[2] == 0, "core must shut down last");
}

/* =========================================================================
 * Scenario 2 — sigma_module_find returns correct descriptors
 * ========================================================================= */
static void test_find(void) {
    sigma_module_reset();

    sigma_module_register(&core_mod);
    sigma_module_register(&service_mod);
    sigma_module_register(&app_mod);

    MOCK_ASSERT(sigma_module_find("boot.core") == &core_mod, "find core");
    MOCK_ASSERT(sigma_module_find("boot.service") == &service_mod, "find service");
    MOCK_ASSERT(sigma_module_find("boot.app") == &app_mod, "find app");
    MOCK_ASSERT(sigma_module_find("boot.none") == NULL, "find missing returns NULL");
}

/* =========================================================================
 * Scenario 3 — ARENA alloc receives NULL ctx when no arena provider registered
 * ========================================================================= */
static void *isolated_received_ctx = (void *)0x1; /* sentinel non-NULL */
static int isolated_init_fn(void *ctx) {
    isolated_received_ctx = ctx;
    return 0;
}
static const sigma_module_t iso_mod = {.name = "boot.iso",
                                       .version = "1.0.0",
                                       .role = SIGMA_ROLE_USER,
                                       .alloc = SIGMA_ALLOC_ARENA,
                                       .deps = no_deps,
                                       .init = isolated_init_fn,
                                       .shutdown = NULL};

static void test_isolated_no_provider(void) {
    sigma_module_reset();
    isolated_received_ctx = (void *)0x1;

    /* No arena provider registered — ARENA alloc gives NULL ctx. */
    sigma_module_register(&iso_mod);
    sigma_module_init_all();

    MOCK_ASSERT(isolated_received_ctx == NULL, "ARENA alloc with no provider should pass NULL ctx");

    sigma_module_shutdown_all();
}

/* =========================================================================
 * Scenario 4 — Custom panic hook fires; init_all returns non-zero
 * ========================================================================= */
static bool panic_fired = false;
static void custom_panic(const char *mod, const char *reason) {
    (void)mod;
    (void)reason;
    panic_fired = true;
}

static const char *bad_dep[] = {"does.not.exist", NULL};
static const sigma_module_t bad_mod = {.name = "boot.bad",
                                       .version = "1.0.0",
                                       .role = SIGMA_ROLE_USER,
                                       .deps = bad_dep,
                                       .init = core_init,
                                       .shutdown = NULL};

static void test_panic_on_missing_dep(void) {
    sigma_module_reset();
    panic_fired = false;

    sigma_module_set_panic_fn(custom_panic);
    sigma_module_register(&bad_mod);
    int r = sigma_module_init_all();

    MOCK_ASSERT(panic_fired, "custom panic fn should fire on missing dep");
    MOCK_ASSERT(r != 0, "init_all should return non-zero after panic");

    sigma_module_set_panic_fn(NULL);
    sigma_module_shutdown_all();
}

/* =========================================================================
 * main
 * ========================================================================= */
int main(void) {
    printf("test_module_boot (integration)\n\n");

    mock_run("full_lifecycle", test_full_lifecycle);
    mock_run("find", test_find);
    mock_run("isolated_no_provider", test_isolated_no_provider);
    mock_run("panic_on_missing_dep", test_panic_on_missing_dep);

    return mock_summary("test_module_boot");
}
