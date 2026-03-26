# sigma.string.a — Malloc Variant Static Library

**Purpose:** Zero-dependency string and stringbuilder utilities using system malloc/free.

**Status:** Phase 0 prep complete (FR-2603-sigma-core-011) — ready for toolsmith packaging  
**Part of:** [ORCHESTRATION-BR-2603-q-or-001](../../q-or/ORCHESTRATION-BR-2603-q-or-001.md)

📖 **[→ Complete Usage Guide with Examples](sigma.string.a-USAGE.md)**

---

## Overview

`sigma.string.a` provides standalone string and stringbuilder functions with **zero dependencies** — no sigma.memory, no controllers, pure libc allocation (malloc/free/realloc). Suitable for:

- Sampling/profiling tools (can't link sigma.memory due to circular dependency)
- Minimal embedded environments (no mmap, no controller overhead)
- Testing utilities needing pure malloc allocations
- Command-line tools with no sigma.* ecosystem requirements

**NOT for Sigma.* ecosystem projects** — use `sigma.core.text.o` (controller variant) instead.

---

## API Functions (22 total)

**ABI-compatible with sigma.core/strings.h** — functions match vtable signatures exactly for drop-in compatibility.

### String API (8 functions)

#### String Lifecycle
- `string string_dupe(const char *cstr)` — Create string from C string
- `string string_copy(string src)` — Duplicate existing string
- `void string_dispose(string s)` — Free string memory

#### String Operations
- `usize string_length(string s)` — Get string length
- `string string_concat(string s1, string s2)` — Concatenate two strings
- `int string_compare(string s1, string s2)` — Compare strings (strcmp wrapper)
- `string string_format(string fmt, ...)` — Create formatted string (printf-style)

#### Utilities
- `char *string_to_array(string s)` — Copy string to char array

### StringBuilder API (14 functions)

#### StringBuilder Lifecycle
- `string_builder stringbuilder_new(usize capacity)` — Create new builder (0 defaults to 16)
- `string_builder stringbuilder_snew(string str)` — Create builder from existing string
- `void stringbuilder_dispose(string_builder sb)` — Free builder memory

#### StringBuilder Operations
- `void stringbuilder_append(string_builder sb, string str)` — Append string
- `void stringbuilder_appendf(string_builder sb, string fmt, ...)` — Append formatted string
- `void stringbuilder_appendl(string_builder sb, string str)` — Append string + newline
- `void stringbuilder_lappends(string_builder sb, string str)` — Append newline + string
- `void stringbuilder_lappendf(string_builder sb, string fmt, ...)` — Append newline + formatted string
- `void stringbuilder_clear(string_builder sb)` — Reset to empty

#### StringBuilder Conversion
- `string stringbuilder_toString(string_builder sb)` — Convert to new string
- `void stringbuilder_toStream(string_builder sb, FILE *stream)` — Write to stream

#### StringBuilder Queries
- `usize stringbuilder_length(string_builder sb)` — Get current length
- `usize stringbuilder_capacity(string_builder sb)` — Get current capacity
- `void stringbuilder_setCapacity(string_builder sb, usize new_capacity)` — Set capacity

### VTable Usage

**Direct calls:**
```c
string s = string_dupe("hello");
string_dispose(s);
```

**VTable calls (ABI-compatible with controller variant):**
```c
string s = String.dupe("hello");
String.dispose(s);
```

**Memory rule:** All functions returning `string` or `string_builder` allocate memory — caller must dispose.

---

## Build (Manual, for Development)

**Compile source:**
```bash
cd /path/to/sigma.core
gcc -c src/string_malloc.c -o build/string_malloc.o -Iinclude -std=c2x -Wall
```

**Create static library:**
```bash
ar rcs build/libsigma_string.a build/string_malloc.o
```

**Link library:**
```bash
ranlib build/libsigma_string.a  # index it (optional)
```

---

## Test

**Compile test suite:**
```bash
gcc test/standalone/test_string_malloc.c build/string_malloc.o \
    -o build/test_string_malloc -Iinclude -std=c2x -Wall
```

**Run tests:**
```bash
./build/test_string_malloc
# Expected output:
# ==============================================
# sigma.string.a Standalone Test Suite
# ==============================================
# 
# --- String Tests ---
# Running: string_dupe ... ✓
# Running: string_copy ... ✓
# Running: string_length ... ✓
# Running: string_concat ... ✓
# Running: string_compare ... ✓
# Running: string_format ... ✓
# Running: string_to_array ... ✓
# Running: string_dispose (NULL safe) ... ✓
# Running: string lifecycle (allocate → use → dispose) ... ✓
# 
# --- StringBuilder Tests ---
# Running: stringbuilder_new ... ✓
# Running: stringbuilder_snew ... ✓
# Running: stringbuilder_append ... ✓
# Running: stringbuilder_appendf ... ✓
# Running: stringbuilder_appendl ... ✓
# Running: stringbuilder_lappends ... ✓
# Running: stringbuilder_lappendf ... ✓
# Running: stringbuilder_clear ... ✓
# Running: stringbuilder_toString ... ✓
# Running: stringbuilder_capacity ... ✓
# Running: stringbuilder_dispose (NULL safe) ... ✓
# Running: stringbuilder lifecycle ... ✓
# 
# ==============================================
# Tests: 21/21 passed
# ==============================================
# ✓ All tests passed!
```

**Memory leak check:**
```bash
valgrind --leak-check=full ./build/test_string_malloc
# Must report: "All heap blocks were freed -- no leaks are possible"
```

---

## Installation (Toolsmith Handles This)

**FR-2603-toolchain-006** will create build/install script for:

- Static library: `/usr/local/lib/libsigma_string.a`
- Header: `/usr/local/include/sigma/string_malloc.h`

**Consumer projects link:**
```bash
gcc myapp.c -o myapp -lsigma_string
```

---

## Dependencies

**Runtime:** None (libc only: malloc, free, realloc, memcpy, strlen, strcmp, strcpy, fwrite, vsnprintf)  
**Headers:** `sigma.core/types.h` (for ABI-compatible type definitions: `string`, `usize`)  
**Build:** gcc, ar, ranlib (standard toolchain)  
**Test:** valgrind (optional, for leak checks)

**Zero sigma.memory dependency** — no allocators, no controllers. Types from `sigma.core/types.h` ensure ABI drop-in compatibility with sigma.core ecosystem.

---

## Usage Examples

### String Example (Direct Calls)
```c
#include <sigma/string_malloc.h>
#include <stdio.h>

int main(void) {
    string greeting = string_dupe("Hello");
    string target = string_dupe("world");
    string message = string_concat(greeting, target);
    
    printf("%s\n", message);  // "Helloworld"
    
    string formatted = string_format(message);  // Note: fmt is string, not const char*
    printf("%s\n", formatted);  // "Helloworld"
    
    // Cleanup
    string_dispose(greeting);
    string_dispose(target);
    string_dispose(message);
    string_dispose(formatted);
    
    return 0;
}
```

### String Example (VTable calls)
```c
#include <sigma/string_malloc.h>
#include <stdio.h>

int main(void) {
    string greeting = String.dupe("Hello");
    string target = String.dupe("world");
    string message = String.concat(greeting, target);
    
    printf("Length: %zu\n", String.length(message));  // "Length: 10"
    printf("Compare: %d\n", String.compare(greeting, target));  // non-zero
    
    // Cleanup
    String.dispose(greeting);
    String.dispose(target);
    String.dispose(message);
    
    return 0;
}
```

### StringBuilder Example (Direct Calls)
```c
#include <sigma/string_malloc.h>
#include <stdio.h>

int main(void) {
    string_builder sb = stringbuilder_new(64);
    
    stringbuilder_append(sb, "Hello");
    string fmt = string_dupe(" %s");
    stringbuilder_appendf(sb, fmt, "World");  // Note: fmt is string
    string_dispose(fmt);
    
    stringbuilder_appendl(sb, "!");
    stringbuilder_appendl(sb, "Second line");
    
    string result = stringbuilder_toString(sb);
    printf("%s\n", result);  // "Hello World!\nSecond line\n"
    
    // Cleanup
    string_dispose(result);
    stringbuilder_dispose(sb);
    
    return 0;
}
```

### StringBuilder Example (VTable Calls)
```c
#include <sigma/string_malloc.h>
#include <stdio.h>

int main(void) {
    string_builder sb = StringBuilder.new(64);
    
    StringBuilder.append(sb, "Line 1");
    StringBuilder.appendl(sb, "Line 2");
    
    string result = StringBuilder.toString(sb);
    StringBuilder.toStream(sb, stdout);  // Write directly to stdout
    
    // Cleanup
    String.dispose(result);
    StringBuilder.dispose(sb);
    
    return 0;
}
```

**Compile:**
```bash
gcc example.c -o example -lsigma_string
./example
```

---

## Test Coverage

**String Tests (9 total):**
1. `test_string_duplicate` — malloc + strcpy, dispose, NULL/empty handling
2. `test_string_copy` — malloc + memcpy, pointer independence
3. `test_string_length` — strlen wrapper, NULL safety
4. `test_string_concat` — malloc + double copy, NULL handling
5. `test_string_compare` — strcmp wrapper, NULL/identity handling
6. `test_string_format` — vsnprintf + malloc, NULL format
7. `test_string_to_array` — malloc + memcpy, pointer independence
8. `test_string_dispose_null` — free(NULL) safety
9. `test_string_lifecycle` — full allocate → use → dispose cycle

**StringBuilder Tests (12 total):**
1. `test_stringbuilder_new` — initial capacity, preallocation, NULL safety
2. `test_stringbuilder_from_string` — copy construction, capacity match
3. `test_stringbuilder_append` — buffer growth, multiple appends
4. `test_stringbuilder_appendf` — formatted append, buffer expansion
5. `test_stringbuilder_appendl` — line append (auto-newline), multiple lines
6. `test_stringbuilder_lappends` — variadic line append, NULL terminator
7. `test_stringbuilder_lappendf` — formatted line append, buffer management
8. `test_stringbuilder_clear` — length reset, buffer preservation
9. `test_stringbuilder_to_string` — string extraction, NULL safety
10. `test_stringbuilder_capacity` — reallocation, shrink/grow operations
11. `test_stringbuilder_dispose_null` — free(NULL) safety
12. `test_stringbuilder_lifecycle` — full new → append → to_string → dispose cycle

**Valgrind verified:** Zero leaks, 79 allocs/79 frees, 5,704 bytes allocated.

---

## Limitations

**Not included (by design):**
- set_allocator hooks (hardwired to malloc/free/realloc — simplicity)
- Integration with sigma.memory controllers
- Advanced rope/stringtable structures

**For Sigma.* projects:** Use `sigma.core.text.o` (Phase 4) — provides stringtable_controller + memorope_controller for optimized string/stringbuilder operations with memory_controller integration.

---

## Files

- **Source:** `src/string_malloc.c` (198 lines)
- **Header:** `include/string_malloc.h` (public API, 113 lines)
- **Tests:** `test/standalone/test_string_malloc.c` (standalone, 239 lines)
- **Docs:** `docs/sigma.string.a.md` (this file)

---

## Next Steps

1. **sigma.core team:** Mark FR-2603-sigma-core-011 complete in WIP
2. **Handoff to toolsmith:** FR-2603-toolchain-006 (build .a, install script, publish to /usr/local/lib)
3. **Parallel work:** sigma.collections team preps sigma.arrays.a (FR-2603-sigma-collections-005)

---

## Version History

**v1.0.0 (2026-03-25):**
- Initial release
- 8 core String functions
- Zero dependencies (libc only)
- Valgrind verified (zero leaks)
- Part of Phase 0 Quick Wins
