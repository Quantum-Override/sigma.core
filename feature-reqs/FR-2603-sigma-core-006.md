# FR-2603-sigma-core-006: Allocator.dispose Rename

**ID:** FR-2603-sigma-core-006  
**Type:** Feature Request  
**Owner:** sigma.core  
**Filed:** 2026-03-26  
**Status:** open  
**Priority:** High  
**Tags:** phase-1, allocator, naming, br-2603-q-or-001  
**Orchestration:** [ORCHESTRATION-BR-2603-q-or-001.md](../q-or/ORCHESTRATION-BR-2603-q-or-001.md) — Phase 1 (sigma.core foundation)  
**Blocks:** FR-2603-sigma-memory-001 (sigma.memory rename), Phase 2

---

## Purpose

Rename `Allocator.free()` to `Allocator.dispose()` for semantic clarity and consistency with String/StringBuilder disposal pattern. Part of ecosystem-wide naming standardization.

---

## Context

**Current API:**
- `String.dispose()` ✓
- `StringBuilder.dispose()` ✓
- `FArray.dispose()` ✓ (sigma.collections)
- `Allocator.free()` ✗ (inconsistent)

**Problem:** "free" is C stdlib terminology (malloc/free), misleading when allocator may be arena/bump/reclaim (no actual free list). "dispose" conveys intentional cleanup without implementation assumptions.

**Solution:** Rename `Allocator.free → Allocator.dispose` throughout sigma.core.

---

## Requirements

### R1: Update sc_allocator_i Interface

**Current vtable (sigma.core/include/memory.h):**
```c
typedef struct sc_allocator_i {
    object (*alloc)(usize);
    void (*free)(object);       // OLD NAME
    object (*realloc)(object, usize);
    // ... other fields ...
} sc_allocator_i;
```

**Updated vtable:**
```c
typedef struct sc_allocator_i {
    object (*alloc)(usize);
    void (*dispose)(object);    // NEW NAME
    object (*realloc)(object, usize);
    // ... other fields ...
} sc_allocator_i;
```

### R2: Update Allocator Vtable Implementation

**Current implementation (sigma.core/src/allocator.c or memory.c):**
```c
const sc_allocator_i Allocator = {
    .alloc = allocator_alloc_fn,
    .free = allocator_free_fn,     // OLD
    .realloc = allocator_realloc_fn,
    // ...
};
```

**Updated implementation:**
```c
const sc_allocator_i Allocator = {
    .alloc = allocator_alloc_fn,
    .dispose = allocator_dispose_fn,  // NEW (or rename implementation function)
    .realloc = allocator_realloc_fn,
    // ...
};
```

**Note:** Internal implementation function can stay as `allocator_free_fn` or be renamed to `allocator_dispose_fn` for consistency (prefer rename).

### R3: Update All sigma.core Call Sites

**Search pattern:**
```bash
cd sigma.core
grep -rn "Allocator\.free" src/ test/ include/
```

**Update all matches:**
```c
// OLD:
Allocator.free(ptr);

// NEW:
Allocator.dispose(ptr);
```

**Expected locations:**
- `src/strings.c` — String/StringBuilder disposal
- `src/module.c` — Module cleanup
- `src/io.c` — File/buffer cleanup
- `test/*.c` — Test cleanup paths

### R4: Update Documentation

**Update sigma.core/docs/Allocator.md:**
- Replace all `Allocator.free()` references with `Allocator.dispose()`
- Add migration note:
  ```markdown
  ## v1.1.0 Breaking Change: Allocator.free → Allocator.dispose
  
  **Rationale:** Semantic clarity — "dispose" conveys intentional cleanup
  without malloc/free implementation assumptions.
  
  **Migration:** Global find/replace `Allocator.free` → `Allocator.dispose`
  ```

**Update sigma.core/CHANGELOG.md:**
```markdown
## v1.1.0 (2026-03-26)

### Breaking Changes
- **Allocator.free → Allocator.dispose** (FR-2603-sigma-core-006)
  - Renamed for semantic clarity and ecosystem consistency
  - Migration: Replace all `Allocator.free()` calls with `Allocator.dispose()`
```

---

## Test Cases

**TC1: Allocator.dispose compiles**
- **Given:** sigma.core source updated to use `Allocator.dispose()`
- **When:** `make compile` executed
- **Then:** No compilation errors or warnings

**TC2: Allocator.dispose runtime behavior unchanged**
- **Given:** Test code using `Allocator.dispose(ptr)`
- **When:** Test executed with valgrind
- **Then:** Memory freed correctly, no leaks (same behavior as old `.free()`)

**TC3: No Allocator.free references remain**
- **Given:** sigma.core source tree
- **When:** `grep -rn "Allocator\.free" src/ test/ include/`
- **Then:** Zero matches (all renamed to `.dispose()`)

**TC4: sigma.core test suite passes**
- **Given:** All source updated to use `Allocator.dispose()`
- **When:** `./rtest all` executed
- **Then:** All tests pass (no regressions)

**TC5: Example code in docs uses dispose**
- **Given:** sigma.core/docs/Allocator.md, CHANGELOG.md, README.md
- **When:** Grep for `Allocator.free` in docs/
- **Then:** Zero matches (all examples updated)

---

## Acceptance Criteria

**Phase 1 Completion Gate:**
- [ ] `sc_allocator_i.free` renamed to `.dispose` in vtable definition
- [ ] `Allocator` vtable implementation updated (`.dispose = allocator_dispose_fn`)
- [ ] All sigma.core call sites updated (strings.c, module.c, io.c, test files)
- [ ] All 5 test cases pass (TC1-TC5)
- [ ] Documentation updated (Allocator.md, CHANGELOG.md)
- [ ] sigma.core test suite passes (no regressions)
- [ ] Commit message: "Rename Allocator.free → Allocator.dispose per FR-2603-sigma-core-006"

**Handoff Protocol:**
sigma.core publishes v1.1.0 → sigma.memory team implements FR-2603-sigma-memory-001 (same rename in sigma.memory)

---

## Notes

**Breaking Change Justification:**
- Pre-1.0 acceptable for ecosystem cleanup
- Automated migration (global find/replace)
- Semantic clarity improves code readability
- Consistent with String/StringBuilder naming convention

**Coordination with Phase 3B:**
- Phase 1: sigma.core + sigma.memory rename internally
- Phase 3B: toolsmith orchestrates ecosystem-wide rename (anvil, sigma.test, sigma.build, etc.)
- See FR-2603-toolchain-005 for cascade plan

**Why "dispose" not "release"?**
- "release" already used in sc_alloc_use_t (hook delegation)
- "dispose" matches String/StringBuilder precedent
- "dispose" implies ownership transfer completes (vs. "release" = ref counting)

---

## Related

- **Blocks:** FR-2603-sigma-memory-001 (sigma.memory rename)
- **Depends on:** None (Phase 1 foundation)
- **Related:** FR-2603-sigma-core-004 (Application API), FR-2603-sigma-core-005 (sc_alloc_use_t structure)
- **Future:** FR-2603-toolchain-005 (ecosystem-wide cascade, Phase 3B)
