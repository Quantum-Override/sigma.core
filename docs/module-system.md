# Sigma.Core — Module System & Role-Based Capability Grants

**Date:** March 16, 2026  
**Updated:** March 17, 2026  
**Status:** Implementation in progress — Phase 1–4 active (TDD, mock framework first)  
**Touches:** sigma.core · sigma.memory · sigma.test · every test set source file

---

## Table of Contents

1. [Problem Being Solved](#1-problem-being-solved)
2. [Design Overview](#2-design-overview)
3. [sigma.core — New Files](#3-sigmcore--new-files)
   - 3.1 [module.h](#31-moduleh)
   - 3.2 [module.c](#32-modulec)
   - 3.3 [config.sh changes](#33-configsh-changes)
4. [sigma.memory — Changes](#4-sigmamemory--changes)
   - 4.1 [memory.c constructor](#41-memoryc-constructor)
   - 4.2 [module.c (new)](#42-modulec-new)
5. [sigma.test — Changes](#5-sigmatest--changes)
   - 5.1 [sigtest.c constructor](#51-sigtestc-constructor)
   - 5.2 [Enqueue API (new)](#52-enqueue-api-new)
   - 5.3 [main() update](#53-main-update)
   - 5.4 [sigtest.h additions](#54-sigtesth-additions)
   - 5.5 [module.c (new)](#55-modulec-new)
6. [Test Set Sources — Convention Change](#6-test-set-sources--convention-change)
7. [Role-Based Capability Grants](#7-role-based-capability-grants)
   - 7.1 [sc_trusted_cap_t (sigma.memory)](#71-sc_trusted_cap_t-sigmamemory)
   - 7.2 [Role dispatch in init_all](#72-role-dispatch-in-init_all)
8. [Startup Sequence (Before vs After)](#8-startup-sequence-before-vs-after)
9. [Design Evolution Notes](#9-design-evolution-notes)
10. [Impact on Other Projects](#10-impact-on-other-projects)
11. [Future: sigma.build Code Generation](#11-future-sigmabuild-code-generation)
12. [Phased TDD Implementation Plan](#12-phased-tdd-implementation-plan)

---

## 1. Problem Being Solved

Three components each carry `__attribute__((constructor))`:

| Component | Constructor does | Needs up before it |
|---|---|---|
| sigma.memory `memory.c` | `init_memory_system()` — SYS0/SLB0 bootstrap | nothing |
| sigma.test `sigtest.c` | `init_default_hooks()` — arena create + alloc | **sigma.memory** |
| any test set `.c` | `testset()` + `testcase()` calls — alloc | **sigma.memory + sigma.test** |

Only sigma.memory uses the priority form `constructor(101)` to force ordering, which VS Code
IntelliSense flags as an error. Without that priority annotation, the three constructors fire in
link order — undefined, fragile, wrong.

**The fix:** every constructor becomes trivially safe (just registers a pointer). A single
`sigma_module_init_all()` call in `main()` drives init in dependency order.

---

## 2. Design Overview

```
constructor():  sigma_module_register(&memory_module)   // ptr store only — safe any time
constructor():  sigma_module_register(&sigtest_module)  // ptr store only — safe any time
constructor():  sigma_testset_enqueue(register_X_tests) // ptr store only — safe any time

main() {
    sigma_module_init_all();        // topo-sort deps → init memory, then sigtest
    sigma_testsets_run_pending();   // call each enqueued fn → testset()/testcase()
    run_tests(test_sets, hooks);
    sigma_module_shutdown_all();    // reverse order — cleans up sigtest, then memory
}
```

Three tiers, one rule: **constructors only store pointers**. All init work happens explicitly.

---

## 3. sigma.core — New Files

### 3.1 `module.h`

**Path:** `sigma.core/include/sigma.core/module.h`  
**Installed to:** `/usr/local/include/sigma.core/module.h`

```c
#pragma once
#include <sigma.core/types.h>

// ── Module roles ────────────────────────────────────────────────────────────
typedef enum {
    SIGMA_ROLE_SYSTEM   = 0,  // lowest level (sigma.memory, sigma.core) — init first
    SIGMA_ROLE_TRUSTED  = 1,  // privileged access to Memory capability grant
    SIGMA_ROLE_ISOLATED = 2,  // dedicated allocator ctx; fenced lifecycle (sigma.test archetype)
    SIGMA_ROLE_USER     = 3,  // all other modules
} sc_module_role;

// ── Module descriptor ───────────────────────────────────────────────────────
// Declare one static instance of this struct per project in module.c.
// init() receives NULL for SYSTEM/USER roles; sc_trusted_cap_t* for TRUSTED.
// deps[] is a NULL-terminated list of module names this module depends on.
typedef struct sigma_module_s {
    const char    *name;
    const char    *version;
    sc_module_role role;
    const char   **deps;          // NULL-terminated; NULL means no deps
    int  (*init)(void *ctx);      // return 0 on success, non-zero aborts init_all
    void (*shutdown)(void);       // may be NULL
} sigma_module_t;

// ── Registry API ────────────────────────────────────────────────────────────
// sigma_module_register: called from constructor(); thread-safe (atomic append).
// Returns 0 on success; -1 if registry is full (SC_MAX_MODULES exceeded).
int  sigma_module_register(const sigma_module_t *mod);

// sigma_module_init_all: topological sort by deps[]; for each module in order:
//   TRUSTED role → call Memory_grant_trusted(mod->name); pass result as init ctx
//   all other roles → call mod->init(NULL)
// Returns 0 if all inits succeed; first non-zero init return value otherwise.
// Aborts (no partial init) on dependency cycle or missing dep.
int  sigma_module_init_all(void);

// sigma_module_shutdown_all: call shutdown() in reverse init order.
void sigma_module_shutdown_all(void);

// sigma_module_find: look up a registered module by name (post-registration, pre-init ok).
const sigma_module_t *sigma_module_find(const char *name);
```

### 3.2 `module.c`

**Path:** `sigma.core/src/module.c`  
**Package:** `sigma.module` (see section 3.3)

Key implementation constraints:

- **No heap.** The registry is a static array: `static const sigma_module_t *registry[SC_MAX_MODULES]`.
  `SC_MAX_MODULES` = 32.
- **Topo-sort** uses Kahn's algorithm (BFS in-degree) — no recursion, O(V+E), detects both missing
  deps and cycles cleanly. Missing dep → `panic_fn(mod, dep_name)`. Cycle → `panic_fn(mod, reason)`.
- **Panic hook**: `sc_module_panic_fn` function pointer; default = `fprintf(stderr,...) + abort()`.
  User-replaceable via `sigma_module_set_panic_fn()`. Panic fns are expected never to return.
- **Role dispatch** (see section 7.2): TRUSTED → `trusted_grant_fn(name)`;
  all others → ctx from `alloc` field: DEFAULT=NULL, SYSTEM=stdlib hooks,
  ARENA=`arena_provider_fn(name)`, CUSTOM=`mod->alloc_hooks`.
  Both `trusted_grant_fn` and `arena_provider_fn` are registered by sigma.memory at its own init.
- **Shutdown**: stores the init order in a static array; shutdown walks it in reverse.
- **Test reset**: `sigma_module_reset()` behind `#ifdef TSTDBG` — zeroes registry, init_order, hook
  pointers, and resets `initialized` guard. Acknowledged false-stability risk (tests only).

```c
// Internal sketch — not final code, but captures the contract:
#define SC_MAX_MODULES 32

static const sigma_module_t *registry[SC_MAX_MODULES];
static uint8_t               registry_count = 0;
static const sigma_module_t *init_order[SC_MAX_MODULES];
static uint8_t               init_count  = 0;

int sigma_module_register(const sigma_module_t *mod) {
    if (registry_count >= SC_MAX_MODULES) return -1;
    registry[registry_count++] = mod;
    return 0;
}
```

Topo-sort: for each module in `registry`, if not yet in `init_order`, recursively resolve its
`deps[]` first (each dep looked up by name in `registry`), then append self.

### 3.3 `config.sh` changes

Add a new package:

```bash
declare -A PACKAGES=(
    ["system.alloc"]="sigma.system.alloc | sys_alloc"
    ["core.utils"]="sigma.core.utils    | time guid"
    ["module"]="sigma.module        | module"          # ← NEW
)
declare -A PACKAGE_REQUIRES=(
    ["system.alloc"]=""
    ["core.utils"]=""
    ["module"]=""                                       # ← NEW — no bundled deps
)
```

`TST_REQUIRES` in sigma.core itself doesn't need `sigma.module` — tests assert on module
registration behaviour and link the module object directly via `TEST_EXTRA_OBJECTS`.

---

## 4. sigma.memory — Changes

### 4.1 `memory.c` constructor

**File:** `sigma.memory/src/memory.c`

```c
// BEFORE:
__attribute__((constructor(101))) static void init_memory_system(void) {
    // ... full bootstrap ...
}

// AFTER:
// init_memory_system() body unchanged — just remove the constructor attribute.
// It is now called by sigma_module_init_all() via memory_module.init.
static int memory_module_init(void *ctx) {   // ctx always NULL for SYSTEM role
    (void)ctx;
    init_memory_system();   // existing body, no changes
    return 0;
}

static void memory_module_shutdown(void) {
    // existing teardown if any; otherwise empty
}

__attribute__((constructor)) static void register_memory_module(void) {
    sigma_module_register(&memory_module);   // ptr store only — SYS0 not up yet, safe
}
```

### 4.2 `module.c` (new)

**Path:** `sigma.memory/module.c`

```c
#include <sigma.core/module.h>

static const char *memory_deps[] = { NULL };   // no deps — always first

static int  memory_module_init(void *ctx);     // implemented in memory.c
static void memory_module_shutdown(void);      // implemented in memory.c

static const sigma_module_t memory_module = {
    .name     = "sigma.memory",
    .version  = "0.3.0",
    .role     = SIGMA_ROLE_SYSTEM,
    .deps     = memory_deps,
    .init     = memory_module_init,
    .shutdown = memory_module_shutdown,
};

__attribute__((constructor)) static void register_memory_module(void) {
    sigma_module_register(&memory_module);
}
```

`memory_module_init` and `memory_module_shutdown` are declared `static` in `memory.c` and their
signatures match the forward declarations here — linked into the same translation unit via the
build system's object list, so no header exposure needed.

---

## 5. sigma.test — Changes

### 5.1 `sigtest.c` constructor

**File:** `sigma.test/src/sigtest.c`

```c
// BEFORE:
__attribute__((constructor)) static void init_default_hooks(void) {
    sigtest_arena = Allocator.Arena.create("sigtest.runner", SCOPE_POLICY_DYNAMIC);
    // ... allocs hooks registry ...
}

// AFTER:
// init_default_hooks() body unchanged (updated for v0.3.0 API — create_reclaim etc).
// Constructor now only registers.
static int sigtest_module_init(void *ctx) {
    (void)ctx;
    init_default_hooks();
    return 0;
}

static void sigtest_module_shutdown(void) {
    cleanup_test_runner();   // existing cleanup body
}

__attribute__((constructor)) static void register_sigtest_module(void) {
    sigma_module_register(&sigtest_module);
}
```

The `atexit(cleanup_test_runner)` call inside `testset()` is **removed**. `cleanup_test_runner`
is now called explicitly by `sigma_module_shutdown_all()` via `sigtest_module_shutdown`.
The `atexit_registered` guard logic in `testset()` is also removed.

### 5.2 Enqueue API (new)

**File:** `sigma.test/src/sigtest.c`

```c
#define ST_MAX_ENQUEUED_SETS 64

typedef void (*st_register_fn)(void);

static st_register_fn enqueue[ST_MAX_ENQUEUED_SETS];
static uint8_t        enqueue_count = 0;

// Called from test set constructors — no alloc, no deps required.
void sigma_testset_enqueue(st_register_fn fn) {
    if (enqueue_count < ST_MAX_ENQUEUED_SETS)
        enqueue[enqueue_count++] = fn;
    // silent drop on overflow; debug builds: assert(enqueue_count < ST_MAX_ENQUEUED_SETS)
}

// Called from main() after sigma_module_init_all() — runs testset()/testcase() calls.
void sigma_testsets_run_pending(void) {
    for (uint8_t i = 0; i < enqueue_count; i++)
        enqueue[i]();
    enqueue_count = 0;
}
```

### 5.3 `main()` update

**File:** `sigma.test/src/sigtest.c`, `#ifdef SIGTEST_TEST` block

```c
// BEFORE:
int main(void) {
    int retResult = run_tests(test_sets, current_hooks);
    cleanup_test_runner();
    return retResult;
}

// AFTER:
int main(void) {
    sigma_module_init_all();         // memory → sigtest (in dep order)
    sigma_testsets_run_pending();    // drain enqueue → testset()/testcase() calls
    int result = run_tests(test_sets, current_hooks);
    sigma_module_shutdown_all();     // reverse: sigtest shutdown → memory shutdown
    return result;
}
```

### 5.4 `sigtest.h` additions

```c
// sigma.test/include/sigtest.h

typedef void (*st_register_fn)(void);

// Enqueue a test registration function to be called after module init.
// Call from __attribute__((constructor)) in test set sources.
void sigma_testset_enqueue(st_register_fn fn);

// Drain the enqueue, calling each registered fn. Called by main() after
// sigma_module_init_all(). Not intended for direct user call.
void sigma_testsets_run_pending(void);
```

### 5.5 `module.c` (new)

**Path:** `sigma.test/module.c`

```c
#include <sigma.core/module.h>

static const char *sigtest_deps[] = { "sigma.memory", NULL };

static int  sigtest_module_init(void *ctx);    // in sigtest.c
static void sigtest_module_shutdown(void);     // in sigtest.c

static const sigma_module_t sigtest_module = {
    .name     = "sigma.test",
    .version  = "1.0.1",
    .role     = SIGMA_ROLE_USER,
    .deps     = sigtest_deps,
    .init     = sigtest_module_init,
    .shutdown = sigtest_module_shutdown,
};

__attribute__((constructor)) static void register_sigtest_module(void) {
    sigma_module_register(&sigtest_module);
}
```

---

## 6. Test Set Sources — Convention Change

Every test set `.c` file currently has a single constructor that calls `testset()` +
`testcase()` directly — this requires both memory and sigtest to be initialized.

**New convention: split into enqueue + registration function.**

```c
// ── BEFORE ──────────────────────────────────────────────────────────────────
__attribute__((constructor)) void register_bootstrap_tests(void) {
    testset("Memory: SYS0 Bootstrap", set_config, set_teardown);
    testcase("BST-01: SYS0 size", test_bst01_sys0_size);
    // ...
}

// ── AFTER ───────────────────────────────────────────────────────────────────
// Step 1: constructor only enqueues — safe at any time, no init deps
__attribute__((constructor)) static void enqueue_bootstrap_tests(void) {
    sigma_testset_enqueue(register_bootstrap_tests);
}

// Step 2: actual registration — called by sigma_testsets_run_pending() after
//         sigma_module_init_all(), when both memory and sigtest are up
static void register_bootstrap_tests(void) {
    testset("Memory: SYS0 Bootstrap", set_config, set_teardown);
    testcase("BST-01: SYS0 size", test_bst01_sys0_size);
    // ...
}
```

**Rules:**
- `enqueue_*` function: `static`, no direct `testset`/`testcase`/`Allocator` calls — only
  `sigma_testset_enqueue(fn)`.
- `register_*` function: `static`, called only by sigtest after init. Contains all
  `testset()`/`testcase()` calls exactly as before.
- The function named `register_bootstrap_tests` in the old code becomes the body of the new
  `register_*` function. The old constructor body shrinks to one line.

This applies to **every** test set source in every project that links sigma.test.

---

## 7. Role-Based Capability Grants

### 7.1 `sc_trusted_cap_t` (sigma.memory)

Defined in `sigma.memory/include/memory.h`:

```c
// Forward-declared in sigma.core/module.h — full definition in sigma.memory/memory.h
typedef struct sc_trusted_cap_s {
    const char         *grantee;       // module name (who was granted)
    sc_ctrl_registry_s *sub_registry;  // 256-slot registry, mmap'd separately
    uint8_t             guid[16];      // reserved — future signed grant verification
} sc_trusted_cap_t;

// Called by sigma_module_init_all() for TRUSTED modules — not public API.
// Returns NULL if sigma.memory is not initialized or grantee is not allowed.
sc_trusted_cap_t *Memory_grant_trusted(const char *module_name);
```

`sigma.core/module.h` forward-declares `sc_trusted_cap_t` as an opaque pointer so `module.h`
does not pull in `sigma.memory/memory.h`. The full struct is only needed by TRUSTED module
implementations (sigma.tasking) and sigma.memory itself.

Forward declaration in `module.h`:
```c
// Opaque — full definition in sigma.memory/memory.h
typedef struct sc_trusted_cap_s sc_trusted_cap_t;
```

### 7.2 Role dispatch in `init_all`

```c
// In sigma.core/src/module.c — inside sigma_module_init_all()
for (int i = 0; i < init_count; i++) {
    const sigma_module_t *mod = init_order[i];
    void *ctx = NULL;

    if (mod->role == SIGMA_ROLE_TRUSTED) {
        // trusted_grant registered by sigma.memory during its own init.
        // If sigma.memory is absent from the link, ctx stays NULL.
        ctx = trusted_grant ? trusted_grant(mod->name) : NULL;
    } else {
        switch (mod->alloc) {
        case SIGMA_ALLOC_SYSTEM: ctx = &system_alloc_use; break;
        case SIGMA_ALLOC_ARENA:  ctx = arena_provider ? arena_provider(mod->name) : NULL; break;
        case SIGMA_ALLOC_CUSTOM: ctx = mod->alloc_hooks; break;
        default:                 ctx = NULL; break;  // SIGMA_ALLOC_DEFAULT
        }
    }

    if (mod->init && mod->init(ctx) != 0) {
        fprintf(stderr, "sigma.module: init failed for '%s'\n", mod->name);
        return -1;
    }
}
```

`system_alloc_use` is a static `sc_alloc_use_t` in `module.c` initialised to `{malloc, free, realloc}`.
`trusted_grant` and `arena_provider` are function pointers set when sigma.memory's init
runs — sigma.core stays the lower layer with no link-time dependency on sigma.memory.

Register from sigma.memory's `memory_module_init()`:

```c
// sigma.memory/src/memory.c
static int memory_module_init(void *ctx) {
    (void)ctx;
    init_memory_system();
    sigma_module_set_trusted_grant(memory_grant_trusted);   // registers TRUSTED hook
    sigma_module_set_arena_provider(memory_create_arena_use); // registers ARENA alloc hook
    return 0;
}
```

This is the entire coupling surface — two fn-pointer registrations, both optional from
sigma.core's perspective. No header from sigma.memory is included by sigma.core.

---

## 8. Startup Sequence: Before vs After

### Before (fragile, priority-dependent)

```
ELF loader runs constructors in link order:
  constructor(101): init_memory_system()      ← SYS0/SLB0 up
  constructor():    init_default_hooks()       ← allocs arena + hooks registry
  constructor():    register_bootstrap_tests() ← calls testset()/testcase()

main():
  run_tests(test_sets, current_hooks)
  cleanup_test_runner()                        ← via atexit
```

### After (explicit, order-independent constructors)

```
ELF loader runs constructors in any order — all are trivially safe:
  constructor(): sigma_module_register(&memory_module)   // ptr store
  constructor(): sigma_module_register(&sigtest_module)  // ptr store
  constructor(): sigma_testset_enqueue(register_X_tests) // ptr store

main():
  sigma_module_init_all()
    // topo-sort: memory has no deps → init first
    //   memory_module_init(NULL) → init_memory_system() → SYS0/SLB0 up
    // sigtest depends on sigma.memory → init second
    //   sigtest_module_init(NULL) → init_default_hooks() → arena + hooks alloc'd
  sigma_testsets_run_pending()
    // memory up + sigtest up → safe to call testset()/testcase()
    register_bootstrap_tests()
    register_slab_tests()
    // ... all enqueued fns ...
  run_tests(test_sets, current_hooks)
  sigma_module_shutdown_all()
    // reverse order: sigtest_module_shutdown() → cleanup_test_runner()
    //                memory_module_shutdown()
```

---

## 9. Design Evolution Notes

### `sigma.system.alloc` Removed

The `sigma.system.alloc` package (`src/sys_alloc.c`) is being retired. It served as a
malloc-backed shim so modules could run without sigma.memory, but it required an extra
package dependency and created a parallel `Allocator` symbol that caused link-order issues.

The replacement is the **`use` pattern** described below.

### The `use` Pattern — Lightweight Allocator Hook

A new type `sc_alloc_use_t` (defined in `sigma.core/allocator.h`) carries three optional
function pointers:

```c
typedef struct sc_alloc_use_s {
    void *(*alloc)(usize size);
    void  (*release)(void *ptr);
    void *(*resize)(void *ptr, usize size);
} sc_alloc_use_t;
```

**Rules:** Any field set to `NULL` means "use the system default" (`malloc`/`free`/`realloc`).
The struct itself may also be a `NULL` pointer — components that receive an `sc_alloc_use_t*`
must treat `NULL` as full malloc fallback.

**Where it applies:**

| Interface | Mechanism |
|---|---|
| `String` / `StringBuilder` | `alloc_use(ptr)` on the vtable; `NULL` → system malloc |
| `SIGMA_ALLOC_ARENA` modules | `ctx` passed to `init()` is `sc_alloc_use_t*` (or NULL if sigma.memory absent) |

For `SIGMA_ALLOC_ARENA` modules: sigma.memory registers an `arena_provider` fn during its own init.
When `sigma_module_init_all()` processes an ARENA module it calls `arena_provider(name)`,
which allocates an arena and returns a pointer to a filled `sc_alloc_use_t` wrapping it.
If sigma.memory is not in the link, `arena_provider` is NULL and the module receives
`NULL` — it handles allocation internally via stdlib.

**Coupling surface** (two fn-pointer registrations, both optional):

```c
// Called by sigma.memory's memory_module_init()
sigma_module_set_trusted_grant(memory_grant_trusted);
sigma_module_set_arena_provider(memory_create_arena_use);
```

sigma.core has zero compile-time dependency on sigma.memory. sigma.memory depends on
sigma.core (as always). The only runtime coupling is these two fn-pointer writes.

---

## 10. Impact on Other Projects

| Project | Change required |
|---|---|
| **sigma.core** | New `module.h` + `module.c`, new `sigma.module` package in config.sh |
| **sigma.memory** | Remove `constructor(101)`; add `module.c`; add `memory_module_init` / `shutdown` wrappers; set `memory_grant_trusted_fn` on init |
| **sigma.test** | Remove `atexit` from `testset()`; add `sigma_testset_enqueue` + `sigma_testsets_run_pending`; update `main()`; add `module.c` |
| **every test set `.c`** | Split constructor → enqueue + register pattern (mechanical find-replace) |
| **sigma.tasking** | Add `module.c` with `SIGMA_ROLE_TRUSTED`; `tasking_module_init(ctx)` stores `sc_trusted_cap_t *`; use `cap->sub_registry` for per-task bump controllers |
| **sigma.text** | Add `module.c` with `SIGMA_ROLE_USER`, dep `["sigma.memory", NULL]` if using slab allocator, or dep `["sigma.core", NULL]` if using sys_alloc |
| **sigma.collections** | Add `module.c` with `SIGMA_ROLE_USER`, dep `["sigma.core", NULL]` |
| **anvil** | Add `module.c`; `main()` calls `sigma_module_init_all()` before parsing |

`sigma.module` package must be added to `TST_REQUIRES` / `REQUIRES` in every project that
calls `sigma_module_init_all()` or `sigma_module_register()`.

---

## 11. Future: sigma.build Code Generation

The manual `module.c` file per project is the stable contract. When sigma.build lands, the
`build.anvl` `module` sub-object generates it:

```anvl
project := {
    name := "sigma.tasking"
    module := {
        role     := trusted
        requires := ["sigma.memory", "sigma.core"]
        init     := "tasking_module_init"
        shutdown := "tasking_module_shutdown"
    }
}
```

sigma.build reads this → emits `module.c` with the correct `sigma_module_t` struct. The
manually written `module.c` files serve as the template and ground truth for the generator.
No `module.c` file is checked in once sigma.build is generating them; they become build
artifacts.

---

## 12. Phased TDD Implementation Plan

All implementation follows test-first TDD: write failing tests → implement to green.

### Phase 1 — Mock Test Framework
**File:** `test/utilities/mock_runner.h`

Shared, zero-dependency test utility. No sigma.test linkage — module unit tests are
completely standalone. Provides:
- `MOCK_ASSERT(cond, msg)` — captures `__FILE__`/`__LINE__`, increments fail counter
- `mock_run(name, fn)` — executes a test function, prints PASS/FAIL
- `mock_summary(suite)` — prints totals, returns fail count (suitable for `exit()`)

### Phase 2 — Header API
**File:** `include/sigma.core/module.h`

Full public API (4 roles, descriptor struct, registry/init/shutdown/find/panic functions).
Also exposes the two hook-registration functions called by sigma.memory:
- `sigma_module_set_trusted_grant(fn)` — registers TRUSTED ctx provider
- `sigma_module_set_arena_provider(fn)` — registers ARENA alloc provider

`sigma_module_reset()` compiled only when `TSTDBG` is defined.

### Phase 3 — Unit Tests (write first — all red)
**File:** `test/unit/test_module.c`

Compiled with `-DTSTDBG`. Uses `mock_runner.h` only — no sigma.test, no sigma.memory.
Stub `sigma_module_t` instances use static flag arrays to record init/shutdown calls.
`sigma_module_reset()` called at the start of each test function.

| # | Test | Verifies |
|---|---|---|
| 1 | `single_register_init` | init fires exactly once |
| 2 | `dep_chain` | A→B: A inits before B |
| 3 | `diamond_dep` | B,C both dep A; D deps B+C → A init only once |
| 4 | `shutdown_reverse` | shutdown fires in reverse init order |
| 5 | `role_system_null_ctx` | SYSTEM role → `init(NULL)` |
| 6 | `role_trusted_ctx` | TRUSTED role → `init` receives non-NULL ctx |
| 7 | `alloc_arena_ctx` | ARENA alloc → `init` receives non-NULL `sc_alloc_use_t*` |
| 7a | `alloc_system_ctx` | SYSTEM alloc → `init` receives stdlib hooks (no provider needed) |
| 7b | `alloc_custom_ctx` | CUSTOM alloc → `init` receives `alloc_hooks` pointer |
| 8 | `init_failure_aborts` | `init()` non-zero → `init_all` fails; later modules NOT called |
| 9 | `find_by_name` | `sigma_module_find` returns correct descriptor pointer |
| 10 | `max_modules` | 32 register succeeds; 33rd returns -1 |
| 11 | `missing_dep_panic` | missing dep → panic hook fires with module name |
| 12 | `cycle_panic` | A↔B cycle → panic hook fires |

### Phase 4 — Implementation
**File:** `src/module.c`

Static-only state (`registry[]`, `init_order[]`, `initialized` guard,
`panic_fn`, `trusted_grant`, `arena_provider`, `system_alloc_use` stdlib hooks).

Topo-sort: Kahn's BFS in-degree algorithm — no recursion, no heap, O(V+E):
- Compute in-degree per module (number of unresolved deps)
- Seed queue with in-degree=0 nodes (SYSTEM modules first naturally)
- Process: append to `init_order`, decrement dependents' in-degrees, enqueue newly zeroed
- After BFS: if `init_count != registry_count` → cycle → `panic_fn`
- Missing dep: detected during in-degree computation → `panic_fn`

Alloc dispatch inside `sigma_module_init_all`:
```
SIGMA_ROLE_TRUSTED                    → ctx = trusted_grant(name)  or NULL
all other roles, SIGMA_ALLOC_DEFAULT  → ctx = NULL
all other roles, SIGMA_ALLOC_SYSTEM   → ctx = &system_alloc_use
all other roles, SIGMA_ALLOC_ARENA    → ctx = arena_provider(name) or NULL
all other roles, SIGMA_ALLOC_CUSTOM   → ctx = mod->alloc_hooks
```

### Phase 5 — Integration Test
**File:** `test/integration/test_module_boot.c`

Standalone binary. No sigma.test, no sigma.memory. Links only local `src/` objects.
Verifies the module system lifecycle with a realistic 3-module dep chain, including
initialisation ordering, shutdown reversal, and panic hook customisation.

### Phase 6 — Package & Config
**`config.sh` changes:**
- Remove `sigma.system.alloc` from `PACKAGES` and `PACKAGE_REQUIRES`
- Remove `memory_sys` from `TEST_VARIANT_REQUIRES` and `TEST_EXTRA_OBJECTS`
- Add `["module"]="sigma.module | module"` to `PACKAGES`
- Add `["module"]=""` to `PACKAGE_REQUIRES`
- Add `["module"]="standard"` and `["test_module"]="standard"` to `TEST_CONFIGS`

**Verification:**
1. `./rtest unit/module` — all 12 scenarios green (mock_runner output)
2. `./rtest integration/module_boot` — lifecycle assertions pass
3. `cbuild packages` → `build/sigma.module.o` produced cleanly
4. `nm build/sigma.module.o | grep " U "` — only `fprintf`/`abort`/`strcmp` undefined (no Allocator)
