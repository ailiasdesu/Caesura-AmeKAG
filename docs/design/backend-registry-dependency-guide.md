# BackendRegistry 依赖说明文档

> 更新日期: 2026-07-18 | 公共接口: 28 | Registry 服务槽位: 20 | 模块: 16

## 核心原则

BackendRegistry 是引擎运行时后端的**单一服务定位器**。子系统通过它获得其他模块的接口指针（`I*`），而非直接 include 具体实现头文件。

RPC/Editor 是宿主入站传输适配器，不是引擎后端。它们由宿主持有并通过回调注入能力，不占 Registry 槽位，也不进入 `Caesura::Engine` 聚合目标。当前 `main.cpp` 只实例化 `RpcServer`，`EditorServer` 尚未接入。

**这不是"隐藏依赖"——这是显式解耦。** 每个模块的依赖关系通过 BackendRegistry 的 `get*()` 调用清晰可见，且强制走接口。

## 为什么不是"接口爆炸"

28 个公共接口对应 16 个模块，其中 20 个长生命周期引擎服务进入 Registry。这与"接口爆炸"（通常指每个类都有接口）有本质区别：

1. **接口对应子系统，不随代码膨胀。** 模块新增功能在接口内部扩展方法，不创建新接口。
2. **消费者只调用自己需要的。** `script` 模块调用 `getRenderDevice()`，不关心 `getCryptoEngine()`。
3. **接口是稳定的契约。** 实现可替换（SoLoud→FMod，bgfx→Vulkan），消费者不改一行代码。

## 依赖关系一览

每个模块的已知依赖（通过 BackendRegistry `get*()` 访问）：

| 模块 | 依赖的接口 | 用途 |
|------|-----------|------|
| **entry/Engine** | 20 个 Registry 服务 | 组合根，创建、注册并按逆序注销后端 |
| **script** | Render/Audio/Input/MiniGame/Storage/Resource/Debug/Steam 接口及 `ISandboxQuota` | Lua/KAG 绑定调用；Steam Binding 每次从 Registry 解析后端 |
| **render** | `IRenderDevice`, `ITextureBudget`, `ISandboxQuota` | 粒子渲染、纹理预算与配额 |
| **render** | `IDebugManager` (宏) | 零开销日志 |
| **audio** | `ISandboxQuota` | 资源配额检查 |
| **resource** | `IJobSystem` | 异步图片解码 |
| **live2d** | `ITextureManager`, `IRenderDevice` | PNG 降级渲染 |
| **minigame** | `IInputRouter`；`IRenderDevice` 由组合根注入 | 输入焦点与 3D 渲染 |
| **rpc** | 无 Registry 依赖 | 宿主向所创建的传输适配器注入 Lua、帧捕获、归档或动画命令 |
| **storage** | `ICryptoEngine` | 存档加密 |

**其余模块（archive, debug, di, input, job, platform, steam）不通过 Registry 访问其他模块服务。**

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
