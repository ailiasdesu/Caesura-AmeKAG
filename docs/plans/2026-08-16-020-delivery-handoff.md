# Caesura (AmeKAG) — 交接文档（2026-08-16 第 20 轮迭代 / round 96 起点）

> 面向后续 agent 的完整上下文。本轮为 **100 轮冲刺收尾轮**（round 96-100，阶段 F）：
> round 95 已完成（bundle 死锁修复 + SceneOutline 虚拟化 + audio UI 集成 + expr 性能守卫 + debug 边界），
> 100 轮冲刺接近尾声（95/100 完成）。
> **先读 AGENTS.md（模块边界铁律）+ 本文件 + ROADMAP-100.md（轮次记录权威在 plans/audit/ROADMAP-100.md）。**

---

## 1. 项目状态与当前基线（round 95 / 2026-08-16）

| 维度 | 基线 | 说明 |
|---|---|---|
| C++ 用例 | **908/908**（round 95 起 test_debug_manager_edge 加入，891→908） | 全量构建零错误（Debug） |
| Lua 用例 | **126/126 + 20 孤儿** | 主套件 126 全过；run_orphan_tests.lua 20/20（每测试子进程隔离） |
| Web (web/) | **~237**（vitest + 集成测试，含 bundle sweep/audio-ui 等） | round 93=216，round 94-95 累积增长 |
| Editor (editor/) | **467/467** | tsc 干净；round 95 SceneOutline 虚拟化 453→467 |
| ctest | **10/10 + AI smoke 跳过** | headless_http_smoke / headless_rpc_smoke / LSP 冒烟等 |
| 耦合 / 覆盖 | **PASS** | count_coupling.py --ci；check_test_coverage.py（无注册漂移） |
| 里程碑 | **90% 完成**（round 90）；已到 round 96（95/100 完成） | 100 轮 sprint 收尾阶段 F |
| CI | 三平台 CI 此前已绿（round 60 push + 多次修复后 5/5 job PASS） | Windows D+R / macOS / Linux / Package |
| 契约命令 | **118/118 运行时覆盖 100%**（round 90 S2v） | command-contracts.md 权威（最新见 api/command-contracts.md） |
| 能力矩阵 | **79 项能力**（round 90 刷新引入 S2v + C10） | docs/design/engine-capability-matrix.md |
| 教程库 | **15 个**（tutorial_01–15） | demo/tutorial/*.ks，引擎 + Web 双验证 |
| story bundle | **20 场景** | cache/story/story.lua；ks_bake --web 导出 |

> 上一权威交接：`docs/plans/2026-08-14-019-delivery-handoff.md`（round 89 状态）。
> round 90-95 的逐轮明细与提交见 ROADMAP-100.md；能力矩阵当前态见 engine-capability-matrix.md。
---

## 2. 近期完成（round 90-95 摘要）

| 轮次 | 里程碑/主题 | 关键内容 |
|---|---|---|
| **90** | **90% 里程碑** | 编辑器面板集成 E2E（editor 402→408）；**契约运行时覆盖 100%**（118/118，12 个死 handler 补 test_contract_runtime_gaps.lua）；web E2E 冒烟 +5（修 syncAudioStatus bus.se 单对象 filter bug）；教程 14/15；resource 二轮（ProviderChain 抛错传播、AsyncLoader 缺 in-flight 去重记录）；发布流程实测（Release + CPack ZIP 87.9MB） |
| **91** | 硬化二轮 | **ProviderChain 异常隔离修复**（每 provider try/catch 回退）；**AsyncLoader in-flight 并发去重**（per-(path,type) 表）；AiPanel 深度 +17；**web i18n 全链路 parity +7**（修 web 从不加载真实 assets/lang 退化内置词典；修 setLanguage 只切 current 不重绘）；纹理预算二轮 +6；能力矩阵 61→79 |
| **92** | 编辑器设置 + LSP rename | **编辑器设置面板 +15**（SettingsPanel.tsx + lib/settings.ts，主题 dark/light、字体/行号、localStorage）；**LSP rename 深度 +52**（101→153）；**backlog/rollback 深度 +9/59**；**EngineConfig 边界 +9/48**；API 文档深度审计；**发现 api_stats.py 纯虚方法 regex 漏计 const 方法** |
| **93** | LSP + api_stats + 播放器接线 | **LSP rename 原点导航集合修复**（definition 加第 4 参 navSet）；**api_stats const 方法修正**（278→379，ITextureBudget 2→7）；**播放器设置 UX 接线 +17**（textSpeed/skipMode 双向接线，音量单向锁定；记录 wasmoon Lua false→null）；input/platform 二轮 +6（**记录 NullPlatformBackend::resizeWindow 不 gate m_initialized**）；gen-index --check 三平台 CI 守卫 |
| **94** | Null resize + Monaco + settings + bundle sweep + storage 并发 | **NullPlatformBackend resize gate 修复**（round 93 记录项）；**Monaco 契约深度 +13**（EditorArea.test 11→24；修 hardcode 2049=Ctrl+Backspace 应 2097）；**settings/config 深度 +74**（孤儿 19→20）；**bundle sweep 真实守卫**（发现 full_pipeline_demo/tutorial_07 bundle 路径 runFromBundle 死锁，留 round 95）；**storage 并发 +9/69**（SaveManager 无主线程守卫记录）；指南二次审计；**主代理 CI 修复：CRLF 根因（.gitattributes 无 *.json 规则）+ SceneTree.test.ts jsdom 头** |
| **95** | **bundle 死锁修复 + 虚拟化** | **① bundle 路径 [save]/[load] 死锁修复（round 94 真 bug）**：根因 scheduler 在 handler 后写 token_index（[save] 捕获前一 token）+ 同场景紧邻 [save]→[load]；修复 load resume 分支推进光标越过当前 [load]；sweep 恢复全部 20 场景（撤销 round 94 排除），92/92 全绿；**② SceneOutline 虚拟化 +14**（固定行高+scrollTop 窗口化：flattenOutlineRows/getVisibleWindow/scrollTopToRevealRow 纯函数；editor 453→467）；**③ web audio UI 集成 +5**；**④ expr 规模化性能守卫 +4/9**（**记录：深层嵌套三元病态 O(n³) N=100 达 9.3s，已知病理不作基准**）；**⑤ debug/log 边界 +17/65**（test_debug_manager_edge.cpp，C++ 891→908；**发现 2 真 bug：SubSys 枚举 12 值 vs m_errorCounts array<7> 静默丢弃、beginFrameProfile 不重置 luaGcMs 跨帧泄漏**）；⑥ ROADMAP 审计 93-94（修正 round 94 ④排除场景数 18→16） |

---

## 3. 剩余轮次计划（round 96-100 冲刺）

| 轮次 | 计划主题 | 说明 |
|---|---|---|
| 96 | **debug bug 修复首轮** | 优先闭环 round 95 记录的两个不变量破坏：**DebugManager SubSys 枚举 12 值 vs m_errorCounts array<7>**（Live2D/MiniGame/Storage/Resource/Archive 计数静默丢弃→补全）+ **beginFrameProfile 不重置 luaGcMs 跨帧泄漏**（补 begin/end 对称复位 + 针对性测试）。其余 debug 边界同步 |
| 97 | **debug/expr 深度 + 性能** | expr O(n³) 病态（深层嵌套三元 9.3s@N=100）——评估加深度护栏或按实际场景缓存的渐进策略，不硬优化为 O(n)；补 debug 其余子系统统计覆盖 |
| 98 | **更多深度可选面** | 素材池候选：宏系统两缺陷（ctx._macroExpansions 只增不重置致 1000 误报；宏体内嵌宏定义收集残缺）、editor SceneTree 潜伏缺陷修复后的稳定性复验、web backlog 上限探针 |
| 99 | **G12/G13 收尾** | 文档同步（能力矩阵刷到 round 96-99）、dead code 清理抽查、耦合再收敛确认、release-process 复核 |
| 100 | **100 轮总结报告** | 生成 docs/plans/ 的 100 轮总结（覆盖 90 轮主线 + 阶段 A-F 成果 + 缺陷闭环清单 + 交接给后续 campaign），统一 push + 一次三平台 CI |

> 阶段 F（round 91-100）收尾原规划：G12 代码质量 / G13 文档同步 / G14 发布流程 + 100 轮总结报告。
> round 90 已实测发布流程（Release+CPack），故 91-95 改走硬化/深度/性能路线；96-100 回归 G12/G13 主线并收官。
---

## 4. 关键技术/踩坑（务必记住，含 round 94-95 新增）

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

### 4.3 Shell 与编码（round 54-95 高频坑）
- **git bash 强制**：路径一律正斜杠 /；反斜杠会被 bash 吃掉（工具名的反斜杠路径 externalua 会变不存在）。工具显示名 pwsh 但后端实为 bash。
- **PowerShell > 重定向默认 UTF-16LE**：生成 markdown 会写坏文件（read 报 binary、Lua find 失效）。生成文档用 Set-Content -Encoding utf8 或 python 写。
- **CRLF 陷阱（round 94 CI 根因）**：.gitattributes 若无对应规则（如 *.json），Windows checkout 变 CRLF，生成的 index/story 等产物 git diff 红。改生成脚本前先确认 .gitattributes 覆盖该扩展名。
- **taskkill 铁律**：DSH 宿主就是 node.exe，**绝对禁止 taskkill //F //IM node.exe**（会连同当前会话一起杀）。清理残留进程必须先按 CommandLine 精确识别（vitest 等），逐个 taskkill //F //PID <pid>，命令行含 dsh/npx-cli.js @deepseek-ai/dsh 的绝不能杀。
- git commit -m 消息含 ${...} 会被 shell 展开（bad substitution）——用 bash 单引号包 -m。

### 4.4 Lua/表达式（round 50-95）
- **Lua 语言限制**：表构造器不能直接索引（`{1,2}[1]` 语法错误，需 `({1,2})[1]`）；插值表达式 `${ {a=1}.a }` 求值前须给前导构造器加括号。
- **表达式翻译管线**：三元在 [ ]/括号内从内向外预翻译（translate_brackets/translate_parens）；[eval] 需运行时 translateOperators；?? → or（字符串字面量内不翻译）；插值用平衡花括号扫描器 match_brace（quote 感知，未闭合保留原文）。
- **迭代守卫**：while/for/until 均有每场景 65536 帧护栏（WHILE_MAX_ITERS）；大 timeout + 永不真条件不再无限轮询。
- **宏系统两缺陷（待修）**：ctx._macroExpansions 只增不重置（合法调用累计>1000 误报）；宏体收集遇第一个 endmacro 即停（嵌套宏定义不支持）。
- **wasmoon（web）**：Lua false 经 lua.global.get 映射为 null（布尔断言须显式 ==false）；JS 代理返回值是 userdata 会破坏引擎 type 检查（用 Lua 字面量桥 + load() 解析绕开）。

### 4.5 round 94-95 新增关键技术
- **bundle save/load 自我引用死锁**（round 95 修复）：scheduler 在 handler 执行后写 token_index（[save] 捕获前一 token）+ 同场景紧邻 [save]→[load] 恢复点在前 → 重跑 save/load 块死循环。修复 load resume 分支：current_scene==_pendingLoadScene && _pendingLoadToken<=token_index 时推进光标越过当前 [load]（跨场景 load 不变）。
- **sceneSources 假象**：runScene 未传 sceneSources，load-resume 不进入静默 no-op——source 路径看似正常实为未覆盖到 bundle 路径；bundle sweep 才是真实守卫。
- **DebugManager SubSys vs errorCounts 数组失配**（round 95 记录待修）：SubSys 枚举 12 值 vs m_errorCounts array<7>，Live2D/MiniGame/Storage/Resource/Archive 子系统错误计数静默丢弃。
- **beginFrameProfile 不重置 luaGcMs**（round 95 记录待修）：跨帧 GC 计时泄漏，污染后续帧统计。
- **expr 病态 O(n³)**：深层嵌套三元（N=100 达 9.3s）——已记录**已知病理不作基准**，热路径正常规模化守卫（500 扁平链式三元 0.52s、300 插值段 0.50s）已设护栏。
- **api_stats.py const 方法漏计**（round 93 修复）：纯虚方法 regex 须匹配 (?:const )?= 0;。

---

## 5. 待办 / 已知缺陷列表

| 项 | 状态 | 说明 |
|---|---|---|
| **DebugManager SubSys 数组失配** | **待修（round 96 优先）** | SubSys 枚举 12 值 vs m_errorCounts array<7>，5 个子系统错误计数静默丢弃（round 95 发现） |
| **beginFrameProfile 不重置 luaGcMs** | **待修（round 96-97）** | 跨帧 GC 计时泄漏，污染后续帧统计 |
| **expr 深层嵌套三元 O(n³)** | 记录待评估 | N=100 达 9.3s；已知病理，不作基准，round 97 评估加深度护栏或策略 |
| **宏系统两缺陷** | 待修 | _macroExpansions 只增不重置致 1000 误报；宏体内嵌宏定义收集残缺 |
| **Live2D GL/Steam 实机** | **待设备** | GL 需 Linux/macOS 硬件；Steam 需开发者账号 |
| **Metal 后端真机验证 / 移动真机** | **待设备** | macOS 硬件 / Android/iOS 设备；管线与文档已就绪 |
| settings.reset 不写 localStorage | 记录（设计取舍） | web 播放器音频设置 |
| config.ensure_dir 无法从零建 settings/ | 记录 | C++ 侧预建故运行时无感 |
| web backlog 无上限（桌面限 500） | 记录 | round 92 500 页探针证明；属语义差异非缺陷 |

---

## 6. 门禁（每轮强制，见路线图 §5）

全量重建零错误 → C++ 908/908 → Lua 126/126 + 孤儿 20/20 → ctest 10/10（+AI smoke 跳过）→ 耦合 PASS → 覆盖检查 PASS → 语义提交。
推送策略：多轮本地累积语义提交，到目标节点（如 100 轮总结）统一 push + 一次三平台 CI。

## 7. 注意事项

- **bundle sweep 是 bundle 路径真实守卫**：round 95 已恢复全部 20 场景（撤销 round 94 排除）；改 scheduler/compiler 涉及 [save]/[load]/恢复场景语义时，务必跑 web sweep + flow 全绿（92/92）。
- **编辑器前端/Web 改动**：editor/src（tsc + vitest）、web/（vitest + flow + sweep）；产物 editor/dist、web/dist 不入库（既定规则）；vite build 用 es2022 顶层 await。
- **修改生成脚本前确认 .gitattributes 扩展名规则**（round 94 CRLF 根因）。
- **文档权威性**：ROADMAP-100.md = 轮次记录权威；本 handoff = 交接现状；engine-capability-matrix.md = 能力矩阵（本轮后可刷新到 round 96-99）。
- 历史交接：`2026-08-14-019-delivery-handoff.md` 为上一权威状态（round 89）。