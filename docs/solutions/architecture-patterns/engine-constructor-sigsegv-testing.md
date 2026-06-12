---
title: "Engine constructor causes SIGSEGV in test — test EngineConfig instead"
date: 2026-06-12
last_updated: 2026-06-12
category: architecture-patterns
module: entry
tags: [testing, engine, sigsegv, headless, constructor, EngineConfig]
problem_type: knowledge
---

## Context

When writing unit tests for the `entry` module (R4.2), constructing an `Engine` object directly in a test environment caused SIGSEGV because the constructor creates a `GpuMonitor` instance, which requires a valid bgfx context — unavailable in headless test runners.

## Guidance

Test `EngineConfig` (a pure data struct) instead of constructing `Engine` objects directly. `EngineConfig` carries all initialization parameters and defaults; verifying its defaults covers the configuration surface without triggering hardware-dependent construction.

### EngineConfig fields to verify

- `width` / `height` (default: 1280×720)
- `title` (default: "Caesura (AmeKAG)")
- `headless` / `editorMode` flags (default: false)
- All pointer fields (default: nullptr — 10+ backend pointers)
- Width/height range (640×480 to 1920×1080)

### What NOT to test in constructor-only tests

- `Engine` default construction (creates `GpuMonitor` → SIGSEGV)
- `Engine` construction + immediate `shutdown()` without `init()`
- Any path that instantiates `Engine` without first initializing bgfx/SDL3

### When Engine construction IS safe

Full integration tests that call `Engine::init()` with `headless=true` and a complete `EngineConfig` (including Null backends) can construct and run the Engine. These are end-to-end tests, not unit tests.

## Why This Matters

Attempting to test `Engine` construction naively wastes debugging time on SIGSEGV that is not a bug — it's the expected behavior of a hardware-dependent constructor. Testing `EngineConfig` instead provides configuration coverage with zero hardware dependencies, runs in CI everywhere, and executes in microseconds.

## When to Apply

- Adding or modifying `Engine` constructor behavior
- Adding new fields to `EngineConfig`
- Writing unit tests for the `entry` module
- Any test file that includes `Engine.h` — prefer `EngineConfig.h` first

## Examples

### ✅ Correct: test EngineConfig defaults

```cpp
TEST_CASE("EngineConfig default values") {
    EngineConfig cfg;
    CHECK(cfg.width == 1280);
    CHECK(cfg.height == 720);
    CHECK(cfg.title == std::string("Caesura (AmeKAG)"));
    CHECK(cfg.headless == false);
    CHECK(cfg.editorMode == false);
}
```

### ❌ Wrong: construct Engine directly in unit test

```cpp
TEST_CASE("Engine default construction") {
    Engine engine;  // SIGSEGV — GpuMonitor requires bgfx context
    // ...
}
```
