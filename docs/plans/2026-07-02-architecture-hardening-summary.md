# 2026-07-02 架构硬化执行总结

## 背景

本次改进来自对 Caesura (AmeKAG) 代码库的架构扫描和测试入口审计。扫描发现两个会直接影响后续开发可信度的问题：

1. CTest 入口不可用：测试资源被复制到 `build/tests/Debug/`，但 `ctest --test-dir build/tests` 运行时工作目录不是可执行文件目录，导致脚本和音频资源查找失败。
2. `EngineConfig` 默认指针均为 `nullptr`，`Engine` 在 headless 初始化路径仍会访问依赖 GPU 或具体后端的对象，默认 headless 初始化存在崩溃风险。
3. CTest 只有一个大入口 `CaesuraUnitTests`，CI 无法直接定位失败模块。
4. 多个 SaveManager 测试进程共享固定 `test_saves/` 等目录，并发运行会互相删除或覆盖文件，导致 JSON parse、文件锁和计数污染。

本次执行优先处理会影响 CI、测试可靠性和组合根稳定性的基础问题，不扩展到大规模接口重构。

## 已完成改动

### 1. 修复 CTest 测试入口

涉及文件：

- `CMakeLists.txt`
- `tests/CMakeLists.txt`

改动内容：

- 在顶层 `CMakeLists.txt` 启用 `enable_testing()`，使 `ctest --test-dir build` 可以发现测试。
- 将 `CaesuraUnitTests` 的命令改为 `$<TARGET_FILE:CaesuraTests>`，避免依赖 CTest 当前目录解析可执行文件。
- 为 `CaesuraUnitTests` 设置 `WORKING_DIRECTORY "$<TARGET_FILE_DIR:CaesuraTests>"`，确保运行时工作目录为 `build/tests/Debug/`。
- 合并测试资源复制命令的 `COMMENT`，消除同一 `add_custom_command` 中多个 `COMMENT` 造成的 CMake 开发者警告。

效果：

- `ctest -C Debug --test-dir tests --output-on-failure` 可直接通过。
- `ctest -C Debug --test-dir . --output-on-failure` 也可发现并运行测试。

### 2. 加固默认 headless Engine 初始化

涉及文件：

- `src/entry/Engine.cpp`
- `src/render/TextureManager.h`
- `src/render/TextureManager.cpp`
- `tests/cpp/test_entry.cpp`

新增测试：

- `Entry: Engine headless init uses safe default backends`

该测试先在旧实现下复现 `SIGSEGV`，再验证修复后的行为：

- `EngineConfig cfg; cfg.headless = true;`
- 不注入具体 platform/render/audio/miniGame 后端。
- `engine.init()` 必须成功。
- `BackendRegistry` 中 render/audio/platform/miniGame 后端均非空。

生产代码改动：

- `Engine::init()` 在 headless 且未注入音频后端时创建 `NullAudioBackend`。
- `Engine::init()` 在 headless 且未注入小游戏后端时创建 `NullMiniGameBackend`。
- `initPlatformPhase()` 在 headless 路径注册 NullRender/NullPlatform 后，也注册 NullAudio。
- GPU 模式下如果 platform/render/audio 后端缺失，`initPlatformPhase()` 明确返回 `false`，避免空指针崩溃。
- Lua registry 中的 Render/Audio/Platform 指针改为来自 `BackendRegistry`，保证 headless 下拿到 Null 后端而不是 `Engine` 内部空指针。
- MiniGame 后端注入 `BackendRegistry::getRenderDevice()`，使 NullRender 和真实 Render 路径统一。
- `Engine::shutdown()` 清理运行期注册到 `BackendRegistry` 的服务指针，避免单例状态污染后续测试。
- `Engine::shutdown()` 调用 `TextureManager::shutdown()`，释放资源管理器注册和状态。

### 3. 让 TextureManager 支持无 GPU 初始化

涉及文件：

- `src/render/TextureManager.h`
- `src/render/TextureManager.cpp`

改动内容：

- 保留接口方法 `initialize()`，新增实现类内部重载 `initialize(bool gpuAvailable)`，不修改 `ITextureManager` 对外接口。
- headless 初始化时传入 `false`，`TextureManager` 不再创建 bgfx placeholder 纹理。
- 无 GPU 模式下：
  - `getPlaceholderTexture()` 返回 `0`。
  - `loadTexture()`、`loadTextureFromMemory()`、`loadTextureFromRGBA()`、`createSolidTexture()` 返回失败，不触碰 bgfx。
  - `setDevMode()` 只记录状态，延迟到 GPU 可用时再应用 placeholder。
- 设备丢失时标记 GPU 不可用，设备恢复时重新标记可用并重建 placeholder。

这避免了 headless 测试在 bgfx 未初始化时调用 `bgfx::copy/createTexture2D` 造成崩溃。

### 4. 加固 Render Lua 绑定的后端解析

涉及文件：

- `src/script/bindings/RenderBinding.cpp`

全套 CTest 跑通后暴露出一个既有问题：部分脚本绑定测试只注册了 Lua 模块，但 Lua registry 中没有 `Caesura.TextureManager`，`Render.destroy_texture()` 会直接解引用空指针。

改动内容：

- RenderBinding 解析 TextureManager/RenderDevice/VideoPlayer/AsyncLoader 时，先读 Lua registry，再回退到 `BackendRegistry`。
- 对仍然为空的后端返回 Lua 层失败值，而不是崩溃：
  - `load_texture` 返回 `nil, "TextureManager not available"`。
  - `destroy_texture` 返回 `false`。
  - `create_solid_texture` 返回 `nil, "TextureManager not available"`。
  - `load_texture_async` 返回 `0`。
  - `cancel_async_loads` 在无 AsyncLoader 时仍返回成功。
  - `is_valid_handle(TEXTURE, id)` 在无 TextureManager 时返回 `false`。

### 5. 拆分模块级 CTest 入口

涉及文件：

- `tests/CMakeLists.txt`

改动内容：

- 新增 `add_caesura_doctest()` CMake helper，统一设置 `$<TARGET_FILE:CaesuraTests>` 和 `WORKING_DIRECTORY "$<TARGET_FILE_DIR:CaesuraTests>"`。
- 保留完整回归入口 `CaesuraUnitTests`。
- 新增 5 个模块级 CTest 入口：
  - `CaesuraEntryTests`
  - `CaesuraStorageTests`
  - `CaesuraScriptTests`
  - `CaesuraRenderResourceTests`
  - `CaesuraAudioTests`
- 模块级入口使用 doctest `--source-file` 过滤，不增加额外测试二进制，也不重复维护源文件编译列表。

效果：

- `ctest -N -C Debug --test-dir .` 从 1 个测试入口变为 6 个。
- CI 仍能运行完整 `CaesuraUnitTests`，同时可以在模块级入口上更快定位失败区域。

### 6. 增加 CTest 标签与资源调度属性

涉及文件：

- `tests/CMakeLists.txt`

改动内容：

- 扩展 `add_caesura_doctest()` helper，支持：
  - `LABELS`
  - `RUN_SERIAL`
  - `RESOURCE_LOCK`
  - `ARGS`
- 为入口添加标签：
  - `CaesuraUnitTests`: `full;regression`
  - `CaesuraEntryTests`: `entry;lifecycle`
  - `CaesuraStorageTests`: `storage;filesystem`
  - `CaesuraScriptTests`: `script;lua`
  - `CaesuraRenderResourceTests`: `render;resource`
  - `CaesuraAudioTests`: `audio`
- 为共享全局单例的入口添加 `RESOURCE_LOCK global-singletons`。
- 为存储入口添加 `RESOURCE_LOCK save-files`。
- 为音频入口添加 `RUN_SERIAL` 和 `RESOURCE_LOCK audio-device`。
- 为完整回归入口添加 `RUN_SERIAL` 和 `RESOURCE_LOCK global-singletons`。

效果：

- CI 可以用 `ctest -L storage`、`ctest -L audio`、`ctest -L "script|lua"` 选择模块集合。
- 当 CI 并行运行 CTest 时，音频设备、完整回归和全局单例相关入口有明确调度约束。
- 测试调度策略集中在 CMake helper 调用处，避免 CI 脚本额外硬编码模块知识。

### 7. 隔离文件系统测试目录

涉及文件：

- `tests/cpp/TestPaths.h`
- `tests/cpp/test_system.cpp`
- `tests/cpp/test_save_roundtrip.cpp`
- `tests/cpp/test_save_migration.cpp`
- `tests/cpp/test_storage.cpp`

改动内容：

- 新增测试辅助模块 `TestPaths`：
  - `uniqueTempDir(name)` 基于系统临时目录、进程 ID 和原子计数生成唯一目录。
  - `ScopedTempDir` 负责创建和自动清理临时目录。
  - `withTrailingSeparator()` 保持 SaveManager 现有路径契约。
- 将 SaveManager 测试中的固定目录 `test_saves/`、`test_roundtrip/`、`test_mig/`、`test_mig2/`、`test_parse_err/` 和 `caesura_test_storage/` 改为进程唯一临时目录。

效果：

- 并发运行两个 `CaesuraTests.exe --test-case=SaveManager::*` 进程时，不再互相删除或覆盖保存文件。
- 存储测试的磁盘状态更具 locality：目录创建、使用、清理由单个 `ScopedTempDir` 管理。

### 8. 让纯 headless CLI 使用 Null 后端组合

涉及文件：

- `src/main.cpp`
- `tests/CMakeLists.txt`

新增回归入口：

- `CaesuraHeadlessCliSmoke`

问题复现：

- 旧 `main.cpp` 即使在 `--headless` 下，也会创建 `SDL3PlatformBackend`、`BgfxRenderDevice`、`SoLoudAudioEngine`、`BgfxMiniGameBackend` 并传入 `EngineConfig`。
- 因为 `EngineConfig.platform` 非空，`Engine::initPlatformPhase()` 会按 GPU 模式初始化，导致 headless 启动仍创建窗口/GPU/音频设备。
- CTest smoke 在旧实现下复现为 `SEGFAULT`，日志显示 SDL3、bgfx、SoLoud 均被初始化。

改动内容：

- `main.cpp` 在 `headless && !editorMode` 时只注入：
  - `NullAudioBackend`
  - `NullMiniGameBackend`
- 纯 headless 不再注入 platform/render，让 `Engine` 走已有的 `BackendRegistry::registerNullBackends()` 路径，注册 `NullRenderDevice` 和 `NullPlatformBackend`。
- editor mode 仍保留 GPU 后端，因为它明确需要隐藏窗口和帧捕获。
- 新增 `CaesuraHeadlessCliSmoke`，运行 `$<TARGET_FILE:CaesuraAmeKAG> --headless`，并标记 `entry;headless;cli`、`RUN_SERIAL`、`RESOURCE_LOCK global-singletons`。

效果：

- `--headless` 从初始化 SDL3/bgfx/SoLoud 的路径变为 NullRender/NullPlatform/NullAudio/NullMiniGame 路径。
- `CaesuraHeadlessCliSmoke` 从旧实现下的 `SEGFAULT` 变为通过。
- 手动重跑 `CaesuraAmeKAG.exe --headless < NUL` 后退出码为 `0`，输出包含 Null 后端初始化，不再包含真实 SDL3/bgfx/SoLoud 初始化日志。

## 架构影响

### 正向影响

- 测试入口从“只能手动进入 `build/tests/Debug` 运行”升级为标准 CTest 可运行。
- headless 模式的默认初始化语义更清晰：没有真实平台/GPU/音频设备时使用 Null 后端。
- Lua 绑定不再强依赖 Engine 已经把所有指针写入 Lua registry，独立绑定测试和非完整初始化场景更稳。
- `Engine::shutdown()` 对 `BackendRegistry` 的运行期注册做清理，减少单例残留导致的顺序依赖。
- `TextureManager` 明确区分 GPU 可用和不可用状态，降低无窗口/CI/headless 场景的 bgfx 崩溃风险。
- CTest 从单一大入口扩展为完整入口加模块级入口，测试报告有更好的失败定位。
- CTest 入口具备标签和资源锁，CI 可以按模块选择测试，并减少并行调度下的共享资源冲突。
- 存储测试不再共享固定工作目录，支持并发执行同一类测试进程。
- 纯 headless CLI 不再由组合根注入真实平台、渲染和音频后端，启动成本和 GPU/窗口依赖显著降低。

### 保持不变的约束

- `Engine.cpp` 仍作为组合根，可以 include 具体 Null 后端实现。
- `EngineConfig` 指针字段默认仍为 `nullptr`，测试仍覆盖该契约。
- `ITextureManager` 接口未变化；新增 `initialize(bool)` 是 `TextureManager` 具体实现能力，不对模块接口扩散。
- 正常 `main.cpp` 启动路径仍显式创建真实 SDL3/bgfx/SoLoud 等后端。

## 验证结果

已执行并通过：

```powershell
cmake --build . --config Debug --target CaesuraTests
```

```powershell
cmake --build . --config Debug
```

结果：完整 Debug 构建通过，退出码 0。

```powershell
.\CaesuraTests.exe --test-case="Entry: Engine headless init uses safe default backends"
```

结果：`1 passed, 0 failed`，该用例包含 5 个断言。

```powershell
.\CaesuraTests.exe --test-case="E3 Bindings: Render destroy_texture invalid id does not crash"
```

结果：`1 passed, 0 failed`。

```powershell
.\CaesuraTests.exe --test-case="BackendRegistry::setMiniGameBackend"
```

结果：`1 passed, 0 failed`。

```powershell
ctest -C Debug --test-dir tests --output-on-failure
```

结果：`100% tests passed, 0 tests failed out of 1`。

```powershell
ctest -C Debug --test-dir . --output-on-failure
```

结果：`100% tests passed, 0 tests failed out of 6`。

```powershell
ctest -N -C Debug --test-dir .
```

结果：CTest 发现 6 个入口：`CaesuraUnitTests`、`CaesuraEntryTests`、`CaesuraStorageTests`、`CaesuraScriptTests`、`CaesuraRenderResourceTests`、`CaesuraAudioTests`。

```powershell
ctest -N -C Debug --test-dir . -L storage
ctest -N -C Debug --test-dir . -L audio
ctest -N -C Debug --test-dir . -L "script|lua"
```

结果：分别发现 `CaesuraStorageTests`、`CaesuraAudioTests`、`CaesuraScriptTests`。

```powershell
rg -n "LABELS|RUN_SERIAL|RESOURCE_LOCK" build\tests\CTestTestfile.cmake
```

结果：生成的 CTest 文件包含 `LABELS`、`RESOURCE_LOCK`，并为 `CaesuraUnitTests` 与 `CaesuraAudioTests` 生成 `RUN_SERIAL TRUE`。

```powershell
ctest -C Debug --test-dir . -R CaesuraHeadlessCliSmoke --output-on-failure
```

结果：`100% tests passed, 0 tests failed out of 1`。该用例在旧实现下复现 `SEGFAULT`。

```powershell
CaesuraAmeKAG.exe --headless < NUL
```

结果：退出码 `0`，输出包含 `Using NullRenderDevice`、`Using NullPlatformBackend`、`Using NullAudioBackend`，不再出现真实 SDL3/bgfx/SoLoud 初始化日志。

```powershell
$p1 = Start-Process .\CaesuraTests.exe '--test-case=SaveManager::*' ...
$p2 = Start-Process .\CaesuraTests.exe '--test-case=SaveManager::*' ...
```

结果：两个并发进程均为 `13/13 test cases passed, 34/34 assertions passed`。

```powershell
.\CaesuraTests.exe
```

结果：`413/413 test cases passed, 972/972 assertions passed`。

说明：SaveManager 测试已改为进程唯一临时目录，原先的并发目录竞争已修复。完整测试二进制仍建议在 CI 中保留串行完整入口，模块级入口用于定位与分组执行。

## 已知残留问题

本次未处理但建议后续继续改进：

1. `IRenderDevice` 接口仍暴露 `bgfx::TextureHandle`、`bgfx::UniformHandle`、`bgfx::ProgramHandle`，违反“接口不泄漏实现细节”的长期目标。建议引入不透明句柄或渲染模块自有类型，分阶段替换。
2. `BackendRegistry` 同时承担服务定位、Null 后端注册、资源句柄 generation、Lua backend factory 等职责，接口深度不足。建议拆分运行时服务注册、资源句柄管理和 Lua backend 选择。
3. `RenderBinding`、`KAGBinding` 仍大量直接读 Lua registry 字符串 key。建议集中成一个脚本后端上下文解析器，减少 key 漂移。
4. Windows 构建仍存在若干既有编码警告，例如部分头文件含无效 UTF-8 字节。建议单独做编码清理，避免 MSVC `/utf-8` 下产生噪音。
5. 现有 CTest 已有模块级入口、标签和资源锁，但仍共享同一个大测试二进制和全局单例。后续可继续按模块拆出独立二进制，或者把测试全局状态封装成显式 fixture。
6. `scripts/config.lua` 在 headless 下仍按静态配置打印 `render=bgfx audio=soloud platform=sdl3`，虽然实际 C++ adapter 已是 Null 后端。建议后续让 Lua `BackendFactory` 或 `Engine.get_backend_info()` 反映实际注册后端，避免日志误导。

## 后续建议优先级

### P1：继续收敛测试隔离

当前已经有模块级 CTest 入口、标签与资源锁，但它们仍复用同一个测试二进制。建议下一步继续收敛测试隔离：

- 长期可按模块拆成更小的测试目标，但应先评估链接成本，避免让构建时间明显膨胀。
- 为全局单例提供测试 fixture/reset seam，减少对 `RESOURCE_LOCK global-singletons` 的依赖。
- 将 SaveManager 的加密 key、save provider 和目录状态纳入显式 reset 流程，进一步降低跨用例顺序依赖。

### P1：接口去 bgfx 化

优先从 `IRenderDevice` 返回/接收的 bgfx handle 开始替换：

- `bgfx::TextureHandle` -> `TextureHandleId` 或 `RenderTextureHandle`
- `bgfx::UniformHandle` / `ProgramHandle` -> 渲染模块内部查询接口或不透明 ID

目标是让脚本、资源、小游戏模块只依赖 Caesura 自有类型。

### P2：BackendRegistry 职责拆分

建议拆分为：

- `BackendRegistry`：只负责后端接口注册和读取。
- `ResourceHandleRegistry`：负责 `GenerationTracker` 和 handle 生命周期。
- `BackendSelectionBinding`：负责 Lua `Engine.select_*_backend`。
- `NullBackendRegistry` 或组合根 factory：负责 Null 后端创建与注册。

### P2：组合根按运行模式创建后端

`main.cpp` 可在 `--headless` 时直接创建 NullAudio/NullMiniGame，甚至不创建 bgfx render/platform 后端。这样运行模式与后端选择更一致，也减少 Engine 内部 fallback 的负担。
