# Next Steps Plan — Caesura (AmeKAG) Alpha 0.2 → Beta

> Generated: 2026-06-08 | Engine @ `d3f95f0` | CI: 3-platform green | 140 tests / 270 assertions

## Current Baseline

**Engine:** C++20, SDL3 + bgfx + SoLoud + Lua 5.4 + FFmpeg
**Cross-platform:** Windows (MSVC) / macOS (Clang) / Linux (GCC) — CI 全绿
**Test suite:** 140 TEST_CASEs across 23 C++ files + Lua script tests
**CI test gap:** macOS/Linux tests `continue-on-error: true` — failures masked. Windows tests GPU-bound.

---

## Track A: CI 测试可见性 [P0 — Week 1]

### Problem
CI 三平台测试都加了 `continue-on-error: true`，意味着即使测试失败也不会阻塞 CI。当前无法从 CI 状态判断测试是否通过。

### Steps

1. **CI test result reporting**
   - macOS/Linux: 解析 doctest XML/JSON 输出，上传为 CI artifact
   - Windows: 同上，但测试 GPU 依赖仍需 `continue-on-error`
   - 目标：CI summary 显示 pass/fail 计数

2. **Remove continue-on-error from macOS/Linux tests**
   - macOS/Linux 无 GPU 依赖，bgfx 可 headless 初始化（noop 后端或最小 window）
   - 逐步收紧：先观察一周，再移除 `continue-on-error`

3. **Test failure = CI failure**
   - 最终状态：macOS/Linux 测试失败 → CI 红灯
   - Windows 保留 `continue-on-error`（GPU 限制）

### Files
- `.github/workflows/ci.yml`
- `tests/cpp/test_main.cpp`（可能需添加 `--reporters=xml`）

---

## Track B: 无头渲染测试 [P0 — Week 1-2]

### Problem
Windows CI runner 无 GPU，bgfx D3D11 初始化失败导致测试崩溃。需要无头渲染能力或 mock 后端。

### Steps

1. **MockRenderDevice**
   - 实现 `IRenderDevice` 的空操作子类
   - 纹理操作返回有效假句柄，不调用 bgfx
   - `getBackbufferWidth/Height` 返回默认分辨率
   - 用于所有不需要实际绘制的测试

2. **注入点**
   - `BackendRegistry` 支持 `setRenderDeviceForTesting()`
   - 或在测试 `main()` 中通过 `--mock-render` 标志切换

3. **CI 启用 Windows 测试**
   - 无头模式直接运行；GPU 模式保留 `continue-on-error`
   - 目标：Windows CI 测试不再依赖 GPU

### Files
- `src/Render/MockRenderDevice.h/.cpp`（新增）
- `src/Core/BackendRegistry.h/.cpp`
- `tests/cpp/test_main.cpp`

---

## Track C: 测试覆盖率 [P1 — Week 2]

### Problem
140 测试覆盖了所有模块，但缺少覆盖率数据，无法识别薄弱区。

### Steps

1. **CMake 覆盖率目标**
   - `cmake --build build --target coverage`（仅 Linux/macOS，gcov/lcov）
   - Windows 用 OpenCppCoverage 或跳过

2. **覆盖率报告 → CI artifact**
   - 每次 CI run 生成 `coverage/index.html`
   - 上传为 artifact，可从 PR 查看

3. **覆盖率基线**
   - 记录当前覆盖率百分比
   - 设定最低阈值（建议 60% line coverage）

### Files
- `CMakeLists.txt`（新增 `Coverage` 配置）
- `.github/workflows/ci.yml`

---

## Track D: E2E 集成测试 [P1 — Week 2-3]

### Problem
Lua 脚本测试（`tests/scripts/`）未接入 CI，仅 C++ 单元测试自动运行。

### Steps

1. **CI 集成 Lua 测试**
   - 在 CMake 中添加 `lua_test` 目标
   - 运行时：启动引擎 → 执行 `tests/scripts/test_kag_e2e.lua` → 检查退出码
   - macOS/Linux 优先；Windows 等无头渲染就绪

2. **测试脚本标准化**
   - 每个 `test_*.lua` 以 `os.exit(0)` 成功 / `os.exit(1)` 失败
   - smoke test: 启动 → 显示标题 → 退出（5 秒超时）

### Files
- `tests/CMakeLists.txt`
- `.github/workflows/ci.yml`

---

## Track E: Live2D 集成 [P2 — Week 3-4]

### Scope
- Cubism SDK for Native（C++，跨平台）
- `src/Animation/Live2D/` 模块
- Lua API: `KAG.show_live2d(model, x, y)` + `KAG.set_live2d_param(param, value)`
- 异步加载 + JobSystem 更新

### Dependencies
- Track B（无头渲染）— Live2D 需要渲染上下文

---

## Track F: 物理引擎 [P2 — Week 4-5]

### Scope
- Box2D 3.0（C，跨平台，轻量）
- `src/Game/Physics/` 模块
- 服务于 3D 小游戏接口（Track G）
- Lua API: body/world/contact 包装

---

## Track G: 3D 小游戏运行时 [P2 — Week 5-6]

### Scope
- 激活 `IGamePlugin` 接口（已预留）
- bgfx 3D 渲染管线（已有能力）
- Lua API: `KAG.set_input_mode("game")` + `KAG.spawn_game(plugin_name)`
- 示例：简单 3D 场景（旋转立方体）

---

## Track H: AI IDE 辅助开发工具链 [P3]

### Scope
- KAG script generator（基于 Codex/Copilot/Cursor）
- 场景模板库 + AI 提示词优化
- 不嵌入引擎：通过 IDE 工具 + API 文档实现

---

## Priority Order

| Order | Track | Effort | Impact |
|---|---|---|---|
| 1 | A: CI 测试可见性 | 2d | 高 — CI 不再静默失败 |
| 2 | B: 无头渲染 | 3d | 高 — Windows 测试启用 |
| 3 | C: 覆盖率 | 1d | 中 — 质量可视化 |
| 4 | D: E2E 集成测试 | 2d | 高 — 端到端验证 |
| 5 | E: Live2D | 5d | 中 — 差异化功能 |
| 6 | F: 物理引擎 | 3d | 中 — 小游戏基础 |
| 7 | G: 3D 小游戏 | 5d | 中 — 交互升级 |
| 8 | H: AI IDE | ∞ | 低 — 长期愿景 |

**总估算:** ~21 工作日 → Beta release.
