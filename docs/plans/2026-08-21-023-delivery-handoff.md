# Caesura (AmeKAG) — 交接文档（2026-08-21 第 23 轮迭代 / round 121 完成态）

> 面向后续 agent 的完整上下文。承接 022（rounds 116-117 v1.0.1）——023 记录 **rounds 118-121（产品化 Sprint 1-3，2026-08-21 完成）**：
> 流程从「堆 Runtime 功能」正式转入「产品化推进」（总任务书 2026-08-21 入库）。**先读 AGENTS.md（模块边界铁律）+ 本文件
> + ROADMAP-200.md（round 101 起）**。

---

## 1. 项目状态与当前基线（round 121 完成态 / 2026-08-21）

| 维度 | 基线 | 说明 |
|---|---|---|
| 版本 | **v1.0.1**（CMake + tag + GitHub Release 正式版，round 117） | rounds 118-121 为 1.x 稳定化 + 产品化基础设施，不晋升版本 |
| C++ 用例 | **987/987** | rounds 118-121 稳定（118 加固、119-121 为文档/editor/CI，无 C++ 新增） |
| Lua 用例 | **132/132 + 24 孤儿** | 主套件 132 全过 + 孤儿 24（含 SMA 源码守护） |
| Web (web/) | **298/298（20 文件）** | vitest 全绿 |
| Editor (editor/) | **579/579** | vitest 全绿（round 120 +24 Project Manager / round 121 +18 Asset+Drop；同比 022 的 530） |
| HTTP smoke | **54/54** | tests/headless_http_smoke.py 全过（含 project create/duplicate/invalid-name/path-traversal/list） |
| Golden Project | **18/18** | verify_golden_vn.sh（ks_check 零警告 + headless DONE + 2 分支 + 14 feature） |
| ctest | 10/10 + AI smoke | headless_http/rpc/LSP 冒烟惯性通过 |
| 耦合 / 覆盖 | **PASS** | count_coupling.py --ci |
| 契约命令 | **123**（运行时覆盖 100%） | command-contracts.md 权威（只增不删，compatibility.md 承诺） |
| 接口普查 | **31 接口 / 390 纯虚方法** | api-stats.md（自动生成） |
| 能力矩阵 | **82 项** | engine-capability-matrix.md |
| 教程库 | 16 个 | tutorial_01–16 |
| 示例游戏 | 《单程回信》三结局 | verify_sample_game.sh 5/5 + Web 站 31.08MB |
| CI **（round 121 头）** | **三平台 success**（run 32460009896） | Windows D+R / macOS / Linux / Package；golden step 三平台登记（见 §3） |

> 上一权威交接：`docs/plans/2026-08-16-021-delivery-handoff.md`（round 100 起点）→ `022-delivery-handoff.md`（round 117 完成态）。
> round 101-115 阶段 G 明细见 ROADMAP-200.md；round 116-117 见 022；round 118-121 见本文 §2。
> **产品化总任务书**：`docs/plans/audit/Caesura-AmeKAG_产品化推进总任务书.md`（28 节，2026-08-21 入库；战略转向/阶段路线/优先级/Sprint 排期/Sprint4-9+ 候选/六误区权威）。

### 1.1 本阶段定位（产品化推进）

- **战略转向**（总任务书 §1.2）：Runtime 已完整（v1.0.0/1.0.1 发布），从「堆 Runtime 功能」转向「把 Runtime 变成别人能学会/开发/调试/打包/发布/维护的产品」。最终定位 = 现代化 KAG Runtime，面向程序员与独立团队（**非**另一个 Ren'Py、**非**零代码拖拽第一目标）。
- **阶段路线**（禁止跳跃）：Phase0 1.x 稳定化 → Phase1 Caesura Studio（MVP）→ Phase2 Release/Distribution → Phase3 第三方开发者验证（5-10 名陌生用户，Time to First VN ≤ 30 分钟）→ Phase4 官方旗舰作品（30-90min/2-4 角色/多结局/语音/Live2D/Steam 成就）→ Phase5 Plugin/Community Ecosystem。
- **完成态对照**：Phase0 的 1.x 稳定化已落地（compatibility.md + golden_vn + release-gate.md，Sprint 1）；Phase1 Studio MVP 已开工（Sprint 2/3 完成 Project Manager + Asset Browser，接下来是 Script Editor 补齐/Scene Preview/Build Manager）。
- **决策自问**（总任务书）：每一步是否让陌生开发者「从下载到发布首款 VN」的路径更短更稳？

---

## 2. 近期完成（rounds 118-121 摘要）

| 轮次 | 里程碑/主题 | 关键内容 |
|---|---|---|
| **118** | **low 级加固收尾（6 项全落实）+ 交接 022** | ①A-4 CryptoEngine sign/verify 长度校验（Ed25519 私钥=64B fail-closed）；②A-5 CARC 索引 nonce 派生 **[version 4B][sha256(公钥)前 8B]** 消除 GCM nonce 复用（writer / reader 两侧严格一致），CARC 45/45 归档测试绿；③R-2 RpcServer parseId int64 累积 + INT_MAX 钳制（消 UB）；④R-3 EditorServer 已有 loopback + bearer token 门禁（补注释）；⑤ST-3 LocalFileSaveProvider 原子写（tmp+rename）+10MiB 上限；⑥RE-2 ImageDecoder fromBimg m_size 覆盖校验。**022 交接文档发布（rounds 116-117 v1.0.1 状态）**。门禁：987/987 + Crypto 24/24 + Lua 132+24 + web 298 + editor 530 + 耦合 PASS |
| **119** | **产品化 Sprint 1（1.x 稳定化，任务书 Phase0）** | ①**docs/compatibility.md（251 行 9 节）**——KAG3 兼容范围（13 families/TJS 表达式/旧变量/[elsif]/[call]/[end]→ending/[goto]→jump）、Neo-Genesis syntax 稳定性（123 契约只增不删）、save 兼容（AES-GCM CAES 信封 v1→v5 迁移链 + Golden Save）、project/Lua 兼容、版本迁移 1.0→1.1→1.2→2.0、breaking policy（含 exception 列表）；②**Golden Project（tests/projects/golden_vn/）**——story.ks（对话/选择/存档/NVL/tween/layout/i18n 热切换/audio/[if]/转场/end，双语）+ entry.lua + README + scripts/verify_golden_vn.sh（18/18）；头部注明阻塞式 [history]/[replay]/[tween] 由 headless 非阻塞替代（v1.0.0 死等教训）；③**docs/guides/release-gate.md（214 行 9 节）**——Release Gate 签署清单（Runtime/Platform/Packaging/Editor/Sample/Docs/Golden Project/Golden Save/失败 Red-blocker/快速核对），每项 checkbox + 验证命令（三平台真机如实标注待设备）；④**CI 三平台登记 golden step**（Windows vendored lua + Linux lua5.4 + macOS lua：ks_check + headless 全跑）；⑤总任务书入库 docs/plans/audit/ |
| **120** | **产品化 Sprint 2（Project Manager + Template，任务书 §6.3）** | ①**后端 EditorServer 新增 4 端点**——GET /api/project/templates、GET /api/project/list（扫描 ./projects/，含 catch(...) 硬防护防 500）、POST /api/project/create（名称 sanitize [A-Za-z0-9_-] 防空穿越/非法名 400/重名 409/未知模板 400）、POST /api/project/duplicate（404/409）；路径全部相对化规避中文 cwd 序列化问题；②**5 模板**（tools/project_templates/{blank,basic,live2d,kag3,showcase}，差异化 36-111 行 story + README + entry.lua 指向自身 + manifest.json）；所有模板 ks_check 零错 + headless 全 DONE（kag3 修 [set] 契约 var=/value=）；③**editor Project Manager 前端**——rpc.ts 4 方法、store 加 projects/templates/recentProjects/client + loadProjects/loadTemplates/addRecentProject、新建 ProjectManagerView.tsx（模板选择/新建表单/项目列表 Open+Duplicate/最近项目）、recentProjects.ts 持久化（RECENT_LIMIT=20 localStorage 防御读写）、ActivityBar 加 🗂 Projects；editor vitest 561 全绿 + tsc clean；④**HEAD/smoke 54/54**；⑤**踩坑**：headless_http_smoke 得 500 实为引擎进程占 exe 致 LNK1168 构建失败（假 500，需 taskkill 引擎再重建）；[set] 旧式 f.x= 需 var=/value= |
| **121** | **产品化 Sprint 3（Asset Browser + Editor 拖放，任务书 §6.4/6.5）** | ①**Asset Browser（ExplorerView.tsx）**——类型过滤按钮（All/Images/Audio/Scripts，与文本 filter 叠加）、图片缩略图占位（按 kind 配色 THUMB_COLORS）、音频试听按钮（new Audio + catch 降级「不可用—引擎无媒体服务端点」）、draggable dataTransfer；纯函数 filterByType（独立 lib/assetFilter.ts）；②**Editor 拖放（EditorArea.tsx）**——接收 ExplorerView 的 application/x-caesura-asset 载荷，脚本拖入自动打开为 doc（.ks→kag/.lua→lua），图片/音频转提示；纯函数 parseAssetDrop（独立 lib/assetDrop.ts 避免 Monaco import 需 DOM 的坑）；③**测试**：assetFilter 6/6 + assetDrop 5/5 + ExplorerView 16/16，editor 579/579 全绿 + tsc clean，web 298/298；④**踩坑**：Monaco 组件测试 collection 需避免 import EditorArea（clipboard API jsdom 缺 queryCommandSupported）——parseAssetDrop 抽独立 lib 规避 |
### 2.1 提交轨迹（round 118-121，master）

```
fix(archive): A-4 sign/verify + A-5 index nonce binds key hash      (round 118)
fix(rpc): R-2 parseId clamp, R-3 auth gate comment                  (round 118)
fix(storage,resource): ST-3 atomic save + cap, RE-2 bimg span       (round 118)
docs: handoff 022, roadmap round 118, AGENTS ref                    (round 118)
docs: productization master task book (28 sections)                 (round 119)
docs: compatibility policy (KAG3/Neo-Genesis/save/project/Lua)      (round 119)
test(golden): Golden Project regression fixture (18/18 gate)        (round 119)
docs: release gate checklist (task book §21)                        (round 119)
ci: run golden project ks_check + headless on all three platforms   (round 119)
feat(editor-rpc): project manager endpoints (templates/list/create/duplicate)  (round 120)
feat(templates): 5 project templates + manifest                     (round 120)
feat(editor): Project Manager panel (rpc+store+view, recent projects) (round 120)
test(rpc): project endpoints in HTTP smoke (54/54)                  (round 120)
docs(api): regenerate census 25->29 HTTP endpoints                  (round 120, CI fix)
feat(editor): asset filter + drop-parse pure helpers                (round 121)
feat(editor): Asset Browser type filters, thumbnails, audio preview (round 121)
feat(editor): drag-and-drop asset into EditorArea opens scripts     (round 121)
docs(roadmap): record round 121 Sprint 3 Asset Browser + Editor drag-drop (round 121)
```

---

## 3. 已建立的产品化基础设施（可复用资产）

本轮的产出都是**长期可复用资产**，后续 Studio / Release / 第三方验证直接在其上扩展：

| 资产 | 位置 | 说明 / 复用点 |
|---|---|---|
| **Compatibility Policy** | docs/compatibility.md | 1.x 权威兼容承诺（KAG3 范围 / Neo-Genesis 稳定 / save / project / Lua / 迁移 1.0→2.0 / breaking 流程）。**任何契约/存档变更须对照此文件** |
| **Golden Project** | tests/projects/golden_vn/ + scripts/verify_golden_vn.sh | 长期发布回归夹具（18/18 门禁）。**所有改动跑它防回归**；CI 三平台已登记 golden step |
| **Release Gate** | docs/guides/release-gate.md | 正式版本硬性签署清单（Red-blocker）。发布前逐项勾选，三平台真机覆盖如实待设备 |
| **5 项目模板** | tools/project_templates/{blank,basic,live2d,kag3,showcase}/ + manifest.json | Project Manager 新建项目来源；也是独立可复用的起步脚手架（差异化路由到各自 entry.lua/story.ks） |
| **4 Project 端点契约** | src/rpc/EditorServer.cpp（project 端点段，EditorServer 定位为编辑器中立可自由演化） | templates / list / create / duplicate；名称 sanitize + 路径相对化 + 路径穿越防护；Editor 内部 RPC 不在引擎 DTO 链 |
| **recentProjects 持久化** | editor/src/lib/recentProjects.ts | RECENT_LIMIT=20 localStorage 防御读写；store 统一提供 addRecentProject |
| **Asset libs** | editor/src/lib/assetFilter.ts + assetDrop.ts | 纯函数过滤/拖放解析，独立 lib 规避 Monaco/DOM 依赖坑，vitest 可测 |
| **Project Manager + Asset Browser 前端** | editor/src/ide/ProjectManagerView.tsx + ExplorerView.tsx | Studio 的 Project/Asset 两大面板骨架，后续 Script Editor/Scene Preview/Build Manager 接入同一 store/rpc |

### 3.1 复用规则与设计要点

1. **「Editor-internal RPC 可自由演化」是刻意设计**：project 端点实现在 EditorServer 而非引擎 RPC DTO 链（compatibility.md 不覆盖 Editor-internal），后续编辑器端点扩展不违背兼容承诺。
2. **路径一律相对化**：所有 project 相关序列化用相对路径（projects/<name>），规避中文 cwd 序列化问题；模板路径 confine 到 tools/project_templates（拒绝 .. / 绝对路径 / 盘符）。
3. **新建项目骨架 = 5 模板的 entry.lua 自探测多路径回退**（继承 round 113 demo/template 的资产策略）：克隆即跑通，无需改路径。
4. **golden_vn 是「所有改动的回归锚」**：任何涉及 scheduler/compiler/引擎命令/存档/tween 的改动，跑 bash scripts/verify_golden_vn.sh（18/18）防回归；CI 三平台已自动执行。
5. **census 是 CI 的 freshness 门禁**：改接口/端点/Lua 注册后必须重跑 `python scripts/api_stats.py` 并提交 diff，否则 CI「Generated docs freshness」红（round 120 实测）。

---

## 4. 当前待办 / 下一步（按总任务书 Sprint 4+ 候选）

> 总任务书 §23 推荐顺序：Sprint1 稳定性（**完成**）→ 2 Project Manager+Template（**完成**）→ 3 Asset Browser+Script Editor（Asset Browser **完成**；Script Editor 复用现有 LSP/RPC/debugger，尚未独立建设）→ **4 Scene Preview+Debugger** → 5 Build Manager → 6 跨平台 → 7 Steam → 8 第三方测试 → 9+ 旗舰+生态。
> 优先级：P0（影响已有游戏/安全/存档损坏/发布 blocker）> P1（第三方无法完成核心流程/Editor·Build·Packaging 阻塞/跨平台真实验证缺失）> P2（文档与开发体验/性能/API polish）> P3（新 feature）> P4（实验）。新 feature 与 Studio/Packaging/Stability 冲突 → **优先 Studio/Packaging/Stability**。

| 候选 | 任务书节 | 内容 |
|---|---|---|
| **Sprint 4 · Scene Preview + Debugger** | §6.6 | run current scene / hot reload / debugger / inspect / breakpoints（复用现有 LSP/RPC/Script-editor debugger） |
| **Sprint 5 · Build Manager** | §6.7 | Run / Build / Package / clean build / error reporting（一键，无 CMake/bgfx/SDL 概念） |
| **Sprint 6 · 跨平台** | §7 | Windows（portable/CPack/game-only/dev/Steam 包）+ Linux（AppImage/Steam/Deck）+ macOS（.app/DMG/signing/notarization）+ Web（全浏览器）+ 真机硬件矩阵 |
| **Sprint 7 · Steam** | §8 | 从 API 存在→开发者能发布：AppID/Depot/builds/achievements/stats/cloud/branch/overlay/clean-machine 验证 |
| **Sprint 8 · 第三方测试** | §9 | 5-10 陌生开发者，Time to First VN ≤30 分钟，记录阻塞点（不允许作者自己验证） |
| **Sprint 9+ · 旗舰 + 生态** | §9+/§13 | 旗舰作品（30-90min/2-4 角色/多结局/语音/Live2D/Steam 成就）+ Plugin SDK（等核心稳定后再设计：可注册 commands/bindings/asset providers/render passes/audio/platform/editor extensions；需生命周期/权限边界/版本策略/可禁用） |

**022 §5 的 low 项 status（round 118 已全部收尾）**：A-4/A-5/R-2/R-3/ST-3/RE-2 六项全部 **已修复/已确认**（详见 §2 round 118；端口门禁 Crypto 24/24、CARC 45/45、Editor 25/25、RpcServer 26/26）。round 116 triage 文档记录的低级项已清零，**无遗留 low 项**。

### 4.1 与总任务书四个成功里程碑的对齐

| 里程碑 | 现状 | 差距 |
|---|---|---|
| ① 陌生开发者 30 分钟创建并运行含背景/角色/对白/选择/BGM/存档的 VN | Project Manager 新建 5 模板项目 + Runtime 可直接跑 | 差 Script Editor 直接编辑 + Scene Preview 即时反馈（Sprint 3/4） |
| ② 一键打包 Win/Linux/macOS/Web | package_game.sh（Web）+ CPack（Windows）+ release-gate.md 清单 | 差 Build Manager 一键 + 跨平台打包验证（Sprint 5/6） |
| ③ 能发布 Steam | ISteamBackend 接口存在（CAESURA_HAS_STEAM 条件编译） | 差 Steam 发布链路全流程（Sprint 7） |
| ④ 愿意做第二个作品 | 5 模板 + golden_vn + 教程 16 + 示例游戏 | 差社区/生态/文档体验打磨（Sprint 8/9+） |

---

## 5. 门禁（每轮强制）

全量重建零错误 → C++ 987/987 → Lua 132/132 + 孤儿 24/24 → web 298/298 → editor 579/579
→ **HTTP smoke 54/54 → Golden verify_golden_vn 18/18 → ctest（+AI smoke 跳过）→ 耦合 PASS → 覆盖 PASS → 语义提交。**

- 推送策略：多轮本地累积语义提交，到目标节点统一 push + 一次三平台 CI（每轮省约 15 分钟等待）。
- **freshness 门禁**：凡改接口/端点/Lua 注册数量，重跑 `python scripts/api_stats.py` 并提交 census diff（round 120 中间提交 CI 曾因此红）。
- **子代理纪律**（项目 memory，长期铁律）：每轮批量扇出 4-8 个子代理并行（拆文件集），主代理独占接口契约（api/I*.h、RPC 结构）、共享耦合点（main.cpp if-constexpr 链、BackendRegistry、kag/init.lua 预加载清单、tests/CMakeLists.txt）、门禁修复闭环、分语义提交与推送。子代理不碰主代理独占文件、不跑全量门禁、不提交。

---

## 6. 注意事项 / 踩坑

### 6.1 本轮新增（rounds 118-121）

- **假 500 = 引擎进程占 exe 致 LNK1168**（round 120）：headless_http_smoke 报 500 时先查是否为构建失败——JobSystem/引擎残留进程占用 exe 无法覆盖链接；需 taskkill 残留引擎进程再重建。
- **[set] 契约**：新式必须 var=/value=，旧式 f.x= 不再接受（round 120 kag3 模板修复）。
- **Monaco 组件测试抽 lib**（round 121）：EditorArea.tsx 引入 monaco 真实 ESM，Node env 加载必崩（window/queryCommandSupported 缺失）。**凡是 EditorArea 衍生的纯逻辑，抽到独立 lib（assetDrop/assetFilter）再测试**，避免组件 import Monaco。另见 memory：SceneTree.test.ts 必须保持 `@vitest-environment jsdom` 头（无头时拉入 monaco 必崩，现有全绿依赖 worker 复用掩盖，改动 EditorArea 一行即变确定性失败）。
- **CARC 索引 nonce 派生（A-5）读写两侧严格一致**：CARCWriter 与 CARCReader 必须同步改动，否则归档无法解密。
- **Golden 阻塞式命令 headless 死等**（v1.0.0 教训）：[history]/[replay]/[tween] 阻塞式在 headless 死等；golden_vn 与 verify 脚本用非阻塞替代 + DRIVER mock（is_voice_playing/is_bgm_playing 必须 false），否则播放命令死挂。
- **中文 cwd 路径相对化**：project 端点与相关序列化一律用相对路径，规避中文工作目录序列化问题；路径穿越用 name sanitize [A-Za-z0-9_-] + .. / 绝对 / 盘符拒绝。

### 6.2 长期踩坑（memory 提取）

- **shell 只用 git bash**（本会话铁律）：路径一律正斜杠；反斜杠会被 bash 当转义符吃掉（external/lua/lua.exe 写成反斜杠 → externallualua.exe）；带括号/空格的绝对路径在 cmd 分词会截断（孤儿 runner 的 io.popen 需 call "..." 包裹）。
- **写文件用 write 工具或 UTF-8 显式编码**：PowerShell 的 > 重定向默认 UTF-16LE 会写坏 markdown（read 报 binary、Lua find 失效）。
- **git commit -m 消息含 ${...} 会被 shell 参数展开**（bad substitution 整条失败）——bash 单引号包 -m 或转义 $。
- **main.cpp if-constexpr RPC 分支链**：改动时务必保留原分支 return 收尾，否则静默丢 return 导致运行期 0xC0000005（C4715 警告是信号）。
- **新增 kag 模块必须登记进 kag/init.lua 预加载清单**（sandbox require 只认 package.loaded）；创建全局 mock 的测试须登记 run_orphan_tests.lua（主套件沙箱中途锁全局）。
- **宏系统两缺陷待修**（round 101 锁定回归锁定，仍技术债）：1) scheduler.lua ctx._macroExpansions 只增不重置，合法宏调用累计>1000 误报 "expansion budget exceeded"；2) 宏内嵌套宏定义不支持（收集遇首 endmacro 即停）。
- **文档权威性**：ROADMAP-200.md = round 101+ 轮次记录权威；本 handoff = 交接现状；compatibility.md = 1.x 兼容承诺；总任务书 = 战略/路线/Sprint 排期权威；engine-capability-matrix.md = 能力矩阵（82）。**契约/接口数字以 api-stats.md（自动生成）为准。**
- **编辑器/Web 改动**：editor/src（tsc + vitest）、web/（vitest + flow + sweep）；产物不入库。
- **版本流程**：release-process.md §6 已实测 v1.0.1；gh release 直接正式发布（非 draft）。round 121 为产品化 Sprint，尚未晋升 v1.0.2+。
- 历史交接：022（round 117 完成态）→ 023（本文件，round 121 完成态）。第 4 份产品化交接（022 之后）。