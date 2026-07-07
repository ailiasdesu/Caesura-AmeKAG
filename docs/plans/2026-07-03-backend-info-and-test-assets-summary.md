# 2026-07-03 后端信息与测试资源同步改进总结

## 背景

本轮继续检查上一轮架构硬化后的残留问题，重点处理两个会影响调试可信度的点：

1. headless 模式实际使用 Null 后端，但 Lua 侧 `BackendFactory` 和 `config.lua` 日志仍可能显示静态配置名 `bgfx/soloud/sdl3`。
2. Lua 脚本资源只通过 `CaesuraTests` 的 post-build 事件复制，单独修改脚本后直接运行 CTest 可能读到旧的 `build/tests/Debug/scripts` 副本。

## 已完成改动

### 1. 让渲染后端名称成为 `IRenderDevice` Interface 的一部分

涉及文件：

- `src/render/api/IRenderDevice.h`
- `src/render/BgfxRenderDevice.h`
- `src/di/BackendRegistry.cpp`
- `tests/cpp/test_entry.cpp`

改动内容：

- 为 `IRenderDevice` 增加 `getBackendName()`。
- `BgfxRenderDevice` 通过已有 `BgfxDeviceCore` 返回真实 bgfx renderer 名称，未初始化时返回 `bgfx`。
- `NullRenderDevice` 返回 `NullRender`。
- `Engine.get_backend_info()` 不再硬编码 render 为 `bgfx`，而是通过渲染 Interface 查询真实 Adapter 名称。
- 新增 headless 回归测试，断言 Lua `Engine.get_backend_info()` 返回：
  - `render = NullRender`
  - `audio = NullAudio`
  - `platform = NullPlatform`

架构影响：

- `Engine.get_backend_info()` 的 Interface 更深，调用方不需要知道当前是 NullRender 还是 BgfxRenderDevice。
- 真实 Adapter 自己回答名称，减少 `BackendRegistry` 内部猜测，提升 locality。

### 2. 修正 Lua BackendFactory 与 config 日志

涉及文件：

- `scripts/backend_factory.lua`
- `scripts/config.lua`
- `tests/cpp/test_lua_manager.cpp`

改动内容：

- `BackendFactory.create()` 会优先读取 `Engine.get_backend_info()`。
- `backend.render("name")`、`backend.audio("name")`、`backend.platform("name")` 返回实际注册后端名，而不是静态配置名。
- `[BackendFactory] Created` 日志显示实际后端名。
- `[config] Backends ready` 日志改为通过 backend 的 `name` 命令输出。
- 新增两个 LuaManager 回归测试：
  - `Lua BackendFactory name commands report actual engine backend info`
  - `Lua config ready log reports actual engine backend info`

效果：

- headless CLI 日志现在能反映实际运行后端：
  - `render=NullRender`
  - `audio=NullAudio`
  - `platform=NullPlatform`
- 避免调试时被 `render=bgfx audio=soloud platform=sdl3` 误导。

### 3. CTest 增加测试资源同步 fixture

涉及文件：

- `tests/CMakeLists.txt`

改动内容：

- 新增 CTest 前置测试 `CaesuraSyncTestAssets`。
- 通过 `FIXTURES_SETUP caesura-test-assets` 在运行 doctest 和 headless smoke 前同步：
  - `scripts/` 到 `CaesuraTests` 输出目录。
  - `tests/audio/` 到 `CaesuraTests` 输出目录。
  - `scripts/` 到 `CaesuraAmeKAG` 输出目录。
- 所有 doctest 入口和 headless CLI smoke 增加 `FIXTURES_REQUIRED caesura-test-assets`。

效果：

- 直接运行 `ctest` 时，Lua 脚本测试不会读到过期脚本副本。
- CTest 清单从 7 个入口变为 8 个入口，其中 `CaesuraSyncTestAssets` 是资源同步入口。

### 4. headless CLI smoke 从退出码检查升级为行为检查

涉及文件：

- `tests/CMakeLists.txt`

改动内容：

- `CaesuraHeadlessCliSmoke` 增加 `PASS_REGULAR_EXPRESSION`：
  - 必须输出 `Backends ready: render=NullRender audio=NullAudio platform=NullPlatform`
- 增加 `FAIL_REGULAR_EXPRESSION`：
  - 禁止出现静态 ready 日志 `render=bgfx audio=soloud platform=sdl3`
  - 禁止出现真实 SDL3/bgfx/SoLoud 初始化日志

效果：

- 如果后续组合根再次在纯 headless 模式注入真实 GPU、窗口或音频后端，CTest 会直接失败。
- headless smoke 不再只是“进程退出码为 0”，而是验证关键运行语义。

## 验证结果

已执行并通过：

```powershell
cmake -S . -B build
cmake --build . --config Debug
ctest -C Debug --test-dir . -j 4 --output-on-failure
.\CaesuraTests.exe
git diff --check
```

关键结果：

- 完整 Debug 构建通过，退出码 0。
- CTest 结果：`100% tests passed, 0 tests failed out of 8`。
- doctest 结果：`416/416 test cases passed, 981/981 assertions passed`。
- `git diff --check` 无空白错误，仅提示 `tests/cpp/test_save_migration.cpp` 下次 Git touch 会从 CRLF 转 LF。

## 仍建议后续处理

1. 编码警告仍存在，主要来自 `src/steam/SteamBackend.h`、`src/steam/NullSteamBackend.h`、`src/archive/CRLManager.h`。建议单独做文件编码清理，避免和架构改动混在一起。
2. `IRenderDevice` 仍暴露 `bgfx::*Handle`，长期仍应去 bgfx 化，改为 Caesura 自有不透明句柄。
3. `BackendRegistry` 仍承担注册表、Null 后端、Lua backend selection、资源 handle generation 等多类职责。建议后续拆分 Module，提高 locality。
4. `main.cpp` 内脚本目录发现和 Lua package.path 设置重复较多，后续可抽成组合根内部 helper，减少运行模式分支重复。
