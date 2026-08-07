# Engine Safety & QA Mechanisms

> JobSystem 线程安全、Lua 沙箱、BackendRegistry 依赖、配额/预算与审计机制。
> 本文档是安全与质量机制的总览索引：要点在此，细节在引用文档。

## 1. 线程安全模型

- **主线程约束**：所有渲染/音频/输入后端操作必须发生在主线程
  （`CAESURA_ASSERT_MAIN_THREAD()` 运行时断言，定义见 `src/di/api/ThreadAssert.h`）。
- **JobSystem**（`src/job/`）：多线程任务队列，工作线程仅执行
  **无共享状态**的加载/解码任务（图片解码、资源读取）；结果经
  主线程轮询（`pollMainThreadJobs`）消费，避免跨线程触碰后端。
- **资源异步管线**（`src/resource/`）：`AsyncLoader` 工作线程
  解码 → 主线程 `onComplete` 上传 GPU；缓存（`m_rgbaCache`）与
  完成队列均以互斥锁保护（2026-08 修复 1×1 PNG 解码崩溃时加固）。
- **音频句柄**：SoLoud 句柄生命周期（retire/fade）仅在主线程
  执行；跨线程只传递不持有。
- 详细数据流见 `docs/design/engine-architecture-topology.md`。

## 2. Lua 沙箱分层

| 层 | 机制 | 位置 |
|----|------|------|
| 全局写保护 | `_G` metatable `__newindex` 拦截 | `scripts/sandbox.lua` |
| 模块白名单 | strict 模式代理 Render/DevCore/Debug（函数级白名单，越权即报错） | `scripts/sandbox.lua` |
| I/O 禁用 | `io`/`os` 受限（`package` 只读） | `scripts/sandbox.lua` §5 |
| 表达式沙箱 | `[expr]`/`${...}` 表达式在受限 env 求值 | `scripts/kag/expr.lua` |
| iscript 沙箱 | `[iscript]` 块白名单执行 | `tests/scripts/test_iscript_sandbox.lua` |
| 路径包含 | `confineToModelRoot` 防路径穿越（模型/纹理读取） | `src/live2d/PathConfinement.cpp` |

开发者可设 `_SANDBOX_MODE = "dev"`（config.lua）关闭白名单强制。

## 3. BackendRegistry 依赖与配额

- **唯一访问点**：所有模块经 `BackendRegistry::instance().getXxx()`
  访问后端；`entry`（组合根）持有所有权并注册。
- **配额/预算**：纹理预算（`ITextureBudget`）与沙箱配额
  （`ISandboxQuota`）由 `di/` 提供，超限分配被拒绝。
- 依赖矩阵与注册顺序见 `docs/design/backend-registry-dependency-guide.md`；
  耦合阈值见 `AGENTS.md` §9（业务模块 ≤4 跨模块依赖）。

## 4. 审计与 QA 机制

- **结构日志**：`DebugManager` 结构化日志 + 环形缓冲（`src/debug/`）。
- **错误可见性**：KAG 表达式/命令错误带 `scene:line` 定位；
  未知标签警告（而非静默）。
- **资源护栏**：纹理尺寸上限（D3D11 16384）、解码缓冲上限、
  `csmSizeInt` 截断防护（Live2D 文件读取）、宏展开预算
  （1000 次防自递归）。
- **存档安全**：AES-256-GCM 加密 + 槽位路径包含校验 + schema 迁移。
- **测试基线**：C++ 2968 断言 / Lua 99 用例 / RPC smoke / 耦合检查
  （`python scripts/count_coupling.py --ci`）全绿为合并门槛。

## 5. 相关文档

- `docs/design/engine-architecture-topology.md` — 模块拓扑与数据流
- `docs/design/backend-registry-dependency-guide.md` — 依赖矩阵
- `docs/design/engine-capability-matrix.md` — 能力就绪度
- `docs/solutions/` — 已文档化的 bug 诊断与架构模式
