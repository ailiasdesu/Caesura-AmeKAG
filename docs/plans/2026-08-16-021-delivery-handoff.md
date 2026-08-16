# Caesura (AmeKAG) — 交接文档（2026-08-16 第 21 轮迭代 / round 100 起点）

> 面向后续 agent 的完整上下文。本轮为 **100 轮冲刺最终收尾轮**（round 100，阶段 F 收官）：
> round 99 已完成（跨场景循环栈泄漏修复 + 宏语义裁决 + 跨场景预算落地 + 能力矩阵/文档/perf 收尾），
> 100 轮冲刺已到 **99/100 完成**，仅剩 round 100 最终总结报告 + 统一 push + CI 确认。
> **先读 AGENTS.md（模块边界铁律）+ 本文件 + ROADMAP-100.md（轮次记录权威在 plans/audit/ROADMAP-100.md）。**

---

## 1. 项目状态与当前基线（round 99 完成态 / 2026-08-16）

| 维度 | 基线 | 说明 |
|---|---|---|
| C++ 用例 | **963/963**（round 96=922→97=945→99=963） | 全量构建零错误（Debug）；debug 双 bug 修复补全 SubSys 计数 |
| Lua 用例 | **130/130 + 20 孤儿** | 主套件 130 全过（96=126→97=127→98=128→99=130）；run_orphan_tests.lua 20/20（每测试子进程隔离） |
| Web (web/) | **282/282（16 文件）** | vitest + 集成测试（round 97 bundle 跨场景、round 98 settings desktop-parity 263→282） |
| Editor (editor/) | **506/506** | tsc 干净（round 97=498，round 98 SceneTree+Inspector 至 506） |
| ctest | **10/10 + AI smoke 跳过** | headless_http_smoke / headless_rpc_smoke / LSP 冒烟等惯性通过 |
| 耦合 / 覆盖 | **PASS** | count_coupling.py --ci；check_test_coverage.py（无注册漂移） |
| 里程碑 | **99/100 完成**（round 99）；阶段 F 仅剩 round 100 最终收尾 | 100 轮 sprint 收官阶段 |
| CI | 三平台 CI 此前已绿（round 60 push + round 94/96 修复后 5/5 job PASS） | Windows D+R / macOS / Linux / Package；round 100 最终确认待跑 |
| 契约命令 | **118/118 运行时覆盖 100%** | command-contracts.md 权威（最新见 api/command-contracts.md） |
| 能力矩阵 | **79 项能力**（round 98 刷新，round 99 保持 79 不变） | docs/design/engine-capability-matrix.md |
| 教程库 | **15 个**（tutorial_01–15） | demo/tutorial/*.ks，引擎 + Web 双验证 |
| story bundle | **20 场景** | cache/story/story.lua；ks_bake --web 导出 |

> 上一权威交接：`docs/plans/2026-08-16-020-delivery-handoff.md`（round 96 起点 / round 95 完成）。
> round 96-99 的逐轮明细与提交见 ROADMAP-100.md；能力矩阵当前态见 engine-capability-matrix.md。
> **本轮（round 100）唯一交付：最终总结报告 final-report-100.md + 统一 push + 一次三平台 CI。**
---

## 2. 近期完成（round 96-99 摘要）

| 轮次 | 里程碑/主题 | 关键内容 |
|---|---|---|
| **96** | **DebugManager 双 bug 修复 + skip/auto/advance + kag3_import 二轮 + 交接 020** | **① DebugManager 双 bug 修复（round 95 记录）**：m_errorCounts std::array<7> vs SubSys 枚举 12 值 → 新增 kSubSysCount 单一来源常量、数组 7→12、dumpFullReport 遍历全部 12 子系统；beginFrameProfile 不重置 luaGcMs 跨帧泄漏补复位；断言翻转（高索引子系统计数/dumpFullReport 12 key/luaGcMs 归零）；**② web skip/auto/advance 组合 +10/45 断言**（skip×advance/auto/选择/save-load/三态切换；记录：advance+skip 同开 skip 接管整段、裸 skip 遇 [sel] 自动选第一项）；**③ Inspector 二轮 +16**（lint 交互边界/引擎断开时序/jump 幂等 guard 改 useRef 同步闩锁修真 bug）；**④ kag3_import 二轮 +29/+18 断言**（别名覆盖矩阵/RENAMES/宏参数形态；记录宏体参 &who 不转 %who%、别名+显式共存不 dedup）；**⑤ minigame/live2d 边界 +14**（C++ 908→922；记录 confineToModelRoot("") fail-closed 拒绝对）；**⑥ 交接文档 020** |
| **97** | **expr 复杂度预算 + bundle 跨场景 + schema coerce + RPC 边界 + perf** | **① expr O(n³) 修复**：根因 translate_parens 深嵌套每层重扫（100 层 6.3s）；括号嵌套 >48 时整组不翻译（运行时解析器报错），deep100 6.3s→0.001s；**修 round 95 守卫误伤**（递归深度≠嵌套深度，改括号嵌套计数）；**② bundle 跨场景 +7/48 断言**（A jump B / call 返回 / save-load 恢复 / i18n 保持）；主代理修 round 95 load resume 回归（save.lua 新增 _pendingLoadOriginScene）；**③ ActivityBar/StatusBar +15**（editor 483→498）；**④ schema coerce 深度 +85**（Lua 126→127；记录 string choices 数组误拒/default 不经 coerce/positional 绕过类型强制/死代码 raw==""）；**⑤ RPC/HTTP 边界 +23**（C++ 922→945，test_rpc 28→51；记录非标准 JSON-RPC/错误格式不一致/手写 JSON 容忍非法）；**⑥ perf 四轮刷新 + CI 修复 40b81a9a**（macOS confineToModelRoot fail-closed） |
| **98** | **schema coerce 四缺陷 + settings 桌面 parity + SceneTree 二轮 + 跨场景深度 + input 映射** | **① schema coerce 四缺陷落地**（round 97 FINDING → 修复）：string choices 数组 contains 语义、default 经 coerceValue 规范化、positional 位置参数类型强制、file 空串 dead-code 修复；test_schema_coerce 四组断言翻转；**② web settings 桌面 parity +19**（web 263→282；language 双端同一代码/setLanguage 路由真实 i18n）；**③ SceneTree 二轮 +8**（类型分组折叠/点击导航双写/选中态响应；editor 498→506；顺带修复 R98-F 3 个既有失败断言）；**④ 跨场景切换深度 +60 断言**（新 test_flow_edge_scene.lua，Lua 127→128：跨场景 jump/call/load/循环检测；**发现 3 缺陷仅记录**：跨场景 jump 不重置 _forStackMarks、ctx.macros 会话级泄漏、无跨场景切换预算）；**⑤ input/keyboard 映射 +18 用例/68 断言** |
| **99** | **跨场景循环栈泄漏修复 + 宏语义裁决 + 跨场景预算 + 能力矩阵/文档/perf 收尾** | **① 跨场景循环栈泄漏修复（R98-D 缺陷①落地）**：scheduler 跨场景 [jump] 分支对称重置 _forStack/_whileStack/_ifStack/_switchStack/_forStackMarks；test_flow_edge_scene F1 断言从"锁定 11 次泄漏"翻转为"恰好 2 次 + 栈清空"；**② 跨场景切换预算落地（R98-D 缺陷③）**：新增 budget_scene_switch（SESSION 级 ctx._sceneSwitches，SCENE_SWITCH_MAX=4096，仅 kag_runner.start 重置）；[jump]/[call]/[link] 共享；超限 WARN + fall-through 切断（防 A↔B 乒乓/跨场景 call 无限增长）；**③ 宏跨场景语义裁决（R98-D 缺陷②）**：**结论——ctx.macros 会话级共享 = KAG3 兼容的全局宏语义，非缺陷**；新 test_macro_scene.lua 锁定（跨场景共享/宏参数 %N% 透传/非对称覆盖保持）；**④ 跨场景预算测试**（test_scene_switch_budget.lua 登记主套件；Lua 128→130）；**⑤ 能力矩阵 round 98 刷新**（P2 基线 C++963/Lua128+20/web 282/editor 506/ctest 10+AI；S4 expr 嵌套预算；D1 editor 408→506；能力计数 79 不变；api-stats 重生成幂等零 diff 契约 118）；**⑥ 文档审计 + perf 五刷**（schema 语义零文档修改；perf 439→556 行，round 97-98 零回归） |

---

## 3. 剩余轮次计划（round 100 最终收尾）

| 轮次 | 计划主题 | 说明 |
|---|---|---|
| **100** | **100 轮总结报告 + 最终 push + CI 确认** | ① 生成 docs/plans/audit/final-report-100.md（覆盖 90 轮主线 + 阶段 A-F 成果 + 缺陷闭环清单 + 能力矩阵/基线终值 + 交接给后续 campaign）；② **统一 push**（本轮所有累积语义提交，按 AGENTS.md 约定分语义提交）；③ **一次三平台 CI 确认**（Windows D+R / macOS / Linux / Package 全绿） |

> 阶段 F（round 91-100）至此收官：G12 代码质量 / G13 文档同步 / G14 发布流程 + 100 轮总结报告全部完成。
> round 90 已实测发布流程，round 91-99 走硬化/深度/性能路线，round 100 回归总结报告主线并收尾。
---

## 4. 关键技术/踩坑（务必记住，含 round 97-99 新增）

### 4.1 架构与模块（长期铁律）
- **模块边界**：只能经 api/ 子目录接口（I*.h 纯虚类）跨模块访问；组合根 src/entry/ + src/main.cpp 是唯一 new 具体后端处；所有后端经 BackendRegistry（唯一访问点，宏 DEBUG_* 例外）。
- **BackendRegistry**：存非拥有 I* 指针，Engine 持 unique_ptr；添加后端 = 建 I* 接口 → 实现 → registry set/get → Engine::init() 注册。
- **修改 main.cpp 的 if constexpr RPC 分支链务必保留原分支 return 收尾**，否则运行期 0xC0000005（C4715 警告是信号）。
- **新增 kag 模块必须登记进 kag/init.lua 预加载清单**（sandbox require 只认 package.loaded）。

### 4.2 构建/测试/工具（每轮门禁）
- **全量构建零错误 → C++ 全绿 → Lua 全绿 → 孤儿全绿 → ctest 全绿 → 耦合/覆盖 PASS → 语义提交**（AGENTS.md §5 / ROADMAP §5）。
- **测试从 build/tests/Debug/ 执行**（CWD 需匹配资源路径）；doctest 用 -tc/-ts 过滤；Lua 主套件 run_lua_tests.lua（顺序敏感）+ **孤儿套件 run_orphan_tests.lua 必须独立跑**（全局 mock 与沙箱顺序互斥，永不合并）。
- **孤儿测试 runner 已改为每测试子进程隔离（io.popen）**——Windows/git-bash 下含空格/圆括号的绝对路径（arg[-1]）必须用 call "..." 包裹。
- **测试数量减少或新增失败=禁止合并**（无法归因则回滚）。

### 4.3 Shell 与编码（round 54-99 高频坑）
- **git bash 强制**：路径一律正斜杠 /；反斜杠会被 bash 吃掉。工具显示名 pwsh 但后端实为 bash。
- **PowerShell > 重定向默认 UTF-16LE**：生成 markdown 会写坏文件（read 报 binary、Lua find 失效）。生成文档用 Set-Content -Encoding utf8 或 python 写。
- **CRLF 陷阱（round 94 CI 根因）**：.gitattributes 若无对应规则（如 *.json），Windows checkout 变 CRLF，生成的 index/story 等产物 git diff 红。改生成脚本前先确认 .gitattributes 覆盖该扩展名。
- **taskkill 铁律**：DSH 宿主就是 node.exe，**绝对禁止 taskkill //F //IM node.exe**（会连同当前会话一起杀）。清理残留进程必须先按 CommandLine 精确识别（vitest 等），逐个 taskkill //F //PID <pid>，命令行含 dsh/npx-cli.js @deepseek-ai/dsh 的绝不能杀。
- git commit -m 消息含 ${...} 会被 shell 展开（bad substitution）——用 bash 单引号包 -m。

### 4.4 Lua/表达式（round 50-99）
- **Lua 语言限制**：表构造器不能直接索引（`{1,2}[1]` 语法错误，需 `({1,2})[1]`）；插值表达式 `${ {a=1}.a }` 求值前须给前导构造器加括号。
- **表达式翻译管线**：三元在 [ ]/括号内从内向外预翻译（translate_brackets/translate_parens）；[eval] 需运行时 translateOperators；?? → or（字符串字面量内不翻译）；插值用平衡花括号扫描器 match_brace（quote 感知，未闭合保留原文）。
- **迭代守卫**：while/for/until 均有每场景 65536 帧护栏（WHILE_MAX_ITERS）；大 timeout + 永不真条件不再无限轮询。
- **宏系统两缺陷（record）**：ctx._macroExpansions 只增不重置（合法调用累计>1000 误报）；宏体收集遇第一个 endmacro 即停（嵌套宏定义不支持）。嵌套宏调用正常；round 99 裁决全局宏共享语义。
- **wasmoon（web）**：Lua false 经 lua.global.get 映射为 null（布尔断言须显式 ==false）；JS 代理返回值是 userdata 会破坏引擎 type 检查（用 Lua 字面量桥 + load() 解析绕开）。

### 4.5 round 97-99 新增关键技术（务必记住）
- **expr 括号嵌套预算（round 97）**：translate_parens 对括号组递归 translate，深嵌套每层重扫 O(n³)（100 层 6.3s）；修复：**括号嵌套 >48 时整组不翻译**（运行时解析器报错），flat 三元链不加深（500 链守卫保持）；deep100 6.3s→0.001s。**记录：修 round 95 守卫误伤——深度预算曾误伤 300/500/200 规模化守卫（递归深度≠嵌套深度），改按括号嵌套计数，不要按递归深度评估**。
- **跨场景预算 4096（round 99）**：budget_scene_switch 是 SESSION 级计数器 ctx._sceneSwitches（SCENE_SWITCH_MAX=4096），**仅 kag_runner.start 重置，绝不按 per-run 重置防误清**；[jump]/[call]/[link] 共享；超限 WARN + fall-through 切断（与 round-84 _backJumps 同形）；用于防 A↔B 乒乓与跨场景 [call] 无限增长 call_stack。
- **C1041 PDB 锁（round 97-98 编译坑）**：MSVC 链接时多个并发 cl 进程写同一 PDB 报 C1041（无法打开程序数据库）——全量并行构建偶发；重跑或串行化 link 即可；CI/门禁重跑幂等。
- **SDLK_A 大写陷阱（round 98 input 映射）**：SDL 扫描码/键码常量检测要区分大小写（SDLK_A vs SDLK_a 语义不同）；input 映射深度时注意修饰键+字母组合的位掩码语义，裸 Meta/Win 路由进 KAG 推进链（补 Ctrl 空缺）。
- **跨场景循环栈对称重置（round 99）**：scheduler 跨场景 [jump] 分支（245-267）与同场景 jump（317-321）必须**对称重置** _forStack/_whileStack/_ifStack/_switchStack/_forStackMarks；round 83 修的同场景遗留曾不对称（A 的 for 体内 jump B 后 B 同名 for 从陈旧 f.i 续跑 11 次非 2 次）。
- **round 95 跨场景 load resume 回归（round 97 主代理修复）**：save.lua 恢复时 current_scene 已覆盖为目标场景，round 95 自我引用判断恒真 +1 跳页；新增 **_pendingLoadOriginScene**（覆盖前捕获发起场景），bridge 两处判断改用 origin。
- **schema coerce 四修正（round 98）**：① string choices 数组 contains 语义；② default 经 coerceValue 规范化；③ positional 位置参数类型强制（coerceValue 写入 named key + 位置槽双份）；④ file 空串 dead-code 修复。
- **宏观语义裁决（round 99 结论）**：ctx.macros 会话级共享 = **KAG3 兼容的全局宏语义，非缺陷**（KAG3 原版宏一经定义所有场景可用直至 erasemacro/重定义）。
- **api_stats.py const 方法漏计（round 93 修复）**：纯虚方法 regex 须匹配 (?:const )?= 0;。

---

## 5. 待办 / 已知缺陷列表

以下采自 ROADMAP-100.md 回合 87-99 记录的**未修观察**（多为语义差异/设计取舍/已知病理，未作为缺陷阻断；本轮可按需在 final-report 归档）。

| 项 | 状态 | 说明 |
|---|---|---|
| **CARCWriter addFile 不去重** | 待评估（round 88 记录） | 按 pathHash 全字节比较、重复键更新既有条目（幂等语义与 reader 后写覆盖对齐，numFiles=唯一路径数）；非缺陷但需文档明确 |
| **48B 密钥容忍** | 设计决策（round 88 记录） | CryptoEngine keyLen 严格拒绝 ≠32；16B 拒 / 48B 拒对称 |
| **nonce 复用无检测** | 设计取舍（round 88 记录） | nonce 文档化"调用方负责唯一性"；CSPRNG 96-bit 碰撞 <2⁻⁴⁸，加注册表无收益 |
| **settings 语言默认差异** | 记录（设计取舍） | web settings 语言默认与桌面行为差异（round 98 parity 测试锁定双端 mapping 同一代码，但默认值语义可能不同）；settings.reset 不写 localStorage（round 95 设计取舍） |
| **宏系统两缺陷** | 待修 | _macroExpansions 只增不重置致 1000 误报；宏体内嵌宏定义收集残缺（round 99 裁决全局宏共享语义，但两缺陷本体未改） |
| **kag3_import 宏体参 &who 不转 %who%** | 记录 | convertMacroArgs 仅应用裸文本行，宏体命令参数 &who 不转 %who%（round 96 记录） |
| **别名+显式共存不 dedup** | 记录 | kag3_import 别名与显式共存 last-wins 不 dedup（round 96 记录） |
| **endtag/endform/g known 集环境依赖** | 记录 | KAG3 特有命令 UNSUPPORTED+建议分类受环境依赖影响（round 96 记录） |
| **expr 深层嵌套三元 O(n³)（round 95 已知病理）** | 已修复（round 97） | N=100 9.3s → 括号嵌套 >48 截断；不再作基准 |
| **round 98 跨场景 3 缺陷** | 已修复（round 99） | 跨场景 jump 不重置循环栈、ctx.macros 泄漏、无跨场景预算——①②③全在 round 99 落地/裁决 |
| **diagnostics 不做参数类型校验** | 设计行为（round 57 记录） | 类型是运行时 coerce/clamp，非 LSP 诊断职责 |
| **Live2D GL/Steam 实机** | **待设备** | GL 需 Linux/macOS 硬件；Steam 需开发者账号 |
| **Metal 后端真机验证 / 移动真机** | **待设备** | macOS 硬件 / Android/iOS 设备；管线与文档已就绪 |
| config.ensure_dir 无法从零建 settings/ | 记录 | C++ 侧预建故运行时无感（round 94 记录） |
| web backlog 无上限（桌面限 500） | 记录 | round 92 500 页探针证明；属语义差异非缺陷 |

---

## 6. 门禁（每轮强制，见路线图 §5）

全量重建零错误 → C++ 963/963 → Lua 130/130 + 孤儿 20/20 → ctest 10/10（+AI smoke 跳过）→ 耦合 PASS → 覆盖检查 PASS → 语义提交。
推送策略：多轮本地累积语义提交，到目标节点（**round 100 最终总结**）统一 push + 一次三平台 CI。
**round 100 特别门禁**：final-report-100.md 生成后须重跑全量门禁确认基线数字一致，再统一 push + 三平台 CI 全绿。

## 7. 注意事项

- **bundle sweep 是 bundle 路径真实守卫**：20/20 场景（round 95 恢复）；改 scheduler/compiler 涉及 [save]/[load]/恢复场景语义时，务必跑 web sweep + flow 全绿（92/92）。
- **跨场景切换 = 高风险改动面**：round 97/99 修复了跨场景 load resume 回归、跨场景循环栈泄漏、跨场景预算；任何触碰 scheduler 跨场景分支（245-267 vs 317-321 对称重置、_pendingLoadOriginScene、budget_scene_switch）的改动必须先跑 test_flow_edge_scene + test_scene_switch_budget + 主套件。
- **编辑器前端/Web 改动**：editor/src（tsc + vitest）、web/（vitest + flow + sweep）；产物 editor/dist、web/dist 不入库（既定规则）；vite build 用 es2022 顶层 await。
- **修改生成脚本前确认 .gitattributes 扩展名规则**（round 94 CRLF 根因）。
- **文档权威性**：ROADMAP-100.md = 轮次记录权威；本 handoff = 交接现状；engine-capability-matrix.md = 能力矩阵（round 99 已刷到 79 项不变）。
- **round 100 最终交付物**：final-report-100.md（阶段 A-F 全量成果 + 缺陷闭环 + 基线终值表）+ 统一 push + 三平台 CI 确认；完成后建议在 ROADMAP-100.md 记录 round 100 行。
- 历史交接：`2026-08-16-020-delivery-handoff.md` 为上一权威状态（round 96 起点）。