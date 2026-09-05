# Engine Safety & QA Mechanisms

> JobSystem 线程安全、Lua 沙箱、BackendRegistry 依赖、配额/预算与审计机制。
> 本文档是安全与质量机制的总览索引：要点在此，细节在引用文档。

## 1. 线程安全模型

- **主线程约束**：所有渲染/音频/输入后端操作必须发生在主线程
  （`CAESURA_ASSERT_MAIN_THREAD()` 运行时断言，定义见 `src/di/api/ThreadAssert.h`）。
- **JobSystem**（`src/job/`）：多线程任务队列，工作线程执行
  I/O、解码等后台工作，不得直接操作需要主线程的后端；结果经
  主线程轮询（`pollMainThreadJobs`）消费，避免跨线程触碰后端。
- **资源异步管线**（`src/resource/`）：worker读取/解码，主线程onComplete写入完成缓冲，经SDL事件或drain交付Engine，后者复核请求代次后上传GPU并调用Lua；缓存与完成队列以互斥锁保护。
- **完成屏障与关停**：`waitIdle()`只等待worker及回调发布；poll执行有限快照，嵌套poll不递归执行新回调。关停关闭任务入口、join线程后做最终快照，回调内再次关停会使剩余批次失效。Engine先取消脚本异步请求并完成Job关停，再销毁VFX、渲染、图层、小游戏及资源后端。后台任务自身仍须能正常结束，不能同步等待主线程回调。
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
  `csmSizeInt` 截断防护（Live2D 文件读取）、宏递归防护
  （round-75 起改为**基于拼接深度**——嵌套拼接栈 >100 才报错，替换旧的累计调用计数上限，
  顺序调用 1000+/大循环不再被误判为自递归）。
- **存档安全（2026-09-05 源码同步）**：AES-256-GCM、槽位范围校验和 schema 迁移；本段不作为最新测试已通过的记录。
  - 加密布局：文件前缀魔数 `CAES`（4B）+ nonce（12B）+ GCM tag（16B）+ ciphertext
    （`src/storage/SaveManager.cpp`）。`envelope.dump()` 整体加密，包含 scene、timestamp、schema_version 等元数据；它们随密文认证，不是明文文件头。
  - SaveManager 统一编码/解码，provider 只传输原始字节。默认 `Compatible` 允许旧明文读取；`RequireEncrypted` 拒绝非 CAES，未设 key 或 clear key 后保存失败且不触碰原档。已识别 CAES 的解密失败不能回退 JSON；兼容模式不防整个文件替换为合法明文。
  - `loadLegacyPlaintext()` 是显式只读导入入口：拒绝 CAES、不改变策略、不写回源文件。普通读取和显式导入失败时均不改变输出 `SaveMeta`；合法空对象/数组与失败 null 区分。
  - 云同步经 `ICloudSaveTransport` 分离本地/云端：读取一次 → 按策略/key与存档结构验证 → 提交同一份字节。拒绝发生在目标写入前，有效的严格模式同步保留；直接 provider 字节 API 不执行 SaveManager 策略。
  - schema 迁移链：`v1→v2`(playtime)→`v3`(minigame)→`v4`(live2d)→`v5`(editor)，
    读取时按版本步进迁移 `data` 子对象（`registerBuiltinMigrations`），不自动写盘或创建备份。槽位文件名/AAD 绑定和旧密文防重放尚未提供；详细边界见 [存档安全说明](save-security-audit.md)。
- **测试基线（阶段 G 终态 / round 113 口径，round 114 终验复核）**：C++ doctest
  `976/976` 用例（`8858` 断言）/ Lua 主套件 `132/132` + 孤儿 `24/24` /
  web `297/297`（20 文件）/ editor `530/530` / KAG 契约 `123` /
  ctest `10/10`（+AI smoke 跳过）/ 覆盖 `PASS` / 耦合检查
  （`python scripts/count_coupling.py --ci`）与 api-stats 重生成幂等全绿为合并门槛。

## 5. 相关文档

- `docs/design/engine-architecture-topology.md` — 模块拓扑与数据流
- `docs/design/backend-registry-dependency-guide.md` — 依赖矩阵
- `docs/design/engine-capability-matrix.md` — 能力就绪度
- `docs/solutions/` — 已文档化的 bug 诊断与架构模式
