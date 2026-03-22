# FR-2603-sigma-core-002: Add `SIGMA_ROLE_TRUSTED_APP` role tier and dispatch registration

**ID:** FR-2603-sigma-core-002  
**Type:** Feature Request  
**Owner:** sigma.core  
**Filed:** 2026-03-22  
**Status:** closed  
**Tags:** module, trusted-app, role, dispatch, FT-14  

---

## Summary

sigma.core's `sc_module_role` enum currently has three tiers: `SIGMA_ROLE_SYSTEM`, `SIGMA_ROLE_TRUSTED` (Ring1, R1–R6), and `SIGMA_ROLE_USER`. A fourth tier is needed for first-party application-layer modules — components we own and audit but that should not consume Ring1 system slots.

sigma.memory FT-14 (Trusted Application Registration) is blocked on this change.

## Requested Changes

### 1. Add `SIGMA_ROLE_TRUSTED_APP = 3` to `sc_module_role`

```c
// include/sigma.core/module.h
typedef enum {
    SIGMA_ROLE_SYSTEM      = 0,
    SIGMA_ROLE_TRUSTED     = 1,
    SIGMA_ROLE_USER        = 2,
    SIGMA_ROLE_TRUSTED_APP = 3,   // ← NEW: first-party application tier
} sc_module_role;
```

### 2. Add `sc_trusted_app_grant_fn` typedef

```c
// include/sigma.core/module.h
typedef sc_trusted_cap_t *(*sc_trusted_app_grant_fn)(const char *name,
                                                      usize        arena_size,
                                                      sc_alloc_policy arena_policy);
```

### 3. Add registration fn and dispatch hook

```c
// include/sigma.core/module.h
void sigma_module_set_trusted_app_grant(sc_trusted_app_grant_fn fn);
```

In `src/module.c` dispatch loop: when a module with `role == SIGMA_ROLE_TRUSTED_APP` is
initialised, call `s_trusted_app_grant_fn(mod->name, mod->arena_size, mod->arena_policy)`
and pass the returned `sc_trusted_cap_t *` as `ctx` to `mod->init(ctx)`. Mirrors the
existing `SIGMA_ROLE_TRUSTED` dispatch branch exactly.

## Rationale

Ring1 slots R1–R6 are reserved for system-level infrastructure (sigma.tasking, anvil.lite,
sigma.io, sigma.irq). First-party application modules (e.g. sigma.test) are trusted in the
sense that we own them, but they must not consume system ring slots. `SIGMA_ROLE_TRUSTED_APP`
provides a separate pool backed by sigma.memory's own `s_trusted_app_caps[N]` array.

The cap type returned (`sc_trusted_cap_t`) is identical — no new public type is needed. The
only difference is the pool from which the cap is drawn.

## Consumer

sigma.memory — FT-14: Trusted Application Registration. sigma.test is the first consumer.

## Resolution

Applied to sigma.core. `SIGMA_ROLE_TRUSTED_APP = 3` added to `sc_module_role`; `sc_trusted_app_grant_fn` typedef added; `sigma_module_set_trusted_app_grant()` declared in `module.h` and implemented in `module.c`; dispatch branch mirrors `SIGMA_ROLE_TRUSTED` exactly. Published in sigma.core build +11.
