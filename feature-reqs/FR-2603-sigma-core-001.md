# FR-2603-sigma-core-001: Trusted subsystem fields and grant function signature

**ID:** FR-2603-sigma-core-001  
**Type:** Feature Request  
**Owner:** sigma.core  
**Filed:** 2026-03-21  
**Status:** closed  
**Tags:** module, trusted, ring1, allocator, FT-11  

---

## Summary

sigma.memory FT-11 (Trusted Subsystem Registration) requires that subsystems register a private arena with the module system at Ring1. The current `sigma_module_t` struct and `sc_trusted_grant_fn` typedef do not carry the arena configuration fields needed for this.

Two additions are requested:

1. **Fields on `sigma_module_t`** — `arena_size` and `arena_policy` so each module descriptor carries its own trusted-arena configuration.
2. **Updated `sc_trusted_grant_fn` signature** — extend the callback from `(name)` to `(name, arena_size, arena_policy)` so the grant callback can configure the arena at registration time.

## Background

sigma.memory's Ring0 (SLB0) is the system slab. Ring1 introduces **trusted subsystems** — first-party subsystems granted a private arena by the memory module on startup. The grant callback is the hook that drives this registration.

At the time FT-11 was implemented, `sc_trusted_grant_fn` was `typedef void (*sc_trusted_grant_fn)(const char *name)` — carrying only the subsystem name. There was no way to pass arena configuration without adding fields to `sigma_module_t`.

## Requested Changes

### 1. Append to `sigma_module_t` (at struct end — ABI safety)

```c
typedef struct sigma_module_t {
    // ... existing fields unchanged ...
    usize           arena_size;     // trusted arena size (0 = use default)
    sc_alloc_policy arena_policy;   // trusted arena policy (0 = POLICY_RECLAIM)
} sigma_module_t;
```

> **ABI requirement:** New fields must be appended at the END of the struct. Inserting between existing fields corrupts all pre-compiled `.o` offsets for the struct.

### 2. Update `sc_trusted_grant_fn`

```c
// Before
typedef void (*sc_trusted_grant_fn)(const char *name);

// After
typedef void (*sc_trusted_grant_fn)(const char *name, usize arena_size, sc_alloc_policy arena_policy);
```

## Rationale

- Module descriptors are static/global — embedding `arena_size`/`arena_policy` keeps configuration co-located with the module identity.
- The grant callback already has the `name` parameter; adding arena config avoids a separate lookup or side-channel for per-module arena sizing.
- `arena_size == 0` and `arena_policy == 0` are defined as "use defaults" so old descriptors remain forward-compatible.

## Resolution

Applied to sigma.core. Fields added at end of `sigma_module_t`; `sc_trusted_grant_fn` signature updated; `allocator.h` included in `module.h` (see BR-2603-sigma-core-001). Published in sigma.core build +10.

## Consumer

sigma.memory — [FT-11 Trusted Subsystem Registration](../sigma.memory/docs/design-chronicle.md)
