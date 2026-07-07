# Galgame Startup Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a headless galgame startup smoke test, move Lua startup path bootstrapping out of `main.cpp`, and move Engine Lua registry injection into focused entry helpers without changing the runtime script loading order.

**Architecture:** Keep `main.cpp` and `entry/Engine.cpp` as composition roots, but make them orchestration-only for script path setup and Lua registry population. The new helpers stay in `src/entry/` so implementation dependencies remain inside the composition-root boundary. Each refactor is guarded by a failing source-structure test first, then verified by behavioral headless Lua startup tests.

**Tech Stack:** C++20, CMake, doctest, Lua 5.4 C API, SDL3/bgfx/SoLoud linked through existing targets.

## Global Constraints

- Preserve galgame startup order: `Engine::init()` -> Lua `package.path` setup -> `config.lua` -> `applyDevModeToTextureManager` -> `kag/init.lua` -> `validateCarcOnStartup` -> `config.entry_script` -> sandbox lockdown.
- Do not start a real GPU device in the new smoke test; use `EngineConfig cfg; cfg.headless = true;`.
- Do not change Lua registry key strings: `Caesura.RenderDevice`, `Caesura.AudioBackend`, `Caesura.PlatformBackend`, `Caesura.InputRouter`, `Caesura.VideoPlayer`, `Caesura.TextureManager`, `Caesura.AsyncLoader`, `Caesura.DebugManager`, `Caesura.MiniGameBackend`.
- Do not change `config.entry_script`; the test must load the configured `../demo/entry.lua`.
- `main.cpp` and `entry/Engine.cpp` may still construct or wire concrete backends as composition roots, but new detailed startup logic should live in focused `src/entry/*.cpp` helpers.
- Do not revert unrelated existing worktree changes.
- Verification before completion must include: `cmake --build . --config Debug`, `ctest -C Debug --test-dir . -j 4 --output-on-failure`, `.\CaesuraTests.exe` from `build\tests\Debug`, `git diff --check`, `python scripts\count_coupling.py`, and targeted `rg` checks.

---

## File Structure

- Create `tests/cpp/test_galgame_startup.cpp`: behavioral headless smoke test for `config.lua`, `kag/init.lua`, and configured galgame entry script.
- Modify `tests/CMakeLists.txt`: add the smoke test source and copy `demo/` into test output directories.
- Create `src/entry/StartupScripts.cpp`: script directory discovery and Lua `package.path` setup.
- Modify `src/main.cpp`: replace local `discoverScriptDir()` and `setupLuaPath()` with calls to `Caesura::discoverStartupScriptDir()` and `Caesura::configureStartupLuaPath(lua_State*, const std::string&)`.
- Create `src/entry/Engine_LuaRegistry.cpp`: focused Lua registry service injection helpers.
- Modify `src/entry/Engine.cpp`: replace inline Lua registry injection blocks with helper calls.
- Modify root `CMakeLists.txt`: add `StartupScripts.cpp` and `Engine_LuaRegistry.cpp` to `ENGINE_SOURCES`.
- Modify `tests/CMakeLists.txt`: add `StartupScripts.cpp` and `Engine_LuaRegistry.cpp` to `TEST_SOURCES` if tests call or link the helpers.
- Modify `tests/cpp/test_source_encoding.cpp`: strengthen source-structure guards for `main.cpp` and `Engine.cpp`.
- Modify `tests/cpp/test_entry.cpp`: add a runtime registry smoke assertion that KAG C bindings still resolve backends through Lua registry after `Engine::init()`.

---

### Task 1: Galgame Startup Smoke Test

**Files:**
- Create: `tests/cpp/test_galgame_startup.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `Caesura::Engine`, `Caesura::EngineConfig`, `LuaManager::loadScript`, `lua_State*`.
- Produces: a doctest source file that proves headless startup can load `config.lua`, `kag/init.lua`, and the configured `entry_script`.

- [ ] **Step 1: Write the failing smoke test**

Add `cpp/test_galgame_startup.cpp` to `TEST_SOURCES` in `tests/CMakeLists.txt`, and include it in the existing `CaesuraEntryTests` doctest source filter:

```cmake
ARGS --source-file=*test_entry.cpp,*test_engine_lifecycle.cpp,*test_galgame_startup.cpp
```

Create `tests/cpp/test_galgame_startup.cpp` with this test:

```cpp
#include "doctest.h"
#include "entry/Engine.h"
#include "entry/EngineConfig.h"

#include <string>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Caesura;

namespace {

void configurePackagePath(lua_State* L, const std::string& scriptDir) {
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    const char* current = lua_tostring(L, -1);
    std::string currentPath = current ? current : "";
    lua_pop(L, 1);

    const std::string newPath =
        scriptDir + "?.lua;" +
        scriptDir + "?/init.lua;" +
        scriptDir + "kag/?.lua;" +
        currentPath;

    lua_pushstring(L, newPath.c_str());
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);
}

std::string readEntryScript(lua_State* L) {
    std::string entryScript = "game_logic.lua";
    lua_getglobal(L, "config");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "entry_script");
        if (lua_isstring(L, -1)) {
            entryScript = lua_tostring(L, -1);
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return entryScript;
}

int runLua(lua_State* L, const char* script) {
    int status = luaL_loadstring(L, script);
    if (status == LUA_OK) {
        status = lua_pcall(L, 0, 0, 0);
    }
    return status;
}

} // namespace

TEST_CASE("Galgame startup smoke: headless loads config, KAG init, and configured entry script") {
    EngineConfig cfg;
    cfg.headless = true;

    Engine engine(cfg);
    REQUIRE(engine.init());

    lua_State* L = engine.lua().state();
    REQUIRE(L != nullptr);

    configurePackagePath(L, "scripts/");

    REQUIRE(engine.lua().loadScript("scripts/config.lua"));
    REQUIRE(engine.lua().loadScript("scripts/kag/init.lua"));

    const std::string entryScript = readEntryScript(L);
    CAPTURE(entryScript);
    REQUIRE(entryScript == "../demo/entry.lua");
    REQUIRE(engine.lua().loadScript(("scripts/" + entryScript).c_str()));

    const char* apiProbe =
        "assert(type(Engine) == 'table', 'Engine table missing')\n"
        "assert(type(Engine.get_backend_info) == 'function', 'Engine.get_backend_info missing')\n"
        "local info = Engine.get_backend_info()\n"
        "assert(info.render == 'NullRender', 'render=' .. tostring(info.render))\n"
        "assert(info.audio == 'NullAudio', 'audio=' .. tostring(info.audio))\n"
        "assert(info.platform == 'NullPlatform', 'platform=' .. tostring(info.platform))\n"
        "assert(type(KAG) == 'table', 'KAG table missing')\n"
        "assert(type(KAG.render_text) == 'function', 'KAG.render_text missing')\n"
        "assert(type(_KAG_onClick) == 'function', '_KAG_onClick missing after entry script')\n"
        "assert(type(engine_update) == 'function', 'engine_update missing after entry script')\n"
        "assert(type(engine_render) == 'function', 'engine_render missing after entry script')\n";

    int status = runLua(L, apiProbe);
    if (status != LUA_OK) {
        CAPTURE(lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    CHECK(status == LUA_OK);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
cmake --build . --config Debug
ctest -C Debug --test-dir . -R CaesuraEntryTests --output-on-failure
```

Expected: the new smoke test runs through `CaesuraEntryTests` and fails because `scripts/../demo/entry.lua` cannot be loaded in the test output directory before `demo/` is copied there.

- [ ] **Step 3: Write minimal test-environment implementation**

In `tests/CMakeLists.txt`, copy `demo/` into both the `CaesuraTests` target directory and the generated fixture sync script:

```cmake
file(COPY [[${CMAKE_SOURCE_DIR}/demo/]] DESTINATION [[$<TARGET_FILE_DIR:CaesuraTests>/demo]])
```

Add this to the existing `add_custom_command(TARGET CaesuraTests POST_BUILD ...)`:

```cmake
COMMAND ${CMAKE_COMMAND} -E copy_directory
"${CMAKE_SOURCE_DIR}/demo"
"$<TARGET_FILE_DIR:CaesuraTests>/demo"
```

- [ ] **Step 4: Run test to verify it passes**

Run:

```powershell
cmake --build . --config Debug
ctest -C Debug --test-dir . -R CaesuraEntryTests --output-on-failure
```

Expected: `CaesuraEntryTests` passes and the smoke test proves the headless galgame startup chain loads the real configured demo entry.

- [ ] **Step 5: Diff checkpoint**

Run:

```powershell
git diff -- tests/CMakeLists.txt tests/cpp/test_galgame_startup.cpp
```

Expected: only the new smoke test and demo test-asset sync changed for this task.

---

### Task 2: Extract Startup Script Bootstrap From `main.cpp`

**Files:**
- Create: `src/entry/StartupScripts.cpp`
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/cpp/test_source_encoding.cpp`

**Interfaces:**
- Consumes: `lua_State*` from `engine.lua().state()`.
- Produces:
  - `std::string Caesura::discoverStartupScriptDir();`
  - `void Caesura::configureStartupLuaPath(lua_State* L, const std::string& scriptDir);`

- [ ] **Step 1: Write the failing source-structure test**

Replace the current `Main entry point centralizes script bootstrap helpers` assertions in `tests/cpp/test_source_encoding.cpp` with:

```cpp
TEST_CASE("Main entry point delegates script bootstrap helpers") {
    const auto repoRoot = findRepoRoot();
    REQUIRE_FALSE(repoRoot.empty());

    const auto helperPath = repoRoot / "src" / "entry" / "StartupScripts.cpp";
    const std::string source = readFile(repoRoot / "src" / "main.cpp");

    CHECK(source.find("fopen(\"scripts/kag/init.lua\", \"r\")") == std::string::npos);
    CHECK(source.find("lua_getglobal(L, \"package\")") == std::string::npos);
    CHECK(source.find("Caesura::discoverStartupScriptDir()") != std::string::npos);
    CHECK(source.find("Caesura::configureStartupLuaPath(L, scriptDir)") != std::string::npos);
    REQUIRE(std::filesystem::exists(helperPath));

    const std::string helper = readFile(helperPath);
    CHECK(countOccurrences(helper, "fopen(\"scripts/kag/init.lua\", \"r\")") == 1);
    CHECK(countOccurrences(helper, "lua_getglobal(L, \"package\")") == 1);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
cmake --build . --config Debug
.\build\tests\Debug\CaesuraTests.exe --source-file=*test_source_encoding.cpp
```

Expected: the test fails because `main.cpp` still owns `fopen("scripts/kag/init.lua", "r")` and `lua_getglobal(L, "package")`, and `StartupScripts.cpp` does not exist.

- [ ] **Step 3: Write minimal implementation**

Create `src/entry/StartupScripts.cpp`:

```cpp
extern "C" {
#include <lua.h>
}

#include <cstdio>
#include <string>

namespace Caesura {

std::string discoverStartupScriptDir() {
    if (FILE* f = fopen("scripts/kag/init.lua", "r")) {
        fclose(f);
        return "scripts/";
    }
    if (FILE* f = fopen("../../scripts/kag/init.lua", "r")) {
        fclose(f);
        return "../../scripts/";
    }
    if (FILE* f = fopen("../../../scripts/kag/init.lua", "r")) {
        fclose(f);
        return "../../../scripts/";
    }
    fprintf(stderr, "[main] Warning: Cannot find scripts directory.\n");
    return "scripts/";
}

void configureStartupLuaPath(lua_State* L, const std::string& scriptDir) {
    if (!L) return;

    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    const char* current = lua_tostring(L, -1);
    std::string currentPath = current ? current : "";
    lua_pop(L, 1);

    const std::string newPath =
        scriptDir + "?.lua;" +
        scriptDir + "?/init.lua;" +
        scriptDir + "kag/?.lua;" +
        currentPath;

    lua_pushstring(L, newPath.c_str());
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);
}

} // namespace Caesura
```

In `src/main.cpp`, replace the local static helpers with forward declarations:

```cpp
namespace Caesura {
std::string discoverStartupScriptDir();
void configureStartupLuaPath(lua_State* L, const std::string& scriptDir);
void applyDevModeToTextureManager(lua_State* L);
void validateCarcOnStartup(lua_State* L);
}
```

Replace each `discoverScriptDir()` call with:

```cpp
Caesura::discoverStartupScriptDir()
```

Replace each `setupLuaPath(engine, scriptDir)` call with:

```cpp
Caesura::configureStartupLuaPath(engine.lua().state(), scriptDir)
```

In the main gameplay path, where `lua_State* L` already exists, keep the call shape:

```cpp
Caesura::configureStartupLuaPath(L, scriptDir);
```

Add `src/entry/StartupScripts.cpp` to root `CMakeLists.txt` `ENGINE_SOURCES` and to `tests/CMakeLists.txt` `TEST_SOURCES`.

- [ ] **Step 4: Run test to verify it passes**

Run:

```powershell
cmake --build . --config Debug
.\build\tests\Debug\CaesuraTests.exe --source-file=*test_source_encoding.cpp
ctest -C Debug --test-dir . -R CaesuraHeadlessCliSmoke --output-on-failure
ctest -C Debug --test-dir . -R CaesuraEntryTests --output-on-failure
```

Expected: source guard passes, headless CLI smoke still sees null backends, and entry tests still pass.

- [ ] **Step 5: Diff checkpoint**

Run:

```powershell
git diff -- src/main.cpp src/entry/StartupScripts.cpp CMakeLists.txt tests/CMakeLists.txt tests/cpp/test_source_encoding.cpp
```

Expected: `main.cpp` no longer contains script discovery or Lua package path mutation details.

---

### Task 3: Extract Engine Lua Registry Injection

**Files:**
- Create: `src/entry/Engine_LuaRegistry.cpp`
- Modify: `src/entry/Engine.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/cpp/test_source_encoding.cpp`
- Modify: `tests/cpp/test_entry.cpp`

**Interfaces:**
- Consumes: initialized `BackendRegistry`, `lua_State*`, `IInputRouter*`, `IVideoPlayer*`, `IMiniGameBackend*`.
- Produces:
  - `void Caesura::registerEngineLuaRegistryServices(lua_State* L, IInputRouter* inputRouter, IVideoPlayer* videoPlayer);`
  - `void Caesura::registerMiniGameLuaRegistryService(lua_State* L, IMiniGameBackend* miniGameBackend);`

- [ ] **Step 1: Write the failing source-structure test**

Append this test to `tests/cpp/test_source_encoding.cpp`:

```cpp
TEST_CASE("Engine core delegates Lua registry service injection") {
    const auto repoRoot = findRepoRoot();
    REQUIRE_FALSE(repoRoot.empty());

    const auto helperPath = repoRoot / "src" / "entry" / "Engine_LuaRegistry.cpp";
    const std::string source = readFile(repoRoot / "src" / "entry" / "Engine.cpp");

    CHECK(source.find("\"Caesura.RenderDevice\"") == std::string::npos);
    CHECK(source.find("\"Caesura.AudioBackend\"") == std::string::npos);
    CHECK(source.find("\"Caesura.PlatformBackend\"") == std::string::npos);
    CHECK(source.find("\"Caesura.InputRouter\"") == std::string::npos);
    CHECK(source.find("\"Caesura.VideoPlayer\"") == std::string::npos);
    CHECK(source.find("\"Caesura.TextureManager\"") == std::string::npos);
    CHECK(source.find("\"Caesura.AsyncLoader\"") == std::string::npos);
    CHECK(source.find("\"Caesura.DebugManager\"") == std::string::npos);
    CHECK(source.find("\"Caesura.MiniGameBackend\"") == std::string::npos);
    CHECK(source.find("registerEngineLuaRegistryServices(") != std::string::npos);
    CHECK(source.find("registerMiniGameLuaRegistryService(") != std::string::npos);
    REQUIRE(std::filesystem::exists(helperPath));

    const std::string helper = readFile(helperPath);
    CHECK(countOccurrences(helper, "\"Caesura.RenderDevice\"") == 1);
    CHECK(countOccurrences(helper, "\"Caesura.AudioBackend\"") == 1);
    CHECK(countOccurrences(helper, "\"Caesura.PlatformBackend\"") == 1);
    CHECK(countOccurrences(helper, "\"Caesura.InputRouter\"") == 1);
    CHECK(countOccurrences(helper, "\"Caesura.VideoPlayer\"") == 1);
    CHECK(countOccurrences(helper, "\"Caesura.TextureManager\"") == 1);
    CHECK(countOccurrences(helper, "\"Caesura.AsyncLoader\"") == 1);
    CHECK(countOccurrences(helper, "\"Caesura.DebugManager\"") == 1);
    CHECK(countOccurrences(helper, "\"Caesura.MiniGameBackend\"") == 1);
}
```

- [ ] **Step 2: Add a runtime registry behavior probe**

Append this test to `tests/cpp/test_entry.cpp`:

```cpp
TEST_CASE("Entry: Engine headless Lua registry services back KAG bindings") {
    EngineConfig cfg;
    cfg.headless = true;

    Engine engine(cfg);
    REQUIRE(engine.init());

    lua_State* L = engine.lua().state();
    REQUIRE(L != nullptr);

    const char* script =
        "assert(KAG.set_bus_volume('bgm', 0.5) == true)\n"
        "assert(KAG.get_bus_volume('bgm') == 0.5)\n"
        "assert(KAG.render_text('registry probe', 1, 2, 255, 255, 255, 255) == true)\n";

    int luaStatus = luaL_loadstring(L, script);
    if (luaStatus == LUA_OK) {
        luaStatus = lua_pcall(L, 0, 0, 0);
    }
    if (luaStatus != LUA_OK) {
        CAPTURE(lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    CHECK(luaStatus == LUA_OK);
}
```

- [ ] **Step 3: Run tests to verify RED**

Run:

```powershell
cmake --build . --config Debug
.\build\tests\Debug\CaesuraTests.exe --source-file=*test_source_encoding.cpp
.\build\tests\Debug\CaesuraTests.exe --source-file=*test_entry.cpp
```

Expected: `test_source_encoding.cpp` fails because registry keys still live in `Engine.cpp`. The runtime probe may already pass before the refactor; it is retained as a behavioral regression guard.

- [ ] **Step 4: Write minimal implementation**

Create `src/entry/Engine_LuaRegistry.cpp`:

```cpp
extern "C" {
#include <lua.h>
}

#include "../debug/DebugManager.h"
#include "../debug/api/IDebugManager.h"
#include "../di/BackendRegistry.h"
#include "../input/api/IInputRouter.h"
#include "../minigame/api/IMiniGameBackend.h"
#include "../render/TextureManager.h"
#include "../render/api/ITextureManager.h"
#include "../render/api/IVideoPlayer.h"
#include "../resource/AsyncLoader.h"
#include "../resource/api/IAsyncLoader.h"

namespace Caesura {

namespace {

void setRegistryLightUserData(lua_State* L, const char* key, void* value) {
    lua_pushlightuserdata(L, value);
    lua_setfield(L, LUA_REGISTRYINDEX, key);
}

} // namespace

void registerEngineLuaRegistryServices(lua_State* L,
                                       IInputRouter* inputRouter,
                                       IVideoPlayer* videoPlayer) {
    if (!L) return;

    auto& registry = BackendRegistry::instance();
    setRegistryLightUserData(L, "Caesura.RenderDevice", registry.getRenderDevice());
    setRegistryLightUserData(L, "Caesura.AudioBackend", registry.getAudioBackend());
    setRegistryLightUserData(L, "Caesura.PlatformBackend", registry.getPlatformBackend());
    setRegistryLightUserData(L, "Caesura.InputRouter", inputRouter);
    setRegistryLightUserData(L, "Caesura.VideoPlayer", videoPlayer);
    setRegistryLightUserData(L, "Caesura.TextureManager",
                             static_cast<ITextureManager*>(&TextureManager::instance()));
    setRegistryLightUserData(L, "Caesura.AsyncLoader",
                             static_cast<IAsyncLoader*>(&AsyncLoader::instance()));
    setRegistryLightUserData(L, "Caesura.DebugManager",
                             static_cast<IDebugManager*>(&DebugManager::instance()));
}

void registerMiniGameLuaRegistryService(lua_State* L, IMiniGameBackend* miniGameBackend) {
    if (!L) return;
    setRegistryLightUserData(L, "Caesura.MiniGameBackend", miniGameBackend);
}

} // namespace Caesura
```

In `src/entry/Engine.cpp`, add forward declarations near the other entry helper declarations:

```cpp
void registerEngineLuaRegistryServices(lua_State* L,
                                       IInputRouter* inputRouter,
                                       IVideoPlayer* videoPlayer);
void registerMiniGameLuaRegistryService(lua_State* L,
                                        IMiniGameBackend* miniGameBackend);
```

Replace the registry block in `Engine::initScriptingPhase()` with:

```cpp
    registerEngineLuaRegistryServices(m_lua->state(), m_inputRouter.get(), m_videoPlayer.get());
```

Replace the mini-game registry block in `Engine::initOptionalPhase()` with:

```cpp
    registerMiniGameLuaRegistryService(m_lua->state(), m_miniGameBackend.get());
```

Add `src/entry/Engine_LuaRegistry.cpp` to root `CMakeLists.txt` `ENGINE_SOURCES` and to `tests/CMakeLists.txt` `TEST_SOURCES`.

- [ ] **Step 5: Run tests to verify GREEN**

Run:

```powershell
cmake --build . --config Debug
.\build\tests\Debug\CaesuraTests.exe --source-file=*test_source_encoding.cpp
.\build\tests\Debug\CaesuraTests.exe --source-file=*test_entry.cpp
ctest -C Debug --test-dir . -R CaesuraEntryTests --output-on-failure
```

Expected: source guard passes, registry behavior probe passes, and entry tests pass.

- [ ] **Step 6: Diff checkpoint**

Run:

```powershell
git diff -- src/entry/Engine.cpp src/entry/Engine_LuaRegistry.cpp CMakeLists.txt tests/CMakeLists.txt tests/cpp/test_source_encoding.cpp tests/cpp/test_entry.cpp
```

Expected: `Engine.cpp` no longer contains Lua registry key string literals, and the helper owns the injection details.

---

### Task 4: Full Verification And Coupling Audit

**Files:**
- No planned production edits.

**Interfaces:**
- Consumes: completed Tasks 1-3.
- Produces: fresh command evidence for build, tests, diff hygiene, and coupling.

- [ ] **Step 1: Run full build**

Run:

```powershell
cmake --build . --config Debug
```

Expected: build exits 0.

- [ ] **Step 2: Run all CTest tests**

Run:

```powershell
ctest -C Debug --test-dir . -j 4 --output-on-failure
```

Expected: all registered CTest tests pass.

- [ ] **Step 3: Run full doctest executable**

Run:

```powershell
cd build\tests\Debug
.\CaesuraTests.exe
cd ..\..\..
```

Expected: all doctest cases pass.

- [ ] **Step 4: Run diff whitespace check**

Run:

```powershell
git diff --check
```

Expected: exit 0. Existing CRLF warnings may appear, but no whitespace error should be reported.

- [ ] **Step 5: Run coupling script**

Run:

```powershell
python scripts\count_coupling.py
```

Expected: `entry` may remain high as composition root, and non-composition modules should not exceed project thresholds because new dependencies stay in `src/entry/`.

- [ ] **Step 6: Run targeted source leakage checks**

Run:

```powershell
rg -n "fopen\\(\"scripts/kag/init.lua\"|lua_getglobal\\(L, \"package\"\\)" src/main.cpp
rg -n "\"Caesura\\.(RenderDevice|AudioBackend|PlatformBackend|InputRouter|VideoPlayer|TextureManager|AsyncLoader|DebugManager|MiniGameBackend)\"" src/entry/Engine.cpp
rg -n "discoverStartupScriptDir|configureStartupLuaPath|registerEngineLuaRegistryServices|registerMiniGameLuaRegistryService" src/main.cpp src/entry/Engine.cpp src/entry/StartupScripts.cpp src/entry/Engine_LuaRegistry.cpp
```

Expected: first two commands produce no matches in `main.cpp` or `Engine.cpp`; third command shows only orchestration calls in `main.cpp` / `Engine.cpp` and implementations in helper files.

- [ ] **Step 7: Completion audit**

Check each requirement:

```text
1. Galgame startup smoke test exists and loads config.lua, kag/init.lua, and config.entry_script in headless mode.
2. main.cpp delegates script discovery and package.path setup to StartupScripts.cpp.
3. Engine.cpp delegates Lua registry service injection to Engine_LuaRegistry.cpp.
4. Runtime Lua Engine and KAG APIs still work in headless startup.
5. Build, CTest, full doctest, diff check, coupling script, and targeted source searches have fresh passing evidence.
```

Expected: every item has direct file or command-output evidence before completion is claimed.
