# Sigma.Core Allocation Facade

The Sigma.Core allocation facade provides a pluggable memory allocation interface that allows applications to use custom allocators while maintaining a consistent API.

## Overview

The allocation facade consists of:

- `sc_allocator_i`: Interface structure defining alloc/dispose/calloc/realloc functions
- `sc_current_allocator`: Global pointer to the current allocator
- `sc_set_allocator()`: Function to set the active allocator
- `sc_alloc()`, `sc_calloc()`, `sc_realloc()` / `sc_free()`: Allocation functions that use the current allocator

## Default Behavior

By default, the facade uses standard `malloc()`, `calloc()`, and `realloc()` from the C standard library. No setup is required for basic usage.

```c
#include <sigma.core/alloc.h>

// Allocate memory
void *ptr = sc_alloc(1024);  // Allocates 1024 bytes

// Allocate zeroed memory
void *zeroed_ptr = sc_calloc(10, sizeof(int));  // Allocates 10 * sizeof(int) bytes, zeroed

// Reallocate memory
void *resized_ptr = sc_realloc(ptr, 2048);  // Resizes allocation to 2048 bytes

// Free memory
sc_free(ptr);
sc_free(zeroed_ptr);
sc_free(resized_ptr);
```

## Using Custom Allocators

To use a custom allocator, define an `sc_allocator_i` structure and set it as the current allocator.

```c
#include "alloc.h"

// Define custom allocator functions
static object my_alloc(usize size) {
    // Custom allocation logic
    return my_custom_malloc(size);
}

static void my_dispose(object ptr) {
    // Custom deallocation logic
    my_custom_free(ptr);
}

// Create allocator interface
static const sc_allocator_i my_allocator = {
    .alloc = my_alloc,
    .dispose = my_dispose,
    .calloc = calloc,    // Optional: fallback to standard
    .realloc = realloc   // Optional: fallback to standard
};

// Set as current allocator
sc_set_allocator(&my_allocator);

// Now all allocation functions use the custom allocator
void *ptr = sc_alloc(1024);
void *zeroed = sc_calloc(10, sizeof(int));
void *resized = sc_realloc(ptr, 2048);
sc_free(ptr);
sc_free(zeroed);
sc_free(resized);
```

## Resetting to Default

To reset to the default malloc/free allocator:

```c
sc_set_allocator(NULL);
```

## Integration with Sigma Memory

When `SIGMA_MEMORY_AVAILABLE` is defined, the facade can integrate with the Sigma Memory library's `Allocator` interface.

The facade automatically detects and uses available memory libraries, providing a seamless upgrade path for applications.

## Thread Safety

The current implementation is not thread-safe. The global allocator pointer should be set once at application startup before any allocation calls.

## Error Handling

- `sc_alloc()`, `sc_calloc()`, and `sc_realloc()` return `NULL` on allocation failure
- `sc_realloc()` returns `NULL` on failure, but the original pointer remains valid and unchanged
- Custom allocators should follow the same behavior as `malloc()`, `calloc()`, and `realloc()`
- `sc_free()` accepts `NULL` pointers (no-op)

## Best Practices

1. Set custom allocators early in application initialization
2. Ensure custom allocators handle edge cases (zero size, NULL pointers)
3. Test allocator implementations thoroughly
4. Document allocator requirements for library users
5. Use `sc_calloc()` for arrays that need initialization to zero
6. Be cautious with `sc_realloc()` as it may move memory, invalidating existing pointers