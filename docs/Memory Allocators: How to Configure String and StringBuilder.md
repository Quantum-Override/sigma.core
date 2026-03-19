# Memory Allocators: How to Configure String and StringBuilder

**Applies to:** sigma.text `String`, `StringBuilder`  
**Requires:** sigma.core `sc_alloc_use_t` (`<sigma.core/allocator.h>`)

---

## Overview

By default, `String` and `StringBuilder` allocate through the global `Allocator`
facade — whichever implementation is linked into the binary (sigma.memory's slab
allocator or the minimal stdlib stub).

If you need to redirect their allocations — for example to feed them from a
dedicated arena, a pool, or the raw system allocator — you can supply an
`sc_alloc_use_t` hook struct via the `alloc_use` vtable entry.

`String.alloc_use` and `StringBuilder.alloc_use` are **independent** — setting
one does not affect the other.

---

## The Hook Struct

```c
// <sigma.core/allocator.h>
typedef struct sc_alloc_use_s {
    void *(*alloc)(usize size);
    void  (*release)(void *ptr);
    void *(*resize)(void *ptr, usize size);
} sc_alloc_use_t;
```

Any of the three function pointers may be `NULL`.  A `NULL` field falls back to
the global `Allocator` for that individual operation.  Passing `NULL` as the
pointer to `alloc_use()` restores the full fallback for everything.

| Field | Fallback when `NULL` |
|---|---|
| `alloc`   | `Allocator.alloc(size)` |
| `release` | `Allocator.free(ptr)` |
| `resize`  | `Allocator.realloc(ptr, size)` |

---

## Redirect String to a custom allocator

```c
#include <sigma.core/allocator.h>
#include <sigma.text/strings.h>

static void *my_alloc(usize size)             { return my_pool_alloc(size); }
static void  my_release(void *ptr)            { my_pool_free(ptr); }
static void *my_resize(void *ptr, usize size) { return my_pool_realloc(ptr, size); }

static sc_alloc_use_t my_string_hooks = {
    .alloc   = my_alloc,
    .release = my_release,
    .resize  = my_resize,
};

// Once, during startup (after sigma_module_init_all):
String.alloc_use(&my_string_hooks);

// Now all String.copy(), String.concat(), String.format(), etc.
// allocate through my_pool_*
string s = String.copy("hello world");
// ...
String.dispose(s);

// Restore global Allocator:
String.alloc_use(NULL);
```

---

## Redirect StringBuilder to an arena

A common pattern is to supply a sigma.memory arena received via the module
system (`SIGMA_ALLOC_ARENA`) so that all StringBuilder allocations live in a
scoped region:

```c
static int my_module_init(void *ctx) {
    sc_alloc_use_t *arena = (sc_alloc_use_t *)ctx;
    if (!arena) return ERR;

    StringBuilder.alloc_use(arena);
    return OK;
}
```

When the arena is torn down at shutdown, all `StringBuilder` allocations
within that scope are reclaimed together — no individual `StringBuilder.dispose`
calls needed during teardown.

---

## Using only stdlib (no slab involvement)

Link `sigma.core.sys_alloc.o` and leave both hooks at their default `NULL`
state; drop all allocator configuration entirely.

Or, if you are running alongside sigma.memory but want StringBuilder to bypass
the slab for a specific subsystem:

```c
static void *sys_alloc(usize size)             { return malloc(size); }
static void  sys_release(void *ptr)            { free(ptr); }
static void *sys_resize(void *ptr, usize size) { return realloc(ptr, size); }

static sc_alloc_use_t stdlib_hooks = {
    .alloc   = sys_alloc,
    .release = sys_release,
    .resize  = sys_resize,
};

StringBuilder.alloc_use(&stdlib_hooks);
```

---

## Thread safety

`String.alloc_use` and `StringBuilder.alloc_use` each write a single pointer
into a static variable.  If multiple threads call these setters concurrently
with allocation activity from the same interface, external synchronisation is
required.  

The typical usage pattern — call `alloc_use` once during init before spawning
threads, then never again — needs no locking.

---

## Scope of the setting

`alloc_use` is **process-wide**.  Once set, every call to `String.*` (or
`StringBuilder.*`) in any translation unit uses the supplied hooks until
explicitly changed.  There is no per-call or per-context override.

If you need multiple concurrent allocator contexts, keep separate `string_builder`
instances in separate subsystems and configure `StringBuilder.alloc_use` before
each batch of operations within a single-threaded segment.

---

## Quick reference

| Call | Effect |
|---|---|
| `String.alloc_use(&hooks)` | All String allocs → `hooks.*` (NULL fields → Allocator) |
| `String.alloc_use(NULL)` | Restore global Allocator for all String operations |
| `StringBuilder.alloc_use(&hooks)` | All StringBuilder allocs → `hooks.*` |
| `StringBuilder.alloc_use(NULL)` | Restore global Allocator for all StringBuilder operations |
