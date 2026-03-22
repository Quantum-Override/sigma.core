# FR-2603-sigma-core-003: Add `frame_begin`/`frame_end` to `sc_alloc_use_t`

**ID:** FR-2603-sigma-core-003  
**Type:** Feature Request  
**Owner:** sigma.core  
**Filed:** 2026-03-22  
**Status:** closed  
**Tags:** allocator, alloc-use, frame, FT-15  

---

## Summary

`sc_alloc_use_t` currently exposes three function pointers: `alloc`, `release`, `resize`.
Both the bump and reclaim controllers already support `frame_begin`/`frame_end` internally
(via vtable on `sc_bump_ctrl_s` and `sc_reclaim_ctrl_s`), but this is unreachable from the
public `alloc_use` surface without casting to the concrete controller type.

Any consumer holding an `sc_alloc_use_t` — including trusted-app modules — cannot bracket a
frame without knowing what controller backs it. This breaks the abstraction goal of
`sc_alloc_use_t` as a controller-agnostic allocation interface.

sigma.memory FT-15 (Frame operations on `sc_alloc_use_t`) is blocked on this change.

## Requested Change

Add two nullable function pointer fields to `sc_alloc_use_t`:

```c
// include/sigma.core/allocator.h
typedef struct sc_alloc_use_s {
    void  *(*alloc)      (usize size);
    void   (*release)    (void *ptr);
    void  *(*resize)     (void *ptr, usize size);
    frame  (*frame_begin)(void);          // ← NEW: save cursor / sequence tag; NULL if unsupported
    void   (*frame_end)  (frame f);       // ← NEW: bulk reclaim to saved point; NULL if unsupported
} sc_alloc_use_t;
```

`frame` is already defined in `allocator.h`:
```c
typedef usize frame;
#define FRAME_NULL ((frame)0)
```

## Behaviour Contract

- `frame_begin` returns `FRAME_NULL` on failure or if frames are not supported.
- `frame_end(FRAME_NULL)` is a no-op.
- Callers check for NULL before invoking: `if (use->frame_begin) f = use->frame_begin();`
- For bump-backed `alloc_use`: wired to the bump ctrl's `frame_begin`/`frame_end`.
- For reclaim-backed `alloc_use`: wired to the reclaim ctrl's seq-tag frame mechanism.
- For SLB0 facade `alloc_use`: wired to SLB0 reclaim ctrl's frame support.
- For system alloc (malloc/free): both fields left NULL — no frame support.

## ABI Impact

Adding fields to `sc_alloc_use_t` is ABI-safe for consumers that initialise the struct
by field name (designated initialisers). Any consumer using `{fn1, fn2, fn3}` positional
initialisation will need updating — audit required before merging.

## Rationale

A "sandbox" for a test case is exactly a frame on a bump-backed arena: `frame_begin` saves
the cursor, allocs proceed freely, `frame_end` resets everything atomically. Exposing this
through `sc_alloc_use_t` means any trusted-app module can implement per-operation or
per-test scoping without knowing the concrete controller type. The concept is not new — only
the public surface is missing.

## Consumer

sigma.memory — FT-15: Frame operations on `sc_alloc_use_t`.  
Depends on: FR-2603-sigma-core-002 (FT-14 trusted-app grant must exist to wire the fields).

## Resolution

Applied to sigma.core. `frame_begin` and `frame_end` nullable function pointer fields appended to `sc_alloc_use_t` in `allocator.h`. `frame` typedef and `FRAME_NULL` sentinel were already present. Published in sigma.core build +11.
