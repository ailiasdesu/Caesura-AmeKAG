# 2026-07-16 模块静态库架构迁移总结

## 决策

Caesura 采用“内部独立静态库、最终统一链接”的混合架构：

- 15 个子系统与 `entry` 组合根形成 16 个内部静态库；
- 15 个子系统各有 API-only `INTERFACE` 目标；
- `Caesura::Engine` 只聚合目标，不重复编译生产源码；
- 最终仍交付单一 `CaesuraAmeKAG` 可执行文件，不引入 DLL ABI 和部署负担。

该结构保留统一编译交付的简单性，同时让 CMake 目标图、测试复用和耦合门禁能够表达真实模块边界。

## 当前完成度

| 层次 | 完成度 | 判断依据 |
|---|---:|---|
| 架构迁移 | 96% | 模块/API 目标、组合根所有权、遗留单例收口、Lua hook 复用、非阻塞协程调试状态机、生产库复用、生命周期回滚与源码门禁已落地 |
| 核心视觉小说能力 | 62% | Headless、KAG、存档、音频等代码路径较完整；真实 GPU、字体资产和交互全流程仍有缺口 |
| 跨平台发布就绪 | 30% | 有三平台 CI 构建，但缺真实 GPU 像素验证、可选 SDK 验证和非 Windows 发布包 |

以上比例区分“代码存在”“单元测试覆盖”和“可发布能力”。接口存在或 Null 后端通过测试，不等于真实后端已完成。

## 已完成迁移

### 构建拓扑

- 新增 `cmake/CaesuraModules.cmake`，集中定义模块目标与依赖方向。
- 正式程序只编译 `main.cpp`，通过 `Caesura::Engine` 链接完整引擎。
- `CaesuraTests` 链接生产模块库，不再维护第二套生产源码清单。
- `carc_pack` 复用 `Caesura::Archive`，不再重复编译 CARC/Ed25519 实现。
- MiniGame 内嵌着色器迁移到 `src/minigame`，SaveBinding 迁移到 `src/script/bindings`。
- Live2D 按平台只选择对应 Cubism renderer 源码；Steam/Live2D 特性宏限制在实现模块与组合根内部。

### 模块边界

- `BackendRegistry` 只依赖各模块 `api/I*.h` 接口。
- Script 的 Save、VFX、Render 绑定改为经接口或 Lua registry 访问后端。
- Steam Binding 移除全局后端指针，改为每次经 Registry 解析 `ISteamBackend`；Registry 现有 20 个服务槽位。
- RPC 的 CARC writer 改为由组合根注入工厂。
- TextureManager 经 `ITextureBudget` 获取预算，不再访问具体单例。
- Resource generation tracking 抽为 `IResourceGenerationTracker`。
- 跨模块具体头 include 与耦合阈值由 `scripts/count_coupling.py --ci` 检查。

### 所有权与生命周期

- Engine 以 `unique_ptr` 持有 platform、render、audio、texture、layer、sandbox、job、asset、async、save、crypto、particle、Steam、generation tracker 等服务。
- `EngineConfig` 为 move-only 所有权包；移动后源配置指针清空，未被 Engine 消费时也会释放已注入对象。
- 每个 Engine 实例只允许一次 `init()`；必需阶段失败时立即幂等回滚。
- 公开服务访问器在成功初始化前抛出 `std::logic_error`。
- 资源关闭顺序固定为 AsyncLoader 排空主线程回调、AssetManager、JobSystem、Registry 注销；成员声明顺序也保证异常析构时按 Async、Asset、Job 逆序释放。
- HotReload 由每个 Engine 以 `unique_ptr` 独占，在 Lua VM 关闭前解绑；连续 Engine 实例不再共享 Lua 指针。
- `DebugProtocol` 必须通过构造函数注入所属 `HotReload&`；活动实例经私有 Lua registry 定位，传输线程获得的 command sink 只持有 mailbox 弱引用，shutdown 后自动拒绝命令，不暴露协议或 Lua 裸指针。
- FreeType 生命周期折叠进 `TextRenderer::TTFState`：按需初始化，严格按 face、library 顺序 RAII 释放；Engine 不再管理字体库。
- bgfx 首次后端初始化失败后不再错误地二次 shutdown；frame API 在未初始化/恢复失败状态安全 no-op。
- TextureManager、TextureBudget、JobSystem、AsyncLoader、AssetManager、LuaManager、SaveManager、CryptoEngine、HotReload 与 DebugProtocol 已移除遗留单例 API。
- SaveManager 清除 AES key 时执行安全擦除，AssetManager 关闭时释放全部 provider。
- 源码约束测试防止已迁移的运行时服务重新暴露 `instance()`。

### 本轮额外修复

- 修复 SDL mouse wheel 处理被嵌在 button/motion 分支内而永远不可达的问题。
- F5/F6 keydown 现在实际调用 quicksave/quickload，并过滤键盘自动重复。
- 为 `thumbnail_quality` 与 `thumbnail_format` 增加持久化默认值。
- Headless CLI smoke 增加超时，避免交互式测试环境永久等待 stdin。
- README、C++ API、Live2D 指南和能力矩阵更新为 28 个接口、42 项能力及真实 editor/Live2D 契约。
- 删除 Live2D/MiniGame 中从未接入生产路径的 concrete-global Lua bridge；Live2D 模块同时移除未声明的 Lua 依赖，并用源码门禁防止旧全局指针回归。
- 删除浅层 `FreeTypeContext` 全局模块，修复重复字体加载覆盖旧 `FT_Face` 时的泄漏，并将字体失败降级限制在 `TextRenderer::loadTTF()`。
- 将 HotReload 收口为 Engine 显式所有权，并用两个连续 Engine 的强制重载测试验证实例隔离。
- `DebugProtocol` 使用复合 hook 转发原 Lua hook；仅在自身 hook 仍占用主状态时恢复原 hook，外部已替换的 hook 不会被覆盖；既存协程在协议销毁后仍继续转发 LuaManager 指令预算 hook。
- `DebugProtocol` 以 `Running -> Paused -> ResumePending` 状态机替代 hook 内等待：可 yield 的 coroutine 在线 hook 中立即让出，命令携带 `pauseId` 经线程安全 mailbox 投递并在 Lua owner thread pump，恢复由宿主显式执行；跨暂停旧命令会被拒绝。
- 暂停 coroutine 由 Lua registry 强引用跨 GC 保活；不可 yield 的命中只记录位置与计数并继续执行，step into/over/out、并发 producer 关闭和暂停后预算恢复均有测试覆盖。
- 全局检查改用 registry 全局表的 raw lookup，避免 `_G.__index` 在状态锁内重入 Lua；非 owner 线程 shutdown 会被拒绝，避免跨线程操作 Lua hook。

## 已知边界

1. `RpcServer` 与 `EditorServer` 是宿主持有的入站适配器。当前 `main.cpp` 只实例化 stdio `RpcServer`，其 `run` / `eval` 仍直接执行不可 yield 的 Lua 主状态，也没有 debugger command bridge；HTTP worker 仍不得直接访问 VM，接入前必须完成 owner-thread dispatcher。
2. `DebugProtocol` 已完成非阻塞协程暂停/恢复、Lua hook 共存和 transport-safe command sink，但仍未接入 Engine、RPC、KAG scheduler 或编辑器生产路径。它只会暂停挂载后创建的可 yield coroutine；现有 `kag_runner.lua` 会自行批量 resume，生产桥接必须统一仲裁恢复并处理 Lua 错误/结果栈，还需定义绝对/相对路径与 Windows 大小写一致的 canonical source-id。`init()` / `shutdown()` 也必须在 owner thread 且 Lua 停止执行时调用。`CompositeShaderCache`、`BackendRegistry` 与日志宏使用的 `DebugManager` 是设计保留单例。
3. `CaesuraBuildOptions` 仍传播整个 `src/` 与第三方 include 根；编译器不能单独阻止具体头依赖，当前依靠 CI 耦合脚本约束。
4. OpenGL/Metal 的完整 shader 产物和真实 GPU 像素 smoke 尚未完成；现有多数渲染测试不创建真实 GPU 上下文。
5. Live2D Cubism 与 Steamworks 默认关闭，当前主测试覆盖 Null 后端；macOS Metal Live2D 路径仍是 stub。
6. MiniGame 场景加载、存档缩略图、RPC 日志和字体切换仍有占位实现；`mini_game` 与 `live2d` 专用 Lua 表尚未在 Script 模块按接口重新接入。
7. 仓库未包含可分发 CJK 字体，因此 FreeType 初始化成功不代表发布包可直接显示完整 CJK 文本。
8. TextureManager 的显存预算执行仍有缺口；通用 `JobSystem::waitIdle()` 也不保证执行最后入队的主线程完成回调，当前由 AsyncLoader 在 shutdown 中补充排空。

## 下一批优先级

1. 增加主线程命令队列，再将 HTTP Editor 作为可选宿主适配器接入；不要把传输层放入 BackendRegistry。
2. 在 Script 模块实现基于 `IAnimationBackend` / `IMiniGameBackend` 的专用绑定，明确 MiniGame Lua 栈与碰撞回调生命周期，再恢复对应 demo。
3. 由 `main.cpp` 以可选 RAII 对象持有 `DebugProtocol`，增加 scheduler-aware resume adapter，让 `RpcServer` 的 `run` / `eval` 与 KAG `on_click` 走同一 owner-thread 可 yield 恢复路径，并通过通用 DTO/callback 接入命令；不要加入 `BackendRegistry`。
4. 为 D3D11、OpenGL、Metal/Vulkan 生成同能力 shader，并增加像素级 GPU smoke。
5. 完成字体资产策略、真实存档缩略图与默认加密密钥策略。
6. 验证 Cubism、Steam、FFmpeg 的真实 SDK/媒体路径和三平台发布包。

## 最终验证

本轮使用全新 `build-architecture-verify/` 目录验证，未复用旧测试二进制：

- CMake 配置成功（Windows/MSVC，`CAESURA_LIVE2D=OFF`）。
- Debug 全量构建成功；16 个内部模块库、`CaesuraAmeKAG`、`CaesuraTests` 与 `carc_pack` 均完成链接。
- CTest `8/8` 通过，0 失败。
- doctest `480/480` 个测试用例、`2066/2066` 个断言通过，0 失败、0 跳过。
- `python scripts/count_coupling.py --ci` 通过，所有模块均在阈值内且无 API 边界违规。
- 工作区与暂存区 `git diff --check` 均为 0 错误，仅有既存行尾转换提示。

新鲜目录验证同时发现测试产物没有复制 `SDL3` 运行库，Windows 会以
`0xc0000135` 退出。现已为主程序和测试目标使用 SDL target 的真实文件名执行
构建后复制，消除对旧构建目录残留 DLL 的依赖。
