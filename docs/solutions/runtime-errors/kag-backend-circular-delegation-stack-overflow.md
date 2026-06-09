---
title: "C Stack Overflow from KAG↔Backend Circular Delegation"
date: 2026-06-08
last_reviewed: 2026-06-09
category: runtime-errors
module: Scripting
problem_type: runtime_error
component: development_workflow
symptoms:
  - '"engine_update: C stack overflow" on every frame in demo mode'
last_reviewed: 2026-06-09
  - "PANIC when sandbox blocks engine globals (_GAME_MOUSE_X, _CAESURA_GPU_QUALITY, etc.)"
  - "bgfx debug-text shader FATAL errors on engine startup"
  - "Demo loads assets then immediately shows red ErrorUI crash screen"
  - "Lua memory allocator calling lua_gc(LUA_GCCOLLECT) inside alloc hook (UB in Lua 5.4)"
root_cause: logic_error
resolution_type: code_fix
severity: critical
status: active
tags:
  - lua
  - sandbox
  - stack-overflow
  - backend
  - circular-dependency
  - kag-binding
  - unified-backend
related_components:
  - "KAGBinding (C++)"
  - "BackendFactory (Lua)"
  - "sandbox.lua"
  - "layers.lua"
  - "demo.lua"
---

# C Stack Overflow from KAG↔Backend Circular Delegation

## Problem

The Caesura (AmeKAG) engine demo crashed immediately with a C stack overflow on the first frame. Four interconnected bugs prevented any rendering: a circular delegation loop between KAGBinding and BackendFactory, a Lua 5.4 spec violation in the memory allocator, missing sandbox whitelist entries for engine globals, and per-frame texture thrashing in the demo script.

## Symptoms

- engine_update: C stack overflow printed to stderr on the very first frame
last_reviewed: 2026-06-09
- Engine entered ErrorUI (red crash screen with [R]etry/[T]itle/[Q]uit)
- PANIC: unprotected error in call to Lua API (Sandbox: cannot create global '_GAME_MOUSE_X')
- gfx FATAL: Failed to create vertex shader and ragment shader on startup
- Engine process consumed 108-112MB but rendered nothing visible

## What Didn't Work

- **Reducing texture recreation rate alone** — did not fix the stack overflow; demo still crashed
- **Removing the lua_Alloc GC call alone** — process started but still overflowed
- **Adding sandbox whitelist entries alone** — unblocked _GAME_MOUSE_X but overflow persisted
- **Removing layers/layers.render** — demo ran without C stack overflow, confirming the bug was in the render path, but masking the root cause

## Solution

### Fix 1: Break KAG ↔ BackendFactory circular delegation (ROOT CAUSE)

**src/Scripting/KAGBinding.cpp** — Four KAG C functions (show_text, show_image, clear_screen, wait_click) delegated to _CAESURA_BACKEND.show_text etc., which in scripts/backend_factory.lua delegated BACK to KAG.show_text, forming infinite recursion:

`cpp
// BEFORE (infinite loop):
static int lua_KAG_show_text(lua_State* L) {
    lua_getglobal(L, "_CAESURA_BACKEND");   // Get backend table
    lua_getfield(L, -1, "show_text");       // Get backend.show_text
    lua_remove(L, -2);
    lua_insert(L, 1);
    lua_call(L, lua_gettop(L) - 1, LUA_MULTRET);  // Call it → calls KAG.show_text again!
    return lua_gettop(L);
}

// AFTER (direct implementation):
static int lua_KAG_show_text(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    printf("[KAG] %s\n", text);
    lua_pushboolean(L, 1);
    return 1;
}
`

Same fix applied to show_image, clear_screen, wait_click.

### Fix 2: Remove lua_gc from allocator (Lua 5.4 UB)

**src/Core/Engine.cpp** — s_luaAllocFn called lua_gc(L, LUA_GCCOLLECT)
inside the memory allocator, which is explicitly forbidden by Lua 5.4.
GC inside the allocator can trigger finalizers that call back into allocations,
causing unbounded C-stack growth.

`cpp
// BEFORE (UB in Lua 5.4):
static void* s_luaAllocFn(void* ud, void* ptr, size_t osize, size_t nsize) {
    lua_State* L = static_cast<lua_State*>(ud);
    int memKB = lua_gc(L, LUA_GCCOUNT, 0);
    if (memKB > 256 * 1024 && nsize > osize) {
        lua_gc(L, LUA_GCCOLLECT, 0);  // FORBIDDEN in allocator!
        ...
    }
    ...
}

// AFTER (safe):
static void* s_luaAllocFn(void* ud, void* ptr, size_t osize, size_t nsize) {
    if (nsize == 0) { free(ptr); return nullptr; }
    return realloc(ptr, nsize);
}
`

### Fix 3: Add engine globals to sandbox whitelist

**scripts/sandbox.lua** — Added 11 runtime globals set by C++ engine main loop:

`lua
_CAESURA_GPU_QUALITY  = true,
_CAESURA_VFX_ENABLED  = true,
_CAESURA_GPU_TIME_MS  = true,
_CAESURA_GPU_AVG_MS   = true,
_CAESURA_GPU_DEGRADED = true,
_CAESURA_VOICE_COMPLETE = true,
_GAME_MOUSE_X   = true,
_GAME_MOUSE_Y   = true,
_GAME_MOUSE_DOWN = true,
_GAME_KEY_F5    = true,
_GAME_KEY_F6    = true,
`

### Fix 4: Pass batch.commands to submit_batch

**scripts/layers.lua** — Changed ackend.submit_batch(batch) to ackend.submit_batch(batch.commands)
so C++ receives the command array (not the batch wrapper table).

### Fix 5: Throttle per-frame texture operations

**scripts/demo.lua** — Changed background pulse to every 120 frames (2s) instead of every frame,
and progress bar texture to every 30 frames (0.5s). Card textures created once only.

## Why This Works

The circular delegation loop was the primary bug: KAG.show_text → _CAESURA_BACKEND.show_text → KAG.show_text created unbounded Lua↔C recursion. Each call consumed C stack, and Lua 5.4's LUAI_MAXCCALLS (200) was quickly exhausted. By giving KAGBinding functions direct implementations, the call chain becomes demo → backend.render_text → _CAESURA_BACKEND.show_text → KAG.show_text → printf, a finite depth-4 chain with no cycles.

The allocator fix eliminates a secondary overflow path where per-frame texture allocation triggered GC inside the allocator, potentially running finalizers that allocated again.

The sandbox whitelist and layers fixes were correctness issues that manifested as secondary errors once the primary crash was resolved.

## Prevention

- **Never delegate C binding functions to Lua-side implementations of the same name** — creates implicit circular dependencies. If a C function needs to delegate, delegate to a DIFFERENT-named function.
- **Never call lua_gc(LUA_GCCOLLECT) inside a lua_Alloc hook** — this is UB in Lua 5.4+ and can cause C stack overflow, data corruption, or crashes. Use a flag + main-loop GC pattern instead.
- **When adding engine globals, add them to sandbox.lua's _G_whitelist immediately** — the sandbox runs after script loading, so any global created during main loop execution must be whitelisted.
- **Avoid per-frame texture create/destroy cycles** — each create_solid_texture allocates a GPU RTT. Throttle to ≤2Hz for animated backgrounds.

## Related

- docs/solutions/ — created with this first entry
- scripts/sandbox.lua:115 — _G_whitelist definition
- src/Core/Engine.cpp:44 — allocator hook (now fixed)
- src/Scripting/KAGBinding.cpp:262 — show_text delegation (now direct)
- scripts/backend_factory.lua:95 — BackendFactory.create backend.show_text definition
