# script 模块审计

> 审查范围：`src/script/`（api/ILuaManager.h + vm/LuaManager + state/GameState + bindings/* 全部 13 个绑定）、`tests/cpp/test_script_boundary.cpp`、`tests/cpp/test_script_bindings.cpp`、`tests/cpp/test_lua_manager.cpp` 及 docs 相关章节。
> 模块角色：Lua 5.4 VM 宿主 + KAG 绑定层（耦合预算 ≤14）。本模块为绑定层，触达绝大多数被绑定模块属预期行为。

## 概述

script 模块由 1 个纯虚接口（`ILuaManager.h`）、1 个 VM 实现（`LuaManager`）、1 个游戏状态（`GameState`）与 13 个 Lua C 绑定文件（KAG/Render/DevCore/Debug/Save/Steam/VFX/MiniGame/Sma/AI/UnifiedBinding）构成。绑定层通过 `BackendRegistry` 与各模块 `api/I*.h` 接口解耦，所有跨模块头文件引用均为 `../../<module>/api/I*.h`，**未发现脚本模块自身违规 include 其他模块具体实现头**。耦合实测 **11/14**（含 di），预算内。整体健康度：**良好**。主要风险集中在：全局静态缓存线程安全（RenderBinding）、异步取消闩锁不复位（AIBinding）、以及 di↔script 之间 Lua 绑定代码的归属重叠（拉高两模块耦合、构成双向 include）。

## P0 关键问题

无严格违反「模块边界铁律」的直接 P0——脚本模块自身所有跨模块 include 均在预算内且均为 `api/`。但有一项接近 P0 的结构性风险，建议按 P1 高优先级处理（见 P1-1）：Lua 绑定实现（`registerEngineBindings` / `get*FromLua` / `setLuaState`）位于 `di/BackendRegistry.*` 而非 script 模块内，使 di 反向依赖 Lua（`lua.h`）并形成 di↔script 双向 include。该项前序 di 审计（g0_di.md）判定为「优秀」，予以认可登记录档，但从 script 视角仍建议收敛。

## P1 重要问题

**P1-1（架构·分层归属）di 承载本应属于 script 的 Lua 绑定代码**
- 位置：`src/di/BackendRegistry.h:76-77,121-127`、`src/di/BackendRegistry.cpp:213-355`（`registerEngineBindings`、`lua_Engine_*`、`get*FromLua`、`setLuaState/getLuaState`）
- 问题：script 是「绑定层」，Lua 全局 `Engine` 模块及 registry 指针解析逻辑却实现在 di 容器里；di 为此 includes `lua.h/lauxlib.h`，使「纯依赖注入容器」反向绑定到第三方 Lua C API。同时 `BackendRegistry.cpp:21` include `../script/api/ILuaManager.h` 而 script 侧 `LuaManager.cpp`/bindings include `di/BackendRegistry.h`——构成 di↔script 双向 include 环（AGENTS.md §7.1 禁循环依赖的最轻形式）。
- 修复建议：把 `registerEngineBindings`+`get*FromLua`+Lua registry key 常量整块迁入 `src/script/bindings/EngineBinding.*`；di 只保留 `forward struct lua_State;` 与 `set/get(ILuaManager*)`，删除 `setLuaState/getLuaState`。收口后 di→script include 环消失，di 不再需要 Lua。
- 工作量：M（重构 + 迁移 + 更新调用点 Engine.cpp/main.cpp/测试）。

**P1-2（正确性·取消闩锁不复位）AIBinding 全局 `g_cancelFlag` 一经置位永不复位**
- 位置：`src/script/bindings/AIBinding.cpp:33,297-301,349-366`
- 问题：`lua_AI_cancel` 置 `g_cancelFlag=true` 后再无任何复位路径；worker 任务在 `g_cancelFlag.load()` 时无条件把 `request->error="cancelled"` 覆盖结果。因此**任何一次 cancel 之后，后续新发起的 `AI.query_async`（包括与本次取消毫无关系的请求）其结果都会被错标为 "cancelled"**，直至进程结束。这是实质性逻辑 bug。
- 修复建议：改为按请求 id 维度取消——维护 `std::atomic<uint64_t>` 的「最近取消 id」，worker 仅在 `requestId <= cancelId` 时置 cancelled；或引入每请求原子 `cancelled` 标志由 cancel 遍历置位。不要用进程级一次性闩锁。
- 工作量：S（改取消语义 + 补一个「cancel 后新请求不受影响」的测试）。

**P1-3（线程安全）RenderBinding 进程级静态指针缓存非线程安全、跨 lua_State 串扰**
- 位置：`src/script/bindings/RenderBinding.cpp:29-79`（`g_cachedTexture/g_cachedRender/g_cachedVideo/g_cachedAsync` + `invalidateBindingCaches`）
- 问题：四个还原从 Lua registry 解析的后端指针被缓存在**全局函数静态变量**中。①若引擎存在多个 `lua_State`（editor/RPC 场景逐步试探多状态），后创建的状态会沿用前一状态缓存的指针（`getRender` 命中缓存后不再看 `L`），registry 中的真实后端被掩盖；②缓存非 `thread_local`、无原子操作，一旦 Lua 在 owner 线程外被驱动即产生数据竞争。当前测试靠「串行创建-销毁」规避，属脆弱设计。
- 修复建议：改为 `thread_local` + 以 `lua_State*` 为键；或将后端解析完全收敛到 di 的 `BackendRegistry::get*FromLua(L)`，删除本地静态缓存。
- 工作量：M（涉及热路径，需回归 render_text/submit_batch 测试）。

**P1-4（异步完成依赖隐式顺序）AIBinding 回调不读原子 `done` 标志**
- 位置：`src/script/bindings/AIBinding.cpp:26-30,293-344`
- 问题：`AiRequest::done` 声明为原子且 worker 最后写 `done=true`，但主线程回调直接读 `request->error/result` 而不检查 `done`。正确性完全押在 JobSystem「onComplete 恰在 worker 结束并 happens-after」这一隐式契约上（docs/design/engine-safety-and-qa-mechanisms.md:12 声明了主线程轮询模型，通常成立，但代码未自证）。
- 修复建议：回调入口检查 `request->done.load()`；或去掉该字段改为仅要求 JobSystem 保证顺序并补断言。
- 工作量：S。

**P1-5（接口泄漏第三方类型）`ILuaManager::state()` 暴露 `lua_State*`**
- 位置：`src/script/api/ILuaManager.h:20`
- 问题：AGENTS.md §7.3 禁止接口暴露第三方具体类型（以 `bgfx::TextureHandle` 为例）。`lua_State*` 是 Lua C API 的不透明句柄，经 script 接口（并被 `di::BackendRegistry::getLuaState` 二次外泄）扩散到订阅端。此类型恰是 script 模块核心通货，属边界模糊；解决 P1-1 后（di 不再持 `lua_State`）仅 script VM 内部暴露即可控。
- 修复建议：若下游无需裸 `lua_State*`，将 `state()` 收敛为 script 内部/私有能力；无法收敛则至少在接口注释明示线程归属（owner 线程）与生命周期。
- 工作量：M。

## P2 建议

1. **RenderBinding_Shutdown 悬空声明**：`src/script/bindings/RenderBinding.h:12` 声明 `void RenderBinding_Shutdown();` 但全仓库无定义、无调用。删除声明（或实现并接通关闭流程）。工作量 S。
2. **UnifiedBinding 死代码仍被打包**：`cmake/CaesuraModules.cmake:248` 编译 `UnifiedBinding.cpp`，但其 `registerUnifiedBackendBinding` 生产路径不再调用（仅测试调用），且 `LuaManager.cpp:14` 注释声称「NOT compiled into engine」与 CMake 事实矛盾。应从 CMake 源列表移除（连同对应测试用例）。工作量 S。
3. **SteamBinding 注册位置不一致**：`registerSteamBinding` 由组合根 `src/entry/Engine.cpp:465` 注册，而非 `LuaManager::registerModules()`；各绑定注册分散两处，易遗漏。建议统一收敛到 LuaManager::registerModules()（组合根只注册后端指针）。工作量 S。
4. **KAGBinding 热路径未缓存**：`KAGBinding.cpp:108-120` 的 `getAudio/getRender` 每次调用做 `lua_getfield(registry)` 字符串查找（render_text 等高频），而 RenderBinding 已缓存。可复用 di 的 `get*FromLua` 或加同样缓存（注意线程安全，见 P1-3）。工作量 S。
5. **registerModules 访问级别不匹配**：`ILuaManager.h:19` 声明 `virtual void registerModules() = 0;`（public），`LuaManager.h:40` 实现为 private——合法但语义矛盾（接口承诺可通过多态调用的虚函数在实现类中被私有遮挡）。改为 public 或从接口移除。工作量 S。
6. **resumeKAGCoroutine 空实现**：`LuaManager.h:27 / LuaManager.cpp:157-160` 为 no-op 占位，接口/实现/测试（test_lua_manager.cpp:250「does not crash」）均存在。属预留 API，建议标注 reserved 或移除。工作量 S。
7. **KAG 注册日志数不符**：`LuaManager.cpp:138` 打印「32 APIs」、`KAGBinding.cpp:103` 打印「35 APIs」，而 `kag_functions[]` 实有 ~38 项。日志与实现计数漂移，建议自动推导或在注释注明。工作量 S。
8. **测试 include 具体实现头**：`test_script_bindings.cpp:14-17 / test_script_boundary.cpp:12` 直接 include `render/ParticleSystem.h`、`input/InputRouter.h` 等具体头——测试文件允许（验证实现），但建议集中到 tests/mocks/ 减少对生产实现的侵入式依赖。工作量 S。
9. **loadScript 无路径白名单校验**：`LuaManager.cpp:145-155` 的 `loadScript` 直接用 `luaL_dofile` 加载任意路径，沙箱仅靠启动后 `lockdownScriptEnv()` 加载 sandbox.lua（`src/main.cpp:872` 等）。若 RPC 等可触发 `loadScript` 指向白名单外路径，可能绕过 io 白名单（本项目记忆：io.open 白名单只覆盖 scripts/assets/tests/demo 前缀）。建议函数内校验路径前缀。工作量 S。

## 耦合分析

权威计数（`python scripts/count_coupling.py`）：**script → 11/14 模块（30 次 include）**，预算 ≤14 ✅。

| 目标模块 | 次数 | 引用文件 |
|---|---|---|
| di（BackendRegistry） | 9 | LuaManager.cpp + DebugBinding/RenderBinding/SaveBinding/SmaBinding/SteamBinding/VFXBinding |
| render | 8 | KAG/Render/DevCore/VFX/Sma |
| resource | 3 | RenderBinding |
| audio | 2 | KAGBinding/UnifiedBinding |
| minigame | 2 | MiniGameBinding/UnifiedBinding |
| debug | 1 | DebugBinding |
| input | 1 | DevCoreBinding |
| job | 1 | AIBinding |
| platform | 1 | DevCoreBinding |
| steam | 1 | SteamBinding |
| storage | 1 | SaveBinding |

解读：10 个业务模块 + di 全部经 `api/I*.h` 接口引用，无具体实现头泄漏，符合绑定层职责。**注意反向环节**：di 侧对 script 有 1 处 include（`BackendRegistry.cpp`→`ILuaManager.h`），与 script→di 构成双向依赖（见 P1-1）；这是全仓现存唯一一对双向模块依赖，虽在单二进制静态链接内可解析，仍是应收敛的点。其余模块（audio/render/resource 等）对 script 无反向依赖。

## 审查结论

script 模块健康状况**良好**：耦合严格受控（11/14）、跨模块引用全部走 `api/` 接口、后端访问统一经 BackendRegistry/Lua registry，与 di 的 interplay 继承前序 di 审计（优秀）结论。无 P0。优先修复两项 P1 正确性/健壮性：**AIBinding 取消闩锁不复位**（P1-2，S 级）与 **RenderBinding 全局静态缓存线程安全/跨状态串扰**（P1-3，M 级）；架构层建议将 Lua 绑定从 di 迁回 script（P1-1）以消除 di↔script 环。P2 中删除悬空的 RenderBinding_Shutdown 与停止编译 UnifiedBinding 为低成本高收益的清理。整体无需返工式重构，局部收敛即可。
