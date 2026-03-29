# Using The Module System

**Applies to:** sigma.core `module.h` / `module.c`

**Version Updates:**
- **v1.2.0**: Added `Module` Vtable interface, application bootstrap hook (`Module.set_bootstrap`), and 3-phase initialization
- **v1.0.0**: Initial module system with topological dependency resolution

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
    .role     = SIGMA_ROLE_PERIPHERAL,
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

## Module Interface (Vtable Pattern)

**Since:** v1.2.0

The module system provides a global `Module` interface for consistent access to lifecycle functions:

```c
#include <sigma.core/module.h>

int main(void) {
    if (Module.init_all() != 0) {
        return 1;   // init failed; panic hook already fired
    }

    // application work here

    Module.shutdown_all();
    return 0;
}
```

### Module Interface API

```c
typedef struct sc_module_i {
    int  (*register_module)(const sigma_module_t *mod);
    int  (*init_all)(void);
    void (*shutdown_all)(void);
    void (*set_bootstrap)(sc_app_bootstrap_fn fn);
} sc_module_i;

extern const sc_module_i Module;
```

| Function | Description |
|---|---|
| `Module.register_module(mod)` | Register a module descriptor (same as `sigma_module_register`) |
| `Module.init_all()` | Initialize all modules in 3 phases (same as `sigma_module_init_all`) |
| `Module.shutdown_all()` | Shutdown all modules in reverse order (same as `sigma_module_shutdown_all`) |
| `Module.set_bootstrap(fn)` | Register application early setup hook (runs after SYSTEM, before others) |

**Legacy functions** (`sigma_module_register`, `sigma_module_init_all`, `sigma_module_shutdown_all`) remain available for backward compatibility. New code should prefer the `Module` interface.

---

## Application Bootstrap Hook

**Since:** v1.2.0  
**Use case:** Configure global resources (allocators, logging) after SYSTEM modules initialize but before ecosystem modules consume those resources.

### The Problem

Applications often need to configure allocators via `Application.set_allocator()` after `sigma.memory` initializes. However, if the application depends on ecosystem modules (anvil, sigma.string, etc.), those modules initialize **before** the application's `init()` hook runs:

```
sigma.memory.init()  ✓ (SYSTEM module)
    ↓
anvil.init()         ← allocates using DEFAULT allocator
    ↓
sigma.test.init()    ← TOO LATE to call Application.set_allocator()
```

### The Solution: Bootstrap Hook

Register a bootstrap function that runs **after SYSTEM modules** but **before other modules**:

```c
#include <sigma.core/application.h>
#include <sigma.core/module.h>
#include <sigma.memory/memory.h>

static void bootstrap_allocator(void) {
    // sigma.memory is now initialized (SYSTEM module)
    // Configure app-wide allocator before ecosystem modules allocate
    Application.set_allocator(Memory.get_allocator());
}

int main(int argc, char **argv) {
    // Register bootstrap hook (runs after SYSTEM, before USER modules)
    Module.set_bootstrap(bootstrap_allocator);
    
    // Initialize all modules in 3 phases:
    // 1. SYSTEM modules (sigma.memory, sigma.core)
    // 2. Bootstrap hook (Application.set_allocator runs here)
    // 3. Other modules (anvil, sigma.string, etc. using configured allocator)
    Module.init_all();
    
    run_application();
    
    Module.shutdown_all();
    return 0;
}
```

### When to Use Bootstrap

Use `Module.set_bootstrap()` when you need to:
- Configure `Application.set_allocator()` before libraries allocate
- Initialize logging before modules log during init
- Establish security context before modules acquire capabilities
- Set up any global resource that SYSTEM modules provide but other modules consume

### Bootstrap Constraints

- Bootstrap hook runs exactly **once** per process (during `Module.init_all()`)
- Multiple calls to `set_bootstrap()` replace the hook (last wins)
- Bootstrap runs with no arguments (capture state in closures if needed)
- Bootstrap **must not** call `Module.init_all()` (infinite recursion)
- Only **SYSTEM** modules are guaranteed initialized when bootstrap runs

---

## Three-Phase Initialization

**Since:** v1.2.0

`Module.init_all()` initializes modules in three phases to support application bootstrap:

### Phase 1: SYSTEM Modules
All modules with `role = SIGMA_ROLE_SYSTEM` initialize first, in topological order:
- sigma.core
- sigma.memory

Context passed: `NULL` (SYSTEM modules never receive alloc contexts)

### Phase 2: Bootstrap Hook
If registered via `Module.set_bootstrap()`, the bootstrap function runs here. All SYSTEM modules are initialized and ready to use.

### Phase 3: Remaining Modules
All non-SYSTEM modules initialize in topological order:
- `SIGMA_ROLE_TRUSTED` modules (if any)
- `SIGMA_ROLE_PERIPHERAL` modules (libraries)
- `SIGMA_ROLE_APPLICATION` modules (executables)

Context passed: depends on `role` and `alloc` designation

**Example execution timeline:**
```
Module.init_all()
    1. sigma.core.init(NULL)        // SYSTEM
    2. sigma.memory.init(NULL)      // SYSTEM
    3. bootstrap_allocator()        // BOOTSTRAP (if registered)
    4. anvil.init(ctx)              // USER (uses configured allocator)
    5. sigma.string.init(ctx)       // USER
    6. my_application.init(ctx)     // USER
```

---

## Calling init\_all from main() (Legacy Pattern)

**Note:** This pattern still works but is superseded by the Module Vtable interface shown above.

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
| `SIGMA_ROLE_SYSTEM` | sigma.memory only | `NULL` |
| `SIGMA_ROLE_TRUSTED` | sigma.tasking | `sc_trusted_cap_t *` granted by sigma.memory |
| `SIGMA_ROLE_PERIPHERAL` | libraries (sigma.text, sigma.collections, anvil, etc.) | depends on `alloc` field |
| `SIGMA_ROLE_APPLICATION` | executables with main() (sigma.test, sigma.cli, etc.) | `sc_trusted_cap_t *` from app-tier pool |

Roles affect *ordering*, not just the ctx argument.  All `SYSTEM` modules
initialise before `TRUSTED`, which initialise before `PERIPHERAL` and `APPLICATION`.

---

## Allocator Designation (PERIPHERAL modules)

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
    .role  = SIGMA_ROLE_PERIPHERAL,
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
    .role        = SIGMA_ROLE_PERIPHERAL,
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

### Basic Module Registration

1. Add `module.c` to your project.
2. Declare a `static const sigma_module_t` with your name, role, alloc mode,
   deps, and lifecycle functions.
3. Register it with `sigma_module_register()` from a constructor.
4. Call `Module.init_all()` once from `main()`.
5. Let `Module.shutdown_all()` clean up on exit.

### Application Bootstrap (v1.2.0+)

For applications that need to configure resources before ecosystem modules initialize:

1. Define a bootstrap function: `static void bootstrap_app(void) { ... }`
2. Register it before init: `Module.set_bootstrap(bootstrap_app);`
3. Init runs in 3 phases: SYSTEM → bootstrap → others
4. Use `Application.set_allocator()` in bootstrap to configure global allocator

### Module Interface (v1.2.0+)

Prefer the `Module` Vtable for new code:
- `Module.register_module(mod)` - register a module
- `Module.init_all()` - 3-phase initialization
- `Module.shutdown_all()` - cleanup in reverse order
- `Module.set_bootstrap(fn)` - application early setup hook

Legacy functions (`sigma_module_*`) remain for backward compatibility.
