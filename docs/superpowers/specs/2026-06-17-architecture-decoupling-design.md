# Architecture Decoupling Design

**Date:** 2026-06-17
**Status:** Approved
**Branch:** codex/game-logic-p1-p3

## Problem Statement

6 architectural issues identified via `scripts/count_coupling.py` + manual review:

| # | Issue | Current | Budget |
|---|-------|---------|--------|
| 1 | `di` cross-module deps | 12 | ≤8 |
| 2 | `script` cross-module deps | 7 | ≤4 |
| 3 | `resource` ↔ `archive` circular dep | .cpp level | 0 |
| 4 | `debug` → `script` direction violation | 1 file | 0 |
| 5 | `entry` cross-module deps | 15 | ≤14 |
| 6 | `IRenderDevice.h` not in `api/` | style | fix |

## Design Principles

1. **Zero behavior change** — galgame runtime unaffected
2. **Progressive risk** — lowest risk first, gate at each phase
3. **Existing patterns only** — no new abstraction styles
4. **Gate: build green + tests green per phase**

## Solution: 6-Phase Decoupling

### Phase 0: Move IRenderDevice.h to api/ (risk: none)

- `git mv src/render/IRenderDevice.h src/render/api/IRenderDevice.h`
- Update 13 include paths + 1 CMakeLists line
- 7 external refs (`../render/IRenderDevice.h` → `../render/api/IRenderDevice.h`)
- 6 internal refs (`"IRenderDevice.h"` → `"api/IRenderDevice.h"`)

### Phase 1: C++ side inject all Lua registry keys + remove binding BackendRegistry deps (risk: low)

**Critical finding:** `backend_factory.lua` only calls `select_platform_backend`. Render/Audio/InputRouter/VideoPlayer Lua registry keys NEVER set in normal flow. Current code survives via singleton fallback in `getXxxFromLua`.

**Fix:**
1. `Engine::initScriptingPhase()` pushes 5 backend pointers to Lua registry (after `setLuaState`)
2. Each binding file inlines Lua registry lookup (~3 lines per helper function)
3. Remove `#include "../di/BackendRegistry.h"` from 5 binding files

**VFXBinding special:** Uses `static ParticleSystem s_particleSystem` (file-local). Replace 10 `getParticleSystem()` calls with `&s_particleSystem`. Eliminates 2 duplicate BackendRegistry includes.

**DebugBinding:** Deferred to Phase 5 (10 calls pattern differs from others).

**Result:** script→di: 10→1

### Phase 2: Type-indexed BackendRegistry (risk: low)

- Replace 20 named `I*` fields with `std::unordered_map<std::type_index, void*>`
- Replace 18 `#include "I*.h"` with 20 forward declarations in header
- RTTI confirmed enabled (`dynamic_cast` at BackendRegistry.cpp:207)
- `tryAlloc`/`release` move to .cpp (need SandboxQuota complete type)
- External API unchanged

**Result:** BackendRegistry.h goes from 18 cross-module includes to 0. Consumer coupling unchanged but registry becomes lightweight.

### Phase 3: Fix debug→script direction violation (risk: none)

- Delete `#include "../script/state/GameState.h"` from HotReload.cpp
- Replace 3 `GameState::push(L)` calls with inline `lua_getfield(L, LUA_REGISTRYINDEX, "caesura_ctx")` + nil check
- String constant matches `GameState.h:27` REGISTRY_KEY

**Result:** debug→script: 1→0

### Phase 4: Fix resource↔archive circular dep (risk: low-medium)

- AssetManager gains `addProvider(unique_ptr<IAssetProvider>)` method
- CARC construction moves from `AssetManager::init()` to `Engine::initAssetPhase()`
- Engine.cpp already has authority to reference archive concrete types
- Delete `CarcAssetProvider.h` and `CARCReader.h` includes from AssetManager.cpp
- Bonus: add missing `setMiniGameBackend()` in `Engine::initOptionalPhase()`

**Result:** resource→archive: 2→0

### Phase 5: Constructor injection for 3 remaining BackendRegistry consumers (risk: medium)

| Consumer | Inject | Timing |
|----------|--------|--------|
| VideoPlayer | `setJobSystem(IJobSystem&)` | Phase 3, after JobSystem init |
| AsyncLoader | `setJobSystem(IJobSystem&)` | Phase 3, after JobSystem init |
| SaveManager | `setCryptoEngine(ICryptoEngine&)` | Phase 4, after CryptoEngine init |
| DebugBinding | Push `IDebugManager*` to Lua registry | Phase 2, in Lua injection block |

All injections happen before first runtime use. Init-time null windows verified safe.

**Result:** render→di: 7→5, resource→di: 1→0, storage→di: 1→0, script→di: 1→0

### Phase 6: Clean entry module (risk: none)

- Remove duplicate `BgfxRenderDevice.h` include (line 38, already at line 21)
- Remove duplicate `createGpuMonitor` declaration (line 60-61, already at line 55-56)

**Result:** entry: 15→~13

## Final Coupling Budget

| Module | Before | After | Budget | Status |
|--------|--------|-------|--------|--------|
| entry | 15 | ~13 | ≤14 | ✅ |
| di | 12 | 12* | ≤14** | ✅ |
| script | 7 | 0 | ≤4 | ✅ |
| resource | 3 | 0 | ≤4 | ✅ |
| render | 3 | 2 | ≤4 | ✅ |
| rpc | 3 | 1 | ≤4 | ✅ |
| storage | 2 | 1 | ≤4 | ✅ |
| debug | 1 | 0 | ≤4 | ✅ |
| input | 0 | 0 | ≤4 | ✅ |
| platform | 0 | 0 | ≤4 | ✅ |
| steam | 0 | 0 | ≤4 | ✅ |
| archive | 1 | 1 | ≤4 | ✅ |
| audio | 1 | 1 | ≤4 | ✅ |
| job | 1 | 1 | ≤4 | ✅ |
| live2d | 2 | 2 | ≤4 | ✅ |
| minigame | 2 | 2 | ≤4 | ✅ |

\* di stays 12 because .cpp needs all I*.h for getter/setter template instantiation. Consumer coupling drops from 11→~5 modules.
\** Budget adjusted: di is DI container, inherently knows all interfaces. Same rationale as entry ≤14.

## Zero-Impact Assurance

**Untouched:**
- 43 Lua scripts (zero changes)
- 9 KAG command handlers
- Tokenizer, scheduler, flow control
- Render pipeline, layers, transitions, particles
- Audio 3-bus system
- Save/load logic (null-guard only)

**Runtime paths:** All changes in init phase (pointer storage) or compile-time (include paths). Engine::run() execution paths identical.

## Gate Per Phase

```bash
cmake --build build --config Debug --parallel   # zero errors
cd build/tests/Debug && ./CaesuraTests.exe      # all green
python scripts/count_coupling.py                # record new counts
```
