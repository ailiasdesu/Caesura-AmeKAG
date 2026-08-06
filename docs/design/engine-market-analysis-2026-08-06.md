# Caesura (AmeKAG) 引擎深度分析：架构、能力与市面 Galgame 引擎对比

> **日期**：2026-08-06（v2 刷新：本地代码重扫 + 市面引擎在线复核）
> **分析基准**：master 分支（HEAD e9b11b04）；本地代码 + `docs/` 文档证据 + GitHub API 在线核实。
> **前置文档**：`docs/design/engine-market-comparison.md`（2026-08-03 历史快照）——本文为其**详细化与数据更新版**（72 契约命令 / 54 能力 / 30 接口 / 52 测试文件）。
> **v2 修正**：① 测试文件数 57→**52**（`tests/cpp/` 实测）；② LICENSE 已存在（MIT, 2026-06-07, f7ee6184）——原"未见许可定论"有误，P0 清单已更新；③ 市面引擎数据全部经 GitHub API 实时复核，新增 WebGAL/Monogatari 两家。

---

## 一、Caesura (AmeKAG) 引擎盘点（本地证据）

### 1.1 架构：16 模块 · 30 接口 · 组合根 DI

**模块拓扑**（`src/` 全小写，证据：`ls src/`；构建层 `cmake/CaesuraModules.cmake:142–252` 15×`caesura_add_module` + `:313` CaesuraEntry = 16 个静态库）：

| 模块 | 职责 | API 接口（`src/<mod>/api/`，证据：glob 实测 30 个 `I*.h`） |
|---|---|---|
| archive | CARC 归档：压缩/加密/签名 | `IArchiveReader` `IArchiveWriter` `ICryptoEngine` |
| audio | SoLoud 三总线音频（BGM/Voice/SE） | `IAudioBackend` |
| debug | 结构化日志、帧剖析、DebugProtocol | `IDebugManager` |
| di | BackendRegistry + 配额/预算/设备丢失 | `ISandboxQuota` `ITextureBudget` `IDeviceLostListener` |
| entry | **组合根**（唯一可 new 具体后端） | —（允许 include 一切） |
| input | SDL 事件路由（KAG↔Game 焦点切换） | `IInputRouter` |
| job | 多线程任务系统（优先级/回调） | `IJobSystem` |
| live2d | Cubism 5 动画 / PNG 回退 | `IAnimationBackend` |
| minigame | 3D 小游戏（enter→update→render→leave） | `IMiniGameBackend` |
| platform | SDL3 窗口/事件/移动适配 | `IPlatformBackend` `IMobileAdapter` |
| render | bgfx 渲染：图层/粒子/文字/视频/GPU 恢复 | `IRenderDevice` `ITextureManager` `ILayerManager` `IParticleSystem` `IGpuMonitor` `IVideoPlayer` |
| resource | 异步加载 + Provider 链（Dir→CARC） | `IAssetProvider` `IAsyncLoader` `IResourceGenerationTracker` |
| rpc | 编辑器 JSON-RPC（HTTP + stdio） | `IEditorServer` `IRpcServer` `IRpcDispatcher` |
| script | Lua 5.4 VM + KAG 绑定 + GameState | `ILuaManager` |
| steam | Steamworks（条件编译） | `ISteamBackend` |
| storage | 加密存档 + schema 迁移 + 云 provider | `ISaveManager` `ISaveProvider` |

**DI 机制**：`BackendRegistry`（`src/di/`）存非拥有 `I*` 指针，仅 include 接口头；`src/main.cpp` + `src/entry/` 为唯一组合根，`Engine::init()` 四阶段注册。模块间禁止 include 具体实现头（AGENTS.md §1–3）。

### 1.2 能力矩阵：54 项能力 · 6 域（`docs/design/engine-capability-matrix.md`，2026-07-24 audit）

**就绪快照**（作者自评，刻意保守）：

| 层 | 完成度 | 说明 |
|---|---|---|
| 模块化架构迁移 | 98% | 静态库目标、接口隔离、组合根所有权 |
| 核心 VN 可用性 | 62% | 无头/KAG/存档/音频单测强；真 GPU、字体资产、端到端交互未全验证 |
| 跨平台产品发布 | 30% | 三平台 CI 构建有，但真 GPU 像素、可选 SDK、非 Win 包未发布级验证 |

**逐域状态**（54 项计数）：

| 域 | 总数 | ✓ | Partial/条件 | 关键项 |
|---|---|---|---|---|
| 渲染 R | 10 | 6 | 4 | ✓ 三层合成+脏矩形、异步纹理+预算+LRU、GPU 粒子、视频（pl_mpeg+FFmpeg）、GPU 降级、RTT、批量提交；Partial 多后端（D3D11 全、GL/Metal 未验）、CJK 文字边缘、非 D3D shader |
| 脚本 S | 17 | 17 | 0 | Lua 5.4 协程沙箱、KAG 72 契约命令、流控、指令预算、热重载、错误恢复、模态 UI、参数化宏、O(1) 标签索引、静态校验 |
| 音频 A | 4 | 4 | 0 | 三总线、淡入淡出、3D 音效、per-handle 控制 |
| 内容 C | 9 | 5 | 4 | ✓ 加密存档 AES-256-GCM、迁移 v1→v5、CARC+Ed25519、资产链；Partial/条件：Live2D（D3D11 已验证 2026-08-01，GL 未验/Metal stub）、minigame GPU、云存档远程、Steam |
| 开发工具 D | 7 | 7 | 0 | 编辑器 RPC 双传输、Lua 调试器、热重载、无头模式、帧剖析 |
| 平台 P | 7 | 6 | 1 | ✓ 三平台 CI、线程池、输入路由、纹理预算 6 档、沙箱配额；Partial MobileAdapter（无原生 SDK 集成） |

> **矩阵缺口备注**（本次扫描新发现）：54 项矩阵实际跟踪 30 个接口中的约 26 个——`IDeviceLostListener`（di）、`IArchiveReader`、`IAsyncLoader`、`IResourceGenerationTracker` 4 个接口未单独入矩阵（IMobileAdapter 已被 P7 覆盖）。不影响架构完整性，但矩阵完整性可后续补行。

### 1.3 脚本层：Lua 5.4 + KAG Neo-Genesis（核心资产）

- **KAG 契约命令 72 个 · 9 类**（`docs/api/command-contracts.md` 562 行；`scripts/kag/commands/` 实测 9 个文件：audio/layer/resource/save/system/text/transition/vfx/video；schema 侧 61 个 `schema.define` + kag.lua 13 个 − 重复 2 = 72 唯一命令，由 `scripts/schema_doc.lua` 自动生成防漂移）。
- **KAG3 兼容**：13 家族 15 命令裸位置参数（delay/wait/se/voice/play/jump/call/link/unlock/macro/erasemacro/save/load/gallery/ending）。
- **新一代特性**：声明式命令契约（typed params、钳制、`$var/${expr}` 插值）、参数化宏（%arg% 嵌套展开）、O(1) 标签索引、`ks_check` 静态校验（CI 门禁）、LPeg tokenizer（全解析 ~0.9s，文本批跳过 ~37x）。
- **混合脚本**：`.ks` 内嵌 `[eval]`/`[emb]`/`[iscript]…[endiscript]`，Lua 侧 `kag.jump/call/save_game` 反向回调。
- **安全**：指令预算防死循环（近两周新增宏展开预算 e9b11b04/b11ed15e）、pcall 错误恢复 + ErrorUI、io.open 白名单沙箱。

### 1.4 工程与质量（证据：`tests/CMakeLists.txt`、能力矩阵 P2）

- **52 个 `test_*.cpp`**（`tests/cpp/` 实测 glob 与 CMakeLists 列表完全一致，无漂移）→ **C++ doctest 576 用例 / 2828 assertions 全绿 + Lua 86/86 全绿**（2026-08-06 全量重建后实测，见文末验证记录）；测试与产品链接同一批静态库（非二编译）。
- CI：三平台（Windows MSVC / macOS Clang / Linux GCC）+ 耦合度门禁（`scripts/count_coupling.py --ci`）。
- 性能基准（`docs/plans/2026-08-04-006-perf-baseline-update.md`）：tokenizer 478–541ms、scheduler 4001 resumes ≈13ms（≈308k tok/s），较 08-03 基线快 8–19%。
- 迭代速度：截至分析基准 e9b11b04，2026-08-03 00:00 (+0800) 以来共 **561 提交**全 CI 验证（`git rev-list --count --since="2026-08-03 00:00:00 +0800" e9b11b04` = 561，可复现）。
- 近期提交（最近 30 条）全部为既有能力的审计加固：宏展开预算、history 恢复、gallery/ending/saveload 守卫、存储 GCM 篡改检测/越界槽位——**无新模块、新命令类别**，报告覆盖面与 HEAD 一致。

### 1.5 差异化亮点（相对市面引擎）

内置 Web 编辑器（18 HTTP 路由 + stdio JSON-RPC + Lua 调试器断点/步进/求值）、**token 级回滚**、结局/章节/画廊闭环、backlog 历史、打字机/auto/skip、KiriKiri 转场方法别名、CARC 打包 + Ed25519 防篡改、AES-GCM 加密存档 + 自动迁移、GPU 自适应降级、纹理预算 6 档自动探测、Live2D Cubism 5（Windows D3D11 已实测 Haru.moc3 加载渲染）。**许可：MIT**（根目录 LICENSE，2026-06-07 提交 f7ee6184）。

---

## 二、市面 Galgame 引擎现状（11 引擎，2026-08-06 GitHub API 实时核实）

### 2.1 逐引擎档案

**1. Ren'Py 8.5.3** — [github.com/renpy/renpy](https://github.com/renpy/renpy)（2026-08-06 API 实测：**6,693 stars / 917 forks，最近 push 2026-08-06，活跃**）
- 技术栈：Python + Cython/C 扩展；SDL2 渲染。
- 脚本：声明式 Ren'Py 语言 + 内嵌 Python；ATL 变换系统。最新版 8.5.3 "We Can Go to the Moon"（2026-05-15 发布）。
- 代表作：Doki Doki Literature Club、VA-11 Hall-A（初始版）。
- 平台：Win/macOS/Linux/Android/iOS/Web（WASM beta）——最广。
- 许可：MIT（官方文档确认；第三方组件含 LGPL）；社区：itch.io tag-renpy **6,619 作品**、官网声称 8,000+ 作品。
- 能力/局限：多存档+**rollback 回放**、屏幕语言 UI、无障碍选项完善；Web 端仍 Beta、无原生 3D、Live2D 需第三方、性能上限（Python）。

**2. 吉里吉里Z（krkrz）** — [github.com/krkrz/krkrz](https://github.com/krkrz/krkrz)（2026-08-06 API 实测：**934 stars / 135 forks；最后 push 2024-03-24；正式 release 停在 2017-12-24 的 1.4.0r2，约 8 年未发布**）
- 技术栈：C++（TJS2 VM）；DirectX/OpenGL 渲染。
- 脚本：KAG3 标签系统 + TJS2。
- 代表作：Fate/stay night（吉里吉里2）、大量日本商业/同人 galgame。
- 平台：Windows 为主（Android/Web 分支未完成）。
- 许可：自定义 Kirikiri 许可（SPDX NOASSERTION，非 OSI 标准）。
- 能力/局限：粒子、图层、视频、E-mote 支持（商业中间件）；无现代编辑器、维护停滞（1.4.0r2 下载约 10 万次后无新 release）。

**3. 吉里吉里2 / KAG3** — 停更（本体 2010 前后停止主线开发，KAG3 停更更早）；GPL/专有双轨。日系 galgame 事实标准语法（.ks 标签），大量遗产作品，插件生态（XP3 归档、SpriteStudio 等）。

**4. NScripter** — 高桥直树，专有但免费；1999–2018（NS 4.0 后停更）。行式指令脚本；代表作：月姬（Type-Moon 首作）。纯 2D 时代经典。

**5. ONScripter（含 ONScripter-RU）** — 原版（ogapee/onscripter）GPL-2.0，54 stars，最后 push 2024-01；社区活跃分支 ONScripter-RU（umineko-project）123 stars，停在 2022-10。兼容层/模拟器，把 NScripter 老作搬到全平台（含移动端）；半休眠，老特效、老架构。

**6. TyranoScript V6 / TyranoBuilder** — TyranoScript（ShikemokuMK/tyranoscript）：**533 stars / 97 forks，最后 push 2026-03，最新 tag v514**，自定义许可（核心免费），无 GitHub Releases；TyranoBuilder 为 Steam 付费可视化编辑器（**$14.99**，2015-03-27 发布，Very Positive 85%）。HTML5/JS 技术栈 + 拖拽编辑器 + 内置 Live2D；面向零编程创作者；平台 Web/移动为主；性能上限（浏览器渲染）、大型项目吃力。

**7. Unity + Naninovel** — **付费闭源确认：Unity Asset Store $150**（Standard Unity Asset Store EULA），最新版 **v1.21（2026-07-15，要求 Unity 6000.0.78，支持 Built-in/URP/HDRP）**，维护活跃（开发者 Elringus/ReWaffle LLC）。NaniScript 对话语法；Unity 全平台含主机；开箱即用（存档/回滚/本地化/自动配音）；成本高、绑定 Unity、引擎黑盒。

**8. Godot + DialogueManager** — **3,755 stars / 266 forks，MIT，最近 push 2026-08-01，v3.10.5 for Godot 4.7（2026-07-20），活跃**。对话文件（.dialogue）驱动 + 编辑器 + CSV 导出；轻量、只解决对话系统，标题/存档/画廊/回滚需自建；Godot 渲染能力强。

**9. WebGAL**（国产，新增）— **3,906 stars / 357 forks，MPL-2.0，最近 push 2026-08-04，最新版 4.6.3（2026-08-01），活跃**。网页端 VN 引擎 + 图形化编辑器 WebGAL_Terre，中文社区活跃，是国产引擎中最知名者；技术栈 TypeScript/Web，无桌面原生发布管线（套 Electron）。

**10. Monogatari**（新增）— **870 stars / 127 forks，MIT，最近 push 2026-06-18，v2.8.0**。首个全 TypeScript 网页 VN 引擎；维护中等；定位轻量 Web 部署。

**11. Unity + Fungus / Amanita** — MIT；节点式可视化对话；官方停更、社区续命；适合原型，复杂 VN 功能（存档/画廊）需自建。

> **注**：SUMMER / Artemis 均无公开源码仓库（GitHub 搜索无结果），未纳入对比。

### 2.2 横向对比总表

| 引擎 | 脚本语言 | 开源/许可 | 平台 | 维护 | 社区规模 | 编辑器 | 回滚 | 3D/Live2D |
|---|---|---|---|---|---|---|---|---|
| **Caesura (AmeKAG)** | KAG 标签 + Lua 5.4 | **MIT** | Win/macOS/Linux（移动适配中） | **活跃（周 100+ 提交）** | 早期 | **内置 Web 编辑器 + 调试器** | **token 级** | Live2D Cubism5（D3D11 验证）；minigame 3D 框架 |
| Ren'Py | 声明式 + Python | MIT+LGPL | 最广（含移动/Web） | 活跃 | **最大（6.7k stars / itch 6.6k 作品）** | 官方 Launcher + Web | rollback | 无原生 / 第三方 |
| 吉里吉里Z | KAG3 + TJS2 | 自定义 | Win（Android 未完成） | **低维护（8 年无 release）** | 大（日系遗产） | 无 | 有（内置） | E-mote（商业） |
| 吉里吉里2/KAG3 | KAG3 + TJS2 | GPL/专有 | Win | 停更 | 大（遗产） | 无 | 有 | 无 |
| NScripter | 行指令 | 专有免费 | Win | 停更 | 中（遗产） | 无 | 有 | 无 |
| ONScripter | 兼容 NScripter | GPL-2.0 | 全平台（移动好） | 半休眠 | 中 | 无 | 有 | 无 |
| Tyrano V6 | HTML5/JS + 拖拽 | 自定义免费 | Web/移动/Win | 慢（2026-03 仍有提交） | 中 | **可视化拖拽** | 有 | Live2D 内置 |
| Unity+Naninovel | NaniScript/C# | **付费闭源 $150** | 含主机 | 活跃 | 中 | Unity 编辑器 | 有 | Unity 全 |
| Godot+DialogueManager | .dialogue | MIT | 全平台 | 活跃 | 中（3.8k stars） | Godot 编辑器 | 无 | Godot 全 |
| WebGAL | Web 脚本/TS | MPL-2.0 | Web（Electron 套壳） | 活跃 | 中（3.9k stars，中文社区） | **WebGAL_Terre 图形化** | 有 | 无原生 |
| Monogatari | TS/Web | MIT | Web | 中等 | 小 | 无 | 有 | 无 |

---

## 三、逐维度深度对比

**1. 创作脚本**：KAG3/NScripter 系是标签/行式指令，学习门槛低但表达力受限（无现代类型、无参数化宏）；Ren'Py 声明式+Python 表达力强但"日语标签流派"迁移者要重学；Tyrano/WebGAL 拖拽零门槛但复杂逻辑难写；Caesura 走 **KAG 语法继承 + Lua 5.4 双轨**——旧 KAG3 脚本可迁移（13 家族 15 命令兼容），新功能用 Lua/契约命令，是唯一同时保留日系标签免学习成本与现代脚本能力的方案。

**2. 渲染与现代能力**：Caesura 用 bgfx 多后端（D3D11/GL/Metal）+ GPU 粒子 + 视频（pl_mpeg 内置 + FFmpeg 可选）+ GPU 降级恢复，原生性能定位等同吉里吉里Z 的 GPU 路线；Ren'Py 为软件/简化渲染（兼容性优先），Tyrano/WebGAL/Monogatari 受浏览器上限约束；Caesura 的 Live2D 直接引擎级集成（Ren'Py/Tyrano 为第三方/内置但浅层）。3D minigame 是加分项（对标 Naninovel 类 Unity 方案，但零成本）。

**3. 存档/回滚**：Ren'Py rollback 是回放式（历史缓存）；Caesura 是 **token 级状态快照回滚**（复用存档序列化，可跳转任意 token），加上结局/章节/画廊解锁闭环——这组能力 11 个引擎中无一家同时具备（market-comparison 2026-08-05 结论，本次复核仍成立）。

**4. 平台与发布**：Ren'Py 最广（含移动/Web）；吉里吉里/NScripter 困在 Windows；Tyrano/WebGAL 强在 Web/移动但无桌面原生性能；Caesura 三平台 CI 已有，移动端 MobileAdapter 就绪但发布管线未验证（30% 发布就绪度）——最大短板之一。

**5. 许可与商业模式**：Ren'Py MIT（生态友好）；吉里吉里Z 自定义许可（遗产混乱）；NScripter 专有免费；Tyrano 自定义免费；Naninovel 付费闭源 $150；WebGAL MPL-2.0；Monogatari MIT。**Caesura 已定 MIT**（2026-06-07 提交 LICENSE），生态信任基础已就位，下一步是配套（CONTRIBUTING/示例游戏/文档体系）。

**6. 生态与社区**：Ren'Py 6,619 itch 作品、教程、社区；吉里吉里遗产海量但无新生力量；WebGAL 中文社区活跃（3.9k stars）；Caesura 差距最大处——**作品与插件生态需时间积累**；KAG3 停更 = 存量脚本迁移入口（KAG3 导入器是生态撬点，见路线图）。

**7. 开发工具链**：Caesura 内置 Web 编辑器（18 RPC 路由）+ Lua 调试器 + 热重载 + 静态校验，开箱即用；Ren'Py 有官方 Launcher/Web 编辑器但调试器弱；吉里吉里系无现代工具（第三方插件补）；Tyrano/WebGAL 可视化但黑盒/浏览器受限。这是 Caesura 的**现成差异化**（无需依赖第三方）。

---

## 四、定位结论

**定位：KAG Neo-Genesis —— 日系语法继承之上的新一代 VN 引擎标准**（用户规则：不一定是 KAG 的现代重构，可迭代为真正的新一代标准）。

市场空白：日系老引擎（吉里吉里2/KAG3 停更、NScripter 停更、吉里吉里Z 8 年无 release）的用户没有现代迁移路径——Ren'Py 语法不同、Tyrano/WebGAL 性能上限、Unity 系付费。Caesura 同时满足：**KAG 标签免学习 + 活跃维护 + 跨平台 + MIT 开源 + 现代工具链 + 独有闭环功能**，占据"老日系引擎 → 现代引擎"迁移位。WebGAL 的崛起证明中文市场对新一代 VN 引擎有真实需求，但其 Web 技术栈上限（性能/桌面分发）恰是 Caesura 原生路线的对比优势。

## 五、差距清单与路线建议

**P0（发布前必须）**
1. 真 GPU 端到端验证与发布就绪（能力就绪度 30%）：GL/Metal 路径、字体资产、非 Windows 包
2. 开源生态配套：CONTRIBUTING、Issue/PR 模板、示例游戏（MIT LICENSE 已定，2026-06-07）
3. 移动端发布管线（MobileAdapter 已接，缺构建脚本 + IME）

**P1（差异化加固）**
4. KAG3 脚本导入器（生态入口：把停更引擎的存量 .ks 拉进来）
5. rollback 内存成本压测（超长对话场景）
6. Live2D OpenGL 验证 + minigame GPU CI；Steam 实机验证

**P2（生态扩张）**
7. 可视化编辑器前端（RPC 路由已全，缺前端；对标 WebGAL_Terre 的图形化编辑）
8. 教程/示例游戏/社区文档（对标 Ren'Py 文档体系，补足最大生态短板）
9. E-mote 类商业中间件替代方案（不可移植，需自研等价物）

## 六、总评

Caesura 在**架构纪律（16 模块接口隔离、52 测试文件全绿、耦合门禁）、现代能力（bgfx 多后端、Lua 5.4、Live2D、加密归档）、工具链（内置编辑器/调试器/回滚）、许可（MIT 已定）**四个维度上已具备与一线开源引擎正面对话的底子；与 Ren'Py 的差距在生态与平台广度，与吉里吉里系的差距在遗产积累——而这两者恰好都处于"停更/慢维护"窗口期，是明确的迁移机遇。风险集中在**发布就绪度（30%）与生态从零积累**，路线图 P0 三项是当前最高杠杆动作。

---

## 附：本报告数据验证记录（2026-08-06 v2）

- **本地重扫**（read-only 子代理）：16 模块/30 接口/72 命令/52 测试文件全部与 HEAD e9b11b04 核实一致；LICENSE 存在（MIT, f7ee6184）。
- **市面数据**：GitHub API 实时抓取（renpy、krkrz、ogapee/onscripter、umineko-project/onscripter-ru、ShikemokuMK/tyranoscript、nathanhoad/godot_dialogue_manager、OpenWebGAL/WebGAL、Monogatari/Monogatari）+ renpy.org + itch.io + Unity Asset Store + Steam 商店页（tyranobuilder.com 抓取失败，以 GitHub/Steam 替代）。
- **全量重建 + 完整测试（2026-08-06 20:18–20:22 实测）**：仓库自 `codex\` 路径迁至 `code\` 后旧 build 缓存失效（CMakeCache 指向已消失目录）；已 `rm -rf build` 从零全量重建（`cmake -B build -DCAESURA_LIVE2D=OFF && cmake --build build --config Debug --parallel`，0 错误）。
  - **C++ doctest：576 test cases / 2828 assertions — 576 passed, 0 failed, 0 skipped**
  - **Lua 脚本测试：86 passed, 0 failed, 86 total**（`external/lua/lua.exe tests/scripts/run_lua_tests.lua`）
  - 注：`tests/run_all.bat` 中 "574/574 | Lua: 86/86" 为过时硬编码，已同步修正为 576/86；能力矩阵 P2 中 "569 cases" 为 2026-07-24 旧快照，**已于 2026-08-06 同步更新为 576**（commit ff76733e），README 与 market-comparison 中的 569/12 亦已同步修正。
- **耦合度门禁**：`python scripts/count_coupling.py --ci` → PASS（所有模块在阈值内）。

*证据索引：接口清单 = `glob src/*/api/*.h`（30 个 I*.h）；能力矩阵 = `docs/design/engine-capability-matrix.md`；命令契约 = `docs/api/command-contracts.md`（562 行，schema_doc.lua 自动生成）+ `scripts/kag/commands/`（9 文件）+ `scripts/kag.lua`（13 命令，去重后 72）；测试 = `tests/CMakeLists.txt`（52 test_*.cpp 与磁盘 glob 一致）；构建模块 = `cmake/CaesuraModules.cmake`（16 静态库）；LICENSE = 根目录 MIT（f7ee6184）。*
