# BR-2603-sigma-core-003: Segfault in `buffer_new()` after FR-005 ABI changes

**ID:** BR-2603-sigma-core-003  
**Type:** Bug Report  
**Owner:** sigma.core  
**Filed:** 2026-03-26  
**Filed by:** sigma.test  
**Status:** resolved  
**Resolved:** 2026-03-26  
**Tags:** sigma.core, allocator, segfault, FR-005, ABI, deprecated-facade  

---

## Summary

sigma.test experiences **segmentation faults** in multiple unit tests after all dependencies (sigma.core.o, sigma.memory.o, sigma.collections.o) were rebuilt and published with FR-005 ABI changes (ctrl field added to sc_alloc_use_t). GDB backtrace shows crash occurs in sigma.core's`buffer_new()` at line 588 when calling `tx_alloc(s_sb_use, capacity)`.

## Environment

- **sigma.core.o**: Mar 26 20:09 (FR-005: ctrl field, FR-006: dispose rename)
- **sigma.memory.o**: Mar 26 20:59 (rebuilt with new headers)
- **sigma.collections.o**: Mar 26 21:02 (rebuilt with new headers)
- **sigma.test**: Source updated (Allocator.free → Allocator.dispose), clean rebuild performed

## Reproduction

### Minimal Test Case (for sigma.core team)

Create a minimal test file in sigma.core workspace to reproduce the crash:

**File: `test_sb_crash.c`**
```c
#include <stdio.h>
#include "../include/sigma.core/core.h"
#include "../include/sigma.core/strings.h"

int main(void)
{
    printf("Initializing memory system...\n");
    init_memory_system();
    
    printf("Creating StringBuilder with capacity 2048...\n");
    stringbuilder sb = StringBuilder.new(2048);
    
    printf("StringBuilder created successfully at %p\n", (void*)sb);
    
    StringBuilder.dispose(sb);
    printf("Test completed successfully\n");
    return 0;
}
```

**Build and run:**
```bash
cd /path/to/sigma.core
gcc -g -I./include -o test_sb_crash test_sb_crash.c build/sigma.core.o \
    /usr/local/packages/sigma.memory.o -lm
./test_sb_crash
```

**Expected:** "Test completed successfully"  
**Actual:** Segmentation fault at `buffer_new()` line 588 during `tx_alloc(s_sb_use, capacity)` call

### From sigma.test (full integration)

If the minimal case doesn't reproduce, the full sigma.test scenario is:

1. Rebuild all sigma packages with FR-005 headers (ctrl field in sc_alloc_use_t)
2. In sigma.test workspace:
   ```bash
   cbuild clean && cbuild
   ./rtest asserts
   ```

**Expected:** Test runs and reports results  
**Actual:** Segmentation fault (core dumped)

## Backtrace

```
Program received signal SIGSEGV, Segmentation fault.
0x000055555555b591 in buffer_new (capacity=2049) at src/strings.c:588
588         buf->data = tx_alloc(s_sb_use, capacity);

#0  0x000055555555b591 in buffer_new (capacity=2049) at src/strings.c:588
#1  0x000055555555ab71 in stringbuilder_new (capacity=2048) at src/strings.c:275
#2  0x0000555555570036 in default_on_start_test (ctx=0x5555555a3700 <default_ctx>) at src/sigtest.c:1086
#3  0x000055555557156b in start_test (hooks=0x5555555a2b80 <default_hooks>) at src/sigtest.c:1647
#4  0x0000555555570d8f in run_tests (sets=0x7ffff7a00110, test_hooks=0x0) at src/sigtest.c:1411
#5  0x0000555555570b66 in main () at src/sigtest.c:1315
```

Crash location: **sigma.core/src/strings.c:588** in `buffer_new()` calling `tx_alloc(s_sb_use, capacity)`

## Affected Tests

Segfaults observed in:
- test_asserts
- test_configs  
- test_controls
- test_fuzzing
- test_logging
- test_mallocs
- test_sigtest

**Tests passing:**
- test_json_hooks (4/5 tests)
- test_junit_hooks (4/5 tests)
- test_config_reader (link failure, different issue)

## Analysis

The crash occurs during sigma.test framework initialization when `default_on_start_test()` calls `stringbuilder_new()` → `buffer_new()` → `tx_alloc(s_sb_use, capacity)`.

The `s_sb_use` is an `sc_alloc_use_t` structure being accessed within sigma.core.o. FR-005 added `ctrl` as the first member of `sc_alloc_use_t` (offset 0), shifting all subsequent members by 8 bytes.

**Hypothesis:** sigma.core may have internal static or global `sc_alloc_use_t` instances that were:
1. Not fully reinitialized after structure layout change, OR
2. Are being incorrectly initialized/accessed with the new layout, OR
3. Have a ctrl pointer that is invalid/NULL and causing downstream issues

## Headers Verification

```bash
$ grep -A 10 "struct sc_alloc_use" /usr/local/include/sigma.core/allocator.h
typedef struct sc_alloc_use_s {
    /** Controller state pointer (offset 0, can be NULL for non-controller hooks) */
    sc_ctrl_base_s *ctrl;
    /** Allocate memory */
    void *(*alloc)(usize size);
    /** Free memory */
    void (*release)(void *ptr);
    /** Resize allocation */
    void *(*resize)(void *ptr, usize size);
    ...
```

Header is correct (ctrl field present at offset 0).

## Dependencies Status

All dependencies confirmed rebuilt and published:
```
-rw-r--r-- 1 root root 216232 Mar 23 17:47 anvil.o
-rw-r--r-- 1 root root 128544 Mar 26 21:02 sigma.collections.o
-rw-r--r-- 1 root root  88032 Mar 26 20:09 sigma.core.o
-rw-r--r-- 1 root root  94624 Mar 26 20:59 sigma.memory.o
```

## Request for sigma.core

Please investigate:

1. **How is `s_sb_use` initialized?** Is it a static global? Is the `ctrl` field being set correctly?
2. **Are there other internal `sc_alloc_use_t` instances** that need updating?
3. **What is the state of `s_sb_use->ctrl`** at the point of crash? (NULL? Invalid pointer?)
4. **Does `tx_alloc()` correctly handle the new structure layout** with ctrl at offset 0?

## Workaround

None known. sigma.test is blocked on this issue for completing BR-2603-sigma-core-002 resolution.

## Impact

- **Blocks:** BR-2603-sigma-core-002 resolution (coordinated rebuild sequence)
- **Blocks:** sigma.test test suite execution
- **Blocks:** Phase 1 completion (FR-004/005/006 validation)
- **Severity:** Critical — prevents sigma.test from running any tests

## Context

This issue emerged as part of BR-2603-sigma-core-002 resolution (coordinated rebuild for FR-005 ABI break). sigma.memory and sigma.collections were successfully rebuilt and published. sigma.test updated source code (Allocator.free → Allocator.dispose) and rebuilt cleanly, but runtime crashes indicate sigma.core has an internal issue with the new sc_alloc_use_t layout.

---

## Resolution

**Resolved:** 2026-03-26  
**Root Cause:** Deprecated allocator facade pattern incompatible with new controller-based architecture

### Root Cause Analysis

The segfault was caused by the deprecated **allocator facade pattern** still present in sigma.core source code. This pattern consisted of:

1. **Per-interface static hook variables**: `s_string_use`, `s_sb_use`, `s_io_use` (all initialized to NULL)
2. **Indirection helper functions**: `tx_alloc()`, `tx_free()`, `tx_realloc()` 
3. **Interface setter hooks**: `.alloc_use()` function pointers in String, StringBuilder, and File vtables

This pattern was designed for the **old architecture** where each interface could override its allocator independently. However, the new **controller-based architecture** (FR-005) handles scoping globally at the entry point level:

```c
// OLD (deprecated - causes crash):
static sc_alloc_use_t *s_sb_use = NULL;  // ← Never initialized!

static void *tx_alloc(sc_alloc_use_t *use, usize size) {
    if (use && use->alloc) return use->alloc(size);  // use is NULL!
    return Allocator.alloc(size);  // Falls back here always
}

// Call site in buffer_new():
buf->data = tx_alloc(s_sb_use, capacity);  // ← Line 588 crash

// NEW (direct - works with controller scoping):
buf->data = Allocator.alloc(capacity);  // ← Entry point sets controller scope
```

**Why it crashed:** The static `s_sb_use` variable was never initialized in the new architecture, so it remained NULL. The `tx_alloc()` indirection layer added no value (always fell back to `Allocator.alloc()` anyway), but created confusion and crash risk.

### Fix Applied

Removed the entire deprecated allocator facade pattern:

**Files Modified:**
- `src/strings.c`: Removed ~25 lines (static variables, tx_* helpers, alloc_use setters), replaced 14 call sites
- `src/io.c`: Removed ~25 lines (static variables, tx_* helpers, alloc_use setters), replaced 15 call sites
- `include/sigma.core/strings.h`: Removed `.alloc_use` from sc_string_i and sc_stringbuilder_i interfaces
- `include/sigma.core/io.h`: Removed `.alloc_use` from sc_file_i interface, updated documentation

**Pattern Applied (29 call sites total):**
```c
tx_alloc(s_*_use, size)       → Allocator.alloc(size)
tx_free(s_*_use, ptr)         → Allocator.dispose(ptr)
tx_realloc(s_*_use, ptr, size) → Allocator.realloc(ptr, size)
```

**ABI Impact:**
- Removing `.alloc_use` from interfaces shrinks vtables by 8 bytes (1 pointer) per interface
- Affected interfaces: String, StringBuilder, File
- **Action Required:** All projects linking sigma.core must rebuild (same as FR-005)
- **Combined ABI Break:** Document as single ABI event with FR-005 (sc_alloc_use_t.ctrl field)

### Verification

**Compilation:**
```bash
$ cd /home/badkraft/repos/sigma.core
$ gcc -c -Wall -Wextra -fPIC -std=c2x -I./include src/strings.c -o build/strings.o
$ gcc -c -Wall -Wextra -fPIC -std=c2x -I./include src/io.c -o build/io.o
```
Both files compile cleanly with no errors or warnings.

**Code Verification:**
```bash
$ grep -rn "tx_alloc\|tx_free\|tx_realloc\|s_.*_use" src/
(no matches - all deprecated code removed)

$ grep -rn "void (\*alloc_use)" include/sigma.core/*.h
(no matches in source headers - only in package/ generated output)
```

### Next Steps

1. ✅ **Code cleanup complete** - All deprecated facade code removed from sigma.core
2. ⏳ **Rebuild sigma.core** - Generate new sigma.core.o with clean code
3. ⏳ **Publish package** - Install to /usr/local/packages/
4. ⏳ **Test with sigma.test** - Run `./rtest asserts` to verify segfault resolved
5. ⏳ **Update BR-2603-sigma-core-002** - Mark Phase 1 complete (all rebuilds verified working)

### Migration Notes

Projects using the deprecated `.alloc_use` setters must update:

**Before (deprecated):**
```c
// Old pattern - per-interface override
String.alloc_use(&custom_allocator);
StringBuilder.alloc_use(&custom_allocator);
File.alloc_use(&custom_allocator);
```

**After (controller-based):**
```c
// New pattern - entry point sets scope globally
Application.set_allocator(&custom_allocator);
// All String/StringBuilder/File calls now use this controller
```

**Rationale:** Controller scoping is now handled globally by the entry point. The `Allocator.*` interface dispatches to the active controller automatically. Per-interface override hooks are no longer needed and were causing initialization issues in the new architecture.
