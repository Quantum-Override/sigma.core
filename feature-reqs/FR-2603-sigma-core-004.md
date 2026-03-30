# FR-2603-sigma-core-004: Application Allocator API

**ID:** FR-2603-sigma-core-004  
**Type:** Feature Request  
**Owner:** sigma.core  
**Filed:** 2026-03-26  
**Status:** open  
**Priority:** Critical  
**Tags:** phase-1, allocator, application-api, br-2603-q-or-001  
**Orchestration:** [ORCHESTRATION-BR-2603-q-or-001.md](../q-or/ORCHESTRATION-BR-2603-q-or-001.md) — Phase 1 (sigma.core foundation)  
**Blocks:** FR-2603-sigma-memory-002 (Application delegation), Phase 2

---

## Purpose

Implement Application-level allocator control API to provide single indirection point for all memory allocation in the Sigma.* ecosystem. Resolves allocator scoping vulnerability (BR-2603-q-or-001) by establishing centralized control.

---

## Context

**Problem:** Current architecture allows builders (Context.build, Vars.build) to assume SLB0 allocator without enforcement, creating heap corruption risk if app uses custom controllers.

**Solution:** Application API provides single set point at module_init, with all downstream code delegating through Application.get_allocator().

**Design Principle:** Single responsibility — Application owns allocator policy, components respect it.

---

## Requirements

### R1: Application.set_allocator() API

**Add function:**
```c
/**
 * @brief Sets the application-wide allocator
 * @param use Allocator hook to use for all subsequent allocations
 * 
 * CONSTRAINTS:
 * - Must be called from module_init hook (before any allocations)
 * - Must be called at most once (asserts on second call)
 * - NULL is valid (resets to default SLB0)
 * 
 * THREAD SAFETY: Single-threaded during module init (no lock needed)
 */
void Application_set_allocator(sc_alloc_use_t *use);
```

**Implementation:**
- Static global: `static sc_alloc_use_t *s_app_allocator = NULL;`
- Assert if called twice: `assert(s_app_allocator == NULL || use == NULL);`
- Store pointer: `s_app_allocator = use;`

### R2: Application.get_allocator() API

**Add function:**
```c
/**
 * @brief Gets the current application-wide allocator
 * @return Allocator hook, or SLB0 default if never set
 * 
 * GUARANTEES:
 * - Never returns NULL (safe fallback to SLB0)
 * - Callable from any context (read-only, thread-safe)
 */
sc_alloc_use_t* Application_get_allocator(void);
```

**Implementation:**
```c
sc_alloc_use_t* Application_get_allocator(void) {
    if (s_app_allocator != NULL) {
        return s_app_allocator;
    }
    // Safe fallback: return SLB0 hook
    return Allocator.to_alloc_use((sc_ctrl_base_s*)Allocator.slb0());
}
```

**Note:** Requires `Allocator.to_alloc_use()` helper (see FR-2603-sigma-memory-004), but that's Phase 2. For Phase 1, use inline SLB0 hook construction or wait for sigma.memory.

### R3: Integration into Application Interface

**Update sc_application_i vtable:**
```c
typedef struct sc_application_i {
    // ... existing fields ...
    
    /**
     * @brief Set application-wide allocator (once, at module_init)
     */
    void (*set_allocator)(sc_alloc_use_t *use);
    
    /**
     * @brief Get current application-wide allocator
     */
    sc_alloc_use_t* (*get_allocator)(void);
} sc_application_i;
```

**Update Application vtable initialization:**
```c
const sc_application_i Application = {
    // ... existing assignments ...
    .set_allocator = Application_set_allocator,
    .get_allocator = Application_get_allocator
};
```

### R4: Documentation

**Update sigma.core/docs/Application.md:**
- Add "Memory Allocation" section
- Explain Application.set_allocator() timing constraint (module_init only)
- Provide example usage:
  ```c
  void my_module_init(void) {
      reclaim_allocator r = Allocator.create_reclaim(1 << 20);
      Application.set_allocator(Allocator.to_alloc_use(r));
  }
  ```
- Document fallback behavior (SLB0 if never set)
- Document assert behavior (double-set forbidden)

---

## Test Cases

**TC1: set_allocator at module_init**
- **Given:** Application is initialized
- **When:** `Application.set_allocator(custom_hook)` called from module_init
- **Then:** `Application.get_allocator()` returns `custom_hook`

**TC2: get_allocator default fallback**
- **Given:** Application is initialized, `set_allocator()` never called
- **When:** `Application.get_allocator()` called
- **Then:** Returns SLB0 hook (not NULL)

**TC3: set_allocator assert on double-set**
- **Given:** Application is initialized, `set_allocator(hook1)` already called
- **When:** `Application.set_allocator(hook2)` called
- **Then:** Assert fires (test with `NDEBUG` disabled)

**TC4: set_allocator(NULL) resets to default**
- **Given:** Application is initialized, `set_allocator(custom_hook)` called
- **When:** `Application.set_allocator(NULL)` called
- **Then:** `Application.get_allocator()` returns SLB0 hook

**TC5: Thread-safety of get_allocator (read-only)**
- **Given:** Application allocator set to `custom_hook`
- **When:** Multiple threads call `Application.get_allocator()` concurrently
- **Then:** All threads receive same `custom_hook` pointer (no race)

---

## Acceptance Criteria

**Phase 1 Completion Gate:**
- [ ] `Application.set_allocator()` and `Application.get_allocator()` implemented
- [ ] sc_application_i vtable updated with new function pointers
- [ ] All 5 test cases pass (TC1-TC5)
- [ ] Documentation updated (Application.md)
- [ ] sigma.core test suite passes (no regressions)
- [ ] Commit message: "Add Application allocator API per FR-2603-sigma-core-004"

**Handoff Protocol:**
sigma.core publishes v1.1.0 → sigma.memory team unblocked for Phase 2 (FR-2603-sigma-memory-002)

---

## Notes

**Design Decision: Why assert on double-set instead of silent override?**
- Early detection of misconfiguration (multiple modules trying to control allocator)
- Forces intentional architecture (single allocator policy per application)
- If override needed, caller must explicitly reset with `set_allocator(NULL)` first

**Migration Path:**
- No breaking changes (additive API)
- Existing code works unchanged (SLB0 default fallback)
- Opt-in: Apps call `set_allocator()` at module_init to customize

**Future Enhancement (deferred):**
- FR-2603-sigma-core-012: Add `Application.push_allocator()` / `Application.pop_allocator()` stack for scoped overrides (if needed)

---

## Related

- **Blocks:** FR-2603-sigma-memory-002 (Application delegation)
- **Depends on:** None (Phase 1 foundation)
- **Related:** FR-2603-sigma-core-005 (sc_alloc_use_t structure), FR-2603-sigma-core-006 (Allocator.dispose rename)
