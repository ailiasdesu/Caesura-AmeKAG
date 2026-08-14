# 市场引擎对比素材（最终报告用）

> **用途**：为「Caesura (AmeKAG) vs 主流引擎」对比报告提供结构化素材。
> **数据基准**：结合仓库内 2026-08-06 在线核实快照（docs/design/engine-market-analysis-2026-08-06.md，数据经 GitHub API/官网/itch.io 实时复核）+ 领域公开常识。
> **重要说明**：本次调研时 web_search 因账户余额不足不可用（"Insufficient Balance"），无法实时在线复核；凡无法二次核实处一律标注「待核实」。最终报告发布前建议补一轮在线复核。
> **复核进展（round 28）**：web_search 已修复（改走 opencode-go 提供方）。已复核：**Ren'Py 最新版 8.5.0**（renpy.org/release/8.5.0 官方确认，2025 发布，标题 "In Good Health"；Lemmasoft 公告帖 renpy 8.5 released）；**TyranoScript/TyranoStudio 免费可商用**（tyranoscript.com、Steam app 3634660）；**Naninovel 确认 Asset Store 约 $150**（naninovel.com/releases/1.21）；**VNM 商业付费**（Steam app 495480）；**Live2D 商用授权规则**（help.live2d.com/sdk/sdk_001，SDK 发行许可证机制）；**Godot+DM 为 MIT**（非 MPL-2.0，MPL-2.0 系 WebGAL）。其余「待核实」项见文末清单。
> **Caesura 侧证据来源**（本地已验证）：docs/api/command-contracts.md（**84 个** KAG Neo-Genesis 契约命令，自动生成）；docs/design/skeletal-mesh-animation.md（SMA 骨骼网格动画，**GPU 蒙皮 bgfx compute 已交付 round 18**）；Ollama AI `[ai_dialog]` 集成。

## 0. 被调研引擎清单

| # | 引擎 | 类型 | 调研深度 |
|---|---|---|---|
| 1 | **Ren'Py** | 独立脚本引擎 | ★★★★☆ |
| 2 | **KiriKiri2 / KAG3** | 日系标准 | ★★★★☆ |
| 3 | **吉里吉里Z（KiriKiri Z）** | 日系继任 | ★★★☆☆ |
| 4 | **NScripter / ONScripter** | 日式经典/移植 | ★★★☆☆ |
| 5 | **TyranoBuilder / TyranoScript** | 可视化 Web | ★★★☆☆ |
| 6 | **Visual Novel Maker（VNM）** | Unity 系 | ★★★☆☆ |
| 7 | **Unity + Naninovel** | 通用引擎 + 商店插件 | ★★★★☆ |
| 8 | **Godot + DialogueManager** | 通用引擎 + 开源 | ★★★☆☆ |
| 9 | **WebGAL** | Web 系 | ★★★☆☆ |
| 10 | **Monogatari** | Web 系 | ★★★☆☆ |
| 11 | **Live2D Cubism SDK** | 动画层参照 | ★★★☆☆ |

---

## 1. Ren'Py

**一句话**：全球开发者占有率最高的开源 VN 引擎，Python + 声明式 Screen Language（SL）+ ATL 动画语言，生态最大。

| 维度 | 内容 |
|---|---|
| 技术栈 | Python（2→3x 迁移，现 8.x 基于 Python 3）+ Ren'Py Script / Screen Language / ATL；渲染软件光栅 + OpenGL；自研引擎 |
| 平台 | 最广：Windows/macOS/Linux + 移动（Android/iOS）+ Web |
| 开源/许可 | MIT（引擎，附加 LGPL 组件）；免费开源 |
| 脚本/叙事 | label + say/menu/jump/call 声明式；Python 内嵌；变量/flag；多存档槽；历史回滚 rollback（回放式缓存） |
| 渲染能力 | 2D + ATL（动画/转场）；跨平台一致性优先（性能非重点）；无原生 Live2D/骨骼动画（第三方）；特效靠 ATL/transform |
| 性能/启动 | 冷启动较慢（Python VM + 资源解包）；中低端硬件友好 |
| 资产/打包 | 加密归档（rpa 打包，可加密）；素材目录规范；.rpyc 编译缓存 |
| 生态工具链 | 最大社区：官方 Launcher/Web 编辑器、Ren'Py Cloud、Lemmasoft 论坛、文档体系完善；itch.io 大量作品（约 6.6k+） |
| 授权模式 | MIT（引擎）；游戏内容归作者；Steam 发布成熟 |
| 与 Caesura 差距 | **Caesura 强**：KAG 标签免学习、bgfx GPU 多后端（D3D11/Metal/GL）性能上限更高、token 级回滚、引擎级 Live2D/骨骼动画、CARC 加密+签名、内置 Web 编辑器；**Caesura 弱**：生态/社区/作品量/文档/平台广度悬殊 |

> **要点**：Ren'Py 是「生态即护城河」典型——技术门槛低、社区大、发布链路成熟。Caesura 争创作者需在降低上手成本与社区配套补课；但 Ren'Py 无 GPU 深度利用/动画体系，是 Caesura 技术差异化阵地。

来源：renpy.org；仓库 GitHub RELEASE（在线核实 2026-08-06，见 engine-market-analysis-2026-08-06.md）。

---

## 2. KiriKiri2 / KAG3

**一句话**：日本业界曾是标准之一的引擎组（历史地位高），KAG3 是 Caesura 的**脚本语法兼容来源**。

| 维度 | 内容 |
|---|---|
| 技术栈 | C++（KiriKiri2）+ TJS2（类 JS 脚本）+ KAG3（TJS 之上标签式脚本层）；DirectX/OpenGL |
| 平台 | 主要 Windows |
| 开源/许可 | KiriKiri2 主仓库 GPL/自定义；KAG3 有专有与开放分支；许可混乱 |
| 脚本/叙事 | KAG3 标签式（[r]/[s]/[l] 等，即 Caesura 兼容对标的源）；TJS2 面向对象能力强；变量/条件/存档/回滚内置 |
| 渲染能力 | 2D + 粒子/转场；经典日系表现；GPU 利用有限（偏 CPU/2D） |
| 性能/启动 | 轻量快速启动；老硬件可跑 |
| 资产/打包 | 自带加密打包（xp3 等）；素材规范 |
| 生态工具链 | 曾为日系标准，大量存量作品与工具；官方已停更多年（约 2010 后）；编辑器依赖第三方 |
| 授权模式 | GPL / 专有混合（自定义/混乱；KAG3 版权在原作者） |
| 与 Caesura 差距 | **Caesura 强**：活跃维护、跨平台（KAG3 困于 Win）、Lua 5.4 现代脚本、bgfx 多后端、token 回滚、Live2D/SMA、MIT 清晰授权、内置编辑器；**Caesura 弱**：存量作品/社区积累（但 KAG3 停更=迁移入口，导入器是撬点） |

> **要点**：KAG3 是 Caesura 的语法祖先——Caesura 的 KAG Neo-Genesis 在其上做现代重构（标签兼容 + Lua + GPU + 跨平台 + 有界流控 + 安全白名单），是「继承语法、抛弃遗留问题」路线。存量 .ks 迁移是生态切入。

来源：KiriKiri2 GitHub（GPL）；KAG 文档社区（在线核实 2026-08-06）。

---

## 3. 吉里吉里Z（KiriKiri Z）

**一句话**：KiriKiri2 的现代化 GPU 继任者，曾引入 E-mote（商业 2.5D 动画）集成，但维护缓慢。

| 维度 | 内容 |
|---|---|
| 技术栈 | C++ + TJS2 + KAG3 兼容层；GPU（D3D/GL）渲染 |
| 平台 | 主要 Windows（Android 移植未完成） |
| 开源/许可 | BSD 风格（部分组件）；E-mote 为商业闭源 |
| 脚本/叙事 | 兼容 KAG3 脚本；TJS2 脚本；内置存档/回滚 |
| 渲染能力 | GPU 渲染较 2 代强；E-mote 2.5D 动画（商业中间件，不可移植）；无自研骨骼/Live2D 原生（Live2D 需第三方接入） |
| 性能/启动 | 中等；GPU 开启后表现好 |
| 资产/打包 | 加密打包延续 |
| 生态工具链 | 日系传承、存量多；低维护（2021 后提交稀疏）；第三方工具补 |
| 授权模式 | 部分开源（BSD）/ 商业组件 |
| 与 Caesura 差距 | **Caesura 强**：活跃维护、跨平台、GPU 蒙皮 SMA/自研 E-mote 等价物（part variant 命令已见）、Live2D 引擎级集成、清晰 MIT；**Caesura 弱**：作品生态、E-mote 商业位品牌认知度 |

> **要点**：E-mote（商业 2.5D）是吉里吉里Z 招牌——Caesura 的 SMA 骨骼网格动画（含 E-mote 风格 part variant 开关）正是自研等价物，是可写入宣发的技术对标点。吉里吉里Z 维护缓慢给了追赶窗口。

来源：吉里吉里Z 官网/GitHub（在线核实 2026-08-06）；本地 SMA 设计文档。

## 4. NScripter / ONScripter

**一句话**：日式经典行脚本引擎（NScripter）及其跨平台开源移植（ONScripter），主跑老游戏。

| 维度 | 内容 |
|---|---|
| 技术栈 | NScripter：专有行指令脚本；ONScripter：C++ 跨平台重实现 |
| 平台 | NScripter=Windows；ONScripter=Windows/macOS/Linux/Android/iOS（移动友好） |
| 开源/许可 | NScripter 专有免费；ONScripter GPL-2.0 |
| 脚本/叙事 | 行式指令（*label、@say 等）；简单分支/变量/存档/回滚；表达能力弱 |
| 渲染能力 | 简单 2D、老式特效（默认画作/转场、noise/quake 等）；无现代 GPU/Live2D |
| 性能/启动 | 极轻量、快；专为老机型 |
| 资产/打包 | 明文 / 简单打包（arc.nsa 等）；无加密（ONScripter 用于跑老作，非新作发布） |
| 生态工具链 | 海量存世老作品（NScripter 曾垄断日系同人）；官方停更（2018 前后）；ONScripter 半休眠 |
| 授权模式 | 专有免费 / GPL |
| 与 Caesura 差距 | **Caesura 强**：现代全栈（GPU/脚本/加密/编辑器/跨平台 CI）、行脚本生态迁移（Caesura 兼容 [r]/[s] 类基础命令但目标非模拟旧作）；**Caesura 弱**：能跑存量老作品（重放/兼容场景） |

> **要点**：NScripter/ONScripter 定位是**老游戏兼容/模拟**，与新作创作引擎不同赛道。Caesura 面向新作，仅需最低限度命令兼容（已做）即可，无需成为模拟器。

来源：ONScripter GitHub（ogapee 分支，GPL）；NScripter 官方（在线核实 2026-08-06）。

---

## 5. TyranoBuilder / TyranoScript

**一句话**：日本可视化脚本工具（HTML5/JS），拖拽式作 VN，多端 Web 发布，上手极快。

| 维度 | 内容 |
|---|---|
| 技术栈 | HTML5 + JavaScript（TyranoScript 语义）+ TyranoBuilder（拖拽编辑器）；浏览器/Electron |
| 平台 | Web / 移动（iOS/Android）/ Windows（套壳）；桌面原生性能弱 |
| 开源/许可 | TyranoScript 免费使用（官方 tyranoscript.com "Free to use"，兼容智能手机；可商用）；TyranoStudio（现厂商主导工具，Steam app 3634660）亦标注免费、可商用；许可细则见 [tyranoscript.com](https://tyranoscript.com/)、[Steam 页](https://store.steampowered.com/app/3634660/) |
| 脚本/叙事 | 标签式 + JS；拖拽节点（Builder）；变量/分支/存档/回滚内置 |
| 渲染能力 | 浏览器能力上限：2D + Live2D 内置（HTML5 Live2D）+ 简单转场；无深度 GPU（WebGL 受浏览器限制） |
| 性能/启动 | 中；浏览器环境，大场景/长对话可能卡顿 |
| 资产/打包 | Web 资源打包（.zip 可拆）；加密弱 |
| 生态工具链 | Tyrano 可视化拖拽编辑器（零门槛）；JS 社区；日系用户多；维护较慢（2026-03 前仍有提交） |
| 授权模式 | 免费、可商用（TyranoScript 官方 "Free to use"；TyranoStudio "perfect for commercial use"，见 [alternativeto](https://alternativeto.net/software/tyranostudio/about/)） |
| 与 Caesura 差距 | **Caesura 强**：桌面原生 C++ 性能、GPU 多后端、CARC 加密归档、token 回滚、编辑器 RPC/调试器、自研动画体系；**Caesura 弱**：拖拽可视化编辑、Web 多端发布、HTML5 生态 |

> **要点**：Tyrano 的护城河是「拖拽 + 零代码 + Web 发布」——面向非程序员创作者。Caesura 的 Web 编辑器路线正对标的可视化方向尚未做到拖拽级（RPC 路由全、缺前端），这是追赶点。

来源：TyranoStudio 官网/GitHub（ShikemokuMK/tyranoscript）；在线核实 2026-08-06（tyranobuilder.com 抓取失败，以 GitHub/Steam 替代）。

---

## 6. Visual Novel Maker（VNM）

**一句话**：基于 Unity 的可视化 VN 工具（类 RPG Maker 的 VN 版），拖拽为主，脚本能力受限。

| 维度 | 内容 |
|---|---|
| 技术栈 | Unity（C# 底层）+ 专有可视化脚本/数据库表驱动 |
| 平台 | Unity 支持范围（Win/mac/移动/Web/主机视导出配置） |
| 开源/许可 | 商业付费（Steam/官方折扣有售）；闭源 |
| 脚本/叙事 | 拖拽事件 + 变量/条件/分歧；数据库表（角色/流程）；复杂逻辑不如脚本引擎；存档/回滚内置 |
| 渲染能力 | 继承 Unity 渲染（可出 3D/特效）；Live2D 需 Unity 生态系统商用组件（VNM 本身动画浅） |
| 性能/启动 | Unity 冷启动/资源较重；中等硬件要求 |
| 资产/打包 | Unity 资源打包；依赖 Unity 构建 |
| 生态工具链 | Unity 生态；VNM 自家文档/模板；社区比 Ren'Py 小 |
| 授权模式 | 商业付费（非开源） |
| 与 Caesura 差距 | **Caesura 强**：开源 MIT、零授权费、KAG 语法、纯原生 VN 性能（无 Unity 包袱）、CARC 加密；**Caesura 弱**：拖拽可视化编辑、Unity 生态（Asset Store 海量资源） |

> **要点**：VNM 卖点是「Unity 支撑 + 可视化」（类 RPG Maker 心智），但受 Unity 授权/包体/学习曲线影响。Caesura 的开源原生路线在「纯 VN 场景」性能与授权成本上反超。

来源：Visual Novel Maker 官方/Steam 页（在线核实 2026-08-06）。

---

## 7. Unity + Naninovel

**一句话**：Unity Asset Store 上最流行的 VN 付费插件，提供近乎开箱的全套 VN 能力，与 Unity 生态深度耦合。

| 维度 | 内容 |
|---|---|
| 技术栈 | Unity（C#）+ NaniScript（Naninovel 自定义脚本语言）+ 可视化编辑器集成 |
| 平台 | Unity 全平台（含主机 Steam/Switch 视发布） |
| 开源/许可 | 商业闭源付费（Asset Store 原价约 **$150**、一次授权终身、可商用，见 [Asset Store 页](https://assetstore.unity.com/packages/tools/game-toolkits/naninovel-visual-novel-engine-135453) 与 [naninovel.com/releases/1.21](https://naninovel.com/releases/1.21)）；闭源 |
| 脚本/叙事 | NaniScript 声明式 + 可视化（Timeline/字段）；变量/分支/存档开箱全套（含回滚/自动存档/云存档集成） |
| 渲染能力 | 继承 Unity 全渲染（URP/HDRP、3D、粒子、后处理、Live2D 支持（Unity Live2D SDK））；回滚/转场/打字机全套 |
| 性能/启动 | Unity 固有；高质量但包体/内存大 |
| 资产/打包 | Unity 资源资产管线；依赖 Unity Addressables |
| 生态工具链 | Unity 生态 + NaniScript 文档/社区；成熟商业支持；中文资料多 |
| 授权模式 | 单买授权（付费一次终身）；非开源 |
| 与 Caesura 差距 | **Caesura 强**：零授权费、开源 MIT、纯原生轻量、独立于 Unity（无平台抽成/授权变更风险）、CARC+签名加密、token 级回滚、自带编辑器；**Caesura 弱**：Unity 海量资源生态、成熟商业插件功能广度（全套开箱即用） |

> **要点**：Naninovel 是「付费省心」路线代表——功能全但闭源+授权费+Unity 依赖。Caesura 以「开源免费 + 原生性能 + 独立自研全栈」差异化，同时应警惕其「全套功能广度」是自身成长参考线。

来源：Unity Asset Store Naninovel 页；在线核实 2026-08-06。

---

## 8. Godot + DialogueManager

**一句话**：通用开源引擎 Godot + 社区对话插件（.dialogue 脚本），轻量、只管对话，其余靠 Godot 全栈。

| 维度 | 内容 |
|---|---|
| 技术栈 | Godot（开源引擎，GDScript/gdshader）+ Dialogue Manager 插件（.dialogue 标记语言） |
| 平台 | Godot 全平台（含移动/Web） |
| 开源/许可 | MIT（Godot 与 DM 均开源免费） |
| 脚本/叙事 | .dialogue 对话脚本（声明式、支持分支/变量）；存档/回滚不内置弹层（对话状态需自己管理，回滚弱） |
| 渲染能力 | 继承 Godot 全渲染（2D/3D/粒子/后处理）；Live2D 需社区适配 |
| 性能/启动 | Godot 轻量快速 |
| 资产/打包 | Godot 资源体系；导出各平台 |
| 生态工具链 | Godot 编辑器（可视化节点）；DM 社区；文档良；无独立 VN 商业生态 |
| 授权模式 | 开源免费（MIT）；无商业授权费 |
| 与 Caesura 差距 | **Caesura 强**：主打完整 VN（回滚/存档/画廊/backlog 闭环）、KAG 语法、CARC 加密、自带 VN 编辑器/调试器、动画体系；**Caesura 弱**：Godot 的通用全栈与可视化编辑器成熟度、3D 通用能力、生态 |

> **要点**：Godot+DM 是「通用引擎 + 轻量对话插件」路线——适合把 VN 嵌进 3D/玩法游戏。纯 VN 场景下不如专用引擎顺手。Caesura 定位专用引擎，回滚/加密/动画是 DM 缺项。

来源：Godot 官网；nathanhoad/godot_dialogue_manager（在线核实 2026-08-06）。

---

## 9. WebGAL

**一句话**：华语社区活跃的开源 Web VN 引擎（TypeScript），图形化编辑器 WebGAL Terre，中文生态代表。

| 维度 | 内容 |
|---|---|
| 技术栈 | Web（TypeScript + Electron 套壳桌面）；HTML5/Canvas/WebGL |
| 平台 | Web / 桌面（Electron）/ 移动浏览器；无原生 GPU/二进制 |
| 开源/许可 | MPL-2.0（开源）；Terre 编辑器开源 |
| 脚本/叙事 | WebGAL 脚本 + 可视化（WebGAL Terre 图形化拖拽）；变量/分支/存档/回滚内置 |
| 渲染能力 | 浏览器上限：2D + 简单特效；无原生 Live2D/骨骼 GPU（可用 Web Live2D 库，弱）；GPU 利用受 WebGL 限制 |
| 性能/启动 | 中等；Web 分发快但大场景受浏览器限制；Electron 包体大 |
| 资产/打包 | Web 资源打包（可混淆）；加密弱（Web 天然可解） |
| 生态工具链 | WebGAL Terre 图形化编辑器（华语可视化代表）；3.9k+ stars 中文社区；文档活跃 |
| 授权模式 | MPL-2.0 开源（弱 copyleft：可免费商用、无版税；义务是修改/新增了 MPL 覆盖的源文件时需以 MPL-2.0 开源其源码；静态链接边界明确） |
| 与 Caesura 差距 | **Caesura 强**：原生 C++ 性能（GPU 多后端、帧率稳）、CARC+Ed25519 强加密（Web 无解）、token 回滚、自研动画体系、三平台 CI、桌面分发质量；**Caesura 弱**：图形化拖拽编辑器（WebGAL Terre 领先）、Web 免安装分发、华语社区活跃度 |

> **要点**：WebGAL 证明「华语市场对新一代 VN 引擎有真实需求」，其图形化 Terre 是可视化标杆；但 Web 技术栈上限（性能/加密/桌面质量）恰是 Caesura 原生路线的对比优势。**可视化编辑器前端是 Caesura 追赶优先级最高项之一**（RPC 路由已全、缺前端）。

来源：OpenWebGAL/WebGAL（MPL-2.0，在线核实 2026-08-06）。

---

## 10. Monogatari

**一句话**：老牌 Web VN 引擎（TypeScript），轻量、纯浏览器，社区规模较小。

| 维度 | 内容 |
|---|---|
| 技术栈 | Web（TypeScript）；HTML5/Canvas |
| 平台 | Web（可套壳桌面/移动） |
| 开源/许可 | MIT 开源 |
| 脚本/叙事 | Monogatari 语言（脚本 + 条件/变量）；存档/回滚内置；可视化弱（靠手写脚本/JSON 结构） |
| 渲染能力 | 浏览器 2D 上限；无原生 GPU/动画体系；Live2D 弱 |
| 性能/启动 | 轻量快速（纯 Web）；大内容受浏览器限制 |
| 资产/打包 | Web 资产；无强加密 |
| 生态工具链 | 基础文档/编辑器弱；社区小、更新较慢；无大型可视化工具 |
| 授权模式 | MIT 开源免费 |
| 与 Caesura 差距 | **Caesura 强**：完整原生全栈、GPU/动画/加密/回滚/编辑器、桌面质量；**Caesura 弱**：Web 免安装分发、轻量上手 |

> **要点**：Monogatari 与 WebGAL 同属 Web 赛道但更轻量小众；对 Caesura 参考价值低于 WebGAL。其纯 Web 轻量定位与 Caesura 原生路线互补不冲突。

来源：Monogatari/Monogatari（MIT，在线核实 2026-08-06）。

---

## 11. Live2D Cubism SDK（动画层参照）

**一句话**：非完整 VN 引擎，而是**业界 2D 角色动画标准 SDK**——Caesura 已引擎级集成（原生对齐此层）。

| 维度 | 内容 |
|---|---|
| 技术栈 | 原生 C++ / Unity / 各平台 SDK；Web（Cubism Web） |
| 平台 | 原生（Win/mac/iOS/Android）+ 各引擎集成 |
| 开源/许可 | 商业闭源 SDK（免费使用+商用需授权；Live2D 公司发售）；非开源 |
| 能力 | 2D 网格形变（mesh deformation）表情/呼吸/视线/口型；不与叙事/存档/渲染系统耦合，需宿主引擎接入 |
| 渲染能力 | GPU 三角网格形变（高效）；高质量 2D 动态立绘 |
| 与 Caesura 差距 | **Caesura 强**：引擎级集成（live2d 模块 + Cubism 5，D3D11 实测）+ 自研 SMA 骨骼网格（GPU 蒙皮 compute）可作 Live2D 补充/等价物 + PNG 回退（无 SDK 降级）+ 开源 MIT；**Caesura 限制**：Live2D 需商用授权才能商用发布（同业界规则，非 Caesura 独有） |

> **要点**：Live2D 是行业标准动画层。Caesura 直接集成 Cubism SDK（原生级）+ 自研 SMA（E-mote 风格 part variant），在动画层具备对标一线能力；商业授权是行业通例，Caesura 提供 PNG 回退保证无 SDK 也能跑。

来源：Live2D Cubism 官方（商用授权机制见 [help.live2d.com/sdk/sdk_001](https://help.live2d.com/en/sdk/sdk_001/)：SDK 开发期免费，公开出版（Publication）需按营收/作品数量判断是否签 SDK 发行许可证协议）；本地 docs/guides/live2d-setup.md。

---

## 12. 对比总结表

| 引擎 | 脚本/叙事 | 渲染 | 平台 | 回滚 | 动画(Live2D/骨骼) | 加密打包 | 编辑器/IDE | 生态社区 | 授权 | 维护 |
|---|---|---|---|---|---|---|---|---|---|---|
| **Caesura (AmeKAG)** | KAG+Lua 5.4（84 契约命令） | bgfx（D3D11/Metal/GL）+GPU 粒子/视频 | Win/mac/Linux（+移动适配中） | **token 级** | **Live2D 引擎级 + SMA GPU 蒙皮** | **CARC+AES-GCM+Ed25519** | **内置 Web 编辑器+RPC+调试器** | 早期 | **MIT** | **活跃（周百+提交）** |
| Ren'Py | 声明式+Python | 2D 软件/OGL | **最广（含移动/Web）** | rollback(回放) | 无原生(第三方) | rpa(可加密) | 官方 Launcher/Web | **最大** | MIT+LGPL | 活跃 |
| KiriKiri2/KAG3 | TJS2+KAG3 | 2D DX/GL | Win | 有 | 无 | xp3 | 无(第三方) | 大(遗产) | GPL/专有混乱 | **停更** |
| 吉里吉里Z | TJS2+KAG3 | GPU DX/GL | Win(Android 未完) | 有 | **E-mote(商业)** | 加密 | 无(第三方) | 大(遗产) | BSD/商业 | **低维护** |
| NScripter | 行指令 | 简单 2D | Win | 有 | 无 | 明文/arc | 无 | 中(遗产) | 专有免费 | **停更** |
| ONScripter | 兼容行 | 老特效 | **全平台(移动好)** | 有 | 无 | 明文 | 无 | 中(遗产) | GPL | 半休眠 |
| Tyrano | HTML5/JS+拖拽 | Web(WebGL)+Live2D | Web/移动/Win | 有 | Live2D(Web) | 弱(Web) | **TyranoBuilder 拖拽** | 中 | 免费(可商用) | 慢 |
| VNM | 拖拽+数据库 | Unity 全 | Unity 全 | 有 | 需 Unity 组件 | Unity | **可视化** | 中 | 商业付费 | 中 |
| Unity+Naninovel | NaniScript | Unity 全(URP 等) | **Unity 全(含主机)** | 有 | Live2D 支持 | Unity | Unity 编辑器 | 中(商业) | **付费闭源(~$150)** | 活跃 |
| Godot+DM | .dialogue | Godot 全 | Godot 全 | 弱 | 社区适配 | Godot | Godot 编辑器 | 中 | **MIT** | 活跃 |
| WebGAL | Web 脚本 | Web(WebGL) | Web/Electron | 有 | Live2D(Web 弱) | 弱(Web) | **WebGAL_Terre 可视化** | 中(华语) | MPL-2.0 | 活跃 |
| Monogatari | Web 脚本 | Web(Canvas) | Web | 有 | 弱 | 弱(Web) | 弱 | 小 | MIT | 较慢 |
| Live2D SDK | 非叙事 | GPU 网格 | 原生/含引擎 | — | **Live2D 本体** | — | SDK 工具 | 大 | **商业闭源** | 活跃 |

> 注：加粗为该列相对突出项；「待核实」= 本次未能在线二次核实。

---

## 13. Caesura 定位分析

### 13.1 差异化优势（相对市面引擎）

1. **KAG 语法继承 + 现代脚本双轨**（唯一同时具备）：
   - 旧 KAG3 标签免学习成本（兼容层 13 家族+延续）=> 存量日系创作者可迁移；
   - Lua 5.4 + 84 个声明式契约命令（typed params/钳制/$var 插值/参数化宏）=> 现代表达力；
   - 唯一混合路径：.ks + [eval]/[emb]/[iscript] 内嵌 Lua + kag.* 反向回调。

2. **原生 GPU 深度利用 + 自研动画体系**：
   - bgfx 多后端（D3D11/Metal/GL）跨平台性能上限远高于 Web 系（Tyrano/WebGAL/Monogatari）与软件渲染（Ren'Py）；
   - **SMA 骨骼网格动画（GPU 蒙皮 bgfx compute，round 18 已交付）** + E-mote 风格 part variant = 对标吉里吉里Z E-mote 的自研等价物；
   - **Live2D 引擎级集成**（Cubism 5，D3D11 实测）+ PNG 回退（无 SDK 可用降级）。

3. **独有回滚闭环**：**token 级状态快照回滚**（复用存档序列化）+ 结局/章节/画廊解锁闭环——市场对比中**无一家同时具备**（Ren'Py 是回放式缓存、其余多为快照或弱回滚）。

4. **安全加固属强项**：**CARC 加密归档（AES-256-GCM + Ed25519 签名防篡改）** + 加密存档（AES-GCM + schema 迁移）——Web 系（WebGAL/Monogatari/Tyrano）天然无解；商业 Unity 系有但闭源依赖 Unity。

5. **活跃维护 + 清晰授权 + 三平台 CI**：MIT 许可（2026-06-07 定）；周百+提交、586 C++ 用例 + Lua 全绿、三平台 CI + 耦合度门禁；Ren'Py 之外开源竞品多处于停更/慢维护窗口。

6. **自带开发者工具链**：内置 Web 编辑器（RPC 双传输 + Lua 调试器断点/步进/求值 + 热重载 + 静态校验 ks_check）开箱即用——无需依赖第三方。

### 13.2 短板（相对最强竞品）

| 短板 | 相对谁 | 说明 |
|---|---|---|
| **生态/社区/作品量** | Ren'Py/Live2D/Web 系 | 最大差距；无作品积累、无教程体系、无插件市集；docs 文档体系已有但面向开发者，创作者向缺失 |
| **平台/发布广度** | Ren'Py/Unity 系 | 移动端 MobileAdapter 已接但发布管线未验证（发布就绪度约 30%）；无 Web 发布；无主机 |
| **可视化拖拽编辑** | Tyrano/WebGAL_Terre/Naninovel | RPC 路由已全，但拖拽级可视化编辑器**前端未做**（缺前端资产）；对标 WebGAL_Terre 是明确追赶项 |
| **通用引擎全栈** | Godot/Unity 系 | 若做 3D/玩法游戏，专用 VN 引擎不如通用引擎顺手（Caesura 的 minigame 是 VN 内嵌 3D 小游戏，非通用 3D） |
| **文档/上手成熟度** | Ren'Py | 缺乏面向零基础创作者的 guide/tutorial/示例工程 |
| **E-mote 品牌认知** | 吉里吉里Z | SMA 技术等效但缺「E-mote」级别的商业生态位声望与存量兼容 |

### 13.3 追赶路径建议（按杠杆排序）

1. **P0 — 发布就绪**：真 GPU 端到端验证 + 移动端（Android）发布管线打通（MobileAdapter 已接、补构建脚本 + IME）；补齐最大短板「平台广度」。
2. **P1 — 生态撬点**：**KAG3 脚本导入器上线**——把停更引擎的存量 .ks 拉进生态（KAG 兼容已是先天优势，导入器是转化入口）；配套 CONTRIBUTING/Issue/PR 模板 + 示例游戏（MIT 底子已就位）。
3. **P1 — 可视化编辑器前端**：基于已有 RPC 路由做**拖拽级 Web 编辑器前端**（对标 WebGAL_Terre）——这是从「开发者工具」走向「创作者工具」的分水岭。
4. **P2 — 创作者文档/教程**：对标 Ren'Py 文档体系，做面向零基础作者的 guide/教程，配合示例工程。
5. **P2 — 差异化加固**：token 回滚内存压测；SMA GPU 蒙皮 + E-mote 风格宣传化；Live2D/Metal/GL 全路径验证；Steam 实机验证；Ollama AI `[ai_dialog]` 集成（当前市面引擎均无原生 LLM 对话命令——可作为**宣发差异化新卖点**，类 killer feature）。

> **战略判断**：Caesura 占据「老日系引擎（KAG3/NScripter 停更、吉里吉里Z 慢维护）=> 现代开源原生引擎」的迁移位，同时以「GPU 原生性能 + 自研动画 + 加密归档 + AI 集成 + 内置工具链」区别于 Web 系与付费 Unity 系。最大风险在**生态从零起步**与**发布就绪度**，P0/P1 杠杆动作是当前最高优先级。

---

## 附：数据可信度与待核实清单

- **复核状态（round 28）**：web_search 已修复可用（改走 opencode-go 提供方），本轮对优先清单逐项在线复核；数据主体仍以仓库 2026-08-06 在线核实快照（采信）+ 本轮 web_search 二次确认为准。
- 本轮已确认项（原「待核实」→ 已确认，见正文改写）：
  - **TyranoScript / TyranoStudio**：免费可商用（官方 [tyranoscript.com](https://tyranoscript.com/) "Free to use"；Steam app [3634660](https://store.steampowered.com/app/3634660/) 亦免费可商用）。
  - **Naninovel**：Asset Store 一次授权约 **$150**、可商用（[Asset Store](https://assetstore.unity.com/packages/tools/game-toolkits/naninovel-visual-novel-engine-135453)、[release 1.21](https://naninovel.com/releases/1.21)）。
  - **Visual Novel Maker**：商业付费（[Steam app 495480](https://store.steampowered.com/app/495480/)）；具体以区域定价/折扣为准。
  - **Live2D Cubism SDK 商用授权**：开发期免费，公开出版按营收/作品数量判断是否需 [SDK 发行许可证协议](https://help.live2d.com/en/sdk/sdk_001/)（Publication License Agreement）。
  - **Godot + DialogueManager**：均为 **MIT 许可**（正文正确，非 MPL-2.0；本素材中 **MPL-2.0 系 WebGAL**）。
  - **Ren'Py 8.5.0**：[官方 release 页](https://www.renpy.org/release/8.5.0)，2025 发布，标题 "In Good Health"（此前 round 27 已确认）。
  - **WebGAL 商用许可**：MPL-2.0 弱 copyleft，可免费商用、无版税，义务为 MPL 覆盖源文件的源码开放。
- 仍待核实/需补查项：
  1. ~~Ren'Py 当前最新版本号~~（已确认 8.5.0，round 27）+ Web 稳定版状态（本轮未单独核实）；
  2. 吉里吉里Z 最新 release 版本号（本轮确认其 GitHub 仓库 [krkrz](https://github.com/kirikiroid3/krkrz)、快照页 [krkren.github.io](https://krkren.github.io/shapshot.html) 与社区分叉 krkrsdl2 存在且低维护，但未取得具体最新版本号）与 E-mote 授权现状（未核实）；
  3. ~~TyranoBuilder/TyranoScript 确切商业授权条款~~（已确认免费可商用，本轮）+ 最新版本号（已从 tyranoscript.com/Steam 佐证现主导工具为 TyranoStudio，未锁定具体 build 号）；
  4. ~~VNM 商业付费性质~~（已确认，Steam app 495480）+ 具体零售价/折扣与 Unity 版本依赖（本轮未取得精确美元价，需补查）；
  5. ~~Naninovel 价格~~（已确认约 $150、一次授权、可商用）+ 确切断版号（本轮仅见 1.21 release note）与功能边界（未变更）；
  6. WebGAL / Monogatari 最新版本与可视化工具状态（本轮未重新核实，仍按 2026-08-06 快照采信）；
  7. ~~Live2D Cubism 商用授权机制~~（已确认 SDK 发行许可证机制，help.live2d.com/sdk/sdk_001）+ 具体价格档位/营收阈值（本轮未取得精确数字，需补查）；
  8. Caesura 侧 2026-08-06 之后的版本仍应二次核对（84 契约命令、发布就绪度最新值）。

> 最终报告发布前：可用 web_search 逐项复核上述「待核实」清单，并将本素材与最新一份 docs/plans/YYYY-MM-DD-0NN-delivery-handoff.md 交叉核对 Caesura 侧数据。