# BR-2603-sigma-core-001: module.h missing `#include <sigma.core/allocator.h>`

**ID:** BR-2603-sigma-core-001  
**Type:** Bug Report  
**Owner:** sigma.core  
**Filed:** 2026-03-21  
**Status:** closed  
**Tags:** module, allocator, build, include  

---

## Summary

`include/sigma.core/module.h` uses the type `sc_alloc_policy` in the `sc_trusted_grant_fn` typedef without including the header that defines it (`sigma.core/allocator.h`). Any dependent project that includes `module.h` fails to build with:

```
error: unknown type name 'sc_alloc_policy'
```

## Reproduction

Include `sigma.core/module.h` in any project **without** also explicitly including `sigma.core/allocator.h` first:

```c
#include <sigma.core/module.h>   // error: unknown type name 'sc_alloc_policy'
```

## Root Cause

`module.h` introduced `sc_trusted_grant_fn` with the signature:

```c
typedef void (*sc_trusted_grant_fn)(const char *name, usize arena_size, sc_alloc_policy arena_policy);
```

`sc_alloc_policy` is defined in `sigma.core/allocator.h`, but that header was never included in `module.h`. The type was resolved only because sigma.memory's `src/module.c` happened to include `allocator.h` directly before `module.h`.

## Fix Applied

Added `#include <sigma.core/allocator.h>` to `include/sigma.core/module.h`:

```c
// module.h
#include <sigma.core/allocator.h>
```

This makes `module.h` self-contained — dependents no longer need to manually include `allocator.h` to use function pointers typed with `sc_alloc_policy`.

## Impact

All projects that include `module.h` and use `sc_trusted_grant_fn` are affected. sigma.memory was the first consumer and surfaced this during FT-11 integration.

## Resolution

Fixed in sigma.core − the corrected header is published at `/usr/local/include/sigma.core/module.h`. Closed at build +10.
