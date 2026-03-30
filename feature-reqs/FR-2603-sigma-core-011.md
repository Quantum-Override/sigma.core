# FR-2603-sigma-core-011: sigma.string.a — Malloc Variant Extraction

**ID:** FR-2603-sigma-core-011  
**Type:** Feature Request  
**Owner:** sigma.core  
**Filed:** 2026-03-25  
**Status:** closed  
**Completed:** 2026-03-26  
**Priority:** High  
**Tags:** phase-0, quick-win, malloc-variant, orchestration  
**Orchestration:** [ORCHESTRATION-BR-2603-q-or-001.md](../q-or/ORCHESTRATION-BR-2603-q-or-001.md) — Phase 0 (Quick Wins)

---

## Purpose

Extract String functions into standalone malloc variant for zero-dependency usage (sampling, tooling, embedded environments). This is **prep work** — sigma.core team extracts and adapts code, toolsmith builds the `.a` library.

---

## Context

**Problem:** sigma.core.text will soon depend on sigma.memory (controllers), but some use cases need zero-dep strings:
- Sampling/profiling tools (can't link sigma.memory, circular dependency)
- Minimal embedded environments (no mmap, no controller overhead)
- Testing utilities needing pure malloc allocations

**Solution:** Create malloc variant static library (sigma.string.a) — pure libc, system malloc/free, no sigma.memory dependency.

**Orchestration:** Part of Phase 0 (Quick Wins) — runs parallel with Phase 1 (sigma.core foundation). Once sigma.core AND sigma.collections prep complete, toolsmith builds both .a libraries.

---

## Scope

**In scope:**
- Extract String core functions (length, copy, dupe, concat, compare, format, dispose, to_array)
- Adapt to pure malloc/free (remove all Allocator.* calls)
- Create standalone source file: `src/string_malloc.c`
- Create standalone header: `include/string_malloc.h` (or reuse subset of existing strings.h)
- Write standalone test suite: `test/standalone/test_string_malloc.c` (no sigma.test dependency)
- Document build instructions in sigma.string.a.md

**Out of scope:**
- StringBuilder (defer — more complex, rope structures)
- set_allocator hooks (hardwired to malloc)
- Integration with sigma.memory controllers
- Packaging/installation (toolsmith handles in FR-2603-toolchain-006)

**NOT building the .a yet** — just prep the code. Toolsmith will create build script + package after both core and collections are ready.

---

## Requirements

### R1: Standalone Source File

**File:** `src/string_malloc.c`

**Contents:**
- String core functions extracted from `src/strings.c`
- All `Allocator.alloc()` → `malloc()`
- All `Allocator.free()` → `free()` (or keep as `dispose` for consistency)
- All `Allocator.realloc()` → `realloc()`
- Remove `static sc_alloc_use_t *s_string_use` global
- Remove dispatch helpers (`tx_alloc`, `tx_free`, `tx_realloc`)
- Pure stdlib: `malloc`, `free`, `realloc`, `memcpy`, `strlen`, `strcmp`, `snprintf`

**Functions to include:**
1. `string_length(const string)` → `strlen()` wrapper
2. `string_copy(const string)` → malloc + memcpy
3. `string_duplicate(const char *)` → `strdup()` or malloc + strcpy
4. `string_concat(const string, const string)` → malloc + memcpy × 2
5. `string_compare(const string, const string)` → `strcmp()` wrapper
6. `string_format(const char *fmt, ...)` → `vsnprintf` + malloc
7. `string_dispose(string)` → `free()`
8. `string_to_array(string)` → malloc + memcpy

**Exclude:**
- `string_builder` functions (defer to future work)
- Any controller/allocator hooks

### R2: Standalone Header

**File:** `include/string_malloc.h`

**Contents:**
```c
#pragma once
#include <stddef.h>

typedef char *string;
typedef size_t usize;

// String lifecycle
string string_duplicate(const char *cstr);  // malloc + strcpy
string string_copy(const string src);       // malloc + memcpy
void string_dispose(string s);              // free()

// String operations
usize string_length(const string s);
string string_concat(const string s1, const string s2);
int string_compare(const string s1, const string s2);
string string_format(const char *fmt, ...);

// Utilities
char *string_to_array(string s);  // malloc + memcpy (caller owns)
```

**OR** reuse subset of existing `include/sigma.core/strings.h` with conditional compilation:
```c
#ifdef SIGMA_STRING_MALLOC
// No sc_string_i vtable, just function declarations
#endif
```

### R3: Standalone Test Suite

**File:** `test/standalone/test_string_malloc.c`

**Requirements:**
- **No sigma.test dependency** — pure stdio + assert
- Test pattern:
  ```c
  #include <stdio.h>
  #include <assert.h>
  #include <string.h>
  #include "string_malloc.h"
  
  void test_string_copy(void) {
      string orig = string_duplicate("hello");
      string copy = string_copy(orig);
      assert(strcmp(orig, copy) == 0);
      assert(orig != copy);  // different pointers
      string_dispose(orig);
      string_dispose(copy);
      printf("✓ test_string_copy\n");
  }
  
  int main(void) {
      test_string_copy();
      test_string_concat();
      test_string_format();
      // ... all 8 functions
      printf("All tests passed.\n");
      return 0;
  }
  ```

**Test cases (minimum 8):**
1. `test_string_duplicate` — malloc + strcpy, dispose
2. `test_string_copy` — malloc + memcpy, dispose
3. `test_string_length` — strlen wrapper
4. `test_string_concat` — malloc + double copy
5. `test_string_compare` — strcmp wrapper
6. `test_string_format` — vsnprintf + malloc
7. `test_string_to_array` — malloc + memcpy
8. `test_string_dispose_null` — free(NULL) safe

**Memory leak check (manual):**
```bash
valgrind --leak-check=full ./test_string_malloc
# Must report: "All heap blocks were freed -- no leaks are possible"
```

### R4: Build Instructions Document

**File:** `docs/sigma.string.a.md`

**Contents:**
```markdown
# sigma.string.a — Malloc Variant Static Library

**Purpose:** Zero-dependency string utilities using system malloc/free.

**Build (manual, for development):**
```bash
gcc -c src/string_malloc.c -o build/string_malloc.o -Iinclude -std=c2x -Wall
ar rcs build/libsigma_string.a build/string_malloc.o
```

**Test:**
```bash
gcc test/standalone/test_string_malloc.c -o build/test_string_malloc \
    -Iinclude -std=c2x -Wall -Lbuild -lsigma_string
./build/test_string_malloc
valgrind --leak-check=full ./build/test_string_malloc
```

**Installation (toolsmith handles this):**
- Static lib: `/usr/local/lib/libsigma_string.a`
- Header: `/usr/local/include/sigma/string_malloc.h`

**Link against (consumer projects):**
```bash
gcc myapp.c -o myapp -lsigma_string
```

**Dependencies:** None (libc only: malloc, free, string.h, stdio.h)

**NOT for Sigma.* ecosystem** — use sigma.core.text.o (controller variant) instead.
```

---

## Test Cases

**TC1: Standalone Compilation**
- **Given:** `src/string_malloc.c` + `include/string_malloc.h`
- **When:** `gcc -c src/string_malloc.c -Iinclude -std=c2x`
- **Then:** Compiles cleanly, no sigma.memory symbols in object file (verify with `nm`)

**TC2: Standalone Test Suite**
- **Given:** `test/standalone/test_string_malloc.c` compiled + linked against string_malloc.o
- **When:** `./test_string_malloc`
- **Then:** All 8+ tests pass, "All tests passed." printed

**TC3: No Memory Leaks**
- **Given:** test_string_malloc binary
- **When:** `valgrind --leak-check=full ./test_string_malloc`
- **Then:** "All heap blocks were freed -- no leaks are possible"

**TC4: Zero Dependencies**
- **Given:** `build/string_malloc.o`
- **When:** `nm build/string_malloc.o | grep -E '(Allocator|sigma_memory)'`
- **Then:** No matches (pure malloc/free, no sigma.memory symbols)

**TC5: Static Library Creation (toolsmith will do, but verify preparedness)**
- **Given:** string_malloc.o
- **When:** `ar rcs libsigma_string.a string_malloc.o`
- **Then:** Archive created, `ar t libsigma_string.a` shows string_malloc.o

---

## Acceptance Criteria

- [ ] `src/string_malloc.c` exists, compiles cleanly (gcc -c -std=c2x)
- [ ] `include/string_malloc.h` exists, no sigma.memory dependencies
- [ ] `test/standalone/test_string_malloc.c` exists, runs, all tests pass
- [ ] Valgrind reports zero leaks for test_string_malloc
- [ ] `nm` on string_malloc.o shows only malloc/free/realloc (no Allocator symbols)
- [ ] `docs/sigma.string.a.md` documents build + test + usage
- [ ] WIP updated: sigma.core ready for toolsmith handoff (FR-2603-toolchain-006)

---

## Deliverables

**To sigma.core repo:**
1. `src/string_malloc.c` — standalone malloc-based String functions
2. `include/string_malloc.h` — public API header
3. `test/standalone/test_string_malloc.c` — standalone test suite (8+ tests, stdio + assert)
4. `docs/sigma.string.a.md` — build + test + usage instructions

**Handoff to toolsmith:**
- sigma.core WIP updated: "sigma.string.a prep complete, ready for packaging"
- Toolsmith proceeds with FR-2603-toolchain-006 (build .a, install to /usr/local/lib, publish)

---

## Dependencies

**Blocks:** FR-2603-toolchain-006 (toolsmith builds sigma.string.a)  
**Blocked by:** None (can start immediately)  
**Parallel with:** FR-2603-sigma-collections-005 (sigma.arrays.a prep)

---

## Notes

**Why not StringBuilder?**
- More complex (rope structures, internal buffering)
- Defer to sigma.string.a v2.0 if needed

**Why standalone test suite?**
- sigma.test depends on sigma.core — circular dependency
- Standalone validates zero-dep claim

**Naming: string_dispose vs. string_free?**
- Use `string_dispose` for consistency with ecosystem (see orchestration naming convention)
- Internally calls `free()`, but API uses `dispose` terminology

**Future:** If users need StringBuilder in malloc variant, file FR-2603-sigma-core-XXX for v2.0.

---

## Completion Summary

**Completed:** 2026-03-26  
**Commit:** [187dcfd](https://github.com/Quantum-Override/sigma.core/commit/187dcfd)

### Scope Expansion

**Original scope:** 8 String functions (StringBuilder deferred to v2.0)  
**Final scope:** 22 functions — 8 String + 14 StringBuilder (user directive: "StringBuilder should have been included in the workload")

### Deliverables

**Files created:**
1. `src/string_malloc.c` (~570 lines) — String + StringBuilder implementation
2. `include/string_malloc.h` (~220 lines) — public API header
3. `test/standalone/test_string_malloc.c` (~460 lines) — 21 comprehensive tests
4. `docs/sigma.string.a.md` — complete build/test/usage documentation

**String API (8 functions):**
- `string_duplicate`, `string_copy`, `string_dispose`
- `string_length`, `string_concat`, `string_compare`
- `string_format`, `string_to_array`

**StringBuilder API (14 functions):**
- `stringbuilder_new`, `stringbuilder_from_string`, `stringbuilder_dispose`
- `stringbuilder_append`, `stringbuilder_appendf`, `stringbuilder_appendl`
- `stringbuilder_lappends`, `stringbuilder_lappendf`
- `stringbuilder_clear`, `stringbuilder_to_string`, `stringbuilder_to_stream`
- `stringbuilder_length`, `stringbuilder_capacity`, `stringbuilder_set_capacity`

### Verification

**Compilation:** Clean (gcc -std=c2x -Wall -Wextra, zero warnings)  
**Symbol check:** Pure libc only (malloc, free, realloc, memcpy, strlen, strcmp, strcpy, fwrite, vsnprintf)  
**Tests:** 21/21 passed (9 String + 12 StringBuilder)  
**Memory safety:** Valgrind clean — 79 allocs/79 frees, 5,704 bytes, **zero leaks**

### Handoff Status

✅ **Ready for toolsmith** (FR-2603-toolchain-006)  
- All code complete and tested
- Documentation complete
- Zero dependencies verified
- Memory safety validated

**Next:** Toolsmith builds `.a` library, creates install script, publishes to `/usr/local/lib/libsigma_string.a`
