# FR-2603-sigma-core-005: sc_alloc_use_t Structure Update

**ID:** FR-2603-sigma-core-005  
**Type:** Feature Request  
**Owner:** sigma.core  
**Filed:** 2026-03-26  
**Status:** open  
**Priority:** Critical  
**Tags:** phase-1, allocator, structure-update, br-2603-q-or-001  
**Orchestration:** [ORCHESTRATION-BR-2603-q-or-001.md](../q-or/ORCHESTRATION-BR-2603-q-or-001.md) — Phase 1 (sigma.core foundation)  
**Blocks:** FR-2603-sigma-memory-002 (Application delegation), Phase 2

---

## Purpose

Add `sc_ctrl_base_s *ctrl` as first member of `sc_alloc_use_t` structure to unify controller + hook design. Enables controller state access via cast, required for Application allocator delegation pattern.

---

## Context

**Current Structure (sigma.core v1.0.0):**
```c
typedef struct sc_alloc_use_s {
    object (*alloc)(usize);
    void (*release)(object);
    object (*resize)(object, usize);
    void (*frame_begin)(void);  // nullable
    void (*frame_end)(void);    // nullable
} sc_alloc_use_t;
```

**Problem:** No way to access underlying controller state from hook. Controllers (SLB0, bump, reclaim) have state (free lists, capacity, metrics) but hook provides no access.

**Solution:** Add `ctrl` pointer as first member. Enables casting `sc_alloc_use_t*` → `sc_ctrl_base_s*` to access controller, and vice versa.

---

## Requirements

### R1: Update sc_alloc_use_t Structure

**New structure definition (sigma.core/include/memory.h):**
```c
/**
 * @brief Allocator hook with controller reference
 * 
 * LAYOUT GUARANTEE:
 * - ctrl is FIRST member (offset 0)
 * - Enables cast: (sc_alloc_use_t*)ctrl and (sc_ctrl_base_s*)use
 * 
 * USAGE:
 * - Controllers populate ctrl pointer when creating hooks
 * - Application.set_allocator() can inspect ctrl for diagnostics
 * - Future: frame_begin/frame_end can delegate to ctrl->vtable
 */
typedef struct sc_alloc_use_s {
    sc_ctrl_base_s *ctrl;       /**< Controller state (first member, offset 0) */
    object (*alloc)(usize);      /**< Allocate memory */
    void (*release)(object);     /**< Free memory */
    object (*resize)(object, usize); /**< Resize allocation */
    void (*frame_begin)(void);   /**< Begin frame (nullable) */
    void (*frame_end)(void);     /**< End frame (nullable) */
} sc_alloc_use_t;
```

**Key Constraints:**
- `ctrl` must be **first member** (guaranteed offset 0 for cast compatibility)
- `ctrl` can be NULL for non-controller hooks (e.g., malloc wrapper)
- Existing function pointers unchanged (binary compatible layout after first member)

### R2: Update All Hook Constructors in sigma.memory

**Coordinate with sigma.memory team:**
- Update `slb0_to_hook()` to populate `ctrl` field
- Update `bump_to_hook()` to populate `ctrl` field
- Update `reclaim_to_hook()` to populate `ctrl` field

**Example update (slb0):**
```c
sc_alloc_use_t slb0_to_hook(slb0_allocator slb0) {
    sc_alloc_use_t use = {
        .ctrl = (sc_ctrl_base_s*)slb0,  // NEW: first member
        .alloc = slb0_alloc_fn,
        .release = slb0_release_fn,
        .resize = slb0_resize_fn,
        .frame_begin = NULL,
        .frame_end = NULL
    };
    return use;
}
```

### R3: Update Documentation

**Update sigma.core/docs/Allocator.md:**
- Document `ctrl` field purpose (controller state access)
- Explain casting pattern:
  ```c
  sc_alloc_use_t *use = Application.get_allocator();
  sc_ctrl_base_s *ctrl = use->ctrl;
  if (ctrl != NULL) {
      // Access controller vtable for diagnostics
  }
  ```
- Document NULL semantics (non-controller hooks)
- Document first-member guarantee (offset 0 cast safety)

---

## Test Cases

**TC1: ctrl field offset 0**
- **Given:** `sc_alloc_use_t use` variable
- **When:** `offsetof(sc_alloc_use_t, ctrl)` evaluated
- **Then:** Returns 0 (first member guarantee)

**TC2: Hook construction populates ctrl**
- **Given:** `slb0_allocator slb0 = Allocator.slb0()`
- **When:** `sc_alloc_use_t use = slb0_to_hook(slb0)`
- **Then:** `use.ctrl == (sc_ctrl_base_s*)slb0` (pointer equality)

**TC3: Cast compatibility (use → ctrl)**
- **Given:** `sc_alloc_use_t *use` pointer with non-NULL ctrl
- **When:** `sc_ctrl_base_s *ctrl = (sc_ctrl_base_s*)use`
- **Then:** `ctrl == use->ctrl` (cast preserves controller pointer)

**TC4: Cast compatibility (ctrl → use)**
- **Given:** `sc_ctrl_base_s *ctrl` from bump allocator
- **When:** Cast to `sc_alloc_use_t *use = (sc_alloc_use_t*)ctrl`
- **Then:** `use->ctrl == ctrl` (first member cast round-trip)

**TC5: NULL ctrl for non-controller hooks**
- **Given:** Custom malloc-wrapper hook with no controller state
- **When:** Hook constructed with `use.ctrl = NULL`
- **Then:** `Application.get_allocator()->ctrl == NULL` (valid NULL case)

---

## Acceptance Criteria

**Phase 1 Completion Gate:**
- [ ] `sc_alloc_use_t` structure updated with `ctrl` as first member
- [ ] All sigma.core code using sc_alloc_use_t compiles without warnings
- [ ] All 5 test cases pass (TC1-TC5)
- [ ] sigma.memory team notified of structure change (coordinate hook constructor updates)
- [ ] Documentation updated (Allocator.md)
- [ ] sigma.core test suite passes (no regressions)
- [ ] Commit message: "Add ctrl field to sc_alloc_use_t per FR-2603-sigma-core-005"

**Handoff Protocol:**
sigma.core publishes v1.1.0 → sigma.memory team updates hook constructors in Phase 2

---

## Notes

**Binary Compatibility:**
- **Breaking:** Adding first member shifts all subsequent offsets
- **Impact:** Low (all code using designated initializers recompiles cleanly)
- **Migration:** Recompile all projects linking sigma.core (no source changes needed)

**Why First Member (offset 0)?**
- Enables zero-cost cast: `(sc_ctrl_base_s*)use` and `(sc_alloc_use_t*)ctrl`
- Standard C guarantee: first member has same address as struct
- Simplifies controller <-> hook conversions (no offset arithmetic)

**Future Enhancement (Phase 2):**
- FR-2603-sigma-memory-004: `Allocator.to_alloc_use()` helper populates ctrl automatically
- FR-2603-sigma-core-002: `frame_begin/frame_end` on sc_alloc_use_t (nullable, controller-specific)

---

## Related

- **Blocks:** FR-2603-sigma-memory-002 (Application delegation)
- **Depends on:** None (Phase 1 foundation)
- **Related:** FR-2603-sigma-core-004 (Application API), FR-2603-sigma-core-006 (Allocator.dispose rename)
