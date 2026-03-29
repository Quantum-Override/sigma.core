# Application Interface

**Feature**: FR-2603-sigma-core-004  
**Status**: Implemented (Phase 1)  
**API**: `sigma.core/application.h`

## Overview

The `Application` interface provides application-level configuration and services for sigma.core. In Phase 1, it focuses on global allocator management.

## Key Functions

### `Application.set_allocator(sc_alloc_use_t *use)`

**Purpose**: Configure the application-wide allocator used by all sigma.core operations.

**Parameters**:
- `use` - Allocator hook to use for all allocations, or `NULL` to reset to default SLB0

**Constraints**:
- Must be called from `module_init` hook (before any allocations)
- Must be called at most **once** (asserts on second call in debug builds)
- Single-threaded context during module init (no lock needed)

**Example**:
```c
#include <sigma.core/application.h>

void my_module_init(void) {
    // Create custom allocator hook
    sc_alloc_use_t custom = {
        .ctrl = my_controller,
        .alloc = my_alloc_fn,
        .release = my_release_fn,
        .resize = my_resize_fn,
        .frame_begin = NULL,
        .frame_end = NULL
    };
    
    // Set as application-wide allocator
    Application.set_allocator(&custom);
    
    // All subsequent Allocator.* calls use custom allocator
}
```

### `Application.get_allocator(void)`

**Purpose**: Retrieve the current application-wide allocator.

**Returns**: 
- Current `sc_alloc_use_t*` if `set_allocator()` was called
- Default SLB0 allocator if never set
- **Never returns NULL** (safe fallback guaranteed)

**Thread Safety**: 
- Read-only operation, safe to call from any context after initialization

**Example**:
```c
sc_alloc_use_t *current = Application.get_allocator();
// Use current->alloc(), current->release(), etc.
```

## Default Behavior

If `Application.set_allocator()` is never called:
- All allocations use **SLB0** (Sigma.Core's default allocator)
- SLB0 is a simple malloc/free wrapper
- No initialization required - works out of the box

## Integration with sigma.memory

When using `sigma.memory` as your allocator provider, use the **bootstrap pattern** to ensure sigma.memory initializes before other modules allocate:

```c
// sigma.test/main.c (or any executable using sigma.memory)
#include <sigma.core/application.h>
#include <sigma.core/module.h>
#include <sigma.memory/memory.h>

static void bootstrap_allocator(void) {
    // sigma.memory is now initialized (SYSTEM module)
    // Configure app-wide allocator before ecosystem modules init
    Application.set_allocator(Memory.get_allocator());
}

int main(int argc, char **argv) {
    // Register bootstrap hook (runs after SYSTEM, before USER modules)
    Module.set_bootstrap(bootstrap_allocator);
    
    // Initialize all modules:
    // 1. SYSTEM modules (sigma.memory) 
    // 2. Bootstrap hook (Application.set_allocator)
    // 3. Other modules (anvil, sigma.string, etc.) using configured allocator
    Module.init_all();
    
    // Run application
    run_application();
    
    // Cleanup
    Module.shutdown_all();
    return 0;
}
```

**Why Bootstrap Instead of module_init()?**

Applications typically depend on ecosystem modules (anvil, sigma.string, etc.). Without bootstrap:
- sigma.memory.init() → anvil.init() → **app.init()**
- anvil allocates using **default allocator** before app can configure sigma.memory

With bootstrap pattern:
- sigma.memory.init() → **bootstrap_allocator()** → anvil.init() → app.init()  
- anvil allocates using **configured allocator** ✓

```

## Design Rationale

### Why Application-Wide Allocator?

1. **Consistency**: All sigma.core components use the same allocator
2. **Flexibility**: Swap in custom allocators (pools, arenas, tracking)
3. **Integration**: Easy to connect with sigma.memory or other allocators
4. **Safety**: Default fallback prevents NULL pointer dereferences

### Why Single Initialization?

- Changing allocators mid-execution is dangerous (orphaned pointers)
- Module initialization is the natural configuration point
- Assertion catches programming errors early

## Phase 1 Implementation

**Source Files**:
- `include/sigma.core/application.h` - Public API
- `src/application.c` - Implementation

**Tests**:
- `test/unit/test_application.c` - 5 test cases covering:
  - Set/get allocator
  - Default SLB0 fallback
  - Double-init assertion
  - NULL reset behavior
  - Thread-safe get_allocator

## See Also

- [Allocator Interface](allocator.h) - Low-level allocator abstraction
- [Module System](Using%20The%20Module%20System.md) - Module initialization hooks
- [sigma.memory](../../sigma.memory/README.md) - Full memory management system
