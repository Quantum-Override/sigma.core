# Changelog

## [1.1.0] — 2026-03-27

### Added
- application.h: New Application interface for app-level configuration (FR-2603-sigma-core-004)
- application.h: Application.set_allocator() - configure application-wide allocator
- application.h: Application.get_allocator() - retrieve current allocator (defaults to SLB0)
- allocator.h: sc_alloc_use_t.ctrl field as first member (offset 0 guarantee) for controller access (FR-2603-sigma-core-005)
- docs/Application.md: Documentation for Application interface usage and integration

### Changed
- allocator.h: Allocator.free() renamed to Allocator.dispose() for semantic clarity (FR-2603-sigma-core-006)
- All internal code updated: free() → dispose() (strings.c, io.c, collections.c, etc.)
- Test suite: Converted 9 test files to Tests.enqueue() pattern from sigma.test v0.1.0+
- Test suite: Framework now manages initialization order instead of manual init_memory_system() calls

### Fixed
- test_alloc_use_ctrl.c: Corrected test_cast_use_to_ctrl assertion (was comparing pointer address to field value)

### Removed
- test_text_memory_compat.c: Redundant test (all tests now link sigma.memory by default)

---

## [1.0.0] — 2026-03-22

### Added
- allocator.h: bool (*is_ready)(void) appended to sc_allocator_i (Phase 5 diagnostics)
- allocator.h: frame_begin/frame_end nullable fields appended to sc_alloc_use_t for controller-agnostic frame support (FR-2603-sigma-core-003 / FT-15)
- module.h: SIGMA_ROLE_TRUSTED_APP = 3 added to sc_module_role enum — first-party app tier with separate cap pool (FR-2603-sigma-core-002 / FT-14)
- module.h: sc_trusted_app_grant_fn typedef — mirrors sc_trusted_grant_fn, draws from app-tier pool
- module.h: sigma_module_set_trusted_app_grant() declaration and implementation — dispatch hook for TRUSTED_APP modules
- module.h: #include <sigma.core/allocator.h> added — makes module.h self-contained for sc_alloc_policy consumers (BR-2603-sigma-core-001)
- module.h: arena_size and arena_policy appended to sigma_module_t (ABI-safe) — per-module trusted arena configuration (FR-2603-sigma-core-001 / FT-11)
- module.h: sc_trusted_grant_fn updated to accept (name, arena_size, arena_policy)
- pack.sh: post_pack hook syncs include/sigma.core/*.h to flat package/include/ on every cpkg core

### Changed
- module.c: dispatch_init_all — SIGMA_ROLE_TRUSTED_APP branch added, mirrors SIGMA_ROLE_TRUSTED exactly
- module.c: sigma_module_set_trusted_grant aligned with new sc_trusted_grant_fn signature
- module.c: sigma_module_reset clears trusted_app_grant alongside trusted_grant and arena_provider

### Fixed
- allocator.h: sc_alloc_policy field order corrected — create_custom/register_ctrl were installed before alloc/free/realloc in prior published header, causing wrong function pointer offsets in dependents
- module.h: sc_alloc_policy unknown-type error in any dependent including module.h without also including allocator.h explicitly (BR-2603-sigma-core-001)
