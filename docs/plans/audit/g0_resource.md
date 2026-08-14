# resource 模块审计

> 审查对象：`src/resource/`（资源管理：异步加载 + 资产管线 + 图片解码）
> 审查基线：AGENTS.md 模块边界铁律 / BackendRegistry 唯一访问点 / 组合根限制 / 接口纯虚规范 / 耦合预算（resource ≤4）
> 相关文档：`docs/guides/asset-pipeline.md`；测试：`tests/cpp/test_async.cpp`、`test_resource.cpp`、`test_image_decoder.cpp`、`test_di.cpp`
> 审查方式：只读（未修改任何代码）

## 概述

resource 模块承担三类职责：

1. **资产源链（ProviderChain）**：`IAssetProvider` 抽象 + `DirAssetProvider`（目录）实现 + `ProviderChain`（按 priority 降序回退读取）。`AssetManager` 是门面，`init()` 时注入两个 Dir provider，组合根（entry/Engine_Backends.cpp）还可注入 CARC provider（CarcAssetProvider 在 archive 模块实现，反向依赖本模块接口）。
2. **异步加载（AsyncLoader）**：实现 `IAsyncLoader`，把 IO + 解码工作经 JobSystem 派发到 worker 线程，完成后经 SDL 事件（poll）或 `drainCompleted()`（headless）回主线程；带按字节/条目双上限的 RGBA 解码缓存（96MB / 512 entries）。
3. **图片解码（ImageDecoder）**：CPU-only 解码（stb_image 优先，bimg 兜底 DDS/KTX），worker 线程安全、无 GPU。
4. **附加工具**：`XP3Archive`（Kirikiri XP3 打包/解包，作开发期工具，需 miniz）；`GenerationTracker` + `ResourceHandle`（热重载代际失效追踪）。

整体健康度：**良好**。模块边界、BackendRegistry 用法、组合根限制均符合 AGENTS.md；接口均为纯虚 + 接口头内类型定义，未向外暴露 bgfx/SDL 等第三方具体类型。主要问题集中在**命名空间分裂（公共 API 一致性的 P0 异味）**与 **AsyncLoader 的 pendingCount 记账 bug（P1）**，以及若干 XP3Archive 中文路径 / 并发读的 P1/P2 隐患。

---

## P0 关键问题

### P0-1 命名空间分裂：`caesura::` vs `Caesura::`（违反 AGENTS.md §6）
- **位置**：`src/resource/api/IAssetProvider.h:7`（`namespace caesura`）；`ProviderChain.h:9`、`DirAssetProvider.h:7`（同小写）；而 `IAsyncLoader.h:6`、`IResourceGenerationTracker.h:5`、`AssetManager.*`、`ImageDecoder.*` 全部用 `Caesura`。`AssetManager.cpp:15` 外层是 `Caesura::`，内部却引用 `caesura::DirAssetProvider`。
- **问题**：同一模块对外接口分裂为两个大小写命名空间。AGENTS.md §6 明确要求“所有公共类型在 `Caesura::` 下”。连锁影响：
  - `src/archive/CarcAssetProvider.h:10` 被迫写 `::caesura::IAssetProvider` 才能适配小写命名空间（archive 作为下游被绑定到错误命名空间）。
  - 测试被迫双重 `using namespace Caesura; using namespace caesura;`（`test_resource.cpp:11-12`），掩盖命名空间不一致，削弱接口契约一致性检查。
- **修复建议**：将 `IAssetProvider.h` / `ProviderChain.h/.cpp` / `DirAssetProvider.h/.cpp` 统一改用 `Caesura::`，同步更新 `CarcAssetProvider.h` 与测试文件。API 更名前，先跑全量构建与耦合脚本确认无遗漏引用。
- **工作量**：M（涉及 4 个源文件 + 2 个下游引用点 + 测试；纯重命名无逻辑改动，但要全量构建门禁）。

> 说明：严格的“模块边界穿透（跨模块非 api/ include）”未发现违规。`AsyncLoader.cpp:4` include `../di/BackendRegistry.h` 虽非 api/ 路径，但 `scripts/count_coupling.py` 的 `ALLOWED_NON_API_INCLUDES` 显式豁免 `("di","BackendRegistry.h")`——BackendRegistry 是 AGENTS.md §3 规定的“唯一访问点”，di 无 IBackendRegistry 接口头，属项目已文档化的例外，不计为 P0。通过 `BackendRegistry::instance().getJobSystem()` 访问 job 后端也正确遵守了唯一访问点规则。

---

## P1 重要问题

### P1-1 AsyncLoader 缓存命中路径与 poll/drain 的 pendingCount 记账不一致（真实计数 bug）
- **位置**：`src/resource/AsyncLoader.cpp` 136-147（缓存命中入队）、202（排队 `m_pendingCount++`）、229（`poll()` 逐个 `m_pendingCount--`）、248（`drainCompleted()` `-= out.size()`）。
- **问题**：排队路径 enqueue 时 `m_pendingCount++`，poll/drain 消费时 `--`，平衡。但**缓存命中路径**（136-147 行）把结果直接 push 进 `m_completed` 却**没有 `m_pendingCount++`**；而 poll()/drainCompleted() 消费 completed 时**无条件递减**每条。后果：一次缓存命中 → pendingCount 净减 1；多次命中 → 计数变负。当计数降到 <0，`enqueue` 的 `m_pendingCount >= 16`（115 行）队列上限判断形同虚设，可能无限积压排队任务；`pendingCount()`（暴露给进度语义）也返回错误负值。
- **修复建议**：缓存命中分支同样执行 `m_pendingCount++`（与排队路径对齐）；或把 pending 语义改为“在途未消费负载数”并保证两种入队路径对称。
- **工作量**：S（2-3 行修正 + 补测试）。

### P1-2 XP3Archive::pack 中文/非 ASCII 路径逐字节转 wchar 错误
- **位置**：`src/resource/XP3Archive.cpp:327` `fe.name = std::wstring(relPath.begin(), relPath.end())`；配合 131-143 的 `EncodeFileName`（按 16 位 low-endian 写 UTF-16）与 `DecodeFileName` 反向解码。
- **问题**：`relPath` 是 `fs::path::string()` 的窄字符串（UTF-8 多字节），对它做 `std::wstring(begin,end)` 是**逐字节扩展**（每个窄字节→一个 wchar），而非真正的 UTF-8→UTF-16 转码。含中文/日文等非 ASCII 字符的资源路径打包进 XP3 后文件名错乱，解包无法还原。对纯 ASCII 路径无影响，故现有 round-trip 测试（test_resource.cpp:182）用 ASCII 路径未暴露此问题。
- **修复建议**：用 `std::filesystem` 宽字符路径，或引入 UTF-8→UTF-16 显式转码后再 `EncodeFileName`；解包侧已按 UTF-16 处理，保持一致。同时补充中文路径的 pack/list/unpack round-trip 测试。
- **工作量**：M（编码逻辑 + 平台相关路径处理 + 测试）。

### P1-3 ProviderChain/AssetManager 并发读缺锁，契约与多线程用法不一致
- **位置**：`src/resource/AssetManager.h:12`（注释 “Thread-safe for concurrent reads from a single worker thread”）；`ProviderChain::read/exists`（无锁遍历 `m_providers`）；AsyncLoader 经 JobSystem **多个** worker 线程并发调用 `m_assetManager->read`。
- **问题**：注释声称“single worker thread”，但 AsyncLoader 用 JobSystem 多 worker（`submit` 到共享线程池），实际是**多线程并发读**。`std::vector` 纯并发只读是安全的，但：①`AssetManager::addProvider`（组合根/主线程注入 CARC）若在 worker 读取期间调用，`vector` push_back 与读并发 = UB；②`m_chain.clear()`（shutdown）与 worker 读并发 = UB。目前靠“init 后、加载前注入 provider”的隐式时序约束成立，但无保护、脆弱。
- **修复建议**：`ProviderChain` 加 `std::shared_mutex`（读锁 + 写锁），`addProvider/clear` 走写锁，`read/exists` 走读锁；或文档明确“providers 集合在全部加载开始前固定、之后禁止变更”并加断言。修正注释中的线程模型描述。
- **工作量**：S-M（shared_mutex 改动面小，但需确认主线程 addProvider 时序）。

### P1-4 cancelAll() 后缓存可被在途 worker 完成回调重新污染
- **位置**：`src/resource/AsyncLoader.cpp:208-219`（cancelAll 清 cache）+ 169-192（worker 完成回调写 cache）。
- **问题**：`cancelAll()` 置 `m_cancelRequested=true` 并清空 `m_rgbaCache`，但**已经开始 `processRequest` 的 worker**（在 156 行检查时 flag 尚未置位，或已越过检查）完成后，其 completion 回调（163-195）会无差别地把结果重新写回刚被清零的 cache。于是 cancelAll 之后 cache 可能残留上一次 hot-reload 前的解码结果，破坏“cancelAll = 全部失效”的契约。
- **修复建议**：cancelAll 为每次失效生成代际号（自增 `m_cancelEpoch`），worker 完成回调携带/比对 epoch，不一致则不入 cache；或在写 cache 前复查 `m_cancelRequested`（现 `cancelled` local flag 只用于跳过 `m_pendingCount--`，未用于阻止缓存写入）。
- **工作量**：S（epoch 或复查 flag，逻辑小）。

---

## P2 建议

### P2-1 AsyncLoader 缓存 key 用 `path + "|" + type` 存在分隔符碰撞
- **位置**：`AsyncLoader.cpp:132`（cacheKey）、171（key 重建）。资源路径若含字面 `|` 会与同前缀其他 (path,type) 碰撞，返回错误解码结果。
- **建议**：改用无分隔符歧义的 key（长度前缀拼接或 `std::pair`/哈希）。工作量：S。

### P2-2 ImageDecoder::fromBimg 冗余空指针检查与未初始化路径
- **位置**：`ImageDecoder.cpp:41-85`。入口已判 `!img`，47 行又重复；48-54 行若 `m_format` 非 RGBA8/BGRA8/RGB8 时返回 `out`（ok=false）但 `out.rgba` 已 resize（53 行）浪费分配；56 行 `if (!src) return out` 在 resize 之后同样浪费。
- **建议**：把空/格式校验前移，仅对受支持格式分配 `rgba`。工作量：S。

### P2-3 XP3Archive::pack 大文件偏移用 `long` 截断
- **位置**：`XP3Archive.cpp:376` `fseek(out, static_cast<long>(magicLen), SEEK_SET)`。`indexOffset` 为 `uint64_t`，但 `fseek` 偏移参数用 `long`（Windows 32 位），archive >2GB 时可能错位。建议与 ReadFileBytes 一致用 `_fseeki64/fseeko`。工作量：S。

### P2-4 ImageDecoder 解码测试被禁用（SEH）
- **位置**：`tests/cpp/test_image_decoder.cpp:34-36` 注释 “ImageDecoder tests disabled — stb_image SEH on MSVC/x64”。解码路径（含维度炸弹防护 `kMaxDim/kMaxPixels`）在 CI 上无直接覆盖（test_async.cpp 仅间接用 1x1 PNG 触发一次）。
- **建议**：在禁用 SEH 或换线程的隔离环境中补正常尺寸 + 伪造超大头部解码用例，覆盖 `dimensionsValid` 守卫。工作量：M。

### ~~P2-5~~ ✅ round 33 已修复：`AsyncLoader.cpp` 10 处与 `XP3Archive.cpp` 8 处 `fprintf(stderr)` 全部改为 `DEBUG_ERR(SubSys::Resource/Archive, ...)`（引入 debug 接口，resource 耦合 2→3 仍 ≤4）。`AssetManager.cpp:19` 的 printf 为状态日志，保留。

### P2-6 缓存命中的 `poll()` 路径无测试覆盖
- **位置**：缓存命中把 `CompletedLoad` push 进 `m_completed`，`poll()` 会 `new CompletedLoad` 推 SDL 事件。现有测试（test_async.cpp 212-239）只走 `drainCompleted()`，未覆盖“缓存命中 + poll()”，而该路径正是 P1-1 计数 bug 的触发场景之一。
- **建议**：补 `enqueue(命中) + poll() + pendingCount()==正确值` 用例。工作量：S。

---

## 耦合分析

### resource 模块对外跨模块依赖（入边）
| include | 目标模块 | 是否 api/ | 判定 |
|---|---|---|---|
| `AsyncLoader.cpp:4` `../di/BackendRegistry.h` | di | 否（非 api/），但在 `ALLOWED_NON_API_INCLUDES` 豁免列表 | ✅ 合规（BackendRegistry 唯一访问点例外） |
| `AsyncLoader.cpp:5` `../job/api/IJobSystem.h` | job | 是（api/） | ✅ 合规 |
| `ImageDecoder.cpp:6` `../../external/stb/stb_image.h` | external（非 16 模块，不计入 MODS） | — | ✅ 第三方源码 |

**去重后跨模块数 = 2（di、job）**，含豁免项 ≤4 预算，**达标**（`scripts/count_coupling.py` 对 resource 不会报 EXCEEDS，无非法非-api include）。

### 其他模块对 resource 的依赖（出边，供完整性参考）
| 模块 | include | 方向 | 判定 |
|---|---|---|---|
| archive | `CarcAssetProvider.h` include `resource/api/IAssetProvider.h` | archive→resource（依赖接口） | ✅ api/ |
| di | `BackendRegistry.cpp:16-17` include `resource/api/IAsyncLoader.h`、`IResourceGenerationTracker.h` | di→resource | ✅ api/（di 预算 ≤14） |
| entry | `Engine.cpp:29-31`、`Engine_Assets.cpp:3`、`Engine_Backends.cpp:9/14/15` 等 | 组合根 | ✅ 组合根特权 |
| script | `RenderBinding.cpp:11-12`、`UnifiedBinding.cpp:9` include `../../resource/api/IAsyncLoader.h`、`IResourceGenerationTracker.h` | script→resource | ✅ api/（script 预算 ≤14） |

**结论**：resource 边界干净，无跨模块具体头/绝对路径/`../../../` 违规（`../../../external/stb/` 是第三方非模块路径，脚本 classify=null 不计入）。组合根（entry/Engine_Backends.cpp:83-89）正确持有 `AssetManager`/`AsyncLoader` 所有权，非组合根无 new 具体后端对象。唯一架构异议在 P0-1 命名空间分裂（接口层面，非 include 层面）。

---

## 审查结论

resource 模块**整体健康**：模块边界、BackendRegistry 唯一访问点、组合根限制、接口纯虚化、第三方类型隔离、耦合预算（2/4）全部满足；XP3Archive 有完善的构造/畸形输入/round-trip 测试，AsyncLoader 有较全的生命周期与缓存测试。需优先处理 1 个 P0（**命名空间 `caesura::`/`Caesura::` 分裂**，纯重构、风险低）与 1 个 P1（**缓存命中导致 pendingCount 记账出错，破坏队列上限与进度计数**，当前被测试遗漏）。其余为边界健壮性与编码卫生问题，可随迭代修复。建议修复后：全量构建 + `CaesuraTests` 全绿 + 耦合脚本，再提交。

报告路径：`docs/plans/audit/g0_resource.md`
