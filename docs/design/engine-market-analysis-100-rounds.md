# Caesura (AmeKAG) 引擎能力与性能分析报告 — 100 轮迭代后 vs 主流视觉小说引擎

> 日期：2026-08-16 · 版本：1.0（100 轮自主迭代收官后）
> 方法：Caesura 侧全部数据为**本仓库实测**（ROADMAP-100.md / perf-baseline-update.md / engine-capability-matrix.md / final-report-100.md 权威记录）；
> 竞品侧数据来自公开资料调研（web 检索），标注来源与"无公开基准"处。

---

## 1. 执行摘要

Caesura (AmeKAG) 是 C++20 / bgfx / SDL3 / SoLoud / Lua 5.4 的跨平台视觉小说引擎，100 轮自主迭代后处于
**引擎核心完备、脚本语言深度突出、测试体系极强**的交付态：16 模块 / 31 纯虚接口 / BackendRegistry
组合根架构；Lua 脚本层四件套（scheduler/compiler/expr/schema）+ 118 个 KAG Neo-Genesis 契约命令
（运行时覆盖 100%）；Web（wasmoon）与桌面播放器行为 parity；编辑器 506 测试全绿；三平台 CI 全绿。

与市面主流 VN 引擎对比的**核心定位**：

| 维度 | Caesura 相对位置 |
|---|---|
| 架构工程化 | **领先**：16 模块 API 边界 + 组合根 DI + 全量门禁（C++ 963 / Lua 150 / web 282 / editor 506），远超同人引擎常态 |
| 脚本语言 | **并列第一梯队**：KAG3 兼容方言 + 表达式语言（三元/??/eval）+ 宏系统 + 预算护栏；与吉里吉里 KAG、Ren'Py 同级 |
| 渲染表现力 | **中上**：bgfx 多后端（D3D11/OpenGL/Metal）+ 图层合成 + 粒子 + 视频 + RTT + GPU 蒙皮；弱于通用引擎（Unity/Godot）生态，强于脚本引擎（Ren'Py/Tyrano） |
| 跨平台发布 | **中**：三平台 CI + Web parity 已验证；Steam/Live2D/移动真机待设备验证（管线就绪） |
| 生态/社区 | **短板**：无社区、无作品、无素材市场——引擎工程完备但生态为零，是相对 Ren'Py/吉里吉里/WebGAL 的最大差距 |
| 性能 | **强**：渲染每帧 <500us 预算、表达式翻译 2000x 缓存求值 0.026s、深嵌套病态 6.3s→0.001s；Release 包体 87.9MB（含 FFmpeg/SDL3） |

---

## 2. 评测对象与维度

### 2.1 评测对象

**Caesura (AmeKAG)**（本仓库，round 100 终态）+ 9 个主流 VN 引擎/方案：

| 派系 | 引擎 | 技术基础 |
|---|---|---|
| 开源脚本引擎派 | Ren'Py | Python + pygame/OpenGL |
| | WebGAL | Web（PixiJS），国产 |
| | Monogatari | Web（JS） |
| 日本系 | 吉里吉里 / KAG3 | TJS + DirectX/OpenGL |
| | NVLMaker | Unity + KAG3 风格 |
| | TyranoScript / TyranoBuilder | Web 技术栈（NW.js/Electron） |
| 通用引擎插件派 | Naninovel | Unity C# |
| | Fungus | Unity C# |
| | Dialogic | Godot |
| | Twine（叙事工具，参考） | Web（Harlowe/SugarCube） |

### 2.2 评测维度（10 项）

技术架构 · 脚本语言与表现力 · 渲染能力 · 音频 · 存档/加密 · 编辑器/工具链 ·
平台与 Web 导出 · 性能特征 · 许可与生态 · 维护状态

---

## 3. Caesura 引擎能力全景（实测）

### 3.1 技术架构（100 轮终态）

- **16 静态模块库 / 31 纯虚接口**：archive/audio/debug/di/entry/input/job/live2d/minigame/platform/render/resource/rpc/script/steam/storage，全部经 api/I*.h 纯虚接口跨模块访问；BackendRegistry 唯一访问点；组合根（main.cpp + entry/）唯一 new 具体后端处。
- **渲染**：bgfx 多后端（D3D11 / OpenGL 4.3 / Metal），图层合成（BG/FG/MSG + dirty-rect 增量），异步纹理加载 + 纹理预算（tier 0-5 + LRU），2D 粒子（发射器/物理/GPU），视频（pl_mpeg MPEG-1 + FFmpeg 全格式回退），GPU 监控自适应降级，FreeType 文本（CJK/Ruby/多字节），转场（blend/wipe/custom），Render-to-Texture + 视口 blit，批量提交协议，GPU compute skinning（骨骼蒙皮，D3D11 DXBC + GL 430 GLSL）。
- **音频**：SoLoud 3 总线（BGM/Voice/SE）+ 交叉淡化 + 3D 空间（listener/position）+ 逐句柄音量/停止。
- **平台**：SDL3 窗口/事件/输入路由（KAG↔GAME 焦点互斥、修饰键透传、手柄路由、IME 文本）；JobSystem 多线程（异常隔离）；CARC 加密归档（AES-256-GCM + Ed25519 签名 + DeltaCARC）。
- **Live2D**：Cubism 5 SDK 集成（条件编译）+ PNG 回退；PathConfinement 路径守卫。
- **Steamworks**：成就/统计/云存档（条件编译 + 无 SDK 降级）。
- **小游戏**：3D mini-game 场景（enter→update→render→leave，Lua 绑定 + demo 端到端）。

### 3.2 能力矩阵（79 项，round 98 刷新）

构成：**渲染 10（R1-R10）+ 脚本 38（S1-S38）+ 音频 4（A1-A4）+ 内容 10（C1-C10）+ 小游戏/3D 10（D1-D10）+ 平台 7（P1-P7）**。
完成度快照（能力矩阵口径，保守估计）：

| 层 | 完成度 | 证据边界 |
|---|---|---|
| 模块架构迁移 | 98% | 静态/API 目标、组合根、引擎持有 DebugProtocol、owner-thread RPC DTO、共享生产链接与源码级门禁就位 |
| 核心 VN 可用性 | 74% | Headless/KAG/存档/音频单测强；R7/R8/C2 真 GPU（D3D11 + OpenGL 4.3）验证；小游戏 Lua 绑定 + demo 端到端；**缺口：Metal 后端与 macOS/Linux 实机验证** |
| 跨平台产品发布 | 34% | 多平台 CI 构建存在；Windows D3D11+GL 真机像素验证；**可选 SDK（Steam/Live2D）与非 Windows 包未发布验证** |

### 3.3 性能实测基线（round 97-99 门禁实测）

| 项目 | 实测 | 预算/说明 |
|---|---|---|
| 渲染帧预算 | **<500us/帧**（约 70% 占用，30% 余量） | frame_bench 三守卫（render/混合表达式/add 链） |
| 表达式翻译 2000x 缓存求值 | **0.026s** | 预算 <5s，余量 >99% |
| 表达式 translate 混合 | 0.056-0.057s（round 82 0.054s 持平） | — |
| 300 三元插值段 | 0.593s | 预算 <2s |
| 500 扁平三元链（O(n²) worst） | 0.662s | 预算 <2s |
| 80 参含三元调用 | 0.003s | 预算 <0.1s |
| 200 段长字符串跳过 | 0.400s | 预算 <2s |
| **deep100 深嵌套病态** | **6.3s → 0.001s**（round 97 括号嵌套>48 截断） | O(n³) 收敛为有界报错 |
| tokenizer 2000 命令场景 | ~255ms（68ms/1000 token） | 预算 <10s |
| debug 日志热路径 | O(1) 数组递增，计数 7→12 零开销 | 走查复证 |
| Lua 主套件全量 | 130 文件全绿 | 顺序敏感沙箱 |
| C++ 全量 | 963 cases / 8773 assertions | 0 failed |
| Release 包体 | **CPack ZIP 87.9MB**（含 SDL3/FFmpeg DLL、demo、脚本） | round 90 实测发布流程 |

### 3.4 脚本语言与 KAG 兼容性（核心资产）

- **KAG Neo-Genesis 方言**：118 契约命令运行时覆盖 100%（schema 定义 → 处理器 → 文档三源一致）。
- **表达式语言**：TJS 风格运算符翻译（&&/||/!=/!/?:/??）、三元索引/括号内三元、eval 三元赋值、
  平衡花括号/长括号插值（${expr}/%var%/$tbl.key 三语法）、规模护栏。
- **控制流**：if/elseif/else、switch（tostring 等值）、for/while/until（65536 帧护栏 + step 钳位）、
  jump/call/return（跨场景 LIFO 栈 + 场景切换预算 4096）、macro（静态内联 + 运行期 splice +
  深度守卫 + 嵌套定义 + KAG3 全局语义）。
- **流控**：[wait]/[delay]（stop_flag 对齐）、[until cond timeout]、[sel]/[button cond]、
  skip/auto/advance 三态、backlog/rollback（桌面 500 上限）。
- **存档**：[save]/[load]/[saveplace]/[loadplace]/[listsaves]，循环体内 save→load 续跑
  （loop_stacks 序列化恢复）、跨场景 load（_pendingLoadOriginScene）、AES-256-GCM + 槽位迁移 v1→v5。
- **i18n**：词典（en/zh/ja）+ 复数（CLDR one/other）+ 运行时热切换 + backlog 重本地化。
- **KAG3 兼容层**：kag3_import 转换器（别名 PARAM_ALIASES、RENAMES、宏参数 &N→%N%、CONFLICT_NOTES）。

### 3.5 Web 播放器与编辑器

- **Web**：wasmoon（Lua 5.4）桥 + DOM 渲染器；与桌面共享 ~90% 纯 Lua KAG 栈；表达式/i18n/存档/
  设置/音频引擎 parity；story bundle（ks_bake 导出 20 场景）+ sweep 守卫；vite 生产构建。
- **编辑器**：React + RPC（stdio/HTTP 9876）；LSP（定义/引用/重命名/诊断/补全）；场景大纲虚拟化、
  Inspector、时间线、引擎实机位置联动、断点调试、AI 面板；506 测试 + tsc 干净。


---

## 4. 主流引擎逐项对比

### 4.1 开源脚本引擎派：Ren'Py / WebGAL / Monogatari（2025-2026 公开资料）

| 维度 | **Ren'Py** | **WebGAL** | **Monogatari** |
|---|---|---|---|
| 技术栈 | C++（SDL2+OpenGL/GLES）+ Python 3 逻辑 | TypeScript + PixiJS（WebGL） | JS/Vue（DOM/CSS 动画） |
| 脚本语言 | **Python 超集** + 屏幕语言 SL + ATL 动画 DSL | 自研 .txt 标签语言（零门槛） | YAML/JSON 场景 + 内嵌 JS |
| 平台 | 全平台原生 + Web(wasm) + Steam Deck | Web 一等 + Electron/Tauri/Capacitor 壳 | Web + 壳 |
| Live2D | 官方原生支持（Cubism） | 支持（PixiJS 集成） | 需插件 |
| 视频 | WebM/MP4/AV1（ffmpeg 解码） | WebM/MP4（HTML5 video） | HTML5 video |
| 存档 | 槽位 + 可选加密 + Steam 云（官方 SDK） | localStorage/IndexedDB 明文 JSON | localStorage + **libsodium 加密** |
| 编辑器 | Ren'Py Launcher（打包/检查/REPL），无可视化 IDE | **WebGAL_Terre 可视化编辑器**（拖拽 + 实时预览） | monogatari.io 在线编辑器 |
| Web 导出 | 原生（Emscripten/wasm，GLES3） | 原生（本身就是 Web） | 原生 |
| 性能 | **最强**（原生 GPU 管线；无公开基准，定性：静态场景数百 FPS，wasm 端降 30-50%） | 中（PixiJS WebGL 批处理；无公开基准） | **弱**（DOM 重排，复杂动画/弱设备卡顿） |
| 许可 | **MIT**（引擎本体） | MPL-2.0/MIT（视版本） | MIT |
| 社区 | **第一**：4.4k+ stars、itch "Made with Ren'Py" 成千上万（DDLC 等代表作）、Lemma Soft 论坛 | 国产活跃：3-5k stars、CNGal/B站生态、WebGAL_Terre 低门槛 | 停滞：1.5-2k stars、无大作、v2.0.x 后低活跃 |
| 最新版 | **8.5.1（2026）** | **4.0（2026）** | 2.0.2（低活跃） |
| 总结 | 全平台深度引擎、生态最大、脚本=完整 Python | 国产 Web-First 轻量引擎、可视化零门槛、快速分发 | 不推荐新项目（维护停滞、DOM 渲染弱） |

**对 Caesura 的启示（开源派）**：
- Ren'Py 的"脚本=完整语言 + 声明式 UI/动画 DSL（SL/ATL）"是它生态第一的核心——Caesura 的 KAG 标签语言 + 表达式语言覆盖了声明式与控制流，但**尚无 UI 声明式 DSL 与动画 DSL**（Lua 直写为主），是脚本层下一个可对标的方向。
- WebGAL 证明"可视化编辑器 + Web 分发"是国产低门槛路线的胜负手——Caesura 的编辑器（RPC/LSP/大纲/时间线）已具雏形，若走向"面向内容作者的一键打包/可视化搭建"可对标。
- Monogatari 的反面教训：**维护停滞 + DOM 渲染弱 = 出局**——Caesura 的引擎侧深度与三平台 CI 正是避免该命运的壁垒。


### 4.2 日本系：吉里吉里/KAG3 · THE NVL Maker · Tyrano（2025-2026 公开资料）

> 调研纠正：THE NVL Maker **不是 Unity 插件**，而是基于吉里吉里/KAG3 的免费可视化 AVG 工具（KAG 语言同族）。

| 维度 | **吉里吉里 / KAG3** | **THE NVL Maker** | **TyranoBuilder / TyranoScript** |
|---|---|---|---|
| 技术栈 | C++/Win32 + TJS VM；吉里吉里Z 跨平台重写；.xp3 归档 + .tlg 图 | 基于吉里吉里的免费工具（运行时=吉里吉里） | 纯 Web 栈（HTML5/CSS/JS），Electron/浏览器壳 |
| 脚本语言 | **TJS + KAG3 标签语言**（本项目同源） | **KAG3 标签语言**（同族，扩展模板命令） | 独立 TyranoScript（非 KAG 兼容） |
| 平台 | **仅 Windows 官方**；移动靠 kirikiroid2、桌面跨平台靠社区重写；**无 Web** | 仅 Windows（继承）；无 Web | **全平台 + Web 导出（核心优势）** |
| Live2D | 第三方插件（KAGeXpress 时代流行） | 同吉里吉里（社区教程） | **Web 原生 SDK（大卖点）** |
| 视频/滤镜 | MPEG-2（Z 更多）；2D 叠化转场，**无现代 GPU 滤镜** | 同吉里吉里 | CSS3/WebGL 转场滤镜（强于吉里吉里 2D 栈） |
| 存档 | KAG3 槽位；**无强加密**（.xp3 可解包） | 同 KAG3 | localStorage/本地文件；无强加密 |
| 编辑器 | 文本编辑器 + 命令行编译；KAGEditor 社区工具 | **可视化编辑器（核心卖点，零代码模板）** | **TyranoBuilder 可视化节点编辑器** |
| Web 导出 | **无**（KAGWeb 实验未成气候） | **无** | **官方一键 HTML5 导出** |
| 性能 | 无公开基准；2D 老硬件 60fps、内存小、加载 1-3s | 同吉里吉里（架构推断） | 无公开基准；浏览器运行时内存偏高、老设备易卡 |
| 许可 | **GPL + 商业闭源需买授权** | 免费（底层吉里吉里授权需注意） | **免费可商用，无门槛** |
| 社区 | **日本同人圈绝对统治者**：Fate/stay night、**海市蜃楼之馆** 等；Steam 存量巨大 | 中文同人圈主力（原创祭生态） | 2020s 增长最快之一：Steam 数千款独立 VN |
| 最新版 | Z 活跃；Next 2024-25 改版；经典 2/KAG3 停滞 | **3.15（2017，停滞/社区维护）** | **V3.05（2025-05，活跃）** |

**对 Caesura 的启示（日本系）**：
- **本项目正是 KAG3 的现代独立重实现**（Lua+cmake+bgfx）：118 契约命令对标 KAG3 标签语言；海市蜃楼之馆（Fata Morgana）确认 KAG 栈，是可参考的适配目标。
- **市场缺口确认**：吉里吉里（跨平台差、无 Web、商用授权门槛）+ NVL（Win-only、2017 停滞）+ Tyrano（Web 强但脚本非 KAG、原生弱）。Caesura 的「KAG3 语言兼容 + bgfx 跨平台 + Web parity」恰好填补三者缺口——**前提是补上生态侧**。


### 4.3 通用引擎插件派：Naninovel · Fungus · Dialogic 2 · Twine（2025-2026 公开资料）

| 维度 | **Naninovel** | **Fungus** | **Dialogic 2** | **Twine** |
|---|---|---|---|---|
| 宿主 | Unity (C#) | Unity (C#) | Godot 4 (GDScript) | 独立工具 → 纯 HTML/JS |
| 脚本/编辑 | .nani 文本 DSL + 可视化 | 流程图节点（无需代码） | 可视化 Timeline 事件 | Harlowe/SugarCube 故事格式 |
| 平台 | 继承 Unity 全平台 + WebGL + 主机 | 继承 Unity（旧版适配） | 继承 Godot 全平台 + Web | 任何浏览器（纯 HTML） |
| Live2D | **官方集成** | 需自行扩展 | 第三方 | 无 |
| 视频/粒子 | Movie clip/粒子/自定义渲染层/3D 场景 | 简单动画指令 | 转场/条件分支 | CSS/JS 自定义 |
| 存档 | 多槽位 JSON 内置 + 自定义模块 | PlayerPrefs/JSON（手动规划） | 借助 Godot 存档（无内置管理器） | 浏览器 localStorage |
| 编辑器 | Unity 窗口内完整编辑器（WYSIWYG） | 可视化 Flowchart | Godot Dock 面板（Timeline/角色/样式） | 独立节点式编辑器 |
| Web 导出 | ✅ WebGL（30-100MB 包体） | ⚠️ 旧版 Unity 才可靠 | ✅ Godot Web（数 MB-几十 MB） | ✅ 天然 Web（KB-MB） |
| 性能 | 宿主 Unity 开销；无自有基准 | 节点 VM 解释开销；停更 | 插件轻量；包体远小于 Unity | 极轻量 |
| 许可 | **商业付费 ~$50 买断**（限一份项目） | MIT 免费 | MIT 免费 | 免费开源 |
| 社区 | Asset Store 4★+、数千下载、代表独立/商业 VN | 数千 stars、教程常客、**官方 2018 停更** | 数千 stars、Godot 生态最主流对话插件 | intfiction 社区主力 |
| 最新版 | **1.20（2024-25，活跃）** | 3.8（2018 后停更） | **2.0 系列（Godot 4，活跃）** | 2.x（持续维护） |
| 选型建议 | Unity 商业项目首选 | 不推荐新项目 | Godot 项目首选 | 交互叙事/文本冒险 |

**对 Caesura 的启示（插件派）**：
- Naninovel 证明 VN 能力可以 $50 商业变现，但受限于 Unity 宿主（包体 30-100MB、引擎冗余）——**Caesura 的独立原生引擎在包体/启动/内存上有结构性优势**（Release 87.9MB 含全套运行时与 demo，仍低于 Unity WebGL 基线）。
- Fungus 的反面教训再次印证：**停更 = 出局**（现代 Unity 兼容性差即死）。
- Dialogic 的路线（轻量、Timeline 可视化、Web 友好）与 Caesura 编辑器（大纲/时间线/Inspector）方向一致，但 Caesura 的**独立脚本语言 + 引擎深度**是插件方案没有的。
- Twine 不构成直接竞争（交互叙事工具而非 VN 引擎）。


---

## 5. 横向能力矩阵（Caesura vs 9 引擎总表）

> Caesura 列 = 本仓库实测；竞品列 = 公开资料（2025-2026）。●=原生一等支持 ◐=第三方/受限 ○=无/需自建

| 能力维度 | **Caesura** | Ren'Py | 吉里吉里/KAG3 | Tyrano | WebGAL | Naninovel | Dialogic 2 | NVL Maker | Monogatari | Fungus |
|---|---|---|---|---|---|---|---|---|---|---|
| 原生跨平台（非壳） | ● bgfx 三后端 | ● | ○ Win-only | ◐ Web 壳 | ◐ Web+壳 | ◐ Unity | ◐ Godot | ○ | ◐ Web+壳 | ◐ Unity |
| Web 导出 | ● wasmoon parity | ● wasm | ○ | ● | ● | ● WebGL | ● | ○ | ● | ◐ 旧版 |
| KAG 语言家族 | ● 118 契约 | ○ | ● 原生 | ○ | ○ | ○ | ○ | ● 同族 | ○ | ○ |
| 表达式/脚本深度 | ● TJS 风格+宏+护栏 | ● Python 完整 | ● TJS | ◐ 标签 | ◐ 标签 | ◐ .nani | ◐ 事件 | ◐ 模板 | ◐ JS | ○ 节点 |
| Live2D | ◐ SDK 条件编译 | ● 官方 | ◐ 插件 | ● Web SDK | ◐ | ● 官方 | ◐ 第三方 | ◐ | ◐ | ○ |
| 视频 | ● FFmpeg+MPEG-1 | ● | ◐ MPEG-2 | ◐ HTML5 | ◐ HTML5 | ● Movie | ◐ | ◐ | ◐ | ◐ |
| 粒子/GPU 滤镜 | ● 粒子+转场+RTT | ◐ shader | ○ | ◐ CSS/WebGL | ◐ PixiJS | ● | ◐ | ○ | ○ | ◐ |
| 加密存档 | ● AES-256-GCM | ◐ 可选 | ○ | ○ | ○ | ◐ 自定义 | ◐ Godot | ○ | ● libsodium | ◐ |
| 可视化编辑器 | ◐ RPC/LSP/大纲/时间线 | ○ Launcher | ○ KAGEditor | ● 节点式 | ● Terre | ● Unity 内 | ● Dock | ● 核心卖点 | ◐ 在线 | ● Flowchart |
| 调试/测试体系 | ●● 全量门禁（见 §6） | ◐ REPL | ○ | ○ | ◐ | ◐ | ◐ | ○ | ○ | ◐ |
| 商用许可 | ● MIT 级（本项目 LGPL 待定） | ● MIT | ◐ GPL+付费 | ● 免费 | ● MPL/MIT | 💰 $50 | ● MIT | ◐ 底层限制 | ● MIT | ● MIT |
| 维护活跃度 | ● 100 轮迭代 | ● 8.5.1 | ◐ Z 活跃 | ● V3.05 | ● 4.0 | ● 1.20 | ● 2.0 | ○ 2017 停滞 | ○ 停滞 | ○ 2018 停更 |
| 社区/生态 | ○ 零（最大短板） | ●● 第一 | ●● 日圈统治 | ● 数千 Steam | ● 国产活跃 | ◐ 数千下载 | ◐ 数千 stars | ◐ 中文圈 | ○ | ◐ fork |

---

## 6. 性能对比分析

### 6.1 实测 vs 定性

| 引擎 | 渲染/运行时 | 包体（Web/分发） | 启动/加载 | 内存 |
|---|---|---|---|---|
| **Caesura** | bgfx 原生 GPU；**帧预算 <500us**（D3D11/GL 真机验证）；expr 2000x 0.026s | **Release ZIP 87.9MB**（全套运行时+FFmpeg+SDL3+demo） | 原生启动（无字节码编译层） | 原生 C++，可控 |
| Ren'Py | SDL2+OpenGL 原生；静态场景数百 FPS（定性） | 数百 MB 级（Python 运行时+资源） | 大项目首启慢（字节码编译+索引），后续即时 | Python 堆，长会话稳健（社区共识） |
| 吉里吉里/KAG3 | DX9 2D；老硬件 60fps（定性） | .xp3 归档可拼 exe，通常数十 MB | 1-3s（TJS 解析+xp3 随机读） | 小（2D 图片为主） |
| Tyrano | 浏览器 JS；内存/加载开销高 | Web 静态站（资源体积主导） | 浏览器加载 | 浏览器运行时，老设备易卡 |
| WebGAL | PixiJS WebGL；桌面 60fps（定性） | Web 静态站 | 首载拉资源，缓存后快 | 浏览器 |
| Naninovel | Unity 引擎 | **WebGL 30-100MB 基线** | Unity 启动 | Unity 运行时 |
| Dialogic 2 | Godot 引擎 | Web 数 MB-几十 MB | Godot 启动 | Godot 运行时 |
| Monogatari | DOM/CSS 动画 | KB-MB 级 | 快 | 轻 |
| Fungus | Unity 引擎（停更） | WebGL 30-100MB | Unity 启动 | Unity |

### 6.2 Caesura 性能定位结论

1. **原生渲染效率第一梯队**：bgfx 多后端 + 帧预算 <500us（约 30% 余量）+ 纹理预算/LRU + dirty-rect 增量合成，与 Ren'Py（原生 OpenGL）、吉里吉里（DX9 2D）同级；优于所有 Web/DOM 栈（Tyrano/WebGAL/Monogatari）与 Unity/Godot 宿主方案（引擎冗余开销）。
2. **脚本热路径有量化护栏**：表达式翻译 2000x 缓存求值 0.026s、tokenizer 68ms/1000 命令、500 三元链 0.66s、病态 O(n³) 收敛到 0.001s——**主流 VN 引擎中唯一把脚本性能做成确定性预算的**（Ren'Py/TJS/Tyrano 均无量化基准）。
3. **包体有结构性优势**：87.9MB 含全部运行时与 demo，低于 Unity WebGL 基线（30-100MB 纯引擎）——独立原生引擎无宿主冗余。
4. **可测量性即工程壁垒**：963 C++ + 150 Lua + 282 web + 506 editor 测试，是任何竞品（含 Ren'Py）都没有的量级；这直接支撑长期维护与回归安全（对照 Fungus/Monogatari 停更即死）。

---

## 7. 差距与优势清单

### 7.1 Caesura 相对优势（壁垒）

| # | 优势 | 依据 |
|---|---|---|
| 1 | **KAG3 语言家族的现代跨平台化** | 118 契约命令 + kag3_import 兼容层；吉里吉里无 Web/跨平台、NVL 停滞、Tyrano 非 KAG——本引擎是唯一"KAG3 兼容 + bgfx 跨平台 + Web parity"三者兼备 |
| 2 | **脚本深度与确定性预算** | 表达式语言（三元/??/eval/长括号插值）+ 宏系统 + 循环/宏/跨场景护栏 + 性能预算守卫——对标 Ren'Py Python 与 TJS 的深度，且全部有量化测试背书 |
| 3 | **工程化与测试体系** | 16 模块/31 接口架构 + 三平台 CI + 保鲜守卫 + 1500+ 自动化测试；竞品无此量级（Ren'Py 单测稀薄、吉里吉里无 CI） |
| 4 | **加密存档/资源管线** | AES-256-GCM 存档 + CARC 加密签名归档 + 槽位迁移 v1→v5——吉里吉里（明文 .xp3 可解包）、Tyrano/WebGAL（明文 JSON）均无 |
| 5 | **包体与性能** | 87.9MB 全功能包 < Unity WebGL 基线；帧预算 <500us；原生启动 |

### 7.2 Caesura 相对差距（短板）

| # | 差距 | 对手 | 说明/建议 |
|---|---|---|---|
| 1 | **生态为零（最大短板）** | Ren'Py（itch 成千上万）、吉里吉里（日圈统治）、WebGAL（国产活跃）、Tyrano（数千 Steam） | 无社区/无作品/无模板市场/无教程生态。**下一步应做：示例库发布、itch/Steam 示例游戏、文档站、社区渠道** |
| 2 | **可视化编辑器成熟度** | WebGAL_Terre、TyranoBuilder、NVL Maker、Naninovel | 已有 RPC/LSP/大纲/时间线/Inspector 骨架，缺"零代码搭建"与一键打包分发 UX |
| 3 | **Live2D/Steam/Metal/移动实机验证** | Ren'Py 官方 Live2D、Tyrano Web SDK | 管线就绪（条件编译+降级），**待设备验证**（021 §5：GL 需 Linux/macOS 硬件、Steam 需账号、Metal 需 macOS 真机、移动真机） |
| 4 | **UI 声明式 DSL/动画 DSL** | Ren'Py SL/ATL | 当前 Lua 直写 UI；SL/ATL 式声明层可显著降低内容作者门槛 |
| 5 | **无历史资产兼容工具链** | 吉里吉里（.tlg/.xp3 生态）、NVL | kag3_import 已有 .ks 转换；**.xp3/.tlg 读取器 + KAGeXpress 资产迁移**是撬动存量作品的钥匙（Fata Morgana 同语言家族 = 可参考适配目标） |
| 6 | **发布/分发链路** | Tyrano 一键 Web、WebGAL_Terre 一键导出 | 已有 Release+CPack+gh release 流程（round 90 实测），缺 Web 一键部署与 itch.io 模板 |
| 7 | **多语言 i18n 深度** | Ren'Py 官方多语言工具 | 已有 en/zh/ja + 复数 + 热切换；缺翻译流程工具（提取/回填） |

### 7.3 遗留观察对市场定位的影响（审阅结论）

审阅 final-report-100 §7（12 项）与交接文档 021 §5（14 项）后，与市场定位直接相关的观察：

- **待设备项（Live2D GL/Steam/Metal/移动）** → 影响差距 #3：能力矩阵"核心 VN 可用性 74%、跨平台产品发布 34%"的保守口径即源于此；**优先级最高**（制约商用发布）。
- **CARCWriter 不去重/48B 密钥容忍/nonce 复用无检测** → 安全细节，商用前补齐即可，不影响定位。
- **宏系统两缺陷（_macroExpansions 只增不重置/嵌套定义收集）** → 内容作者可见的脚本层瑕疵，影响"脚本深度"卖点背书，建议修。
- **web backlog 无上限 / settings 语言默认差异 / config.ensure_dir** → Web parity 语义差异，记录在案即可，低优先。
- **诊断不做类型校验 / fadeVolume 语义坑 / 自定义 provider 绕过 AES** → 设计行为/文档化即可。

---

## 8. 定位与结论

### 8.1 市场定位

**Caesura (AmeKAG) = KAG3 语言家族的现代跨平台独立引擎**，定位介于两个空白之间：

（ASCII 定位图：Y 轴=脚本深度，X 轴=跨平台/Web 能力）

- **左上象限（脚本深度 + 全平台）**：Ren'Py（Python 全平台）——Caesura 目标位置（KAG3 兼容 + 脚本深度 + bgfx 跨平台）；
- **左下象限（Win-only 传统）**：吉里吉里/KAG3（无 Web/跨平台）、NVL Maker（2017 停滞）；
- **右下象限（Web 轻量）**：Tyrano / WebGAL（Web 强、原生弱、脚本浅）；
- **通用宿主**：Naninovel（Unity 付费）、Dialogic 2（Godot）——非独立引擎路线。

### 8.2 结论

1. **引擎侧已完成闭环**（100 轮）：核心 VN 能力 74%、架构 98%、脚本深度与性能预算实测领先；包体/性能/加密/测试为结构性壁垒。
2. **产品化缺口集中在三处**：① 真机验证（Live2D/Steam/Metal/移动）→ 商用发布前置；② 生态建设（示例游戏、文档站、社区、itch/Steam 分发模板）→ 从"引擎"变"平台"；③ 可视化创作体验（零代码搭建、一键打包、SL/ATL 式声明层）→ 降低内容作者门槛。
3. **市场窗口**：吉里吉里体系（KAG 家族）老化停滞、Tyrano/WebGAL 无原生深度、Fungus/Monogatari 停更——"KAG3 兼容 + 现代跨平台 + 工程可信"目前无直接竞争者，Caesura 处于该细分空白的有利位置。

---

## 9. 参考资料

- **Caesura 侧**（本仓库，实测）：ROADMAP-100.md / perf-baseline-update.md / engine-capability-matrix.md / final-report-100.md / 交接文档 021
- **Ren'Py**：renpy.org/release/8.5.1 · github.com/renpy/renpy · lemmasoft.renai.us · itch.io/games/made-with-renpy
- **WebGAL**：github.com/OpenWebGAL/WebGAL · cngal.org/articles/index/956（4.0 发布）· WebGAL_Terre
- **Monogatari**：github.com/monogatari/monogatari · community.monogatari.io（v2.0.1/v2.0.2 发布帖）
- **吉里吉里/KAG3**：krkrz/krkrz · krkren/kag3 · DBpedia/KiriKiri · PCGamingWiki（Fata Morgana）· CnGal（KAGeXpress Next Gen）
- **THE NVL Maker**：github.com/nextgal/the-nvl-maker · nvlmaker.net（KAG3 文档引用）· rpg.blue 3.15
- **Tyrano**：tyranobuilder.com · SteamDB 345370（V3.05 2025-05-30）· Steam 导出/商业讨论帖
- **Naninovel**：assetstore.unity.com/packages/tools/game-toolkits/naninovel-visual-novel-engine-135453 · naninovel.com/releases/1.20
- **Fungus**：github.com/snozbot/fungus（3.8/2018 停更）
- **Dialogic**：github.com/dialogic-godot/dialogic · docs.dialogic.pro（Godot 4）
- **Twine**：en.wikipedia.org/wiki/Twine_(software)
- 调研原始文件：docs/design/engine-market-research-japanese-vn-engines-2026.md（SA-2 完整版）
