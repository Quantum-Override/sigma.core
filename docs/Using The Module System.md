# Using The Module System

**Applies to:** sigma.core `module.h` / `module.c`

---

## Why the Module System Exists

Every Sigma.X component that needs startup work previously relied on
`__attribute__((constructor))` to bootstrap itself.  That approach breaks down
as soon as two components have a dependency relationship: the runtime fires
constructors in link order, which is implementation-defined and fragile.

The module system replaces that pattern with a single, explicit rule:

> **Constructors only store pointers.  All init work runs inside
> `sigma_module_init_all()`.**

`sigma_module_init_all()` topologically sorts all registered descriptors by
their `deps[]` lists, then calls each module's `init()` in the correct order.
The result is deterministic regardless of link order.

---

## Declaring a Module

Add one `module.c` file to your project.  Declare a single static
`sigma_module_t` descriptor and register it from a constructor.

```c
#include <sigma.core/module.h>

// Forward declarations for the lifecycle functions
static int  my_module_init(void *ctx);
static void my_module_shutdown(void);

// NULL-terminated dependency list
static const char *my_deps[] = { "sigma.memory", NULL };

// Static descriptor — must outlive the process
static const sigma_module_t my_module = {
    .name     = "my.project",
    .version  = "0.1.0",
    .role     = SIGMA_ROLE_USER,
    .alloc    = SIGMA_ALLOC_DEFAULT,
    .deps     = my_deps,
    .init     = my_module_init,
    .shutdown = my_module_shutdown,
};

__attribute__((constructor))
static void register_my_module(void) {
    sigma_module_register(&my_module);
}

static int my_module_init(void *ctx) {
    (void)ctx;   // NULL for SIGMA_ALLOC_DEFAULT
    // perform startup work here
    return 0;    // return non-zero to abort init_all
}

static void my_module_shutdown(void) {
    // cleanup
}
```

---

## Calling init\_all from main()

```c
#include <sigma.core/module.h>

int main(void) {
    if (sigma_module_init_all() != 0) {
        return 1;   // init failed; panic hook already fired
    }

    // application work here

    sigma_module_shutdown_all();
    return 0;
}
```

`sigma_module_init_all()` is idempotent — safe to call more than once, but only
the first call does any work.

---

## Module Roles

| Role | Who uses it | `init()` ctx |
|---|---|---|
| `SIGMA_ROLE_SYSTEM` | sigma.memory, sigma.core | `NULL` |
| `SIGMA_ROLE_TRUSTED` | sigma.tasking | `sc_trusted_cap_t *` granted by sigma.memory |
| `SIGMA_ROLE_USER` | everything else | depends on `alloc` field |

Roles affect *ordering*, not just the ctx argument.  All `SYSTEM` modules
initialise before `TRUSTED`, which initialise before `USER`.

---

## Allocator Designation (USER modules)

The `alloc` field on the descriptor controls what ctx `init()` receives and how
memory is routed to the module.

| `alloc` value | ctx passed to `init()` | Use when |
|---|---|---|
| `SIGMA_ALLOC_DEFAULT` | `NULL` | module uses the global Allocator (SLB0) |
| `SIGMA_ALLOC_SYSTEM`  | `sc_alloc_use_t *` with `{malloc, free, realloc}` | module needs stdlib allocation, no slab |
| `SIGMA_ALLOC_ARENA`   | `sc_alloc_use_t *` from the registered arena provider | module needs a dedicated arena (sigma.test archetype) |
| `SIGMA_ALLOC_CUSTOM`  | `mod->alloc_hooks` (you supply all three fn pointers) | module has its own allocator |

Zero-initialised descriptors default to `SIGMA_ALLOC_DEFAULT` — no
configuration required for the common case.

### SIGMA\_ALLOC\_ARENA example

```c
static const sigma_module_t my_module = {
    .name  = "my.sandboxed",
    .role  = SIGMA_ROLE_USER,
    .alloc = SIGMA_ALLOC_ARENA,   // init() receives arena-backed sc_alloc_use_t*
    .deps  = (const char*[]){ "sigma.memory", NULL },
    .init  = my_module_init,
};

static int my_module_init(void *ctx) {
    sc_alloc_use_t *arena = (sc_alloc_use_t *)ctx;
    // arena may be NULL if sigma.memory has no arena provider registered
    if (!arena) return ERR;
    // store arena for internal use
    return OK;
}
```

### SIGMA\_ALLOC\_CUSTOM example

```c
static void *my_alloc(usize size)              { return my_pool_alloc(size); }
static void  my_free(void *ptr)                { my_pool_free(ptr); }
static void *my_resize(void *ptr, usize size)  { return my_pool_realloc(ptr, size); }

static sc_alloc_use_t my_hooks = {
    .alloc   = my_alloc,
    .release = my_free,
    .resize  = my_resize,
};

static const sigma_module_t my_module = {
    .name        = "my.pooled",
    .role        = SIGMA_ROLE_USER,
    .alloc       = SIGMA_ALLOC_CUSTOM,
    .alloc_hooks = &my_hooks,
    .deps        = (const char*[]){ NULL },
    .init        = my_module_init,
};
```

---

## Dependencies

`deps` is a `NULL`-terminated array of module name strings.  The names must
match the `name` field of the corresponding descriptor exactly.

```c
static const char *no_deps[] = { NULL };                             // no deps
static const char *deps[]    = { "sigma.memory", "sigma.text", NULL }; // two deps
```

Missing dependencies and circular dependencies trigger the panic hook rather
than returning an error code.  The process will not continue with a broken
dependency graph.

---

## Panic Hook

The default panic handler writes to `stderr` and calls `abort()`.  Replace it
before calling `sigma_module_init_all()` if you need different behaviour:

```c
static void my_panic(const char *module_name, const char *reason) {
    my_logger_error("Module panic: %s — %s", module_name, reason);
    exit(1);
}

// before init_all:
sigma_module_set_panic_fn(my_panic);
```

The handler **must not return**.  Returning produces undefined behaviour.

---

## Finding a Module at Runtime

```c
const sigma_module_t *mod = sigma_module_find("sigma.memory");
if (mod) {
    // mod->name, mod->version, etc.
}
```

Safe to call at any time after `sigma_module_register()`.

---

## Summary

1. Add `module.c` to your project.
2. Declare a `static const sigma_module_t` with your name, role, alloc mode,
   deps, and lifecycle functions.
3. Register it with `sigma_module_register()` from a constructor.
4. Call `sigma_module_init_all()` once from `main()`.
5. Let `sigma_module_shutdown_all()` clean up on exit.
