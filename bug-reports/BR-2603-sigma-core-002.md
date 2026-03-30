# BR-2603-sigma-core-002: ABI Break from FR-005 ctrl Field Addition

**Status**: Open  
**Priority**: Blocker  
**Severity**: Critical  
**Reporter**: sigma.core / GitHub Copilot  
**Date Filed**: 2026-03-26  
**Affected Projects**: sigma.memory, sigma.test, sigma.core  
**Tags**: #abi-break #binary-compatibility #allocator #rebuild-required

---

## Problem Statement

FR-2603-sigma-core-005 introduced an **ABI-breaking change** to `sc_alloc_use_t` by adding `sc_ctrl_base_s *ctrl` as the **first member**. This shifts all subsequent field offsets by 8 bytes (pointer size), **breaking binary compatibility** with packages built against the old header.

**Binary incompatibility manifests as**:
1. **Segmentation faults** when reading function pointers from wrong offsets
2. **Invalid free() / realloc()** when calling functions through shifted vtable
3. **Test framework crashes** during initialization (sigma.test.o fails in `init_default_hooks`)

**Packages affected**:
- `sigma.memory.o` (built 2026-03-18 with old `allocator.h`)
- `sigma.test.o` (built 2026-03-18 with old `allocator.h`)
- Any other package linking sigma.core headers

---

## Impact Analysis

**Severity: Critical (Blocker)** — Prevents:
- sigma.core Phase 1 completion (cannot integrate with sigma.test)
- sigma.memory Phase 2 work (relies on new Application API from FR-004)
- sigma.test usage in sigma.core unit tests

**Current state**:
- ✅ FR-004, FR-005, FR-006 implemented and tested (standalone)
- ✅ 23/23 tests pass, valgrind clean (0 leaks)
- 🔴 **BLOCKED**: Cannot integrate with sigma.memory/sigma.test (ABI incompatible)
- 🟡 **WORKAROUND**: Created `test/mock_allocator.c` + standalone tests (bypasses dependencies)

**Scope: Build-time**:
- ABI break only affects **binary packages** built with old headers
- Source compatibility preserved (recompiling fixes issue)
- No runtime dependency on specific sigma.memory version

---

## Root Cause

### Old sc_alloc_use_t (pre-FR-005):
```c
typedef struct sc_alloc_use_t {
    void *(*alloc)(usize);         // offset 0
    void (*release)(void *);       // offset 8
    void *(*resize)(void *, usize); // offset 16
    frame (*frame_begin)(void);    // offset 24
    void (*frame_end)(frame);      // offset 32
} sc_alloc_use_t;
```

### New sc_alloc_use_t (FR-005):
```c
typedef struct sc_alloc_use_t {
    sc_ctrl_base_s *ctrl;          // offset 0 (NEW!)
    void *(*alloc)(usize);         // offset 8 (was 0) ← SHIFTED!
    void (*release)(void *);       // offset 16 (was 8) ← SHIFTED!
    void *(*resize)(void *, usize); // offset 24 (was 16) ← SHIFTED!
    frame (*frame_begin)(void);    // offset 32 (was 24) ← SHIFTED!
    void (*frame_end)(frame);      // offset 40 (was 32) ← SHIFTED!
} sc_alloc_use_t;
```

**Binary incompatibility**:
- Code compiled with old header reads `alloc` from offset 0
- New structure has `ctrl` pointer at offset 0
- Reading function pointer from offset 0 yields garbage → segfault on call

**Example crash stack trace** (from valgrind):
```
==118152== Invalid free() / delete / delete[] / realloc()
==118152==    at 0x484DB80: realloc
==118152==    by 0x112E9D: mock_realloc (mock_allocator.c:63)
==118152==    by 0x10D7DB: init_default_hooks (sigtest.c:1278)
==118152==  Address 0x10 is not stack'd, malloc'd or (recently) free'd
```

---

## Discovery Context

**Timeline**:
- **2026-03-18**: sigma.memory.o, sigma.test.o built and published to `local_packages/`
- **2026-03-26**: FR-005 implemented, adds `ctrl` field as first member
- **2026-03-26 15:30**: First test run → segfault in `tx_alloc()` (strings.c:54)
- **2026-03-26 15:45**: Diagnosis: `Allocator.alloc` function pointer reads 0x1A (garbage)
- **2026-03-26 16:00**: Root cause identified: sigma.memory.o has old struct layout

**Investigation steps**:
```bash
# Check package build dates
$ ls -lh local_packages/sigma.memory.o
-rw-rw-r-- 1 badkraft badkraft 56K Mar 18 21:58 local_packages/sigma.memory.o

# Verify Allocator vtable in old package
$ nm local_packages/sigma.memory.o | grep Allocator
0000000000000000 D Allocator

# Inspect data section (shows function pointers at wrong offsets)
$ objdump -s local_packages/sigma.memory.o | grep -A 10 "Contents of section .data.rel.ro.local:"
# Function pointers at offsets 0, 8, 16, 24... (no ctrl field)
```

**Workaround implemented**:
1. Created `test/mock_allocator.c` — minimal Allocator facade using malloc/free/realloc
2. Created standalone tests (no sigma.test dependency)
3. Updated `config.sh` — removed `sigma.memory` from `TST_REQUIRES`
4. Updated `rtest` — links `mock_allocator.c` instead of sigma.memory package

---

## Proposed Solution

### Immediate Actions (Required for Phase 1 completion)

#### 1. Rebuild sigma.memory (Priority: Blocker)
**Owner**: sigma.memory team  
**Timeline**: Before sigma.core v1.1.0 release

**Steps**:
```bash
cd /home/badkraft/repos/sigma.memory
# Pull latest sigma.core headers (includes FR-005 changes)
# Headers are in ../sigma.core/include/sigma.core/allocator.h

# Rebuild all sources
./cbuild clean && ./cbuild

# Rebuild package
./cpkg

# Verify new package has ctrl field at offset 0
nm local_packages/sigma.memory.o | grep Allocator
# Should still show: 0000000000000000 D Allocator

# Verify struct layout changed (indirect via relocation table)
readelf -r local_packages/sigma.memory.o | grep -A 20 "\.data\.rel"
# Function pointer offsets should be +8 from old package
```

**Deliverable**: `sigma.memory.o` rebuilt with `allocator.h` including `ctrl` field

---

#### 2. Rebuild sigma.test (Priority: Blocker)
**Owner**: sigma.test team  
**Timeline**: Before sigma.core v1.1.0 release

**Steps**:
```bash
cd /home/badkraft/repos/sigma.test
# Pull latest sigma.core headers

# Rebuild all sources
./cbuild clean && ./cbuild

# Rebuild package
./cpkg

# Verify no ABI conflicts
# (sigma.test doesn't directly use sc_alloc_use_t, but linked code might)
```

**Deliverable**: `sigma.test.o` rebuilt with new headers

---

#### 3. Integrate sigma.core with rebuilt packages (sigma.core team)
**Owner**: sigma.core agent  
**Timeline**: After sigma.memory + sigma.test rebuilt

**Steps**:
```bash
cd /home/badkraft/repos/sigma.core

# Remove workaround
rm test/mock_allocator.c
rm test/standalone_test_application.c
rm test/standalone_test_alloc_use_ctrl.c

# Restore TST_REQUIRES in config.sh
TST_REQUIRES=("sigma.memory" "sigma.test")

# Restore rtest (remove mock_allocator.c from compile)

# Convert standalone tests to sigma.test framework
# Restore: test/unit/test_application.c (5 tests)
# Restore: test/unit/test_alloc_use_ctrl.c (5 tests)

# Run full test suite
./rtest unit --valgrind
# Expect: All tests pass, zero leaks

# Verify Application API integration
./rtest unit/application --valgrind
# Expect: 5/5 tests pass
```

**Deliverable**: sigma.core v1.1.0 fully integrated with sigma.memory + sigma.test

---

### Long-term Preventions

#### 1. Version Pinning
**Recommendation**: Add package version checks to build system

```bash
# In config.sh or cpkg script
REQUIRED_SIGMA_CORE_MIN="1.1.0"
REQUIRED_SIGMA_MEMORY_MIN="X.Y.Z"

# Check installed package versions
if ! check_package_version "sigma.core" "$REQUIRED_SIGMA_CORE_MIN"; then
    echo "ERROR: sigma.core >= $REQUIRED_SIGMA_CORE_MIN required"
    exit 1
fi
```

#### 2. ABI Versioning
**Recommendation**: Add ABI version field to package structs

```c
// In future releases
typedef struct sc_alloc_use_t {
    uint32_t abi_version;  // e.g., 0x00010100 for v1.1.0
    sc_ctrl_base_s *ctrl;
    // ...
} sc_alloc_use_t;

// Runtime check in critical paths
if (use->abi_version != SC_ALLOC_USE_ABI_VERSION) {
    fprintf(stderr, "ABI version mismatch: expected %08x, got %08x\n",
            SC_ALLOC_USE_ABI_VERSION, use->abi_version);
    abort();
}
```

#### 3. Rebuild Notifications
**Recommendation**: Document ABI breaks in CHANGELOG.md with rebuild instructions

```markdown
## v1.1.0 (2026-03-26) — Phase 1 Foundation

### BREAKING CHANGES (ABI)
⚠️ **ACTION REQUIRED**: Rebuild all packages linking sigma.core

FR-2603-sigma-core-005 added `ctrl` field as first member of `sc_alloc_use_t`.
All field offsets shifted by 8 bytes. Packages built with sigma.core v1.0.x
are binary incompatible with v1.1.0.

**Rebuild checklist**:
- [ ] sigma.memory: `./cbuild clean && ./cbuild && ./cpkg`
- [ ] sigma.test: `./cbuild clean && ./cbuild && ./cpkg`
- [ ] sigma.collections: (if applicable)
- [ ] anvil: Relink against new sigma.core headers
```

---

## Testing Verification

### Pre-rebuild State (Current)
```bash
$ ./build/standalone_test_application
✅ SUCCESS: All tests passed! (11 passed, 0 failed)

$ ./build/standalone_test_alloc_use_ctrl
✅ SUCCESS: All tests passed! (12 passed, 0 failed)

$ valgrind --leak-check=full ./build/standalone_test_application
HEAP SUMMARY: 0 bytes in use at exit
ERROR SUMMARY: 0 errors from 0 contexts
```

### Post-rebuild State (Expected)
```bash
$ ./rtest unit/application --valgrind
=== Running test_application (unit) ===
✅ test_set_get_allocator ... PASS
✅ test_default_slb0_fallback ... PASS
✅ test_double_init_assert ... PASS (asserts correctly)
✅ test_reset_to_default ... PASS
✅ test_get_allocator_thread_safe ... PASS

=== Test complete ===
HEAP SUMMARY: 0 bytes in use at exit
ERROR SUMMARY: 0 errors from 0 contexts

$ ./rtest unit/alloc_use_ctrl --valgrind
=== Running test_alloc_use_ctrl (unit) ===
✅ test_ctrl_offset_zero ... PASS (ctrl at offset 0)
✅ test_hook_construction_with_ctrl ... PASS
✅ test_cast_use_to_ctrl ... PASS
✅ test_cast_ctrl_to_use ... PASS
✅ test_null_ctrl_valid ... PASS

=== Test complete ===
HEAP SUMMARY: 0 bytes in use at exit
ERROR SUMMARY: 0 errors from 0 contexts
```

---

## Resolution Criteria

### Completion checklist:
- [ ] sigma.memory rebuilt with new `allocator.h` (ctrl field at offset 0)
- [ ] sigma.test rebuilt with new headers
- [ ] `test/mock_allocator.c` removed from sigma.core
- [ ] Standalone tests converted to sigma.test framework
- [ ] `config.sh` restored: `TST_REQUIRES=("sigma.memory" "sigma.test")`
- [ ] `rtest` restored: no mock_allocator linkage
- [ ] Full test suite passes with --valgrind (zero leaks)
- [ ] sigma.core v1.1.0 CHANGELOG.md documents ABI break + rebuild instructions
- [ ] All affected projects notified of rebuild requirement

**Verification**: 
```bash
cd /home/badkraft/repos/sigma.core
./rtest unit --valgrind
# Expected: All tests pass, HEAP SUMMARY shows 0 bytes in use at exit
```

**Sign-off**: Q-OR Project Oversight confirms:
1. sigma.memory + sigma.test rebuilt successfully
2. sigma.core v1.1.0 integrated + tested
3. No ABI-related crashes in production smoke tests

---

## Related Issues

- **FR-2603-sigma-core-004**: Application.set_allocator() / get_allocator() (implemented, blocked on integration)
- **FR-2603-sigma-core-005**: sc_alloc_use_t.ctrl field (root cause of ABI break)
- **FR-2603-sigma-core-006**: Allocator.dispose rename (implemented, no ABI impact)
- **Phase 2 sigma.memory work**: Blocked until sigma.core v1.1.0 published

---

## Additional Notes

**Why ctrl field must be first**:
- Enables cast compatibility: `(sc_ctrl_base_s*)use` and `(sc_alloc_use_t*)ctrl`
- Allows controller structs to embed `sc_alloc_use_t` as first member
- Simplifies offset calculations in low-level memory management

**Migration effort**:
- **Low for downstream**: Recompile only (source compatible)
- **Medium for sigma.core**: Remove workarounds, restore integration
- **Low for users**: No API changes (internal structure change only)

**Alternative considered**: Append `ctrl` field at end (preserves ABI)
- **Rejected**: Defeats purpose of FR-005 (cast compatibility requires offset 0)
