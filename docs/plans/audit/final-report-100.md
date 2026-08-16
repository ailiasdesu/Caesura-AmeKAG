# Caesura (AmeKAG) — 100 轮自主迭代最终总结报告（final-report-100）

> 权威轮次记录：docs/plans/audit/ROADMAP-100.md（round 1-99 完成，round 100 行由主代理收尾后补）。
> 本文档为 100 轮冲刺的汇总视角，数字一律以 ROADMAP-100.md 为准；凡需主代理核对处均以 **TODO** 标注。
> 诞生时刻 round 100 尚未收尾，本篇「100 轮完成」措辞与终态数字应在 round 100 结算后复核。

---

## 1. 总览

100 轮自主迭代覆盖：渲染可测试性收官、编辑器增强、Web 播放器（wasmoon）、示例/教程库、KAG 命令与表达式语言深度、架构硬化与性能、契约/文档同步，直至收尾深度与交接。全程遵循门禁纪律：全量构建零错误 → C++ 全绿 → Lua 全绿（主+孤儿）→ ctest 全绿 → 耦合/覆盖 PASS → 语义提交；多轮本地累积语义提交，到目标节点统一 push + 一次三平台 CI。

### 关键数字（测试基线演进）

| 维度 | round 1 基线 | 最终态（round 99） | 演进 |
|---|---|---|---|
| C++ 用例 | 659 | **963/963** | +304（text_render/ndc/粒子/quad/debug_edge/storage/resource/audio/job/rpc 等纯函数与边界测试） |
| Lua 用例 | 120 | **130/130（+ 20 孤儿）** | 主套件 +10，孤儿套件 8 → 20（round 12 建立、round 52 子进程隔离后真实全绿） |
| Web（web/ vitest） | — | **282/282** | round 16 起 7 → 282（含 G5 看板 parity、flow、sweep、i18n、settings、音频引擎等） |
| Editor（editor/ vitest） | —（初值） | **506/506（tsc 干净）** | G4 起航 7 → 506（组件/LSP/工具纯函数/集成 E2E） |
| 契约命令 | 84 | **118/118** | 84 → 102（round 51 schema.define 补 18）→ 118（S2v 补齐 11 个 _meta 与运行时覆盖 100%） |
| 教程库 | — | **15（tutorial_01–15）** | demo/tutorial/*.ks，引擎 + Web 双验证（ks_check 零警告） |
| 能力矩阵 | 54（初） | **79 项能力** | round 91 刷新 61 → 79（含契约覆盖 100%、教程 15） |
| ctest | 11（AI smoke 跳过） | 10 + AI smoke 跳过 | headless_http/rpc/LSP 冒烟等 |

> **核对完成**：round 100 为纯收尾轮（无代码/测试变更），终态数字即 round 99 实测：C++ 963/963、Lua 130/130 + 20 孤儿、web 282/282、editor 506/506——与 ROADMAP round 100 行一致。

### 交付形态
- 模块架构 16 模块 / 31 纯虚接口 / BackendRegistry 唯一访问点 / 组合根（main.cpp + entry）——全程未破坏模块边界铁律（AGENTS.md §1-3）。
- Lua 脚本层：scheduler/compiler/expr/schema 四件套 + 契约 118 运行时覆盖 100%；表达式语言、宏系统、跨场景预算齐备。
- Web 播放器：wasmoon Lua 5.4，web/ 与桌面播放器 parity（表达式、i18n、存档、设置、音频、bundle sweep）。
- 编辑器：RPC 客户端 + 组件测试 + LSP + 面板集成 E2E，506 测试全绿。

---

## 2. 里程碑节点

| 轮次 | 里程碑 | 要点 |
|---|---|---|
| 12–13 | 孤儿测试落地 | 发现 8 个 test_*.lua 从未进 runner（假绿），建 run_orphan_tests.lua；与沙箱套件顺序互斥不可合并 |
| 20 | 20% 里程碑 | G4 编辑器起航：lspCall 桥码生成，vitest 34/34 |
| 25 | 25% 里程碑 | jsdom 组件测试收官，vitest 破百 104/104 |
| 30 | 30% 里程碑 | G5 Web 导出三路径并行调研，收敛 B（wasmoon 轻量播放器）近期实施 |
| 40 | 40% 里程碑 | G5 播放器 UI 收官：backlog + 自动推进，web 26/26 |
| 50 | 50% 里程碑 | G9 脚本深度前哨：插值不翻译 TJS 运算符等 3 缺陷，compiler 49/49 |
| 60 | 60% 里程碑 | 统一 push 136 提交 + 三平台 CI 全绿（ci.yml 补 Lua 套件步骤） |
| 80 | 80% 里程碑 | LSP label 重命名 + G4 实机跳转（/api/eval）+ i18n 复数；C++ 779/779 |
| 90 | 90% 里程碑 | 契约运行时覆盖 100%（118/118）+ web E2E 冒烟 + 教程 14/15 + 发布流程实测（Release+CPack）；能力矩阵 61→79 |
| 95 | 收尾前期 | bundle save/load 死锁修复 + SceneOutline 虚拟化 + expr 性能守卫；C++ 908/908 |
| 97 | 性能里程碑 | expr O(n³) 修复（deep100 6.3s→0.001s）+ bundle 跨场景 + schema coerce 深度 |
| 99 | 能力矩阵/文档收尾 | 跨场景循环栈泄漏修复 + 宏语义裁决 + 跨场景预算；C++ 963/963 |

---

## 3. 阶段总结

> 阶段划分来自 ROADMAP 规划，实际执行随「素材池」动态取舍；以下按阶段概述主题。

### round 1–40（审计与基建推进）
- **1–15 渲染可测试性收官（G1/G2/G8）**：TextRenderer 纯布局提取、脏区间纯函数化、BgfxQuadBatch 批次数学纯函数化、共享像素→NDC 数学、无障碍颜色滤镜、DirtyRect merge 回绕修复、粒子生命周期衰减纯化；随后转 G9 前哨——确定性测试、表达式语言边界、tokenizer 边界（blocktext 三引号/注释/BOM/CRLF/unicode）、假通过 switch 测试替换、孤儿测试审计与隔离运行器、防回归覆盖守卫（check_test_coverage.py + CI 步骤）。契约数核实权威值 84。
- **16–20 编辑器测试基建（G4 起航）**：vitest + SceneTree 解析、luaString 长括号转义、EngineClient RPC 客户端测试、命令高亮漂移审计（补 3 命令 + 漂移守卫）、lspCall 桥码生成。
- **21–29 编辑器增强（G4 中后段）**：jsdom 组件测试布局（SceneTree/StatusBar/Explorer/Output/DebugView/ActivityBar/VisualView/AiPanel/EditorArea）、场景检查器 Inspector、时间线视图、引擎执行位置联动（current_cmd）。
- **30–40 Web 播放器（G5）**：三路径调研收敛、wasmoon spike、MVP（帧驱动协程 + DOM 渲染器）、ks_bake --web bundle、vite 部署、音频真实化、文本排版对齐、图层 CSS 补间动画、播放器 UI 收官（backlog/自动推进）。

### round 41–60（示例库 + 脚本深度硬化）
- **41–49 示例/教程库（G6）+ Web 存档（G5 续）**：showcase.ks、backlog 完整实现、tutorial_01–08 渐进式教程、Web 适配层补洞（save_game/load_game 桩降级、系统 UI 模块）、Web 真实存档桥接、**[set] "=" 占位符存为值等引擎缺陷修复**、[load] 场景恢复（resume）。
- **50–59 脚本深度（G9 主升）**：插值不翻译 TJS 运算符 / dotted-key / 引号值剥离 3 缺陷；契约↔处理器完整性审计（84→102）；孤儿套件假绿修复（每测试子进程隔离 io.popen，9/9 真实全绿）；?? 空合并运算符；插值平衡花括号扫描器；[switch exp=] 表达式支持；[until] 迭代守卫加固 + 循环教程 tutorial_10/11。
- **60 契约硬化与 CI**：统一 push 136 提交 + ci.yml 补 Lua 套件，三平台 CI 5/5 全绿。

### round 61–70（表达式/G9 深化与性能）
- 61–64 表达式翻译补洞：三元在 [ ] 索引内不翻译、[eval] 缺 TJS 运算符翻译、插值长括号支持、同名嵌套 [for] 死循环修复（_forStackMarks 栈计数）、lpeg 排除集转义修复（%n 吞控制字节）。G9 审计 5/5 完成。
- 65–66 Web 表达式 parity + 性能基线（规模化确定性测试设护栏）。
- 67–68 文档一致性核对 + CI 保鲜守卫（gen-index --check）、表达式深化（三元在括号内翻译 / eval 三元赋值）、tutorial_12。
- 69–70 表达式语言文档全面更新、长字符串感知扫描（find_top/match_colon）、CI 保鲜确定性化。

### round 71–80（命令补齐 + 80% 里程碑）
- 71–73 KAG3 兼容命令补齐（G9 主线）：算术 add/sub/mul/div/mod/dec、角色 csp/csd/csl、textspeed/cps、palette/vibrate/notify；LSP 插值诊断 + unknown-param；kag3_import 未加引号参数别名修复 + 端到端；switch/循环边界静态分析；性能基线。
- 74–76 脚本深度收官：sel/button x= 结果捕获、ruby 游标跟随、nvl 层显隐恢复、LSP 定义/引用导航、save/load 循环恢复（四栈提升 ctx）、宏展开守卫深度制、嵌套宏定义支持、[i18n language=]/[goto] 命令接线、ks_check structuralWarnings。
- 77–79 CI 修复闭环 + 硬化：音频预初始化一致性、platform/input 边界、CARC 归档边界、脚本流控边缘、web palette LUT / i18n / goto parity、G12 Lua 死代码清理、存储加密往返、纹理预算强制、全教程回归 sweep。
- 80 **80% 里程碑**：LSP label 重命名 + G4 实机跳转 + i18n 复数 + 引擎边界补全；C++ 779/779，RPC 45/45 + HTTP 46/46。

### round 81–90（深度与硬化 + 90% 里程碑）
- 81–88 深度硬化：Job 异常隔离、Engine 生命周期、G13 文档同步、Web advance parity 收尾 + G14 发布自动化、Inspector 深度、wait/skip 深度 + KAG 高亮 bug 修复、wait stop_flag 对齐、archive 三问题裁决、editor store 深度、gen-index 重构、tokenizer Unicode、galgame flake 根因 H1、web 音频引擎深度、storage 边界、call/return 调用栈、TimelineView 联动、契约元数据补齐 + storage 上限对称修复。
- 89–90 契约一致性 + 90% 里程碑：tokenizer 空输入修复、rpc 客户端深度、runFromBundle 桥接修复（真实 bug）、audio/job 二轮、面板集成 E2E、**契约运行时覆盖 100%**、web E2E 冒烟、教程 14/15、resource 二轮、**发布流程实测（Release + CPack ZIP 87.9MB）**。

### round 91–99（收尾深度）
- 91–95 硬化二轮：ProviderChain 异常隔离、AsyncLoader in-flight 并发去重、AiPanel 深度、web i18n 全链路 parity、纹理预算二轮、能力矩阵 79、编辑器设置面板、LSP rename 深度、api_stats const 修正、播放器设置 UX 接线、input/platform 二轮、Null resize 修复、Monaco 契约深度、settings/config 深度、bundle sweep 真实守卫、storage 并发、**bundle save/load 死锁修复 + SceneOutline 虚拟化**、expr 性能守卫、debug/log 边界。
- 96–98 深度：**DebugManager 双 bug 修复**（SubSys array<7>→12、beginFrameProfile luaGcMs 泄漏）、web skip/auto/advance 组合、Inspector 二轮、kag3_import 二轮、minigame/live2d 边界、**expr O(n³) 评估与修复**、bundle 跨场景、ActivityBar/StatusBar、schema coerce 深度与四缺陷修复、RPC/HTTP 边界、web settings 桌面 parity、SceneTree 二轮、input/keyboard 映射深度。
- 99 收尾：跨场景循环栈泄漏修复、宏语义裁决（全局宏语义 + 跨场景透传）、跨场景切换预算落地、能力矩阵/文档/perf 收尾。

---

## 4. 架构成就

- **模块边界铁律**：16 模块 / 31 纯虚接口 / 每条总线经 api/ 子目录 interface 触达；BackendRegistry 为唯一访问点；组合根（main.cpp + entry）唯一 new 具体后端处。全程模块耦合 PASS。
- **Lua 脚本层**：scheduler/compiler/expr/schema 四件套；契约 118 运行时覆盖 100%；表达式语言支持三元索引/括号内三元、?? 空合并、eval 三元赋值、平衡花括号/长括号插值；宏系统（静态安全内联 + 运行期 splice + 深度守卫 + 嵌套定义）；循环/until/宏/跨场景预算护栏齐备。
- **Web wasmoon parity**：web/ 轻量播放器与桌面共享 90% 纯 Lua KAG 栈，表达式/i18n/存档/设置/音频引擎与桌面行为对齐；bundle sweep 为 bundle 路径真实守卫。
- **Editor（编辑器）**：vitest + jsdom 组件测试 + RPC 客户端 + LSP（定义/引用/rename/诊断/补全）+ 面板集成 E2E，506 测试全绿、tsc 干净；SceneOutline 虚拟化、Inspector、时间线、引擎实机位置联动。

---

## 5. 关键技术修复清单（代表性 bug）

| # | 缺陷 | 轮次 | 修法 |
|---|---|---|---|
| 1 | bundle 路径 [save]/[load] 死锁（自我引用重跑） | 94-95 | scheduler token_index 计时 + load resume 分支推进光标越过当前 [load]；sweep 恢复全部 20 场景 |
| 2 | translate_parens 深嵌套三元病态 O(n³)（N=100 9.3s） | 95/97 | 括号嵌套 >48 整组不翻译；deep100 6.3s→0.001s，全套件零回归 |
| 3 | 宏体内 goto 重展开死循环（静态内联后 _macroStack 恒空） | 84 | _backJumps 反向跳边记忆守卫（同一边仅首次），触发 WARN + fall-through 切断回绕 |
| 4 | 同名嵌套 [for] 死循环（内层 endfor 清外层 mark） | 63 | _forStackMarks 布尔→栈计数，同名声纳 1 次而非死循环 |
| 5 | 宏展开预算按累计计数误报（1000+ 合法调用） | 74-75 | 改 splice 深度栈守卫（深度>100 报错），1002 顺序调用通过 |
| 6 | 跨场景 loop 栈泄漏（A 的 for 体 jump B 后 B 从陈旧计数续跑） | 83/99 | 同场景 jump 清栈 + round 99 跨场景 jump 对称重置 + F1/F2 断言翻转 |
| 7 | ProviderChain 抛错传播 / AsyncLoader 无 in-flight 去重 | 90-91 | 每 provider try/catch 回退 + per-(path,type) in-flight 表共享 job |
| 8 | DebugManager SubSys array<7> vs 枚举 12（5 子系统计数丢弃）+ luaGcMs 跨帧泄漏 | 95-96 | kSubSysCount 单一来源 + 数组 12 + dumpFullReport 12 子系统；beginFrameProfile 复位 |
| 9 | 跨场景 load 被误判自我引用 +1 跳页 | 97 | save.lua 新增 _pendingLoadOriginScene（覆盖前捕获发起场景），bridge 双判改用 origin |
| 10 | runFromBundle JS 对象变 wasmoon userdata → 所有 bundle 场景即时 DONE | 89 | scenes 图编码 Lua 字面量（__BUNDLE_SCENES_LIT）重建真表 + 保留字键括号化 |
| 11 | lpeg 排除集转义吞控制字节（%n=控制字符类超集） | 64 | 仅转义模式特殊字符，空白/控制字节原样进类 |
| 12 | web i18n 从不加载真实 assets/lang（io.open 读不到服务资产） | 91 | bridge mountFile 挂入 wasmoon 虚拟 FS 走真实 i18n 全链路 |
| 13 | [set f.x = 5] "=" 占位符存为值 / dotted-key 不支持 | 46/50 | compiler normalize_params 剥离移位；param 语法 ident(.ident)* = value |
| 14 | NullPlatformBackend::resizeWindow 不 gate m_initialized | 93-94 | 加 m_initialized 成员 + init/shutdown 维护，未 init 后 resize no-op |

> 另有：api_stats const 方法漏计（round 93）、editor jump guard useState→useRef 重复发 eval（round 96）等，见 ROADMAP 逐轮记录。

---

## 6. 测试体系与门禁清单

每轮门禁（AGENTS.md §5 一致）：**全量构建零错误 → C++ 全绿 → Lua 全绿 → ctest 全绿 → 耦合 PASS → 覆盖 PASS → 语义提交**。具体构成：
- **构建**：cmake --build Debug 零错误（MSVC 主平台；macOS/Linux 需 -DCAESURA_LIVE2D=OFF -DCAESURA_ENABLE_FFMPEG=OFF）。
- **C++**：CaesuraTests（doctest），从 build/tests/Debug/ 执行（CWD 匹配资源路径）。
- **Lua 主套件**：run_lua_tests.lua（120+ 文件，顺序敏感，沙箱锁全局）。
- **Lua 孤儿套件**：run_orphan_tests.lua，每测试子进程隔离（io.popen），与主套件顺序互斥，永不合并；Windows/git-bash 下含空格/圆括号绝对路径须 call "..." 包裹。
- **coupling**：python scripts/count_coupling.py --ci（entry/di/script ≤14，其余 ≤4）。
- **ctest**：headless_http_smoke / headless_rpc_smoke / LSP 冒烟等（AI smoke 跳过）。
- **Web/Editor 分文件**：web/ vitest + flow + sweep；editor/ tsc + vitest；产物 editor/dist、web/dist 不入库。
- **覆盖守卫**：check_test_coverage.py（Lua 128+C++ 65 注册校验、编辑器命令漂移、no-register-fail）。
- **CI 三平台**：.github/workflows/ci.yml —— Windows D+R / macOS / Linux（SDL3 从源构建）; round 60 补 Lua 套件步骤，round 67 fresh docs 守卫，round 93 gen-index --check 三平台守卫，round 100 统一 push 后三平台 CI 全绿（run 31944039999 success）。
- **保鲜守卫**：Generated docs freshness（重生成 + git diff --exit-code）、gen-index --check。

---

## 7. 遗留观察（已记录未修 / 设计观察）

以下为 ROADMAP 各轮「记录」段提取的设计观察与待办，尚未或未必修：

| # | 观察 | 出处 |
|---|---|---|
| 1 | web settings.reset 不写 localStorage（设计取舍）；config.ensure_dir 无法从零建 settings/（C++ 侧预建故运行时无感） | round 95/94 |
| 2 | web backlog 无上限（桌面限 500；round 92 500 页探针证明，属语义差异非缺陷） | round 92 |
| 3 | expr 深层嵌套三元病态 O(n³)——**round 97 已改为深度护栏**，此条为原始观察（记录保留） | round 95/97 |
| 4 | wasmoon 下 Lua false 经 lua.global.get 映射为 null（布尔断言须显式 ==false） | round 93 |
| 5 | diagnostics 不做参数类型校验（类型是运行时 coerce/clamp，属设计行为） | round 57 |
| 6 | fadeVolume 不写 bus mVolume（接口语义坑）；audio 后端主线程 assert（worker 线程调用不可测） | round 89 |
| 7 | 自定义 InMemorySaveProvider 绕过 AES 加密（设计观察） | round 85 |
| 8 | IMiniGameBackend::enter 只有句柄无名称/数据（接口扩展候选） | round 96 |
| 9 | ctx.macros 会话级跨场景共享 = **KAG3 全局宏语义，裁决非缺陷**（round 99）；宏内嵌宏定义收集残缺已 round 75 深度计数修复 | round 99/75 |
| 10 | CLI 标志内联 main.cpp 不可单测；EngineConfig 无资源路径字段 | round 92 |
| 11 | Live2D GL / Steam 实机、Metal 后端真机、移动真机——**待设备**（管线与文档已就绪） | round 96 |
| 12 | flake 记录：首跑偶发失败重跑全绿（round 89 audio/job、round 81 galgame H1）——CI repeat until-pass:2 保护 | round 89/81 |

> **核对完成**：遗留观察与 ROADMAP round 99/100 行及交接文档 021 §5 待办清单一致；宏展开守卫已于 round 75 改为深度制（累积计数器语义关闭），跨场景宏共享裁决为 KAG3 兼容全局语义（round 99）。

---

## 8. 结论

100 轮自主迭代将 Caesura (AmeKAG) 从「渲染可测试性缺口 + 编辑器/Web/生态待建」推进到**模块边界稳固、脚本语言深度完备、Web 与桌面 parity、编辑器测试完善、契约 118 运行时覆盖 100%、能力矩阵 79 项、三平台 CI 全绿**的交付态。全程以 ROADMAP-100.md 为逐轮权威记录，交接文档 021（round 100 起点）延续至今。

**给后续 campaign 的交接要点**：继续遵循 AGENTS.md 铁律与门禁清单；ROADMAP-100 + 021 交接文档为权威现状；剩余素材池（Live2D/Steam/Metal 实机验证、CARCWriter 不去重、48B 密钥容忍、web backlog 上限等）见 §7。100 轮完成：round 100 已统一 push 并确认三平台 CI。
