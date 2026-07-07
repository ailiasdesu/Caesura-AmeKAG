# 2026-07-03 继续架构硬化总结

## 背景

本轮在前序 headless、测试入口拆分、BackendFactory 真实后端名改进之后继续收尾，目标是继续处理能低风险落地的架构和质量问题。

## 已完成改进

### 1. Lua backend 统一代理修复

涉及文件：
- `scripts/backend.lua`
- `tests/cpp/test_lua_manager.cpp`

改动：
- 修复 `audio_play`、`audio_stop`、`audio_is_playing` 中误用未定义局部变量 `b` 的问题，统一使用 `be`。
- 修复 `Backend.set_resolution()` 有 `_CAESURA_BACKEND` 时仍绕过统一代理、直接调用 `DevCore` 的问题。
- 新增回归测试：
  - `Lua backend audio helpers prefer unified backend proxy`
  - `Lua backend platform helpers prefer unified backend proxy`

架构影响：
- Lua helper 的 Adapter 路由更一致，调用方通过统一 backend proxy 获得 render/audio/platform 行为。
- 减少脚本层绕过统一 Backend seam 的风险。

### 2. 源文件 UTF-8 质量门

涉及文件：
- `tests/cpp/test_source_encoding.cpp`
- `tests/CMakeLists.txt`
- `src/archive/CARCReader.cpp`
- `src/archive/CRLManager.cpp`
- `src/archive/CRLManager.h`
- `src/minigame/NullMiniGameBackend.cpp`
- `src/resource/XP3Archive.h`
- `src/script/bindings/SteamBinding.cpp`
- `src/steam/SteamBackend.h`
- `src/steam/NullSteamBackend.h`
- `tests/cpp/test_script_boundary.cpp`

改动：
- 新增 `C++ source files are valid UTF-8` 测试，扫描 `src/` 和 `tests/cpp/` 的 C/C++ 源文件。
- 清理无效 UTF-8 装饰字符、箭头、乱码注释，避免 MSVC `/utf-8` 下出现 `C4828`。

架构影响：
- 把“源码编码有效”从人工检查变成自动测试面，后续 CI/本地测试能尽早发现编码污染。

### 3. 编译警告清理

涉及文件：
- `src/entry/Engine.cpp`
- `src/resource/XP3Archive.cpp`

改动：
- `captureFrameBase64()` 将 `std::streamsize` 文件大小先检查并转换为 `size_t`，避免有符号/无符号比较警告。
- `XP3Archive::pack()` 的 `fseek` 偏移显式转换为 `long`，消除 `size_t` 到 `long` 的转换警告。

### 4. DI 对具体渲染 Adapter 的依赖收敛

涉及文件：
- `src/render/api/IRenderDevice.h`
- `src/render/BgfxRenderDevice.h`
- `src/di/BackendRegistry.cpp`
- `tests/cpp/test_source_encoding.cpp`

改动：
- 在 `IRenderDevice` 上增加默认 no-op `setPreferredBackend(const char*)`。
- `BgfxRenderDevice` 通过接口 override 承接 bgfx 子后端选择。
- `BackendRegistry.cpp` 移除 `../render/BgfxRenderDevice.h` include 和 `dynamic_cast<BgfxRenderDevice*>`。
- 新增 `BackendRegistry implementation depends only on render interface` 静态回归测试。

架构影响：
- DI 模块不再为了 Lua `Engine.select_render_backend("bgfx", subBackend)` 识别具体 render Adapter。
- `BackendRegistry` 对 render 模块的 include 数量下降，边界更接近 AGENTS 中“DI 只依赖 Interface”的约束。

## 当前观测

`python scripts/count_coupling.py` 当前输出：
- `di`：12 个跨模块，19 条 include，总体在目标内。
- `script`：10 个跨模块，22 条 include，在目标内。
- `entry`：15 个跨模块，39 条 include，略高于目标 14；但 `entry` 是组合根，继续下降需要更大规模的组合根拆分或启动路径收敛。

## 仍建议后续处理

1. `IRenderDevice` 仍暴露 `bgfx::TextureHandle`、`bgfx::UniformHandle`、`bgfx::ProgramHandle`，长期应改为 Caesura 自有不透明句柄。
2. `main.cpp` 和 `Engine.cpp` 作为组合根仍有较多具体 include 和启动分支，后续可抽取启动配置解析与脚本路径配置 helper。
3. 旧总结文件 `docs/plans/2026-07-03-backend-info-and-test-assets-summary.md` 在当前环境显示乱码，建议后续用本文件和后续干净 UTF-8 文档替代它。

## 验证记录

本轮已执行过局部验证：
- 新增 Lua platform helper 测试先失败，修复后通过。
- 新增 DI 边界测试先失败，修复后通过。
- 新增 UTF-8 测试先失败，清理后通过。
- `cmake --build . --config Debug` 已通过。

最终完整验证结果见本轮最终回复。
