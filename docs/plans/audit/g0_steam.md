# steam 模块审计

> 审查目标：src/steam/（api/ 接口 + SteamBackend 实现 + NullSteamBackend 桩）及其消费者（entry 组合根、di/BackendRegistry、script/bindings/SteamBinding、storage/CloudSaveProvider）与 tests/cpp/test_steam.cpp。
> 依据宪章：AGENTS.md §1 模块边界、§2 接口规范、§3 BackendRegistry 唯一访问点、§4 组合根、§7 禁止事项、§9 耦合预算。
> 基线：docs/plans/audit/SUMMARY.md 中本模块状态为「待深审」。

## 概述

steam 模块是可选集成的 Steamworks 适配层，负责三类能力：**成就（Achievements）**、**统计（Stats）**、**云存档（Remote Storage / Cloud Saves）**，另含生命周期（init/shutdown/每帧 runCallbacks/overlay 状态轮询）与枚举接口（cloud_list）。

模块采用「接口 + 双实现」的经典可选后端模式：

- `api/ISteamBackend.h`：纯虚接口，无数据成员，符合 §2 规范。
- `SteamBackend.h/.cpp`：真实 Steamworks SDK 集成，整文件以 `#ifdef CAESURA_HAS_STEAM` 条件编译；SDK 未配置时退化为返回 false/0/空串的空操作（与空头分支同语义，可编译但不可用）。
- `NullSteamBackend.h`：全 no-op 桩，SDK 不存在时的运行时落点，与 ISteamBackend 表面完全一致。

**接线正确性（重点核查项）**：

| 环节 | 位置 | 是否合规 |
|---|---|---|
| 接口纯虚、无数据成员 | ISteamBackend.h:17 | 合规 |
| api/ 下只有 I*.h 对外 | src/steam/api/ 仅 ISteamBackend.h | 合规 |
| steam 自身零跨模块 include | SteamBackend/NullSteamBackend 仅 include 自身 api + steam_api.h + stdlib | 合规 |
| 组合根创建具体对象 | entry/Engine_Backends.cpp:34-40 createDefaultSteamIntegration()（ifdef make_unique SteamBackend，否则 NullSteamBackend） | 合规：唯一 new 具体后端处 |
| BackendRegistry 注册/访问 | di/BackendRegistry.cpp:27/85/108 + entry/Engine.cpp:464 setSteamBackend | 合规：走注册表统一入口 |
| Lua 绑定取后端 | script/bindings/SteamBinding.cpp:22 BackendRegistry::instance().getSteamBackend() | 合规：未绕过单例 |
| storage 消费 steam | storage/CloudSaveProvider 仅 include ../steam/api/ISteamBackend.h，头文件前向声明避免头文件依赖 | 合规：只走接口 |
| main.cpp | 不含 SteamBackend/NullSteamBackend/config.steam（test_source_encoding.cpp:218-240 断言缺失） | 合规 |

**总评**：steam 模块是 16 个模块中边界最干净的之一——自身出向耦合为 0，所有消费者（di/entry/script/storage）都只碰 `ISteamBackend` 接口或位于组合根，未发现任何 §7 所列的模块边界穿透。但真实 SDK 分支存在**数个仅在开启 `CAESURA_HAS_STEAM` 时才会暴露的潜在 bug**（CI 恒为 OFF，故全部未被测试覆盖），其中统计持久化丢失问题具有实际功能破坏性。

## P0 关键问题

**无。** 未发现违反 AGENTS.md 铁律的问题：

- 无 `../../../` 或绝对路径 include（所有相对 include 均为 `../<module>/` 或自身 api）。
- steam 未 include 任何其他模块的非 api/ 头文件；反向来看，唯一消费它具体实现的 `entry/` 是组合根，合规。
- 无绕过 BackendRegistry 直取单例的情况（`DEBUG_*` 宏例外不涉及本模块）。
- 无循环依赖（依赖图有向无环：di←steam接口、entry→steam具体、script→steam接口、storage→steam接口；steam 不反向依赖任何模块）。
- 非组合根位置无 `new` 具体后端对象。
- 接口未泄漏任何 Steam SDK 具体类型（如 `SteamAPICall_t`、`ISteamUserStats*` 等均未出现在接口中；`STEAM_CALLBACK` 宏仅在实现内部使用）。
- 接口暴露底层细节核查：`ISteamBackend.h` 全部为自有类型（const char/bool/int32/float），符合 §2/§7.3。

## P1 重要问题

### P1-1 [M] 统计持久化丢失：setStatInt/setStatFloat 未置 m_statsDirty
- 位置：`src/steam/SteamBackend.cpp:119-161`
- 问题：仅 `unlockAchievement`（:74）、`resetAchievement`（:99）、`resetAllAchievements`（:112）在成功后置 `m_statsDirty = true`；而 `setStatInt`（:119-127）与 `setStatFloat`（:141-149）**从不置 dirty**。`runCallbacks`（:41-56）的批处理 `StoreStats` 只在 `m_statsDirty` 为真且距上次≥1s 时触发。因此一个只调用 `steam.set_stat_int/float`、从不显式 `steam.store_stats` 的游戏逻辑，其统计修改永远不会进入批处理 flush——若玩家在脏数据被 StoreStats 前退出（或从不触发解锁类操作），则这些统计修改会**静默丢失/不持久**。依赖 `SteamAPI_Shutdown` 兜底保存不可靠且延迟到退出。
- 修复建议：在 `setStatInt`/`setStatFloat` 的 `SteamUserStats()->SetStat` 成功分支后追加 `m_statsDirty = true;`（就地 2 行，不依赖网络）。
- 工作量：S

### P1-2 [S] cloudFileNameAt 使用 static char s_name[256]——线程不安全 + 长文件名静默截断
- 位置：`src/steam/SteamBackend.cpp:253-266`
- 问题：
- ① 返回指向函数内 `static char s_name[256]` 的指针：并发调用者会互相改写同一缓冲区（当前唯一消费者 SteamBinding.cpp:143-146 的 cloud_list 是逐名调用 + 立即 lua_pushstring 拷贝，故现路径安全，但把「共享可变静态缓冲」的指针从 const 方法返回是**易碎的延迟归属**设计，未来多线程/异步调用者会踩）。
- ② `snprintf` 硬 256 上限：文件名长于 255 字符会被**静默截断**，返回的截断串与云端真实文件不匹配——随后 cloud_read/delete/file_size 用该串将失败。Steam 远程存储允许较长文件名（路径可达数百字节），是真实往返破坏场景，且无任何告警。
- 修复建议：将接口 `cloudFileNameAt` 返回类型改为 `std::string`（接口变更，按 §10 全链路：ISteamBackend.h → 双实现 → SteamBinding → 测试）；或保留 `const char*` 但改用私有成员缓冲 + 文档化一次性读取契约，并在截断时返回空串（宁可舍弃也不给坏串）而非截断串。推荐前者。
- 工作量：M（改接口 + 实现 + SteamBinding.cpp:141-147 + 两个测试文件）

### P1-3 [S] SteamBackend.h 在未 include steam_api.h 的情况下展开 STEAM_CALLBACK 宏——SDK 开启时编译错误
- 位置：`src/steam/SteamBackend.h:50-57`
- 问题：`SteamBackend.h` 自身**未 include `steam_api.h`**，却声明了三个 `STEAM_CALLBACK(SteamBackend, ...)` 回调监听器（:53-56）。`SteamBackend.cpp` 第 3 行先包含 SteamBackend.h（此时 `STEAM_CALLBACK` 尚未定义），第 7 行才在 ifdef 内包含 steam_api.h。一旦 `CAESURA_HAS_STEAM` 被开启，头文件内展开该宏时其尚未定义 → **编译期错误**（依赖 include 顺序的偶然正确）。因 CI 恒 `CAESURA_HAS_STEAM=OFF`（CMakeLists.txt:167/250），此缺陷从未被 CI 捕获——开发者首次接入 SDK 即撞墙。
- 修复建议：在 SteamBackend.h 的 ifdef 块内补 `#include <steam/steam_api.h>`（或回调实体移到 .cpp 由 `STEAM_CALLBACK` 在 .cpp 中定义）。修复无法在现有 CI 验证（需 SDK），应加一条静态源码断言（仿 test_source_encoding.cpp 文本扫描）断言 SDK 路径下该头自包含。
- 工作量：S

### P1-4 [S] getStatInt/getStatFloat 与 isAchievementUnlocked 未按 m_statsReceived 门禁，首帧读到默认值
- 位置：`src/steam/SteamBackend.cpp:82-92, 129-161`
- 问题：unlock/reset 在 `!m_statsReceived` 时返回失败并提示调用方重试（:72/:97），而 getStat*/isAchievementUnlocked 未做同样门禁——客户端统计异步到达（RequestCurrentStats 在 init 发出，首帧改前可能未回），首帧读到的 int/float 均为 0、成就是 false，且与「确实为 0/未解锁」无法区分。脚本在启动首帧按「读到的值」做决策会得到错误结果。
- 修复建议：在 get 系列与 isAchievementUnlocked 中加 `if (!m_statsReceived) return 0/0.0f/false;` 以与 set 侧语义统一，并可新增 `statsReady()` 查询接口供显式探查。
- 工作量：S

## P2 建议

### P2-1 [S] SteamAchievement 结构体为接口内死代码
- 位置：`src/steam/api/ISteamBackend.h:11-15`
- 问题：`struct SteamAchievement` 定义了但无任何接口方法引用、也无实现使用，残留类型增加接口表面积。
- 建议：删除，或若后续要支持批量拉取成就列表则真正落地为方法。工作量 S。

### P2-2 [S] runCallbacks 的节流计时用 clock()（CPU 时间）而非墙钟
- 位置：`src/steam/SteamBackend.cpp:48-53`（m_lastStoreStats，SteamBackend.h:48 注释 throttle seconds (clock())）
- 问题：`clock()/CLOCKS_PER_SEC` 是处理器时间，非墙钟。主循环空闲等待时 clock() 几乎不前进，批处理阈值 1s 会显著拉长到实际数秒才 flush（无害但语义与注释相悖）；多线程场景 clock() 行为实现定义。
- 建议：改用 `std::chrono::steady_clock::now()`（或 SDL_GetTicks，平台模块已有计时）。工作量 S。

### P2-3 [S] 日志用 printf 而非引擎日志桥
- 位置：`src/script/bindings/SteamBinding.cpp:179`（printf("[Lua] Steam module registered…")）
- 问题：其余绑定模块经 debug/ 日志；此处裸 printf 破坏日志一致性，无法被日志系统过滤/级别控制，与 debug 模块零开销日志约定相悖。
- 建议：改用引擎 DEBUG_LOG/IDebugManager（该绑定已在 di 依赖下可取得后端）。工作量 S。

### P2-4 [S] Lua cloud_read 16MB 上限与 CloudSaveProvider 64MB 上限不一致
- 位置：`src/script/bindings/SteamBinding.cpp:100`（size > 16*1024*1024）；对照 `src/storage/CloudSaveProvider.h:30`（kMaxChunkedSize = 64MB）
- 问题：同一份云端数据，直接 steam.cloud_read 在 >16MB 时返回 nil，而经 CloudSaveProvider（64MB）可正常读取。语义漂移。
- 建议：统一为同一常量上限（CloudSaveProvider 已是权威 64MB）或注释说明故意收紧。工作量 S。

### P2-5 [S] 本模块测试文件覆盖面薄，状态机逻辑无测试
- 位置：`tests/cpp/test_steam.cpp`（8 个用例全为 NullSteamBackend no-op 断言）
- 问题：test_steam.cpp 只验证桩的静态返回值（必然通过），对真实逻辑零覆盖。不过分块云存档逻辑已被 test_storage.cpp 的 MockSteamBackend（256KB 分块、伪造 .meta 防御、写失败回滚，用例齐全）覆盖；Lua 绑定表已被 test_script_bindings.cpp 经 BackendRegistry 注入假后端覆盖。缺口是 **StoreStats 批处理与统计脏标记状态机**（P1-1 的回归防护）——该逻辑在 SDK 分支内，CI 不可达。
- 建议：把「脏标记 → 批处理」决策抽取为可单测的纯函数/可注入节流时钟，添加不依赖 SDK 的用例（对照 docs/solutions/deferred-gpu-tests.md 的延迟覆盖模式），至少防 P1-1 复发。工作量 M。

### P2-6 [S] storeStats()（显式）未走节流，脚本直呼可高频打网络
- 位置：`src/steam/SteamBackend.cpp:163-170`；`src/script/bindings/SteamBinding.cpp:76-79`
- 问题：store_stats 绑定每次调用实时 StoreStats()，不受 1s 节流保护。脚本循环高频调用会向 Steam 服务器产生过多网络往返。
- 建议：文档化该 API 为显式立即 flush；如需防护可在后端加与批处理同源计数器。工作量 S。

## 耦合分析

**steam 模块出向依赖**（本模块 include 的跨模块头）：

| 文件 | include | 跨模块数 |
|---|---|---|
| api/ISteamBackend.h | stdlib（string/vector/cstdint） | 0 |
| SteamBackend.h | api/ISteamBackend.h + cstdint（+ SDK 时 steam_api.h，第三方非模块） | 0 |
| SteamBackend.cpp | SteamBackend.h + steam_api.h + cstring | 0 |
| NullSteamBackend.h | api/ISteamBackend.h + cstdint | 0 |

**steam 出向跨模块数 = 0**，远低于预算 ≤4，为全库最内聚模块之一（与 di/job 同级优秀），无需解耦。

**steam 入向耦合**（谁依赖 steam 的接口，各自出向因此 +1，均在其预算内）：

| 消费方 | 引用的 steam 头 | 类型 | 预算合规 |
|---|---|---|---|
| di/BackendRegistry.cpp:27 | ../steam/api/ISteamBackend.h | 接口 | 合规 |
| di/BackendRegistry.h:31 | 前向声明 class ISteamBackend | 接口 | 合规 |
| entry/Engine_Backends.cpp:4/25 | NullSteamBackend.h / SteamBackend.h | 具体（组合根允许） | 合规 |
| entry/EngineConfig.h:15 | 前向声明 | 接口 | 合规 |
| script/bindings/SteamBinding.cpp:8 | ../../steam/api/ISteamBackend.h | 接口 | 合规（script 预算 ≤14，富余） |
| storage/CloudSaveProvider.h:8 + .cpp:3 | 前向声明 + ../steam/api/ISteamBackend.h | 接口 | 合规（storage 出向 +1，远低于 ≤4） |
| tests/cpp/test_steam.cpp / test_storage.cpp:78 / test_script_bindings.cpp:17 | steam/NullSteamBackend.h + api + Mock | 允许 | 测试可触达 |

**依赖方向核查**：steam 不依赖任何模块；storage→steam 单向（无环）；`CloudSaveProvider` 以构造注入 `ISteamBackend*`（storage/CloudSaveProvider.h:12），不触及 BackendRegistry 内部、不 new 具体对象，边界正确。**无 P0 穿透、无循环依赖。**

## 审查结论

steam 模块**架构边界合格**：出向耦合 0、接口纯净、唯一具体对象创建在组合根、所有消费者走 `ISteamBackend` 接口或 BackendRegistry，未发现任何违反 AGENTS.md 铁律的 P0。主要风险集中在**真实 SDK 分支的潜在缺陷**（CI 恒 OFF 未能暴露）：

- **P1-3 必改（S）**：SteamBackend.h 用 STEAM_CALLBACK 却少 include steam_api.h，一旦开启标识即编译失败——接入 SDK 的第一道门。
- **P1-1 必改（S）**：setStatInt/float 未置 m_statsDirty，统计修改不触发批处理 flush，存在静默持久化丢失的实际功能 bug。
- **P1-2 建议改（M）**：cloudFileNameAt 静态缓冲 + 255 字符截断，接口缺陷（宜改返回 std::string）。
- **P1-4（S）**：get/isAchievement 未按统计就绪门禁，首帧误读默认值。

P2 层为死代码清理、计时改用稳定墙钟、printf→日志桥、两处容量常量统一、以及为 SDK 分支状态机补充可单测覆盖（防 P1-1 复发）。工作量分布：2 个小修 + 1 个中改（含接口变更）+ 若干 S 级清理即可将本模块从「待深审」提升为「良好→优秀」。

**总体：边界合规、实现清晰（分块云存档对伪造 .meta/超大 reserve 的防御值得肯定），唯一实质风险是 SDK 分支的一处编译阻断与一处持久化 bug，建议在接入 Steamworks 前先修 P1-1/P1-3。**