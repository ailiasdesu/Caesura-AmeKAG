# BackendRegistry 依赖说明文档

> 更新日期: 2026-08-23 | 公共接口: 33 | Registry 服务槽位: 24 | 模块: 16

## 核心原则

BackendRegistry 是引擎运行时后端的**单一服务定位器**。子系统通过它获得其他模块的接口指针（`I*`），而非直接 include 具体实现头文件。

RPC/Editor 是宿主入站传输适配器，不是引擎后端。它们由宿主持有并通过回调注入能力，不占 Registry 槽位，也不进入 `Caesura::Engine` 聚合目标。`main.cpp` 在 `--headless`/`--editor-stdio` 实例化 `RpcServer`，在 `--editor` 实例化 `EditorServer`（HTTP 编辑器，端口 9876）；两者不占 Registry 槽位。

**这不是"隐藏依赖"——这是显式解耦。** 每个模块的依赖关系通过 BackendRegistry 的 `get*()` 调用清晰可见，且强制走接口。

## 为什么不是"接口爆炸"

33 个公共接口（`src/*/api/I*.h`，api-stats 权威计数）对应 16 个模块，其中 24 个长生命周期引擎服务进入 Registry。这与"接口爆炸"（通常指每个类都有接口）有本质区别：

1. **接口对应子系统，不随代码膨胀。** 模块新增功能在接口内部扩展方法，不创建新接口。
2. **消费者只调用自己需要的。** `script` 模块调用 `getRenderDevice()`，不关心 `getCryptoEngine()`。
3. **接口是稳定的契约。** 实现可替换（SoLoud→FMod，bgfx→Vulkan），消费者不改一行代码。

## 依赖关系一览

每个模块的已知依赖（通过 BackendRegistry `get*()` 访问）：

| 模块 | 依赖的接口 | 用途 |
|------|-----------|------|
| **entry/Engine** | 24 个 Registry 服务 | 组合根，创建、注册并按逆序注销后端（含 `IMobileAdapter`、`IMeshRenderer`、`IDisplayService`、`ILifecycleService` 等较新槽位） |
| **script** | Render/Audio/Input/MiniGame/Storage/Resource/Debug/Steam/Job/Platform/Display 接口及 `ISandboxQuota` | Lua/KAG 绑定调用；Steam Binding 每次从 Registry 解析后端；`DevCore.get_display_metrics()` 经 `getDisplayService()` 查询显示度量（coupling 实测 script 跨 11 模块，见 `python scripts/count_coupling.py`） |
| **render** | `IRenderDevice`, `ITextureBudget`, `ISandboxQuota` | 粒子渲染、纹理预算与配额 |
| **render** | `IDebugManager` (宏) | 零开销日志 |
| **audio** | `ISandboxQuota` | 资源配额检查 |
| **resource** | `IJobSystem` | 异步图片解码 |
| **live2d** | `ITextureManager`, `IRenderDevice` | PNG 降级渲染 |
| **minigame** | `IInputRouter`；`IRenderDevice` 由组合根注入 | 输入焦点与 3D 渲染 |
| **rpc** | 无 Registry 依赖 | 宿主向所创建的传输适配器注入 Lua、帧捕获、归档或动画命令 |
| **storage** | `ICryptoEngine` | 存档加密 |
| **platform**（服务提供方，STEP10 新增槽位 #23） | 提供 `IDisplayService`（`src/platform/api/IDisplayService.h`；实现 `SDL3DisplayService` / `NullDisplayService`） | 统一显示度量查询（pixel/logical 尺寸、scaleFactor、DPI、orientation、safeArea）。构造点：组合根 `entry/createDisplayService()` 工厂（SDL3 可用→SDL3 实现，否则 Null），`Engine::init()` 调用 `setDisplayService()` 注册；消费方：Lua `DevCore.get_display_metrics()` |
| **platform**（服务提供方，STEP11 新增槽位 #24） | 提供 `ILifecycleService`（`src/platform/api/ILifecycleService.h`；实现 `LifecycleService`，platform 模块 header-only 中枢） | 统一应用生命周期事件流（`LifecycleEvent` 六事件：Pause/Resume/Background/Foreground/LowMemory/Terminate；消费方注册一次 `ILifecycleListener` 即全平台接入，无平台 ifdef）。构造点：组合根 `entry`——`Engine::initPlatformPhase()` 创建并持有 `unique_ptr`，先 `addListener(this)` 再 `setLifecycleService()` 注册；消费方：Engine 自身（映射 onPause/onResume 音频挂起恢复与 `IMobileAdapter.onLowMemory/onTerminate` → `_G.onLowMemory`/`_G.onTerminate`） |

**其余模块（archive, debug, di, input, job, platform, steam）不通过 Registry 访问其他模块服务。**（platform 行为该表中的服务提供方，自身不访问其他模块服务）

## 新增模块检查清单

添加新模块时：
- [ ] 创建 `src/<module>/api/I<Module>.h` 纯虚接口
- [ ] 判断它是引擎后端还是宿主入站适配器
- [ ] 引擎后端在 BackendRegistry 添加 `set*()` / `get*()`，并在 `Engine::init*Phase()` 注册
- [ ] 入站适配器由 `main.cpp` 持有，通过回调/命令队列注入，不添加 Registry 槽位
- [ ] 消费者只 include 对方 `api/I*.h`，不直接 include 实现头文件
- [ ] 确认不引入循环依赖（A→B→A）

## 循环依赖防火墙

BackendRegistry 的接口指针模式天然防止循环依赖：
- 模块 A 调用 `BackendRegistry::instance().getB()` 获取 `IB*`，不需要知道 B 的具体类型
- 编译期只依赖 `IB.h`（接口），运行时通过指针间接调用
- 如果未来 A 和 B 互相需要，各自通过 BackendRegistry 获取对方的接口，不会形成编译期循环
