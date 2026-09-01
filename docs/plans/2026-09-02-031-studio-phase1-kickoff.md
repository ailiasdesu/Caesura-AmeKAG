# Caesura Studio Phase1 立项草案（DRAFT）

> **状态：DRAFT——待用户评审**

> **本文档不授权任何执行；执行批需用户明确「执行」口令后由队长派发**。

> 输入来源：SA 资产盘点（t147，2026-09-02，browser-e2e-2，只读）/ SB 需求分解+缺口矩阵（t148，2026-09-02，只读）；本文档为 SC 综合（t149）。权威锚点：docs/plans/audit/Caesura-AmeKAG_产品化推进总任务书.md §6（行 234-376）与 §18 工作纪律（行 863-930）、里程碑第一/二（行 1387-1397）、推荐顺序 Sprint 2-5（行 1105-1166）。

## 目标与范围

**锚定任务书原文**（§6 行 234-236）：Phase 1「这是当前最高优先级」，即构建 Caesura Studio。§6.1（行 238-268）：用户打开后沿 New Project → 模板 → 自动创建 → 编辑脚本 → 浏览资源 → 预览场景 → 调试 → Build 走完全流程，**不需要理解** CMake / bgfx / SDL / Lua binding internals / C++ Runtime / 手动 DLL 拷贝 / 内部目录结构。

**北极星判据**（任务书行 1393）：

> 一个完全不了解 Caesura 内部架构的人，可以在 30 分钟内创建并运行一个包含背景、角色、对白、选择、BGM 和存档的 VN。

第二里程碑（行 1397）「一键打包到 Windows / Linux / macOS / Web」为 Phase2 出口，本草案仅至 Phase1 MVP（Build/Package 三键即止于 Phase1 范围，跨平台打包矩阵属 Phase2）。

**范围**：MVP 九视图（§6.2 行 272-301）对应的 R-PM/R-AB/R-SE/R-SP/R-BM 需求族（SB 分解 R-ID 见「需求分解与缺口矩阵」）。**本草案为 DRAFT**：只落目标、现状、候选、缺口、切片与决策位；不派发任何执行。

## 现状资产摘要

> SA 盘点表精编（t147 全表见 build/t147-studio-phase1-asset-inventory.md，file:line 保留）。成熟度分级：现成=代码已存在可复用 / 需改造=存在但缺接线或配置 / 缺口=不存在。

| 资产 | 位置 | 成熟度 | 关键证据 |
|---|---|---|---|
| React/Monaco IDE 前端（含五视图与 Inspector/Console/Debugger 等辅助视图） | editor/（91 tracked，src/ 84 文件） | 现成（待分发接线） | ProjectManagerView.tsx:111/:129/:159/:184/:203、ExplorerView.tsx:55、EditorArea.tsx:53-56（monaco+KagLsp）、DebugView.tsx:148-176、BuildManagerView.tsx:11-37 |
| 编辑器测试面 | editor/src 38 test 文件 / 605 it() | 现成 | ci.yml:133-144（仅 Win job 跑） |
| Studio 桌面壳 | editor/electron/main.cjs | 需改造（缺打包配置） | main.cjs:1-25；package.json:30-31 无 electron-builder build 配置 |
| 发布包编辑器 | web-editor/dist/index.html（11,011B，单文件调试面板） | 现成（仅调试用；非 React IDE） | CMakeLists.txt:545；EditorServer.cpp:1408-1409 |
| EditorServer HTTP RPC | src/rpc/EditorServer.cpp（1533 行） | 现成 | 36 条路由注册 :693-1503（/api/* 34 条：assets :725、reload :773、debug.* 10 条 :944-1313、build :1346、package.web :1366、project.* 7 条 :1449-1503；另有 / 与 /index.html 两条静态 :1408-1409）；token default-deny :639-680；绑 127.0.0.1:9876 |
| RPC 服务层 | src/rpc/services/（Asset 50/Project 550/Packaging 223 行） | 现成 | AssetService.h:19-20；ProjectService.cpp:259/:284-337 |
| stdio JSON-RPC | src/rpc/RpcServer.cpp（995 行） | 现成 | 29 方法 :542-597（含 kagDebug* 6 条） |
| 「LSP」（自研非标准协议） | scripts/kag/lsp.lua（33KB）+ editor/src/lib/kagLsp.ts | 现成（非标准 LSP wire） | lsp.lua completion:109/hover:200/diagnostics:258/definition:549；kagLsp.ts lspCall:49-58 |
| 调试器后端（非 DAP） | src/debug/DebugProtocol.{h,cpp} + scripts/kag_debug.lua | 现成 | DebugProtocol.h:2-8（lua_sethook）、:26-31（Command 四向）、:151（StepMode）；main.cpp:1170 |
| caesura CLI | scripts/caesura.py + caesura_build.py | 现成 | caesura.py create:198（5 模板 :200）/build:232-247/package:250-264；caesura_build.py cmd_build:838/cmd_package:888 |
| CLI ctest 覆盖 | tests/CMakeLists.txt | 现成 | CaesuraBuildCli :271-281（SKIP 77） |
| project templates | tools/project_templates/（5/5 零缺） | 现成 | manifest.json 5 条；每模板含 story.ks+entry.lua+assets/+caesura.project.json+README.md |
| Web 玩家/PWA 打包链 | web/ + package_game.sh + PackagingService | 现成 | web/package.json:2-18；POST /api/package/web :1366 |

**SA 关键差异结论（D 清单要点）**：D5——任务书 §6.5:338「利用现有 LSP」实为自研 kag.lsp Lua 服务+Monaco provider 桥（/api/eval），非标准 LSP wire（src/ 无 language server 进程）；若期待标准协议则属缺口。D6——**最大分发缺口**：React IDE 产物 editor/dist 被 .gitignore:35 忽略，CMake 只装 web-editor/dist 单文件面板：功能资产在场、分发接线缺失。其余 D1-D12 为文档计数陈旧/名实差异，不阻塞。

## 架构取向候选

> **前端双套框架（SA t147 盘点事实）**：仓内有两套前端，语境必须分清——
> ① **web-editor/** = 发布包现装的 **11KB 单文件调试面板**（CMakeLists.txt:545 install(DIRECTORY web-editor/dist/ ...)；非 React/Monaco，无编辑能力，仅调试）；
> ② **editor/** = **React/Monaco 五视图 IDE**（ProjectManagerView/ExplorerView/EditorArea/VisualView/DebugView/BuildManagerView 全在，91 tracked、38 测试文件、605 it()），**但 editor/dist 被 .gitignore 排除且无任何 CMake/npm 打包接线**。
>
> **SA 结论（本架构节立论前提）**：Studio 第一缺口 = **前端分发接线**，不是功能实现——功能资产（MVP 九视图 + 605 it 测试面）已在 editor/ 在场，缺的是「把它装进产品」的打包链。

> 不拍板——三候选各列证据、利弊与规模差，**最终取向留用户决策**（见「开放决策位清单」第 1 位）。

### 候选 A：editor/ 分发接线（功能面已在，补打包链；推荐方向，待用户确认）

- 证据：editor/ 已含 MVP 五视图+辅助视图（SA A1：ProjectManagerView.tsx:111 等全在）、605 it() 测试、Monaco+LSP 桥、electron/main.cjs 桌面壳；复用面覆盖 R-PM/R-AB/R-SE/R-SP/R-BM 前端绝大多数视图。
- 利：资产已在场、测试面大、无重写成本；前端验证周期短（vitest 605 it）；与 SA「第一缺口=分发接线」结论一致——补缺即达 Phase1 前端目标。
- 弊：editor/dist 被 gitignore 且无 CMake 接线（SA D6）——需新建打包链（前端 build 产物进 CMake install/发布包 + electron-builder 配置）；与发布包单文件面板（web-editor/dist 11KB）并存——需明确产品上「Studio 编辑器」与「发布包内置调试面板」的关系或合并路径（web-editor 是 Studio 的精简版还是独立物）。
- 规模差：主要是接线（打包链+electron-builder），非重写——三候选里最小。

### 候选 B：新壳（另一套前端技术栈重写）

- 证据：无现成资产（SA 表内无替代前端）；需重建 605 it 测试面与全部视图——即放弃 editor/ 现成功能。
- 利：可按 MVP 布局图（§6.2 行 287-301）从头设计，无历史包袱。
- 弊：放弃 editor/ 全部现成资产——与任务书 §18.2「优先复用现有基础设施」正面冲突；且不解决「分发接线」问题（新壳仍要接线），却额外承担全量重写。
- 规模差：整体重写（数量级高于 A），本草案不推荐，列入仅作对比。

### 候选 C：混合/渐进（editor/ 分发接线 + 标准 LSP/DAP 接口层渐进升级）

- 证据：A 的资产 + 任务书 §6.5「利用现有 LSP/RPC/debugger」字面（标准协议期望，SA D5 指出现为自研非标准 wire——kag.lsp Lua 服务+Monaco provider 桥经 /api/eval）。
- 利：先做 A（接线）保证 Phase1 按期，再渐进把 LSP/DAP 桥升级为可插拔标准协议——第三方编辑器（VS Code）未来可复用；RPC/debugger 保持现成（A5-A9）。
- 弊：接口层新增工作量（LSP wire/DAP 适配器）；与现有 kag.lsp 双轨并行需协调；属 A 的渐进增强而非独立路线——若产品判断「内嵌编辑器不要求标准协议」，则 C 降级为 A 的后期可选项。
- 规模差：A + 接口层（中等增量，可后置）；是否值得取决于「Studio 内嵌编辑器是否必须标准协议」这一产品判断（开放位#9）。

## 需求分解与缺口矩阵

> SB 分解表（t148）精编：R-ID | 任务书锚点 | 可测验收判据 | 规模 | 依赖。资产列以 SA 事实回填「现成/需改造/缺口」（SB 原为「待SA」占位处改注 SA 结论；未覆盖的补「待确认」）。

| R-ID | 锚点（任务书） | 判据（摘要） | 规模 | 依赖资产（SA 回填） |
|---|---|---|---|---|
| R-PM-1 New Project | §6.3:307 | 新建后含 entry.lua+story.ks+assets 骨架且可立即 Run | S | T2 caesura.py create（现成，模板 5/5） |
| R-PM-2 Open Project | §6.3:308 | 打开后 PM/AB/SE/SP 全组件挂载项目上下文 | M | 项目识别规范：**待确认**（SA 无独立规范资产） |
| R-PM-3 Recent Projects | §6.3:309 | 持久化列表+点击可 Open | S | 已核实端到端接线（D11 调查+队长锚点复核：ProjectManagerView.tsx:54/:375 → store.ts:112/:196-198 → lib/recentProjects.ts 含单测）——UI 层无缺口 |
| R-PM-4 Duplicate | §6.3:310 | 复制→重命名→资产引用不含旧名 | M | **需确认**资产引用面（SA 无专项盘点） |
| R-PM-5 Import | §6.3:311 | 选 KAG3 包→导入+成功/失败/迁移报告 | M | kag3_import 命令行面（**待确认**） |
| R-PM-6 五模板 | §6.3:316-320 | 五者可选可 New 带预览/描述 | M | tools/project_templates（现成 5/5） |
| R-PM-7 Project Settings | §6.3:312 | 读写项目级配置并持久化 | M | 项目配置 schema（**待确认**） |
| R-AB-1..6 目录树/预览/试听/搜索/过滤/元信息 | §6.4:326-332 | 各可执行且结果正确 | S-S × 6 | T8 RPC assets（现成 :725）+ 前端 ExplorerView（现成） |
| R-AB-7 引用关系 | §6.4:333 | 文本标「后续」 | — | **开放问题#2（SB）**：MVP 内 vs 后置 |
| R-AB-8 拖入场景/脚本 | §6.4:334 | 文本标「第一版可以后做」 | — | **开放问题#2（SB）** |
| R-SE-1..7 高亮/导航/引用/diagnostics/补全/definition | §6.5:342-348 | 编辑器中可执行且结果闭环 | M × 7 | T4 kag.lsp+kagLsp.ts（现成，非标准协议；标准协议→候选 C） |
| R-SE-8..10 breaks/step/变量 | §6.5:349-351 | 断点命中→单步→变量视图更新 | M × 3 | T5 DebugProtocol + kag_debug.lua（现成，非 DAP）+ RPC debug.*（现成 :944-1313） |
| R-SE-11 文件管理（tab/tree） | §6.5 未显式列 | — | M | **开放问题#7（SB）**：SE 内建 vs AB 双击 |
| R-SP-1..2 运行/重载 scene | §6.6:359-360 | 预览启动加载当前 .ks；重载生效 | M × 2 | T6 经 R-BM-1 面（现成 sceneRun）/ reload（:773） |
| R-SP-3 热重载脚本 | §6.6:361 | 脚本改动无需整局重启 | M | 引擎热重载面（**待确认**，SA 无专项） |
| R-SP-4 查看当前状态 | §6.6:362 | 变量/backlog/层可见 | S | RPC state（现成 :972） |
| R-SP-5 Inspector 参数 | §6.6:363 | 基础参数可调 | M | **开放问题#1（SB）**交叉 Inspector |
| R-SP-6 快速定位脚本来源 | §6.6:364 | 从预览跳转脚本 | S | **待确认** |
| R-BM-1..3 Run/Build/Package | §6.7:370-373 | 一键三键，不跑 shell | S+M+M | T7 CLI（现成 caesura.py/caesura_build.py）+ T8 build/package.web（现成） |
| R-BM-4 输出/日志面板 | §6.2:299 | 构建日志可见 | M | **开放问题#1（SB）**交叉 Console |

## 里程碑切片草案

> 对齐任务书推荐顺序 Sprint 2→5（行 1105-1166）。切片为演进假设；**是否按此切片执行、每批派发与否，全部留用户「执行」口令后由队长决定**。每条含「入口判据」：上一切片验收通过才进入。

- **切片 M1（Sprint 2：PM + Template）**：R-PM-1/2/3/6/7。入口=架构取向定案（开放位#1）+ 分发接线方案定案（开放位#3/6）。关键路径=editor/dist 分发接线（SA D6）+ electron 打包配置（SA B 壳需改造）。出口判据=R-PM 系列验收判据全过。
- **切片 M2（Sprint 3：Asset Browser + Script Editor）**：R-AB-1..6 + R-SE-1..11（SE-11 归位见开放位#7）。入口=M1 出口。关键路径=Monaco+LSP 桥复用（现成）+ 标准/自研协议取舍（开放位#4）。出口判据=R-AB/R-SE 判据全过。
- **切片 M3（Sprint 4：Scene Preview + Debugger）**：R-SP-1..6 + R-SE-8..10（debugger 在 SE 需求族，此处与 SP 联调）。入口=M2 出口。关键路径=sceneRun/reload/state RPC 复用（现成）+ DebugProtocol 联调。出口判据=R-SP 判据全过（R-SE 调试三项在 M2 验收）。
- **切片 M4（Sprint 5：Build Manager）**：R-BM-1..4。入口=M3 出口。关键路径=CLI（现成）+ build/package.web RPC（现成）+ Electron 桌面壳最终成型。出口判据=首个「不了解内部架构的人 30 分钟全流程」走查（北极星判据行 1393 的 Phase1 落地检查）。

## 开放决策位清单

> 全部需用户拍板；未定前各对应切片不得执行派发。

1. **架构取向**（§架构取向候选；据 SA 前端双套框架：web-editor/=11KB 单文件调试面板，editor/=React/Monaco 五视图 IDE 缺分发接线）：A（editor/ 分发接线，推荐）/ B（新壳重写）/ C（混合/渐进=editor/ 接线+标准 LSP/DAP 渐进升级）。留用户决策——本草案不拍板。
2. **Inspector 与 Console 是否纳入 MVP 分解与优先级**（SB 开放问题#1：本草案按五组件边界处理，Inspector 交叉 R-SP-5、Console 交叉 R-BM-4）。
3. **R-AB-7 引用关系 / R-AB-8 拖入**：MVP 内还是明确后置（任务书 §6.4 文本标注「后续」/「第一版可以后做」）。
4. **Scene Preview 形态**：引擎独立窗口（真 GPU）vs Studio 嵌入式画布（§6.2 布局图画的是嵌入式）——影响 R-SP 实现路径与验证面。
5. **模板展示名映射**：任务书名称（Blank VN / Basic VN / Live2D VN / KAG3 Migration / Advanced·Showcase）vs tools/project_templates 目录名（basic/blank/kag3/live2d/showcase）——UI 展示名需定。
6. **Studio 与 029 衔接**：029（M1-M5）收官后 Studio=下一优先级（任务书 §6 行 236）——是否在 029 文档加注软衔接。
7. **Studio MVP 目标机**：Windows 优先（任务书 Phase2 才跨平台；当前 electron 壳与 vite 链 Windows 已跑通）vs 三平台同期——影响验证矩阵与 electron 打包范围。
8. **Script Editor 文件管理归属**（SB 开放问题#7）：SE 内建 tab 树 vs AB 双击打开——决定 R-SE-11 落点。
9. **标准 LSP/DAP 期望**：任务书 §6.5:338 字面「利用现有 LSP」是否要求标准协议（异于自研 kag.lsp/kag_debug 非标准面）——决定候选 C 是否必要（开放位#1 联动）。

## 验证策略

**锚定任务书工作纪律**（§18 行 863-930）：18.1 先研究再改（读架构文档/真实调用链/已有接口/测试/兼容约束→再改；禁止平行架构）；18.2 优先复用现有基础设施；18.3 每个功能必须带验证（implementation + unit + headless + integration（适用时）+ documentation）；18.4 禁止只改 README 假装完成（无 code/test/validation 则必须标注 experimental / engine-side only / unverified / planned）。

**派生验证要求（每 R-ID 五件套）**：

- **impl**：按模块边界与接口规范实现（AGENTS.md）。
- **unit**：editor/src vitest 面（现成 605 it，ci.yml:133-144）或 C++ doctest（tests/CMakeLists.txt）。
- **headless**：引擎侧经 HeadlessCli（tests/CMakeLists.txt:178）+ golden 链（tests/projects/golden_vn/、scripts/verify_golden_vn.sh）；Studio 侧优先挂既有 RPC/CLI 面，避免平行入口。
- **integration（适用时）**：Studio 全链路经 EditorServer HTTP RPC（36 条路由，其中 /api/* 34 条）或 package_game.sh 链。
- **doc**：按文档分类入 docs/（§12 规则）；文档声称必须与代码实况一致——SA D1-D12 计数/名实陈旧，各切片交付时一并修正，禁止新文档再漂移。

**已知断点（如实记录）**：分发接线（editor/dist 未进 CMake/发布包，SA D6）与 electron 打包配置为 M1 前置缺门；未解决前任何「Studio 完成」声明不成立（§18.4 未验证即标注）。
