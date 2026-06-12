# BackendRegistry 依赖说明文档

> 生成日期: 2026-06-12 | 接口总数: 26 | 模块: 16

## 核心原则

BackendRegistry 是引擎的**单一服务定位器**。所有模块通过它获得其他模块的接口指针（`I*`），而非直接 include 具体实现头文件。

**这不是"隐藏依赖"——这是显式解耦。** 每个模块的依赖关系通过 BackendRegistry 的 `get*()` 调用清晰可见，且强制走接口。

## 为什么不是"接口爆炸"

26 个接口对应 16 个模块，平均每个模块 1.6 个接口。这与"接口爆炸"（通常指每个类都有接口）有本质区别：

1. **接口对应子系统，不随代码膨胀。** 模块新增功能在接口内部扩展方法，不创建新接口。
2. **消费者只调用自己需要的。** `script` 模块调用 `getRenderDevice()`，不关心 `getCryptoEngine()`。
3. **接口是稳定的契约。** 实现可替换（SoLoud→FMod，bgfx→Vulkan），消费者不改一行代码。

## 依赖关系一览

每个模块的已知依赖（通过 BackendRegistry `get*()` 访问）：

| 模块 | 依赖的接口 | 用途 |
|------|-----------|------|
| **entry/Engine** | 全部 26 个 | 组合根，创建并注册所有后端 |
| **script** | `IRenderDevice`, `IAudioBackend`, `IMiniGameBackend`, `ISteamBackend` | KAG 命令执行 |
| **render** | `ITextureManager`, `ILayerManager`, `IParticleSystem` | 模块内调用（同模块） |
| **render** | `IDebugManager` (宏) | 零开销日志 |
| **audio** | `ISandboxQuota` | 资源配额检查 |
| **resource** | `IJobSystem` | 异步图片解码 |
| **live2d** | `ITextureManager`, `IRenderDevice` | PNG 降级渲染 |
| **minigame** | `IRenderDevice` | 3D 渲染 |
| **rpc** | `ILuaManager` | 脚本执行 |
| **storage** | `ICryptoEngine` | 存档加密 |

**其余模块（archive, debug, di, input, job, platform, steam）不依赖其他模块的接口。**

## 新增模块检查清单

添加新模块时：
- [ ] 创建 `src/<module>/api/I<Module>.h` 纯虚接口
- [ ] 在 BackendRegistry 添加 `set*()` / `get*()` 方法
- [ ] 在 Engine::init*Phase() 中注册
- [ ] 消费者通过 `BackendRegistry::instance().get*()` 访问，不直接 include 实现头文件
- [ ] 确认不引入循环依赖（A→B→A）

## 循环依赖防火墙

BackendRegistry 的接口指针模式天然防止循环依赖：
- 模块 A 调用 `BackendRegistry::instance().getB()` 获取 `IB*`，不需要知道 B 的具体类型
- 编译期只依赖 `IB.h`（接口），运行时通过指针间接调用
- 如果未来 A 和 B 互相需要，各自通过 BackendRegistry 获取对方的接口，不会形成编译期循环
