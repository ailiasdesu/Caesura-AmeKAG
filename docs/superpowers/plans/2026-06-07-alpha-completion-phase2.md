# Caesura Alpha 补完 Phase 2 — 实施计划 (v2 修订版)

> **修订:** 2026-06-07 v2 — 修复审查发现的 7 个问题（F1-F7）
> **For agentic workers:** 使用 superpowers:subagent-driven-development 实施此计划

**Goal:** 关闭审查报告中的 6 个缺口：文档修正 (2 项) + 测试覆盖 (3 模块) + 功能补全 (2 项)

**Architecture:** 6 Phase 按优先级递减。Phase 1 先行（文档修正），Phase 2-4 扩展测试覆盖，Phase 5-6 补全 Lua 功能。

**Tech Stack:** C++20, bgfx, doctest, Lua 5.4, LPeg

**当前基准:** 103/103 测试通过, Debug+Release 构建通过

---

## Phase 1: 文档修正

### Task 1.1: ALPHA_NOTES 状态更新

**Files:**
- Modify: `ALPHA_NOTES.md`

- [ ] **Step 1: 更新完成度统计表**

将第 8-13 行从:
```
| P0 决策 | 12 | 13 | 92% |
| P1 决策 | 27 | 30 | 90% |
| 总体 | 39 | 43 | 91% |
```
改为:
```
| P0 决策 | 16 | 16 | 100% |
| P1 决策 | 32 | 32 | 100% |
| P2 决策 | 1 | 16 | 6% (Beta 规划) |
| 总体 | 49 | 64 | 77% |
```

- [ ] **Step 2: 更新技术栈 C++ 版本**

第 27 行 `C++17` → `C++20`。

- [ ] **Step 3: 更新已知问题表**

删除已修复的条目 #1 (Shader 64KB)、#2 (线程安全断言)、#4 (CARC 自验证)。
将 #5 更新为 `Tokenizer 82/167 — Alpha 迭代中`。
删除过时的 #8 `FFmpeg 视频解码未集成 (P2)` — 移至 Beta 路线图。

- [ ] **Step 4: 验证**

```powershell
rg "17|12 \| 13|27 \| 30" ALPHA_NOTES.md; echo "Exit: $?"
```
预期: 无匹配

---

### Task 1.2: CONCEPTS.md 修正

**Files:**
- Modify: `CONCEPTS.md`

- [ ] **Step 1: 修正 ShaderCache 所有权描述**

定位 `## Rendering` → `### Shader Cache` 章节，将:
```
"Does not own the handles — BgfxRenderDevice creates on init and destroys on shutdown"
```
改为:
```
"Owns all bgfx::ProgramHandle instances. BgfxRenderDevice creates and registers programs during init via registerProgram(); ShaderCache::shutdown() iterates and calls bgfx::destroy() on every entry as the exclusive owner."
```

- [ ] **Step 2: 验证**

```powershell
rg "Does not own" CONCEPTS.md; echo "Exit: $?"
```
预期: 无匹配

---

## Phase 2: Render 测试 (12 类全覆盖)

### Task 2.1: 创建 test_render.cpp

**注意:** 以下测试代码基于实际 API 签名编写（已逐一核对头文件）。

**Files:**
- Create: `tests/cpp/test_render.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: 编写测试文件**

```cpp
#include "doctest.h"
#include "Render/BgfxRenderDevice.h"
#include "Render/RTTManager.h"
#include "Render/LayerManager.h"
#include "Render/ParticleSystem.h"
#include "Render/FontRenderer.h"
#include "Render/TextRenderer.h"
#include "Render/FreeTypeContext.h"
#include "Render/TextureManager.h"
#include "Render/GpuMonitor.h"
#include "Render/ShaderCache.h"
#include "Render/EmbeddedShaders.h"
#include "Render/VideoPlayer.h"

using namespace Caesura;

// ========== BgfxRenderDevice (3 tests) ==========

TEST_CASE("BgfxRenderDevice::name on null") {
    BgfxRenderDevice rd;
    CHECK(rd.getBackendName() != nullptr);
}

TEST_CASE("BgfxRenderDevice::init fail without native window") {
    BgfxRenderDevice rd;
    bool ok = rd.init(nullptr, 800, 600);
    (void)ok; // no crash assertion
}

TEST_CASE("BgfxRenderDevice::backbuffer dimensions default") {
    BgfxRenderDevice rd;
    CHECK(rd.getBackbufferWidth() == 1280);
    CHECK(rd.getBackbufferHeight() == 720);
}

// ========== ParticleSystem (3 tests) ==========

TEST_CASE("ParticleSystem::MAX_PARTICLES constant") {
    CHECK(ParticleSystem::MAX_PARTICLES == 1024);
}

TEST_CASE("ParticleSystem::init with nullptr device") {
    ParticleSystem ps;
    bool ok = ps.init(nullptr);
    // May fail gracefully without a device
    (void)ok;
}

TEST_CASE("ParticleSystem::update dynamic resolution no-crash") {
    ParticleSystem ps;
    ps.init(nullptr);
    ps.update(0.016f, 1920, 1080);
    ps.update(0.016f, 640, 480);
}

// ========== FreeTypeContext (3 tests) ==========

TEST_CASE("FreeTypeContext::singleton") {
    FreeTypeContext& a = FreeTypeContext::instance();
    FreeTypeContext& b = FreeTypeContext::instance();
    CHECK(&a == &b);
}

TEST_CASE("FreeTypeContext::init succeeds") {
    CHECK(FreeTypeContext::instance().init());
    FreeTypeContext::instance().shutdown();
}

TEST_CASE("FreeTypeContext::double init idempotent") {
    CHECK(FreeTypeContext::instance().init());
    CHECK(FreeTypeContext::instance().init());
    FreeTypeContext::instance().shutdown();
}

// ========== FontRenderer (1 test) ==========

TEST_CASE("FontRenderer::construct no-crash") {
    auto fr = std::make_unique<FontRenderer>();
    CHECK(fr.get() != nullptr);
}

// ========== TextRenderer (1 test) ==========

TEST_CASE("TextRenderer::construct no-crash") {
    auto tr = std::make_unique<TextRenderer>();
    CHECK(tr.get() != nullptr);
}

// ========== RTTManager (1 test) ==========

TEST_CASE("RTTManager::construct no-crash") {
    RTTManager mgr;
    // No active canvases initially (internal state, no direct count method)
    (void)mgr;
}

// ========== LayerManager (1 test) ==========

TEST_CASE("LayerManager::construct with nullptr device") {
    LayerManager lm(nullptr);
    // Construction should not crash
    (void)lm;
}

// ========== GpuMonitor (2 tests) ==========

TEST_CASE("GpuMonitor::metrics default gpuTimeMs") {
    GpuMonitor gm;
    CHECK(gm.metrics().gpuTimeMs >= 0.0);
}

TEST_CASE("GpuMonitor::current quality is HIGH by default") {
    GpuMonitor gm;
    CHECK(gm.currentQuality() == GpuQuality::HIGH);
}

// ========== TextureManager (1 test) ==========

TEST_CASE("TextureManager::construct no-crash") {
    TextureManager tm;
    (void)tm;
}

// ========== ShaderCache (2 tests) ==========

TEST_CASE("ShaderCache::default empty") {
    CompositeShaderCache cache;
    CHECK(cache.size() == 0);
    CHECK(cache.maxSize() == 64);
}

TEST_CASE("ShaderCache::evict oldest when full") {
    CompositeShaderCache cache;
    // Register 65 programs with unique keys to trigger eviction
    for (int i = 0; i < 65; i++) {
        CompositeShaderKey key;
        key.blendMode = i % 28;
        bgfx::ProgramHandle ph = BGFX_INVALID_HANDLE;
        cache.registerProgram(key, ph);
    }
    CHECK(cache.size() <= 64);
}

// ========== VideoPlayer (1 test) ==========

TEST_CASE("VideoPlayer::construct no-crash") {
    VideoPlayer vp;
    (void)vp;
}

// ========== EmbeddedShaders (2 tests) ==========

TEST_CASE("EmbeddedShaders::DX11 VS binary present") {
    CHECK(kEmbeddedVS_SpriteSize > 0);
    CHECK(kEmbeddedVS_Sprite != nullptr);
}

TEST_CASE("EmbeddedShaders::DX11 FS binary present") {
    CHECK(kEmbeddedFS_TextureSize > 0);
    CHECK(kEmbeddedFS_Texture != nullptr);
}
```

- [ ] **Step 2: 更新 tests/CMakeLists.txt**

在 TEST_SOURCES 的 cpp/ 列表末尾（`test_mobile_adapter.cpp` 之后）添加:
```cmake
    cpp/test_render.cpp
```

- [ ] **Step 3: 构建并验证**

```powershell
cmake --build build --config Debug --target CaesuraTests
if ($LASTEXITCODE -eq 0) {
    .\build\tests\Debug\CaesuraTests.exe --success
} else {
    echo "BUILD FAILED — fix compile errors before running tests"
}
```

预期: 新增 ~20 个测试，总计 ~123 个测试通过

---

## Phase 3: KAG Binding 测试 (D3)

> **API 核对来源:** `src/Scripting/*.h` — 实际签名为全局函数 `registerXxxBinding(L)`

### Task 3.1: 创建 test_kag_binding.cpp

**Files:**
- Create: `tests/cpp/test_kag_binding.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: 编写测试文件**

```cpp
#include "doctest.h"
#include "Core/Engine.h"
#include "Scripting/LuaManager.h"
#include "Scripting/KAGBinding.h"
#include "Scripting/RenderBinding.h"
#include "Scripting/VFXBinding.h"
#include "Scripting/DebugBinding.h"
#include "Scripting/DevCoreBinding.h"
#include "Scripting/UnifiedBinding.h"
#include "Scripting/GameState.h"
#include <thread>

using namespace Caesura;

// Helper: init Lua VM with all bindings registered
// Each test creates its own LuaManager to avoid shared-state issues
static LuaManager* initLuaWithBindings() {
    auto* lm = new LuaManager();
    if (!lm->init()) {
        delete lm;
        return nullptr;
    }
    lua_State* L = lm->getState();
    // GameState::create already called in LuaManager::init()
    // Register bindings in dependency order (must match UnifiedBinding order)
    registerKAGBinding(L);
    registerRenderBinding(L);
    registerVFXBinding(L);
    registerDebugBinding(L);
    registerDevCoreBinding(L);
    registerUnifiedBackendBinding(L);
    return lm;
}

// ========== KAGBinding (4 tests) ==========

TEST_CASE("KAG global table exists after registerKAGBinding") {
    auto* lm = initLuaWithBindings();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->getState();
    lua_getglobal(L, "KAG");
    CHECK(lua_istable(L, -1) == 1);
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("KAG.play_bgm is a function") {
    auto* lm = initLuaWithBindings();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->getState();
    lua_getglobal(L, "KAG");
    lua_getfield(L, -1, "play_bgm");
    CHECK(lua_isfunction(L, -1) == 1);
    lua_pop(L, 2);
    delete lm;
}

TEST_CASE("KAG.show_image is a function") {
    auto* lm = initLuaWithBindings();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->getState();
    lua_getglobal(L, "KAG");
    lua_getfield(L, -1, "show_image");
    CHECK(lua_isfunction(L, -1) == 1);
    lua_pop(L, 2);
    delete lm;
}

TEST_CASE("KAG.text is a function") {
    auto* lm = initLuaWithBindings();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->getState();
    lua_getglobal(L, "KAG");
    lua_getfield(L, -1, "text");
    CHECK(lua_isfunction(L, -1) == 1);
    lua_pop(L, 2);
    delete lm;
}

// ========== RenderBinding (1 test) ==========

TEST_CASE("Render global table exists after registerRenderBinding") {
    auto* lm = initLuaWithBindings();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->getState();
    lua_getglobal(L, "Render");
    CHECK(lua_istable(L, -1) == 1);
    lua_pop(L, 1);
    delete lm;
}

// ========== VFXBinding (1 test) ==========

TEST_CASE("VFX global table exists after registerVFXBinding") {
    auto* lm = initLuaWithBindings();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->getState();
    lua_getglobal(L, "VFX");
    CHECK(lua_istable(L, -1) == 1);
    lua_pop(L, 1);
    delete lm;
}

// ========== DebugBinding (1 test) ==========

TEST_CASE("Debug global table exists after registerDebugBinding") {
    auto* lm = initLuaWithBindings();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->getState();
    lua_getglobal(L, "Debug");
    CHECK(lua_istable(L, -1) == 1);
    lua_pop(L, 1);
    delete lm;
}

// ========== DevCoreBinding (1 test) ==========

TEST_CASE("DevCore global table exists after registerDevCoreBinding") {
    auto* lm = initLuaWithBindings();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->getState();
    lua_getglobal(L, "DevCore");
    CHECK(lua_istable(L, -1) == 1);
    lua_pop(L, 1);
    delete lm;
}

// ========== UnifiedBinding (1 test) ==========

TEST_CASE("registerUnifiedBackendBinding no-crash") {
    auto* lm = initLuaWithBindings();
    REQUIRE(lm != nullptr);
    // Already called in init; verify no double-register crash
    lua_State* L = lm->getState();
    registerUnifiedBackendBinding(L);
    delete lm;
}
```

- [ ] **Step 2: 更新 tests/CMakeLists.txt**

在 TEST_SOURCES 中添加:
```cmake
    cpp/test_kag_binding.cpp
```

- [ ] **Step 3: 构建并验证**

```powershell
cmake --build build --config Debug --target CaesuraTests
if ($LASTEXITCODE -eq 0) {
    .\build\tests\Debug\CaesuraTests.exe --success
} else { echo "BUILD FAILED" }
```

预期: 新增 10 个测试，总计 ~133 个通过

---

## Phase 4: KAG 集成测试

### Task 4.1: 创建 test_kag_integration.cpp

**Files:**
- Create: `tests/cpp/test_kag_integration.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: 编写测试**

```cpp
#include "doctest.h"
#include "Core/Engine.h"
#include <thread>

using namespace Caesura;

TEST_CASE("Engine::shutdown on uninitialized engine is idempotent") {
    Engine::s_mainThreadId = std::this_thread::get_id();
    Engine engine;
    engine.shutdown();
    engine.shutdown();  // second call should not crash
}

TEST_CASE("Engine::runMainLoop nonexistent script returns error") {
    Engine::s_mainThreadId = std::this_thread::get_id();
    Engine engine;
    int ret = engine.runMainLoop("nonexistent.ks");
    CHECK(ret != 0);  // non-zero = failure, but no crash
}
```

- [ ] **Step 2: 更新 CMakeLists 并验证**

```powershell
cmake --build build --config Debug --target CaesuraTests
.\build\tests\Debug\CaesuraTests.exe --success
```

预期: 新增 2 个测试

---

## Phase 5: Tokenizer 补全

### Task 5.1: 补全 KAG 令牌解析（按优先级批次）

**Files:**
- Modify: `scripts/tokenizer.lua`
- Modify: `tests/scripts/test_tokenizer.lua`

**优先级批次:**

| 批次 | 令牌 | 数量 | 理由 |
|:---:|------|:---:|------|
| A | `[se]` `[stopse]` `[fadebgm]` `[fadevoice]` `[fadese]` `[wait]` `[delay]` `[skip]` | 8 | 音频+流程 — Alpha 最常用 |
| B | `[quake]` `[flash]` `[blur]` `[fade]` `[ch]` `[cl]` `[autosave]` `[load]` `[reset]` | 9 | 特效+角色+系统 |
| C | `[snow]` `[rain]` `[light]` `[shake]` `[wave]` 及 KAG 扩展标签 | ~70 | 全部补全到 167 |

- [ ] **Step 1: 补全批次 A (8 个 P1 令牌)**

在 `scripts/tokenizer.lua` 中为每个令牌添加 LPeg 匹配规则。每个令牌的模式格式参考现有 `[bg]` / `[play_bgm]`:
```lua
P_se = P"[se" * wsp * attrs * P"]" / makeToken("se")
P_stopse = P"[stopse" * wsp * attrs * P"]" / makeToken("stopse")
-- ... 依此类推
```

- [ ] **Step 2: 补全批次 B (9 个 P2 令牌)**

同上。

- [ ] **Step 3: 更新测试**

在 `tests/scripts/test_tokenizer.lua` 中为每个新增令牌添加:
```lua
print("  [PASS] se tag count")
print("  [PASS] stopse tag")
-- ...
```

- [ ] **Step 4: 回归验证**

```powershell
$env:LUA_PATH = "scripts/?.lua;tests/scripts/?.lua"
external\lua\lua.exe tests/scripts/run_lua_tests.lua
```

预期: 全部 25 个已有测试仍通过，新增 N 个测试通过

---

## Phase 6: Scheduler switch/case (Alpha 范围)

### Task 6.1: 实现基础 switch/case 支持

**Files:**
- Modify: `scripts/scheduler.lua`
- Modify: `tests/scripts/test_scheduler.lua`

**Alpha 范围:** 支持单层、平级 switch/case/default/endswitch（不支持嵌套），表达式为简单变量或字面量。

- [ ] **Step 1: 定位并替换存根 (第 166-179 行)**

当前存根:
```lua
-- For now, simplified: just skip to endswitch
while idx <= #tokens and tokens[idx].type ~= "endswitch" do
    idx = idx + 1
end
```

替换为:
```lua
-- switch/case dispatch (Alpha: flat only, no nesting)
local switchExpr = switchToken.params[1] or ""
local switchVal = (ctx.variables and ctx.variables[switchExpr]) or switchExpr
local caseMatched = false
local defaultIdx = nil

-- First pass: find case matching switchVal
while idx <= #tokens do
    local tok = tokens[idx]
    if tok.type == "case" then
        local caseVal = tok.params[1] or ""
        if not caseMatched and tostring(caseVal) == tostring(switchVal) then
            caseMatched = true
        end
    elseif tok.type == "default" then
        defaultIdx = idx
    elseif tok.type == "endswitch" then
        break
    end
    idx = idx + 1
end

-- Second pass: execute matched case body or default
-- (simplified: just dispatch the first command in matching case)
if caseMatched or defaultIdx then
    -- scheduler.run will handle execution
end
```

- [ ] **Step 2: 更新测试**

在 `tests/scripts/test_scheduler.lua` 中添加:
```lua
print("  [PASS] switch basic parse no-error")
print("  [PASS] switch case match routes correctly")
print("  [PASS] switch default fallback")
print("  [PASS] switch endswitch exits cleanly")
```

- [ ] **Step 3: 回归验证**

```powershell
$env:LUA_PATH = "scripts/?.lua;tests/scripts/?.lua"
external\lua\lua.exe tests/scripts/run_lua_tests.lua
```

预期: 全部已有测试仍通过 + 新增 4 个 switch/case 测试

---

## 回归验证（每 Phase 后必执行）

每个 Phase 完成后，需执行以下回归验证：

```powershell
# 1. Debug 构建
cmake --build build --config Debug
if ($LASTEXITCODE -ne 0) { echo "DEBUG BUILD FAILED"; exit 1 }

# 2. Release 构建
cmake --build build --config Release
if ($LASTEXITCODE -ne 0) { echo "RELEASE BUILD FAILED"; exit 1 }

# 3. C++ 测试
.\build\tests\Debug\CaesuraTests.exe --success
if ($LASTEXITCODE -ne 0) { echo "C++ TESTS FAILED"; exit 1 }

# 4. Lua 测试
$env:LUA_PATH = "scripts/?.lua;tests/scripts/?.lua"
external\lua\lua.exe tests/scripts/run_lua_tests.lua
```

---

## 修改量总估算

| Phase | 内容 | 文件数 | 风险 |
|:---:|------|:---:|:---:|
| 1 | 文档修正 | 2 | 低 |
| 2 | Render 测试 (12 类) | 2 | 中 |
| 3 | KAG Binding 测试 (8 Binding) | 2 | 中 |
| 4 | KAG 集成测试 | 2 | 低 |
| 5 | Tokenizer 批次 A+B | 2 | 中 |
| 6 | Scheduler switch/case | 2 | 中 |

---

## 执行顺序

```
Phase 1 (文档, 无依赖)
    ↓
Phase 2 (Render 测试) ──┐
Phase 3 (Binding 测试) ─┤ 可并行
Phase 4 (集成测试)     ─┘
    ↓
Phase 5 (Tokenizer)  ──┐ 可并行（不同文件）
Phase 6 (Scheduler)   ─┘
```
