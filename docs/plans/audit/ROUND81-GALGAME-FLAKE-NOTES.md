# ROUND81 — test_galgame_startup.cpp Flake 根因调查笔记 (round 82 复调)

> 轮次：82 (80 -> 81 一轮未复现后换方法的第二轮回调调查)
> 结论先行：仍未复现，但排除了若干候补并锁定了一个真实的代码级脆弱点 + 一个明确的缓解方向。
> 覆盖角度：doctest 执行序 / 进程级全局态 / filesystem 持久态 / JobSystem 线程 / 历史证据 / 压力序实验。

---

## 0. 失败点与当前基线

- 候选失败点 (round 80 记录为 test_galgame_startup.cpp(73))：
  - line 65  REQUIRE(engine.init())
  - line 73  REQUIRE(engine.lua().loadScript("scripts/kag/init.lua"))
- 当前基线 (本环境实测)：build/tests/Debug/CaesuraTests.exe 全量 786/786 通过，7198 断言 (与 round 81 基线一致)；
  本测试单独 15 连跑全绿；entry+galgame 同进程压力序全绿。
- loadScript 实现为 luaL_dofile(m_L,path) != LUA_OK -> return false (src/script/vm/LuaManager.cpp:149)。
  失败 = 单个文件 open 失败 (LUA_ERRFILE) 或 require 链中任一处脚本错误，无重试。

## 1. doctest 执行序 & 进程级全局态 — 排除

- 每个 Engine 独立 lua_State (round 81 已证)；package.loaded 穿越不可能。
- BackendRegistry 单例：grep 全部 set* 调用，engine 类测试全部 scoped (std::make_unique<Engine> 或本地 Engine engine)，
  ~Engine -> shutdown() 恢复空注册表；test_engine_lifecycle.cpp 有 checkEngineRegistryCleared() 断言正向后验。
  "被前序测试污染的 registry" 假设被证伪 (entry/engine_lifecycle 在 galgame 之前，全绿)。
- JobSystem：createJobSystem() 恒建真实 JobSystem (Engine_Backends.cpp:91)，headless 下亦 spawn hw-1 线程 (本机 14 个)；
  ~JobSystem -> shutdown() -> waitIdle()+notify+join (JobSystem.cpp:72-92) 确定性收尾，无跨测试存活线程。
- HotReload：init() scanDirectory() 同步、无线程 (HotReload.cpp:22-47)，shutdown() 清空。
- DebugManager 单例：进程级共享，但 init("logs") 只做文件日志初始化，无跨测试可观察状态。
- 无测试写入 scripts/ 或 demo/ (只读/只加载)；test_live2d 写 temp 目录且自清理。
- settings/ 在测试 CWD (build/tests/Debug) 不存在 -> config.lua apply()->load_all() 读默认值，无跨运行 settings 污染。

## 2. init.lua 失败模式分析

- scripts/kag/init.lua require 链约 30 个模块，全部声明式 (top-level 无文件 IO / 无 error())。
- 每个 require 经标准 Lua loader 走 package.path (测试 line 70 configurePackagePath 预设
  scripts/?.lua; ...; scripts/kag/?.lua) —— 磁盘顺序 open 30+ 文件。
- sma.lua:22 require("json") 不存在 -> pcall 捕获 -> 回退内联 JSON parser (安全，但多一次 filesystem probe)。
- 工程真实启动经 StartupScripts.cpp discoverStartupScriptDir() 支持 scripts/, ../../, ../../../ 多 CWD 兜底；
  测试硬编码 "scripts/"，脆弱点见 §4。

## 3. 压力序实验 (round 82 新增) — 未复现

- entry 套件 34 用例 + galgame 同进程：通过。
- 全套件 1 次在进程序中 galgame 段：[kag/init] All KAG libraries loaded. 出现，通过。
- 目标单测 15 连跑：15/15 全绿。

## 4. 最强假设 (排名) 与验证状态

1. H1 (最强但无法本地复现)：
   Windows filesystem 瞬时 open 失败 / LUA_ERRFILE (AV 扫描、前序 saves/logs 写冲刷、高 IO 负载下
   require 链任一文件 open 失败即整链 false、无重试)。
   证据：loadScript 无重试；链开约 30 文件；Round 81+82 全绿说明需极端 IO 时序才触发。
   与 round 81 "疑似测试执行序/进程环境相关" 的结论相容。
2. H2 BackendRegistry 前序残留：已证伪 (scoped + shutdown 恢复 + 前置全绿)。
3. H3 JobSystem 跨测试线程并发污染 Lua：已证伪 (~JobSystem join 干净)。
4. H4 settings/*.lua 跨运行持久污染：已证伪 (测试 CWD 无 settings/)。
5. H5 CWD 不匹配导致 scripts/ 解析失败：边界脆弱、非周期性 (偏离 AGENTS §5 CWD 约定会确定性失败而非偶发)。

## 5. 建议修复 / 缓解

1. (首选，低风险) loadScript 对 LUA_ERRFILE 加一次性重试 (src/script/vm/LuaManager.cpp)：
   luaL_dofile 前 fopen 探测 + EACCES/EMFILE 短暂重试 (如 3x10ms)，消化 Windows 瞬时文件打开抖动。
   改动极小、其他测试不受影响。
2. (测试侧防御) test_galgame_startup.cpp 对 line 73 require 链 (tokenizer/scheduler/kag 等) 失败时
   CAPTURE 具体 require 名与错误，便于下次复现定位到具体模块。
3. (CWD 硬化) 测试改用 discoverStartupScriptDir() 逻辑 (scripts/ 与 ../../scripts/ 双尝试) 对齐真实 boot 多 CWD 兜底。
4. 若 CI 偶发：loadScript 已打印 "Error loading %s: %s"，失败现场日志即可锁定是否 LUA_ERRFILE。

## 6. 结论

未在本环境复现 (与 round 81 一致，累计 >=7 次全量 + 多轮定向均绿)。当前证据最支持 H1：
Windows 下文件打开瞬时失败 / LUA_ERRFILE。它无需任何跨测试状态 (与 round 81 排除 package.loaded、
registry 污染的结论完全相容)，且与 "进程环境/执行序相关" 的既有判断吻合。
建议做 §5.1 的极轻量防御性重试 + §5.2 的失败现场捕获，把偶发降到可诊断。
