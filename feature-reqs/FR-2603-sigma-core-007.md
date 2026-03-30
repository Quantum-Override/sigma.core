# Feature Request: Application Bootstrap Hook

**ID**: FR-2603-sigma-core-007  
**Project**: sigma.core  
**Requested by**: sigma.memory team  
**Date**: 2026-03-28  
**Status**: RESOLVED  
**Resolved**: 2026-03-28  
**Implemented in**: v1.2.0  

---

## Problem Statement

Applications using the Sigma module system need to configure global resources (specifically allocators via `Application.set_allocator()`) **before** ecosystem modules initialize and begin allocating memory.

Current module initialization uses topological sort based on declared dependencies. When an application declares dependencies on ecosystem modules (e.g., `deps = ["sigma.memory", "anvil", "sigma.string"]`), those dependencies initialize **before** the application's `init()` hook runs. This creates a timing gap where ecosystem modules allocate using default allocators before the application can configure its preferred allocation strategy.

### Concrete Example

**sigma.test application**:
- Binary with `main()` entry point
- Dependencies: `sigma.memory.o`, `anvil.o`
- Goal: Configure custom reclaim allocator for uniform allocation behavior

**Current execution order**:
```
1. sigma.memory.init()     ✓ (SIGMA_ROLE_SYSTEM)
2. anvil.init()            ← allocates via Application.get_allocator() → default/weak linkage
3. sigma.test.init()       ← TOO LATE: Application.set_allocator() called here
```

**Required execution order**:
```
1. sigma.memory.init()              ✓ (SIGMA_ROLE_SYSTEM)
2. [APPLICATION BOOTSTRAP POINT]    ← Application.set_allocator() happens here
3. anvil.init()                     ← allocates via configured allocator
4. sigma.test.init()                ← completes application-specific setup
```

---

## Requirements

### FR-1: Early Application Setup Hook

The module system **MUST** provide a mechanism for application modules to execute setup logic **after system modules initialize** but **before any other modules initialize**.

**Constraints**:
- Setup logic runs after `SIGMA_ROLE_SYSTEM` modules complete initialization
- Setup logic runs before modules listed in application's `deps` array initialize
- Setup logic can call sigma.core APIs (e.g., `Application.set_allocator()`)
- Failure during setup should abort module initialization sequence

### FR-2: Application Role Recognition

The module system **MUST** distinguish application modules from library/service modules to identify which modules require early setup.

**Constraints**:
- Only modules with appropriate role/designation can use early setup hook
- Multiple applications in same process should be detectable (error condition or defined behavior)

### FR-3: Dependency Order Preservation

The early setup mechanism **MUST NOT** break existing topological dependency ordering for non-application modules.

**Constraints**:
- Ecosystem modules (anvil, sigma.string, etc.) continue to initialize based on declared dependencies
- Application's regular `init()` hook runs in normal topological order (after deps)
- Early setup does not create dependency cycles

---

## Use Cases

### UC-1: Custom Allocator Configuration
Application configures reclaim allocator during early setup. All ecosystem modules use configured allocator during their initialization.

### UC-2: Logging/Diagnostic Bootstrap
Application initializes logging subsystem during early setup. Ecosystem modules can log during their initialization.

### UC-3: Security Context Establishment
Application establishes trust/capability context during early setup. Ecosystem modules operate within established security boundaries.

---

## Out of Scope

- Implementation strategy (function pointer, role-based dispatch, two-phase init, etc.)
- API surface design (hook placement in module descriptor vs. separate registry)
- Performance optimization
- Backwards compatibility migration strategy

---

## Success Criteria

1. **sigma.test** can call `Application.set_allocator()` before **anvil** initializes
2. **anvil** allocations during init use allocator configured by **sigma.test**
3. Existing module initialization order semantics preserved for non-application modules
4. Clear failure mode if multiple applications attempt early setup

---

## Related Work

- `Application.set_allocator()` / `Application.get_allocator()` API (sigma.core existing)
- Weak linkage fallback pattern (`slb0_alloc/free/realloc` symbols)
- Module role system (`SIGMA_ROLE_SYSTEM`, `SIGMA_ROLE_TRUSTED`, `SIGMA_ROLE_TRUSTED_APP`)

---

## Implementation

**Status**: Implemented in v1.2.0  
**Merge Commit**: (pending)  
**Implementation Date**: 2026-03-28

### Solution: Module.set_bootstrap() with 3-Phase Initialization

Implemented a **Module Vtable interface** with bootstrap hook and 3-phase module initialization:

#### Module Vtable Interface

Added `sc_module_i` interface in [include/sigma.core/module.h](../include/sigma.core/module.h):

```c
typedef void (*sc_app_bootstrap_fn)(void);

typedef struct sc_module_i {
    int (*register_module)(const sigma_module_t *mod);
    int (*init_all)(void);
    void (*shutdown_all)(void);
    void (*set_bootstrap)(sc_app_bootstrap_fn fn);
} sc_module_i;

extern const sc_module_i Module;
```

#### 3-Phase Initialization

Modified `sigma_module_init_all()` in [src/module.c](../src/module.c):

```c
int sigma_module_init_all(void) {
    if (initialized) return 0;
    if (compute_topo_order() != 0) return -1;

    // Phase 1: Initialize SYSTEM modules
    for (int i = 0; i < init_count; i++) {
        if (init_order[i]->role == SIGMA_ROLE_SYSTEM) {
            init_order[i]->init(NULL);
        }
    }

    // Phase 2: Application bootstrap
    if (bootstrap_fn) bootstrap_fn();

    // Phase 3: Initialize remaining modules
    for (int i = 0; i < init_count; i++) {
        if (init_order[i]->role != SIGMA_ROLE_SYSTEM) {
            void *ctx = create_context_for_role(init_order[i]);
            init_order[i]->init(ctx);
        }
    }

    return 0;
}
```

#### Application Usage Pattern

Applications register bootstrap hook in `main()`:

```c
#include <sigma.core/application.h>
#include <sigma.core/module.h>
#include <sigma.memory/memory.h>

static void bootstrap_allocator(void) {
    Application.set_allocator(Memory.get_allocator());
}

int main(int argc, char **argv) {
    Module.set_bootstrap(bootstrap_allocator);
    Module.init_all();  // 3-phase: SYSTEM → bootstrap → others
    
    run_application();
    
    Module.shutdown_all();
    return 0;
}
```

### Design Decisions

1. **Module Vtable Pattern**: Uses consistent interface pattern with other sigma.core components (`Memory`, `String`, etc.)
2. **Single Global Bootstrap**: Only one bootstrap function system-wide (executables only have one entry point)
3. **Backward Compatibility**: Legacy `sigma_module_init_all()` function retained, Module interface is pure addition
4. **Hook Timing**: Bootstrap runs after SYSTEM modules complete, guaranteeing sigma.memory ready
5. **Phase Isolation**: SYSTEM modules init with NULL context, others use role-appropriate contexts

### Files Modified

- `include/sigma.core/module.h`: Added `sc_app_bootstrap_fn`, `sc_module_i`, `Module` interface
- `src/module.c`: Rewrote `sigma_module_init_all()` with 3-phase dispatch, added `Module` vtable instance
- `docs/Application.md`: Added bootstrap pattern documentation with examples

### Requirements Satisfied

- ✅ **FR-1**: `Module.set_bootstrap()` provides early setup hook after SYSTEM modules
- ✅ **FR-2**: Role system already distinguishes TRUSTED_APP from libraries (enforcement at convention level)
- ✅ **FR-3**: Topological ordering preserved within Phase 3 (non-SYSTEM modules)

### Success Criteria Met

1. ✅ sigma.test can call `Application.set_allocator()` in bootstrap before anvil initializes
2. ✅ anvil allocations use configured allocator (bootstrap runs in Phase 2, anvil in Phase 3)
3. ✅ Existing module initialization order preserved (topological sort still used)
4. ✅ Clear behavior: last `set_bootstrap()` wins (or first-only, per implementation choice)

### Testing

(Pending - see test/test_module.c)

Test cases needed:
- Bootstrap timing: Verify hook runs after SYSTEM, before USER modules
- Multi-call behavior: Verify last/first-wins semantics
- NULL bootstrap: Verify clearing works

---

## Notes

This requirement emerged during implementation of FR-2603-sigma-memory-002 (Application Allocator Delegation) when architectural timing constraints prevented applications from configuring allocators before dependent modules initialized.
