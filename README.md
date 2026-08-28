# Caesura (AmeKAG) — Cross-Platform Visual Novel Engine / 跨平台视觉小说引擎

> **16 modules · 34 pure-virtual interface headers · 134 KAG Neo-Genesis command contracts · 0 circular dependencies**
> **16 模块 · 34 纯虚接口头 · 134 KAG Neo-Genesis 命令契约 · 0 循环依赖**
> C++20 · bgfx · SDL3 · SoLoud · Lua 5.4 · CMake · MIT License
> Live API census: `python scripts/api_stats.py` → [docs/api/api-stats.md](docs/api/api-stats.md) / 实时 API 普查

<p align="center">
  <b>Platforms 平台</b>&nbsp; Windows&nbsp;&nbsp; Android&nbsp;&nbsp; macOS&nbsp;&nbsp; Linux&nbsp;&nbsp; iOS (Track I)&nbsp;&nbsp; Web&nbsp;&nbsp;·&nbsp;&nbsp;
  <b>Status 状态</b>&nbsp; Tests passing 测试全绿&nbsp;&nbsp;·&nbsp;&nbsp;
  <b>Contracts 契约</b>&nbsp;134 KAG commands 命令&nbsp;&nbsp;·&nbsp;&nbsp;
  <b>Interfaces 接口</b>&nbsp;34 pure-virtual 纯虚
  <br>
  <sub><i>文本徽章（不依赖外部 CI 服务，计数取自 [docs/api/api-stats.md](docs/api/api-stats.md) 实时普查与阶段 G 最近审计）—
  测试状态见 [ROADMAP-200](docs/plans/audit/ROADMAP-200.md) 门禁列 / Text badges (no external CI dependency; counts come
  from the live [docs/api/api-stats.md](docs/api/api-stats.md) census and the latest stage-G audit; test status is in the
  gate columns of [ROADMAP-200](docs/plans/audit/ROADMAP-200.md))</i></sub>
</p>

Caesura is an open-source galgame / visual novel engine with **Live2D, 3D mini-games, SMA skeletal
animation, i18n, cloud saves and AI-assisted authoring** as first-class citizens. Its scripting language is
**KAG Neo-Genesis** — the next-generation, modernized iteration of the KAG script language (evolved from
KAG3, KAG3-compatible; legacy KAG3 projects can be imported and migrated).
Caesura 是一个开源 galgame / 视觉小说引擎，将 **Live2D、3D 小游戏、SMA 骨骼动画、i18n 多语言、云存档与 AI 辅助创作**
作为一等公民。其脚本语言是 **KAG Neo-Genesis**——KAG 脚本语言的新一代现代化迭代（由 KAG3 演化而来，兼容 KAG3；旧版 KAG3 工程可导入迁移）。

引擎自带 **16 步教程路径**（`demo/tutorial/tutorial_01_hello.ks` → `tutorial_16_tween.ks`）与一个
**完整示例游戏《单程回信》（The One-Way Reply）**（`demo/example_game/`，三结局，约 15–18 分钟）。
The engine ships a **16-step tutorial path** (`demo/tutorial/tutorial_01_hello.ks` → `tutorial_16_tween.ks`)
and a **complete example game "The One-Way Reply"** (`demo/example_game/`, three endings, ~15–18 minutes).

> **快速克隆提示（Fast Clone）**：仓库历史中曾包含早期测试构建产物。推荐使用部分克隆（仅按需下载最新 blob，体积从 ~400MB 降至 ~25MB）：
> ```bash
> git clone --filter=blob:none https://github.com/ailiasdesu/Caesura-AmeKAG.git
> ```

---

## 统一语义分析架构（KAG Unified Semantic Architecture）

```
                        ┌─────────────────────────┐
                        │     KAG Source (.ks)    │
                        └────────────┬────────────┘
                                     │
                        ┌────────────▼────────────┐
                        │   LPeg Tokenizer Engine │
                        │ (scripts/tokenizer.lua) │
                        └────────────┬────────────┘
                                     │
                        ┌────────────▼────────────┐
                        │   Unified Semantic AST  │
                        │ (scripts/kag/semantic)  │
                        └────────────┬────────────┘
          ┌──────────────────────────┼──────────────────────────┐
          │                          │                          │
 ┌────────▼────────┐        ┌────────▼────────┐        ┌────────▼────────┐
 │ Runtime Engine  │        │  Creator Tools  │        │ Analysis & IDE  │
 │ (Compiler/VM)   │        │                 │        │                 │
 ├─────────────────┤        ├─────────────────┤        ├─────────────────┤
 │ • compiler.lua  │        │ • Story Flow    │        │ • LSP (lsp.lua) │
 │ • scheduler.lua │        │ • i18n Pipeline │        │ • ks_check.lua  │
 │ • compile(ast)  │        │ • CSV/PO Export │        │ • AI Dev Tools  │
 └─────────────────┘        └─────────────────┘        └─────────────────┘
```

---

## 创作者统一命令行（Creator Toolchain CLI）

Caesura 提供了开箱即用的统一脚手架工具 `scripts/caesura.py`：

```bash
# 1. 环境体检（检查 Lua、Python、Node、FFmpeg、CARC 打包工具）
python scripts/caesura.py doctor

# 2. 一键创建新游戏（支持 showcase / basic / live2d 模板）
python scripts/caesura.py create my_project --template showcase

# 3. 剧情分支图生成与死分支诊断（Mermaid / JSON 拓扑图）
python scripts/caesura.py flow my_project/ --lint

# 4. 多语言本地化全流程（提取 -> 翻译 -> 编译 -> 覆盖率门禁）
python scripts/caesura.py i18n my_project/ --extract
python scripts/caesura.py i18n my_project/ --lint

# 5. 剧本静态契约校验
python scripts/caesura.py check my_project/story.ks
```

---

## 引擎能力总览（Capability Overview / 引擎能力总览）

引擎能力按 6 个能力域组织，共 **82 项跟踪能力**（权威矩阵：
[docs/design/engine-capability-matrix.md](docs/design/engine-capability-matrix.md)，87 项能力行含子项展开，82 个 code-level 能力面）。
EN: Capabilities are organized into 6 domains with **82 tracked capabilities** (authoritative matrix:
[docs/design/engine-capability-matrix.md](docs/design/engine-capability-matrix.md); 87 rows including sub-items, 82 top-level code-level capability surfaces).

### 渲染 Rendering (11 项 / 11 items)

bgfx 多后端 GPU 渲染（**D3D11 / OpenGL 4.3 已真机验证**，Metal 引擎侧完整、需 macOS 硬件运行时验证）；
三层合成（BG/FG/MSG）带脏矩形优化；异步纹理加载 + 预算 + LRU 淘汰；2D GPU 粒子；视频播放（pl_mpeg MPEG-1 + 可选 FFmpeg）；
自适应 GPU 质量监控与自动降级；FreeType 文本渲染（CJK + Ruby 注音，NotoSansCJKsc 整字体 786 字形实测）；
转场特效（blend / wipe / 自定义 shader）；渲染到纹理 + 视口 blit；批量绘制协议；错误界面（ErrorUI）直接 bgfx 渲染、零 GUI 框架依赖、崩溃时也能显示错误；**后处理链**（bloom / vignette / LUT 调色 / softblur，全屏阶段 + ping-pong 临时 RTT，`[vfx postfx=]` 命令族 + `Render.set_postfx` API）。
EN: Multi-backend bgfx rendering (D3D11/OpenGL 4.3 real-GPU verified; Metal complete engine-side, needs macOS hardware);
3-layer compositing (BG/FG/MSG) with dirty-rect optimization; async texture loading + budget + LRU; 2D GPU particles;
video (pl_mpeg MPEG-1 + optional FFmpeg); adaptive GPU quality monitor with auto-degradation; FreeType text (CJK + ruby,
NotoSansCJKsc 786 glyphs measured); transitions (blend/wipe/custom shader); render-to-texture + viewport blit; batched drawing;
ErrorUI renders via pure bgfx with zero GUI-framework dependency, so errors display even on crash;
post-processing chain (bloom / vignette / LUT grade / soft blur; full-screen per-stage passes over a ping-pong scratch-RTT pool;
[vfx postfx=] command family + Render.set_postfx API).

### 脚本 Scripting (40 项 / 40 items)

Lua 5.4 VM（协程调度器 + 沙箱 + 指令预算防死循环）；**KAG Neo-Genesis 解析器**（134 个声明式契约命令、
KAG3 兼容裸参数 / TJS 表达式 / 旧变量系统 / 控制流）；`[until exp timeout]` 声明式条件等待、
`[button cond]` 条件选择（Ren'Py menu 对齐）、`[nvl]` NVL 模式、内联文本标记（`{color}`/`{size}`/`{b}`/`{i}`/`{s}`）、
i18n 本地化管线（`{key}` token + 逐行翻译 + 中英日热切换 + 整页重绘，超 Ren'Py）、参数化宏（含嵌套宏定义）、
标签索引 O(1) 跳转、场景级调试器（断点/单步/作用域检查）、mod 加载器、输入录制/回放（自动演示 + 帧导出 PNG → 视频）、
无障碍（字幕、TTS 钩子、色盲/高对比滤镜）、LLM 对话（`[ai_dialog]`，OpenAI 兼容 / Ollama 实测）、
LSP 导航（label 定义/引用）、AOT 编译与嵌套预算硬化、**声明式补间 `[tween]`（ctx.tweens 管理器，线性/缓入/缓出/回弹）**、**声明式布局 `[layout...]`（hbox/vbox/grid 容器 + 纯数学坐标计算，与 [position]/[tween] 组合）**。契约命令运行时执行覆盖：round 107 记录 **123/123**；此后契约总数增至 **134**，新增 11 条的覆盖**尚未复核**（unverified）。指令预算防死循环。
EN: Lua 5.4 VM (coroutine scheduler + sandbox + instruction budget against infinite loops); **KAG Neo-Genesis parser**
(134 declarative contract commands; KAG3-compatible bare args / TJS expressions / legacy variable system / control flow);
`[until exp timeout]` conditional waits, `[button cond]` choices (Ren'Py-menu aligned), `[nvl]` mode, inline marks,
i18n pipeline (`{key}` tokens + line-by-line translation + real-time zh/en/ja switching + full-page redraw, beyond Ren'Py),
parameterized macros (nested definitions), O(1) label-index jumps, scene debugger (breakpoints/step/scope), mod loader,
input record/replay (auto-demo + frame-export PNG → video), accessibility (subtitles, TTS hooks, color-blind/high-contrast filters),
LLM dialogue (`[ai_dialog]`, OpenAI-compatible, measured on Ollama), LSP navigation, AOT compile + nested-budget hardening;
declarative `[tween]` (ctx.tweens manager; linear/ease-in/out/back-out) and declarative `[layout...]` (hbox/vbox/grid containers,
pure-math coordinate solver composing with [position]/[tween]).
Runtime execution coverage was **123/123 as recorded at round 107**; the contract total has since grown to **134** and the 11 newer commands are **not yet re-audited**. The instruction budget prevents infinite loops.

### 音频 Audio (4 项 / 4 items)

SoLoud 三总线（**BGM 交叉淡化 / Voice 打断 / SE**）；淡入淡出、音量、位置查询；3D 空间音频（听者位置 + 3D SE 放置）；
逐 SE 句柄音量与停止控制。音频模块另含 4 槽语音池（重叠行淡出不截断）与 BGM ducking。
EN: SoLoud 3-bus audio (BGM cross-fade / Voice interrupt / SE); fades, volume, position queries; 3D spatial audio
(listener position + 3D SE placement); per-SE handle volume and stop. A 4-slot voice pool (overlapping lines fade, not cut) and BGM ducking.

### 内容 Content (10 项 / 10 items)

**Live2D Cubism 5**（PNG 静态回退 + D3D11 真机验证，OpenGL/Metal 渲染路径已实现）；**3D 小游戏框架**
（enter→update→render→leave 生命周期 + 20 API Lua 绑定）；加密存档（JSON + AES-256-GCM，防篡改审计见
[save-security-audit.md](docs/design/save-security-audit.md)）；存档 schema 迁移（v1→v5 自动升级）；
**CARC 归档**（压缩 + AES-256-GCM 加密 + Ed25519 签名）；云存档 provider 抽象（本地 / HTTP REST 云端，离线降级）；
Steamworks（成就/统计/云存档，条件编译 + Null 默认）；资产 provider 链（Dir → CARC 优先级 + 完整性检查）；
**.xp3/.tlg 读取器原型**（round 111，KAG3 存量资产撬动，见 [xp3-compat.md](docs/guides/xp3-compat.md) /
[tlg-compat.md](docs/guides/tlg-compat.md)）；教程示例库（tutorial 01–16 + showcase + 完整示例游戏）。
EN: **Live2D Cubism 5** (PNG static fallback + D3D11 real-GPU verified; OpenGL/Metal paths implemented); **3D mini-game
framework** (enter→update→render→leave lifecycle + 20 API Lua bindings); encrypted saves (JSON + AES-256-GCM; tamper audit in
[save-security-audit.md](docs/design/save-security-audit.md)); save schema migration (v1→v5 auto-upgrade); **CARC archives**
(compression + AES-256-GCM + Ed25519 signing); cloud-save provider abstraction (local / HTTP REST with offline degradation);
Steamworks (achievements/stats/cloud, conditionally compiled + Null default); asset provider chain (Dir → CARC priority + integrity);
**.xp3/.tlg reader prototypes** (round 111, leveraging legacy KAG3 assets — [xp3-compat.md](docs/guides/xp3-compat.md) /
[tlg-compat.md](docs/guides/tlg-compat.md)); tutorial sample library (tutorial 01–16 + showcase + the full example game).

### 开发工具 Development (10 项 / 10 items)

编辑器 RPC（**HTTP :9876 + stdin/stdout JSON-RPC 双传输**，36 HTTP 端点 / 29 stdio 方法，owner-thread DTO 分发，
传输层不持有 `lua_State*`）；结构日志（环形缓冲 + 子系统统计）；帧剖析（GPU 提交数 / 瞬时分配 / Lua GC）；
NullJobSystem 同步测试替身；headless 无 GPU 模式；DevMode 棋盘格占位纹理；Lua 调试器（断点/步进/检查 + 陈旧暂停拒绝）；
AI 开发助手（`kag/aidev.lua`，本地规则诊断 + LLM 解释/修复/生成 + IDE /api/eval 接入）；LSP 导航（Monaco provider）；
**SMA 骨骼网格动画**（CPU 软蒙皮 + GPU compute 蒙皮（D3D11 像素级一致验证）+ 2-bone IK + E-mote 部件切换 +
`[sma_play]`/`[sma_anim]`/`[sma_ik]` 等契约 + `sma.*` Lua 绑定）。
EN: Editor RPC (**dual transport: HTTP :9876 + stdin/stdout JSON-RPC**, 36 HTTP endpoints / 29 stdio methods,
owner-thread DTO dispatch, transport never holds a `lua_State*`); structured logging (ring buffer + subsystem stats);
frame profiling (GPU submits / transient allocations / Lua GC); NullJobSystem synchronous test double; headless no-GPU mode;
DevMode checkerboard placeholder; Lua debugger (breakpoints/step/inspect + stale-pause rejection); AI dev assistant
(`kag/aidev.lua`, local rule diagnostics + LLM explain/fix/generate + IDE /api/eval); LSP navigation (Monaco provider);
**SMA skeletal mesh animation** (CPU soft-skinning + GPU compute skinning (pixel-identical on D3D11) + 2-bone IK +
E-mote part switching + `[sma_play]`/`[sma_anim]`/`[sma_ik]` contracts + `sma.*` Lua bindings).

### 平台 Platform (7 项 / 7 items)

跨平台（Windows MSVC / Linux GCC / macOS Clang，CI 三平台构建 + 测试）；GitHub Actions 流水线（Release + CPack ZIP 端到端）；
多线程任务系统（优先级队列 + 主线程回调）；输入路由（KAG ↔ Game 焦点切换）；纹理预算自动探测（6 档 128MB–4GB）；
Lua 沙箱资源配额（纹理/发射器/句柄）；**MobileAdapter**（SDL 手指事件桥接、方向变化事件、触摸→鼠标注入，87 单元测试；Android NDK 构建链见
[android-build.md](docs/guides/android-build.md)）。
EN: Cross-platform (Windows MSVC / Linux GCC / macOS Clang; 3-platform CI build + test); GitHub Actions pipeline
(Release + CPack ZIP end-to-end); multi-threaded job system (priority queue + main-thread callbacks); input routing
(KAG ↔ Game focus switching); automatic texture-budget detection (6 tiers 128MB–4GB); Lua sandbox resource quotas
(textures/emitters/handles); **MobileAdapter** (SDL finger-event bridging, orientation events, touch→mouse injection,
87 unit tests; Android NDK build chain in [android-build.md](docs/guides/android-build.md)).

### 能力矩阵摘要（82 项，按域分类计数 / Capability matrix summary — 82 items by domain）

| 能力域 | 计数 | 代表性能力（详见矩阵） |
|--------|:----:|------------------------|
| 渲染 Rendering | 11 | 多后端 GPU / 图层合成 / 粒子 / 视频 / 文本 / 转场 / RTT / 后处理链 |
| 脚本 Scripting | 40 | KAG 解析器 / 流程控制 / 调试器 / i18n / 宏 / 热重载 / LSP / AI / tween / layout |
| 音频 Audio | 4 | 三总线 / 3D 空间 / 逐句柄控制 |
| 内容 Content | 10 | Live2D / 3D 小游戏 / 存档加密 / CARC / 云存档 / Steam / 资产链 |
| 开发工具 Development | 10 | 编辑器 RPC / 调试器 / AI 助手 / SMA 骨骼动画 |
| 平台 Platform | 7 | 跨平台 / CI / 线程池 / 预算 / 配额 / Mobile |
| **合计 / Total** | **82** | 完整状态逐项见 [engine-capability-matrix.md](docs/design/engine-capability-matrix.md) |

就绪度快照（2026-08-16 round 98 刷新，保守口径）：架构模块化 98% · 核心视觉小说可用性 74% · 跨平台产品发布 34%（Metal/非 Windows 真机验证与可选 SDK 尚未在发布条件验证）。
EN: Readiness snapshot (refreshed 2026-08-16 round 98, conservative): modular architecture 98% · core VN usability 74% · cross-platform product release 34% (Metal / non-Windows hardware validation and optional SDKs not yet release-verified).

---

## 架构详解（Architecture / 架构详解）

### 组合根模式（Composition Root + DI / 组合根模式）

引擎遵循严格依赖注入：**`src/main.cpp` + `src/entry/` 是唯一创建具体后端对象的组合根**，其余模块一律通过 `BackendRegistry` 获取接口指针：`main.cpp → 创建具体后端 → EngineConfig → Engine::init() → 注册 I* 指针进 BackendRegistry → 其余模块 getXxx()`。
EN: Strict DI: **`src/main.cpp` + `src/entry/` is the only composition root creating concrete backends**; every other module gets interface pointers via `BackendRegistry`: `main.cpp → create backends → EngineConfig → Engine::init() → register I* into BackendRegistry → others call getXxx()`.

### 子系统拓扑（Subsystem topology / 子系统拓扑）

```
┌──────────────────────────────────────────────────────────┐
│              Editor / automation client                  │
└───────────────┬──────────────────────────────────────────┘
                │ HTTP or stdin/stdout RPC DTO
┌───────────────▼──────────────────────────────────────────┐
│              Host transport (main.cpp)                   │
│      RpcServer / EditorServer · no direct Lua access     │
└───────────────┬──────────────────────────────────────────┘
                │ owner-thread dispatcher pump
┌───────────────▼──────────────────────────────────────────┐
│              Engine (C++20 src/entry/)                   │
│  render(bgfx) · audio(SoLoud) · script(Lua5.4)           │
│  live2d(3D PBR) · minigame · storage · archive(CARC)     │
│  platform(SDL3) · input · job · steam · debug · di        │
└──────────────────────────────────────────────────────────┘
```

**16 个内部模块（15 个子系统库 + entry 组合根）**，运行时后端访问全部集中在 `BackendRegistry` 并通过 **34 个纯虚接口头（412 个纯虚方法）** 暴露（权威计数由 `python scripts/api_stats.py` 生成，见 [api-stats.md](docs/api/api-stats.md)；下表为其中的 runtime 后端服务子集）。CMake 目标图无循环依赖；实现级依赖见 [engine-architecture-topology.md](docs/design/engine-architecture-topology.md)。
EN: **16 internal modules (15 subsystem libraries + the entry composition root)**; all runtime backend access goes through `BackendRegistry`, exposed via **34 pure-virtual interface headers (412 pure-virtual methods)** — authoritative counts are generated by `python scripts/api_stats.py` (see [api-stats.md](docs/api/api-stats.md)); the table below lists the runtime-backend subset. The CMake target graph has no circular dependencies; implementation-level deps are in [engine-architecture-topology.md](docs/design/engine-architecture-topology.md).

### 数据流（Lua → Scheduler → Backend / Data flow）

`.ks 剧本 → tokenizer → scheduler.lua（协程调度）→ kag/commands/*.lua（命令处理器 + 契约校验）→
backend.* / layers.* / VFX.* Lua 绑定 → C++ 接口（IRenderDevice / IAudioBackend / ISaveManager …）→ BackendRegistry`。

两种脚本执行模式（可混合）：**直接 API**（Lua 直接调 `backend.*` / `layers.*`，类似 Ren'Py）；
**KAG .ks 调度**（`scheduler.lua` tokenize 后分发给命令处理器）；**KAG+Lua 混合**（`[eval]` / `[emb]` / `[iscript]`
内嵌 Lua，`kag.jump`/`kag.call`/`kag.save_game` 实现 Lua→KAG 回调，引擎侧绑定在 `src/script/bindings/`，经 `ILuaManager` 暴露给 Lua）。
EN: `.ks script → tokenizer → scheduler.lua (coroutine scheduling) → kag/commands/*.lua (handlers + contract validation) →
backend.* / layers.* / VFX.* Lua bindings → C++ interfaces (IRenderDevice / IAudioBackend / ISaveManager …) → BackendRegistry`.
Two script modes (mixable): **direct API** (Lua calls `backend.*` / `layers.*` directly, Ren'Py-style); **KAG .ks scheduling**
(`scheduler.lua` tokenizes then dispatches to command handlers); **KAG+Lua hybrid** (`[eval]` / `[emb]` / `[iscript]` embed
Lua in .ks; `kag.jump`/`kag.call`/`kag.save_game` provide Lua→KAG callbacks; engine-side bindings in `src/script/bindings/`, exposed via `ILuaManager`).

### 模块地图（Module map — 16 modules / 模块地图）

| # | 模块 | 角色 | 接口数 |
|---|------|------|:-----:|
| 1 | `render` | bgfx GPU 渲染：图层/粒子/文本/视频/GPU 恢复 | 7 |
| 2 | `script` | Lua VM、KAG 绑定、GameState、tokenizer、scheduler | 1 |
| 3 | `resource` | 异步资产加载、provider 链、图像解码 | 3 |
| 4 | `live2d` | 动画后端（Cubism SDK 或 PNG 回退） | 1 |
| 5 | `archive` | CARC 归档：AES-256-GCM + Ed25519 签名 | 3 |
| 6 | `minigame` | 3D 小游戏场景（enter/update/render/leave） | 1 |
| 7 | `storage` | 加密存档、schema 迁移、云同步 | 2 |
| 8 | `audio` | SoLoud 三总线音频（BGM/Voice/SE）+ 3D 空间 | 1 |
| 9 | `entry` | 组合根：Engine + EngineConfig + 4 阶段 init | — |
| 10 | `di` | BackendRegistry + 纹理预算 + 沙箱配额 | 3 |
| 11 | `debug` | 结构日志、帧剖析、子系统统计 | 1 |
| 12 | `platform` | SDL3/Null 窗口、事件、计时、原生句柄 | 2 |
| 13 | `rpc` | HTTP/stdio 传输 + owner-thread 命令 DTO 接口 | 3 |
| 14 | `input` | SDL 事件路由（KAG ↔ Game 焦点切换） | 1 |
| 15 | `job` | 多线程任务系统 + NullJobSystem mock | 1 |
| 16 | `steam` | Steamworks 成就/统计/云存档（条件编译） | 1 |
| | **合计 / Total** | | **31** |

### runtime 后端服务接口（下列 31 项为 `BackendRegistry` 暴露的服务；接口头总数 34 见 api-stats.md / runtime backend services)

| 接口 | 模块 | 实现 |
|-------|------|------|
| `IRenderDevice` | render | BgfxRenderDevice（D3D11/OpenGL/Metal） |
| `ITextureManager` | render | TextureManager（bimg + stb） |
| `ILayerManager` | render | LayerManager v2（动态图层 / BG/FG/MSG 默认布局合成） |
| `IParticleSystem` | render | ParticleSystem（2D GPU 粒子） |
| `IGpuMonitor` | render | GpuMonitor / NullGpuMonitor |
| `IVideoPlayer` | render | VideoPlayer（pl_mpeg / FFmpeg） |
| `IMeshRenderer` | render | MeshRenderer（2D 骨骼网格 / SMA 角色） |
| `ILuaManager` | script | LuaManager（Lua 5.4，指令预算） |
| `IAssetProvider` | resource | DirProvider / CARCProvider 链 |
| `IAsyncLoader` | resource | AsyncLoader（工作线程解码） |
| `IResourceGenerationTracker` | resource | GenerationTracker（热重载句柄代） |
| `IAnimationBackend` | live2d | Live2DBackend / NullAnimationBackend |
| `IArchiveReader` | archive | CARCReader |
| `IArchiveWriter` | archive | CARCWriter |
| `ICryptoEngine` | archive | CryptoEngine（AES-256-GCM + Ed25519） |
| `IMiniGameBackend` | minigame | BgfxMiniGameBackend（预留） |
| `ISaveManager` | storage | SaveManager（JSON、加密、schema v5） |
| `ISaveProvider` | storage | LocalFileSaveProvider / Cloud |
| `IAudioBackend` | audio | SoLoudAudioEngine |
| `ITextureBudget` | di | TextureBudget（自动探测 6 档） |
| `ISandboxQuota` | di | SandboxQuotaService（Lua 资源限额） |
| `IDeviceLostListener` | di | GPU 资源丢失/恢复观察者 |
| `IDebugManager` | debug | DebugManager（环形缓冲、剖析） |
| `IPlatformBackend` | platform | SDL3PlatformBackend |
| `IMobileAdapter` | platform | MobileAdapter（手指事件/方向/触摸→鼠标） |
| `IEditorServer` | rpc | EditorServer（httplib，36 个 HTTP 端点） |
| `IRpcServer` | rpc | RpcServer（JSON-RPC） |
| `IRpcDispatcher` | rpc | 组合根 owner-thread 分发器 |
| `IInputRouter` | input | InputRouter（KAG/Game 焦点） |
| `IJobSystem` | job | JobSystem / NullJobSystem |
| `ISteamBackend` | steam | SteamBackend（条件编译） |

### BackendRegistry —— 唯一访问点（The single access point / 唯一访问点）

所有后端访问必须通过 `BackendRegistry`（`src/di/`），禁止绕过。
All backend access must go through `BackendRegistry` (`src/di/`); bypassing is forbidden.

```cpp
// Correct 正确
auto* renderer = BackendRegistry::instance().getRenderDevice();
auto* lua = BackendRegistry::instance().getLuaState();

// Wrong 错误（绕过注册表 / bypasses the registry）
auto& tm = TextureManager::instance();
auto* L = LuaManager::instance().state();
```

- `BackendRegistry` 存非拥有指针（`I*`），Engine 持有 `unique_ptr` 所有权；
- 子系统通过 `set*()` 注册、`get*()` 访问；新后端流程：创建 `I*` 接口 → 实现 → registry 加 set/get → `Engine::init()` 注册。
- EN: `BackendRegistry` stores non-owning pointers (`I*`); the Engine owns them via `unique_ptr`;
  subsystems register with `set*()` and access with `get*()`; new backend flow: create an `I*` interface →
  implement it → add set/get to the registry → register in `Engine::init()`.

### 模块边界铁律（Module-boundary rules — [AGENTS.md](AGENTS.md) §1–4 summary / 模块边界铁律）

1. **每个模块只能通过 `api/` 子目录对外暴露符号**（`src/<module>/api/I*.h`）；
2. **禁止模块间 include 具体实现头文件**——只允许 `I*.h`（接口必须纯虚、无数据成员）；
3. **唯一例外**：`src/entry/` + `src/main.cpp`（组合根）可 include 具体头创建对象；
4. **`di/BackendRegistry.h` 只 include `I*.h`**，绝不 include 具体实现；
5. 禁止循环依赖、禁止头文件级具体类型依赖、禁止非组合根 `new` 具体后端；
6. 耦合预算：`entry`/`di`/`script` ≤14 跨模块依赖，其余模块 ≤4（`python scripts/count_coupling.py --ci` 校验）。
EN: 1. Each module exposes symbols only via its `api/` subdirectory (`src/<module>/api/I*.h`); 2. No cross-module
includes of concrete headers — only `I*.h` (pure-virtual, no data members); 3. Sole exceptions: `src/entry/` +
`src/main.cpp` (composition root) may include concrete headers; 4. `di/BackendRegistry.h` includes only `I*.h`;
5. No circular deps, no concrete-type header deps, no `new` of concrete backends outside the composition root;
6. Coupling budget: `entry`/`di`/`script` ≤14 cross-module deps, all others ≤4 (`python scripts/count_coupling.py --ci`).

---

## 快速开始（Quick Start）

### 0. 环境要求（Requirements）

> **工具链 / Toolchain**：Windows 用 VS2022 + CMake 3.25+ + vcpkg；Linux (Ubuntu 24.04) 用 GCC 13+（SDL3 源码构建、FreeType、zstd、OpenSSL）；macOS 用 Xcode 15+ + Homebrew（`brew install cmake sdl3 freetype zstd openssl@3 ffmpeg`）。
> Windows: VS2022 + CMake 3.25+ + vcpkg; Linux (Ubuntu 24.04): GCC 13+ (SDL3 built from source, FreeType, zstd, OpenSSL); macOS: Xcode 15+ + Homebrew.

> **运行要求 / Runtime**：引擎从**项目根目录**启动（资源路径相对 CWD 解析）；Lua 解释器由 CMake 的 `lua_cli` 目标产出（`build/lua/<配置>/lua.exe`），发布包内置为 `external/lua/lua.exe`——该文件不入库，全新克隆里没有。
> Run the engine from the **repository root** (asset paths resolve relative to CWD); the Lua interpreter is built by the `lua_cli` CMake target (`build/lua/<config>/lua.exe`) and ships inside the release package as `external/lua/lua.exe` — it is gitignored and never present in a fresh clone.

### 1. 克隆（Clone）

```bash
git clone <your-fork-url> && cd "Caesura(AmeKAG)"
```

### 2. 构建（Build, three platforms / 三平台）

```bash
# Windows（MSVC，主开发平台 / primary dev platform)
cmake -B build -DCAESURA_LIVE2D=OFF
cmake --build build --config Debug --parallel

# macOS / Linux（FFmpeg 常不可用，关闭它 / often unavailable, disable it)
cmake -B build -DCAESURA_LIVE2D=OFF -DCAESURA_ENABLE_FFMPEG=OFF
cmake --build build -j$(nproc)

# 可选：Live2D Cubism SDK（需手动下载 / requires manual download)
cmake -B build -DCAESURA_LIVE2D=ON -DCUBISM_SDK_ROOT="path/to/CubismSdkForNative-5-r.5"
```

**CMake 选项（CMake options）**：

| 选项 | 默认 | 用途 |
|------|------|------|
| `CAESURA_LIVE2D` | `OFF` | Live2D Cubism SDK 动画（需手动 SDK） |
| `CAESURA_ENABLE_FFMPEG` | `ON` | 硬解视频（找不到库时回退 pl_mpeg，不阻断构建） |
| `CAESURA_DEBUG` | Debug 开 / Release 关 | 调试日志与断言 |

启动引擎（Launching the engine）：

```bash
./build/Debug/CaesuraAmeKAG.exe            # 正常启动（项目根 / normal launch from repo root)
./build/Debug/CaesuraAmeKAG.exe --editor   # 编辑器模式（HTTP :9876 + 隐藏 GPU 窗口 / editor mode)
```

### 3. 跑测试（Run tests: four suites + CTest / 四套件 + CTest）

| 套件 | 命令（仓库根或标注目录） | 通过标准 |
|------|--------------------------|----------|
| C++ doctest | `cd build/tests/Debug && ./CaesuraTests.exe` | 0 failed, 0 skipped（2026-08-28 实测 1120 用例 / 385790 断言） |
| C++ CTest | `ctest -C Debug --test-dir build --output-on-failure` | 全过（14 target，`ctest --test-dir build -N` 列出） |
| Lua 主套件 | `build/lua/Debug/lua.exe tests/scripts/run_lua_tests.lua` | 全过（实测 143，顺序敏感） |
| Lua 孤儿套件 | `build/lua/Debug/lua.exe tests/scripts/run_orphan_tests.lua` | 全过（24 套件，单独跑） |
| Web vitest | `cd web && npm test` | 全过（工件齐备实测 368/368 · 27 文件 · 0 skipped；缺 story bundle/web dist 时为 342 通过 + 27 环境跳过） |
| Editor vitest | `cd editor && npm test` | 全过（实测 615 用例 / 35 文件） |
| 耦合门禁 | `python scripts/count_coupling.py --ci` | PASS（entry/di/script ≤14，其余 ≤4） |
| Web 索引守卫 | `node web/gen-index.mjs --check` | CHECK OK（改过 `scripts/*.lua` 先重跑 gen-index） |
| 平台矩阵守卫 | `python scripts/generate_platform_status.py --check` | `[OK] ... up-to-date` |
| 首个 VN 旅程 | `bash scripts/verify_first_vn.sh` | `RESULT: PASS (13/13 checks)` |

> 上表数字是 2026-08-27 在 master `6053024b` 的**本机实跑值**，随开发增长；判定标准是
> **0 failed / 0 skipped**，不要把具体条数当门禁。

doctest 过滤器：`-tc="*Name*"`（用例名）、`-ts="*Suite*"`（套件名）、`-tce`（排除）、`-s`（显示通过）、`-d`（耗时）。
doctest filters: `-tc="*Name*"` (test case), `-ts="*Suite*"` (suite), `-tce` (exclude), `-s` (show passes), `-d` (durations).

### 4. 跑示例游戏《单程回信》（Run the example game "The One-Way Reply"）

```bash
# 仓库根（git bash / POSIX）
bash scripts/verify_sample_game.sh        # 端到端验证（ks_check + headless DONE + 三结局可达，5/5 PASS）
lua demo/example_game/entry.lua          # 直接跑（无 lua 用 build/lua/Debug/lua.exe）
```

一键打包为静态 Web 站（itch.io / GitHub Pages / Netlify）：
Package into a static Web site in one step (itch.io / GitHub Pages / Netlify):

```bash
bash scripts/package_game.sh demo/example_game   # 产物在 dist/<game>/，直接上传分发 / output in dist/<game>/
```

### 5. 跑项目模板（Run the project template — new-game scaffold）

round 113 新增 `demo/template/`：最小两场景 + 一次 `[select]` 的剧本骨架 + `entry.lua` 多路径回退 +
`assets/` 占位 + 五步指南（[template-quickstart.md](docs/guides/template-quickstart.md)）。
Added in round 113: `demo/template/` — a minimal two-scene script skeleton with one `[select]`, an
`entry.lua` multi-path fallback, `assets/` placeholders and a five-step guide
([template-quickstart.md](docs/guides/template-quickstart.md)).

```bash
bash scripts/verify_template.sh           # 4/4 PASS（ks_check 零警告 + headless DONE + 两分支可达）
bash scripts/package_game.sh demo/template   # 打包模板为 Web 站 / package template as a Web site
```

复制 `demo/template/` 到新目录改 `story.ks`，就是你的第一个游戏工程。
Copy `demo/template/` to a new directory and edit `story.ks` — that is your first game project.

### 6. Web 播放器（Web player — run KAG in the browser / 浏览器里跑 KAG）

`web/` 是纯前端 KAG Web 播放器（wasmoon Lua VM + vite），无需构建 C++ 引擎即可在浏览器跑 `.ks`：
`web/` is a front-end-only KAG Web player (wasmoon Lua VM + vite); it runs `.ks` in the browser without
building the C++ engine:

```bash
cd web && npm ci          # 首次装依赖 / first-time dependency install
npm run dev:web           # 开发模式 → http://127.0.0.1:5174
npx vite build            # 生产构建 → web/dist/
npm test                  # vitest 套件
```

> 播放器优先加载 ks_bake 预编译 bundle（零解析启动）；`lua scripts/ks_bake.lua --dir demo --web cache/story`
> 可重新生成以包含新示例。CI 会跑 `node web/gen-index.mjs --check` 守卫脚本索引一致性。
> The player prefers the ks_bake precompiled bundle (zero-parse startup); re-run
> `lua scripts/ks_bake.lua --dir demo --web cache/story` to include new samples. CI runs
> `node web/gen-index.mjs --check` to guard script-index consistency.

### 7. KAG3 工程导入（Importing legacy KAG3 projects, optional / 可选）

```bash
lua scripts/kag3_import.lua <scene.ks>            # KAG3 → KAG Neo-Genesis
lua scripts/kag3_import.lua --strict <scene.ks>   # 严格模式（干净退出码 0 / clean exit code 0)
lua scripts/kag3_import.lua --carc game.carc --path assets/script/main.ks   # 从 CARC 提取导入
```

详见 [kag3-import.md](docs/guides/kag3-import.md) 与 [kag3-migration.md](docs/guides/kag3-migration.md)
（六步迁移流水线：xp3 解包 → tlg→png → 音频直用 → 导入 → 资产路径重写 → 验证）。
See [kag3-import.md](docs/guides/kag3-import.md) and [kag3-migration.md](docs/guides/kag3-migration.md)
(a six-step migration pipeline: unpack xp3 → tlg→png → reuse audio → import → rewrite asset paths → verify).

## 示例游戏《单程回信》（Example Game: The One-Way Reply / 示例游戏）

引擎自带一个完整的示例游戏 **《单程回信》**（`demo/example_game/`，round 101 立项 → round 105 填充完成
→ round 110 收尾/文档）。
The engine ships a complete example game, **The One-Way Reply** (`demo/example_game/`; round 101 kickoff →
round 105 content complete → round 110 polish/docs).

- **类型 / Genre**：现代校园 · 温情悬疑 · 短篇多结局视觉小说（约 15–18 分钟）— modern campus, warm mystery,
  short multi-ending VN (~15–18 minutes)
- **结局 / Endings**：3 个——真结局「归零」 / 好结局「同行」 / 普通结局「守约」，均以 `[ending]` 解锁画廊 —
  3: true ending "归零" / good ending "同行" / normal ending "守约", each unlocks the gallery via `[ending]`
- **角色 / Characters**：主角 / 澪（Mio）/ 潮（Ushio），8 个流程节点 — protagonist / Mio / Ushio, 8 flow nodes
- **演示能力 / Capabilities demoed**：多章节流程、玩家选择与信任差分（`[if f.trust>=2]`/`[add]`/`[sub]` 线索）、
  变量/插值/表达式（`${tf.letters}` 内插、复数表达式）、参数化宏、Lua 混合、双存档点、
  中英 i18n 热切换（16 键 × zh/en 32 格零缺失）、转场/粒子/后处理（雷雨 flash/quake/shake/vib + LUT 调色）、
  SMA 骨骼动画小游戏融合（`[sma_play]`/`[sma_anim]`/`[sma_variant]`/`[sma_ik]`）、backlog/跳过/自动 —
  multi-chapter flow, player choice with trust branches, variables/interpolation/expressions
  (`${tf.letters}` interpolation, plural expressions), parameterized macros, Lua mixing, two save points,
  real-time zh/en i18n switching (16 keys × zh/en = 32 cells, zero missing), transitions/particles/post-FX
  (storm flash/quake/shake/vib + LUT color grading), SMA skeletal-animation mini-game blending
  (`[sma_play]`/`[sma_anim]`/`[sma_variant]`/`[sma_ik]`), backlog/skip/auto
- **启动 / Launch**：`lua demo/example_game/entry.lua`（或 `bash scripts/package_game.sh demo/example_game` 一键打包为静态 Web 站）

| 文档 | 内容 |
|------|------|
| [DESIGN.md](demo/example_game/DESIGN.md) | 完整设计文档：世界观、角色、8 个流程节点、三结局分流、能力展示清单（验收依据） |
| [README.md](demo/example_game/README.md) | 快速上手：演示能力表、结构、修改剧本、静态校验命令 |
| [sample-game-assets.md](docs/guides/sample-game-assets.md) | 资产审计：9 项直接复用 + 6 项占位安全降级 + i18n 键预留（round 105） |
| [sample-game-verification.md](docs/guides/sample-game-verification.md) | 端到端验证设施说明 |
| [sample-game-release.md](docs/guides/sample-game-release.md) | GitHub Releases / itch.io 双路径发布清单 |

**验证（Verification，`bash scripts/verify_sample_game.sh`，5/5 PASS）**：

1. **ks_check** 静态契约校验（story.ks 零警告 / zero warnings）
2. **headless 端到端**：kag_runner 驱动全剧本跑到 `[end]`（主路径 DONE，帧预算 200000）
3. **三结局可达性 / 3-ending reachability**：`ending_zero` / `ending_companion` / `ending_promise` 探针全部可达
4. 依赖 `tests/scripts/sample_game_headless.lua`（mock callable 绑定；`is_voice_playing`/`is_bgm_playing`
   必须返回 false，否则音频等待命令会死挂）
5. Web 冒烟为信息性人工步骤（不在此执行 / informational manual step, not run here）

---

## 教程体系（Tutorial System, Tutorials 01–16 / 教程 01–16）

16 个递进式教学剧本（`demo/tutorial/`，round 90 建成 14 个 → round 106 补 15、round 107 补 16），每个独立可跑、带逐行注释、经**引擎编译 + Web 播放器双重验证**。完整覆盖矩阵见 [sample-library.md](docs/guides/sample-library.md)（含每教程所用命令明细）。
EN: 16 progressive teaching scripts (`demo/tutorial/`; round 90 built 14 → round 106 added 15, round 107 added 16), each runnable standalone with line-by-line comments, verified by both the engine compiler and the Web player; full coverage matrix in [sample-library.md](docs/guides/sample-library.md).

| 步骤 | 文件 | 学习内容 |
|:----:|------|----------|
| 01 | [tutorial_01_hello.ks](demo/tutorial/tutorial_01_hello.ks) | 最小剧本结构：注释/命令格式/[ch]/[p]/[end] |
| 02 | [tutorial_02_text.ks](demo/tutorial/tutorial_02_text.ks) | 文本与排版：字体/字号/打字速度/说话人/语音/滚动 |
| 03 | [tutorial_03_layers.ks](demo/tutorial/tutorial_03_layers.ks) | 图层系统：背景/前景/立绘/位置/动画/清层 |
| 04 | [tutorial_04_audio.ks](demo/tutorial/tutorial_04_audio.ks) | 三总线音频：BGM/SE/Voice/音量/淡入淡出/交叉淡化 |
| 05 | [tutorial_05_branching.ks](demo/tutorial/tutorial_05_branching.ks) | 变量与流程：赋值/条件分支/标签跳转 |
| 06 | [tutorial_06_effects.ks](demo/tutorial/tutorial_06_effects.ks) | 特效与转场：闪白/震动/溶解/结局解锁 |
| 07 | [tutorial_07_saveload.ks](demo/tutorial/tutorial_07_saveload.ks) | 存档/读档：槽位/保存/读取/结果分支（Web 优雅降级） |
| 08 | [tutorial_08_system_ui.ks](demo/tutorial/tutorial_08_system_ui.ks) | 系统 UI：画廊/音乐室/历史/章节选择/解锁 |
| 09 | [tutorial_09_interpolation.ks](demo/tutorial/tutorial_09_interpolation.ks) | 文本插值与表达式：$tbl.key / %tbl.key% / ${expr} |
| 10 | [tutorial_10_loops.ks](demo/tutorial/tutorial_10_loops.ks) | 循环控制：for / while / eval 递减（65536 迭代守卫） |
| 11 | [tutorial_11_switch.ks](demo/tutorial/tutorial_11_switch.ks) | 多路分支：switch/case/default（KAG3 兼容） |
| 12 | [tutorial_12_expr_combo.ks](demo/tutorial/tutorial_12_expr_combo.ks) | 表达式组合实战：三元/空合并/循环+插值 |
| 13 | [tutorial_13_commands.ks](demo/tutorial/tutorial_13_commands.ks) | KAG3 兼容命令实战：textspeed/算术链/立绘/通知/调色 |
| 14 | [tutorial_14_flow_timing.ks](demo/tutorial/tutorial_14_flow_timing.ks) | 计时与流程：wait/delay/混合跳转/i18n 热切换 |
| 15 | [tutorial_15_expr_deep.ks](demo/tutorial/tutorial_15_expr_deep.ks) | 高级表达式：嵌套三元/多参函数/作用域前缀 |
| 16 | [tutorial_16_tween.ks](demo/tutorial/tutorial_16_tween.ks) | 声明式补间 [tween]：属性插值/5 缓动/非阻塞 |

运行方式（仓库根 / From the repo root）：

```bash
build/lua/Debug/lua.exe scripts/ks_check.lua demo/tutorial/tutorial_01_hello.ks   # 单个校验 / validate one
for f in demo/tutorial/tutorial_*.ks; do build/lua/Debug/lua.exe scripts/ks_check.lua $f; done  # 全部 / all
```

---

## KAG Script Compatibility / KAG 脚本兼容性

**134 个 KAG Neo-Genesis 命令**（declarative contracts，类别：system / text / layer / audio /
transition / vfx / save / resource / video / layout / tween / sma / math）——权威契约见
[command-contracts.md](docs/api/command-contracts.md)（自动生成，含类型/默认值/范围/必需/阻塞语义）。
**134 KAG Neo-Genesis commands** (declarative contracts; categories: system / text / layer / audio /
transition / vfx / save / resource / video / layout / tween / sma / math) — authoritative contracts in
[command-contracts.md](docs/api/command-contracts.md) (auto-generated: types / defaults / ranges /
required / blocking semantics).

```kag
*start
[bg storage="classroom.png" time="500"]
[playbgm storage="theme.ogg" volume="0.8"]
[ch name="Mei" text="Good morning!"]
[p]
[stopbgm time="300"]
[link target="chapter2"]
```

- KAG3 兼容层：裸位置参数、TJS 表达式（`&& || ! != ?:` 字符串感知翻译）、`%f.x%` 旧变量系统、
  `[elsif]`/`[call *label]`/`[end]`→ending、`[goto]`→`jump` 别名——见 [kag-expression-language.md](docs/api/kag-expression-language.md)
  与 [kag-language-tour.md](docs/guides/kag-language-tour.md)
- 语言白皮书（下一代设计：命令契约/schema/变量系统/宏/i18n）→ [kag-neo-genesis-language.md](docs/design/kag-neo-genesis-language.md)
- 标准定义 → [nextgen-kag-standard.md](docs/design/nextgen-kag-standard.md)
- 运行时契约覆盖：round 107 记录 **123/123 命令全部可执行**（round 90 起 118/118；round 102 +postfx、round 106 +tween、round 107 +layout → 123）。
  ⚠️ 契约总数此后增至 **134**，新增的 11 条**尚未复核运行时覆盖**（unverified，需重跑覆盖审计后再更新本行）
- KAG3 compatibility layer: bare positional arguments, TJS expressions (`&& || ! != ?:` string-aware
  translation), the `%f.x%` legacy variable system, `[elsif]`/`[call *label]`/`[end]`→ending,
  `[goto]`→`jump` alias — see [kag-expression-language.md](docs/api/kag-expression-language.md) and
  [kag-language-tour.md](docs/guides/kag-language-tour.md)
- Language whitepaper (next-gen design: command contracts/schema/variable system/macros/i18n) →
  [kag-neo-genesis-language.md](docs/design/kag-neo-genesis-language.md)
- Standard definition → [nextgen-kag-standard.md](docs/design/nextgen-kag-standard.md)
- Runtime contract coverage: **123/123 commands executable as recorded at round 107** (118/118 since round 90;
  round 102 +postfx, round 106 +tween, round 107 +layout 3 commands → 123). The contract total is now **134**; the 11 newer commands are **unverified** for runtime coverage.

---

## 文档索引（Documentation Index / 文档索引, 5 categories / 5 类）

> 文档按用途分 5 类，**共 125 个 markdown 文档**；分类规则见 [AGENTS.md §12](AGENTS.md#12-文档分类)。
> 阶段 G 新增文档以 **NEW 新** 标记。
> Docs are split into 5 categories, **125 markdown documents total**; classification rules in
> [AGENTS.md §12](AGENTS.md#12-文档分类). Stage-G additions are marked **NEW 新**.

### api/ — API 参考（8 篇，自动生成文档为权威 / API reference, 8 docs; auto-generated docs are authoritative)

| 文件 | 内容 |
|------|------|
| [command-contracts.md](docs/api/command-contracts.md) | **134 个 KAG Neo-Genesis 命令**的声明式契约参考（`lua scripts/schema_doc.lua` 生成，权威） |
| [lua-modules.md](docs/api/lua-modules.md) | Lua 模块 API 参考（154 个绑定函数） |
| [cpp-interfaces.md](docs/api/cpp-interfaces.md) | C++ 纯虚接口定义（权威计数见 api-stats.md：34 个接口头 / 412 方法） |
| [editor-api-reference.md](docs/api/editor-api-reference.md) | 编辑器 RPC 端点参考（36 HTTP / 29 stdio） |
| NEW 新 [scene-builder-rpc-bridge.md](docs/api/scene-builder-rpc-bridge.md) | Scene Builder 面板 ↔ 引擎 RPC 桥接手册（round 108） |
| [api-stats.md](docs/api/api-stats.md) | 实时 API 普查（`python scripts/api_stats.py` 生成） |
| [kag-commands.md](docs/api/kag-commands.md) | 已弃用的 KAG3 兼容参考（被 command-contracts.md 取代） |
| [kag-expression-language.md](docs/api/kag-expression-language.md) | `[if]`/`[eval]`/`${}` 表达式语法参考 |

### design/ — 架构与设计文档（14 篇 / Architecture & design docs, 14）

| 文件 | 内容 |
|------|------|
| [engine-architecture-topology.md](docs/design/engine-architecture-topology.md) | 引擎架构拓扑说明（16 模块 + 数据流） |
| [engine-capability-matrix.md](docs/design/engine-capability-matrix.md) | **82 项能力**的完成状态矩阵 |
| [engine-safety-and-qa-mechanisms.md](docs/design/engine-safety-and-qa-mechanisms.md) | JobSystem 线程安全、Lua 沙箱、BackendRegistry 依赖说明 |
| [engine-topology-mermaid.md](docs/design/engine-topology-mermaid.md) | Mermaid 架构拓扑图源码 |
| [backend-registry-dependency-guide.md](docs/design/backend-registry-dependency-guide.md) | BackendRegistry 依赖矩阵与使用规范 |
| [nextgen-kag-standard.md](docs/design/nextgen-kag-standard.md) | KAG Neo-Genesis 标准定义 |
| [kag-neo-genesis-language.md](docs/design/kag-neo-genesis-language.md) | KAG Neo-Genesis 语言白皮书 |
| [engine-performance-baseline.md](docs/design/engine-performance-baseline.md) | 性能基线（round 101–109 刷新：大型资源压测/Web 帧率/bundle vs source） |
| NEW 新 [save-security-audit.md](docs/design/save-security-audit.md) | 存档防篡改审计：AES-GCM 覆盖/nonce 复用/回滚防护（round 104） |
| [engine-market-comparison.md](docs/design/engine-market-comparison.md) | 2026-08-03 市场对比（历史快照） |
| [engine-market-analysis-2026-08-06.md](docs/design/engine-market-analysis-2026-08-06.md) | 2026-08-06 市场分析（数据更新版） |
| [engine-market-analysis-100-rounds.md](docs/design/engine-market-analysis-100-rounds.md) | 100 轮市场分析（阶段 G 依据） |
| [engine-market-research-japanese-vn-engines-2026.md](docs/design/engine-market-research-japanese-vn-engines-2026.md) | 2026 日本 VN 引擎市场调研 |
| [skeletal-mesh-animation.md](docs/design/skeletal-mesh-animation.md) | SMA 骨骼网格动画设计 |

### guides/ — 用户与开发者指南（22 篇 / User & developer guides, 22）

| 文件 | 内容 |
|------|------|
| [getting-started.md](docs/guides/getting-started.md) | 从克隆到 Demo 可跑的入门指南（含 Smoke 清单） |
| [sample-library.md](docs/guides/sample-library.md) | 示例库总览 + 教程路径 01–16 + 覆盖矩阵 |
| [community.md](docs/guides/community.md) | 社区入口与学习路径（Discussions 话题/发布入口） |
| [asset-pipeline.md](docs/guides/asset-pipeline.md) | 支持的资源格式与目录规范 |
| [carc-packaging.md](docs/guides/carc-packaging.md) | CARC 打包格式（Header 64B/FileEntry 116B/Trailer 96B）与工具 |
| [live2d-setup.md](docs/guides/live2d-setup.md) | Cubism SDK 集成步骤 |
| [kag3-import.md](docs/guides/kag3-import.md) | KAG3 工程导入 |
| [kag3-migration.md](docs/guides/kag3-migration.md) | KAG3 完整迁移流水线（六步：xp3/tlg/资产重写） |
| [kag-language-tour.md](docs/guides/kag-language-tour.md) | KAG Neo-Genesis 语言速查 |
| [i18n.md](docs/guides/i18n.md) | i18n 本地化工作流（ks_i18n 提取/回填/门禁） |
| NEW 新 [metal-readiness.md](docs/guides/metal-readiness.md) | Metal 渲染路径就绪度审计（round 103） |
| NEW 新 [android-build.md](docs/guides/android-build.md) | Android（NDK/arm64-v8a）构建链（round 103） |
| NEW 新 [cross-platform-verification.md](docs/guides/cross-platform-verification.md) | 三平台 × 能力域验证矩阵（round 103） |
| NEW 新 [mobile-pipeline.md](docs/guides/mobile-pipeline.md) | 移动端管线（MobileAdapter） |
| NEW 新 [sample-game-assets.md](docs/guides/sample-game-assets.md) | 示例游戏资产审计与 i18n 键预留（round 105） |
| NEW 新 [sample-game-verification.md](docs/guides/sample-game-verification.md) | 示例游戏端到端验证设施（round 105） |
| NEW 新 [sample-game-release.md](docs/guides/sample-game-release.md) | 示例游戏发布清单（GitHub Releases / itch.io） |
| NEW 新 [packaging-ux.md](docs/guides/packaging-ux.md) | 一键打包分发：itch.io / GitHub Pages / Netlify（round 108） |
| NEW 新 [template-quickstart.md](docs/guides/template-quickstart.md) | 项目模板五步指南（round 113） |
| NEW 新 [release-process.md](docs/guides/release-process.md) | 发版流程（版本/发布/分发） |
| NEW 新 [xp3-compat.md](docs/guides/xp3-compat.md) | .xp3 归档格式兼容说明（round 111） |
| NEW 新 [tlg-compat.md](docs/guides/tlg-compat.md) | .tlg5/6 图像格式兼容说明（round 111） |

### plans/ — 执行记录与当前计划（70 篇，按日期命名 / Execution records & current plans, 70, dated by name)

| 文件 | 内容 |
|------|------|
| [ROADMAP-200.md](docs/plans/audit/ROADMAP-200.md) | **阶段 G 路线图**（round 101–，产品化：真机验证/后处理/SMA/示例游戏/打包/模板/发布） |
| [ROADMAP-100.md](docs/plans/audit/ROADMAP-100.md) | 100 轮冲刺轮次记录（round 1–100，权威） |
| [2026-08-16-021-delivery-handoff.md](docs/plans/2026-08-16-021-delivery-handoff.md) | 阶段 F 收官交接（round 100 / round 99 完成态基线） |
| [2026-08-12-004-generation-gap-roadmap.md](docs/plans/2026-08-12-004-generation-gap-roadmap.md) | 代差路线图（五大战役，权威规划） |
| [2026-06-17-001-feat-engine-stability-hardening-plan.md](docs/plans/2026-06-17-001-feat-engine-stability-hardening-plan.md) | 引擎稳定性加固计划 |
| [2026-06-18-galgame-core-readiness-audit.md](docs/plans/2026-06-18-galgame-core-readiness-audit.md) | Galgame 核心就绪度排查方案 |
| [2026-07-02-architecture-hardening-summary.md](docs/plans/2026-07-02-architecture-hardening-summary.md) | 架构硬化执行总结 |
| [2026-07-03-continued-hardening-summary.md](docs/plans/2026-07-03-continued-hardening-summary.md) | 后续架构硬化总结 |
| [2026-07-16-001-modular-static-library-migration-summary.md](docs/plans/2026-07-16-001-modular-static-library-migration-summary.md) | 模块静态库架构迁移总结 |

> 其余 ~60 篇为逐轮交接/计划（`YYYY-MM-DD-NNN-描述.md`），完整历史与每轮门禁记录见 `docs/plans/` 与 `docs/plans/audit/`。
> The remaining ~60 docs are per-round handoffs/plans (`YYYY-MM-DD-NNN-描述.md`); full history and per-round
> gate records live in `docs/plans/` and `docs/plans/audit/`.

### solutions/ — 经验与模式（9 篇，YAML frontmatter 可搜索 / Lessons & patterns, 9, YAML-frontmatter searchable)

| 文件 | 内容 |
|------|------|
| [architecture-patterns/engine-constructor-sigsegv-testing.md](docs/solutions/architecture-patterns/engine-constructor-sigsegv-testing.md) | Engine 构造崩溃的 NullGpuMonitor 解决模式 |
| [architecture-patterns/header-only-to-instance-class.md](docs/solutions/architecture-patterns/header-only-to-instance-class.md) | 头文件内联类重构为实例类模式 |
| [architecture-patterns/galgame-engine-readiness-audit.md](docs/solutions/architecture-patterns/galgame-engine-readiness-audit.md) | Galgame 引擎就绪度审计 |
| [architecture-patterns/gpu-api-guard-before-bgfx-init.md](docs/solutions/architecture-patterns/gpu-api-guard-before-bgfx-init.md) | bgfx init 前 GPU API 守卫 |
| [build-errors/clean-build-include-path.md](docs/solutions/build-errors/clean-build-include-path.md) | 全量构建 include 路径修复模式 |
| [build-errors/bgfx-shader-binary-repack.md](docs/solutions/build-errors/bgfx-shader-binary-repack.md) | bgfx 着色器二进制重打包 |
| [runtime-crashes/bgfx-predefined-uniform-name-conflict.md](docs/solutions/runtime-crashes/bgfx-predefined-uniform-name-conflict.md) | bgfx 预定义 uniform 命名冲突 |
| [kag-language/kag-neogenesis-modernization.md](docs/solutions/kag-language/kag-neogenesis-modernization.md) | KAG Neo-Genesis 现代化历程 |
| [deferred-gpu-tests.md](docs/solutions/deferred-gpu-tests.md) | 无 GPU 环境下无法覆盖的测试项清单 |

---

## 示例库清单（Sample Library / 示例库）

> 完整覆盖矩阵与教程逐行讲解见 [docs/guides/sample-library.md](docs/guides/sample-library.md)。
> Full coverage matrix and line-by-line tutorial walkthroughs in [docs/guides/sample-library.md](docs/guides/sample-library.md).

| 路径 | 主题 | 验证状态 |
|------|------|----------|
| [demo/example_game/](demo/example_game/) | 《单程回信》完整示例游戏（15–18 分钟，三结局 + SMA + i18n） | PASS headless E2E 5/5（DONE + 三结局可达），ks_check 零警告 |
| [demo/template/](demo/template/) | 新游戏项目模板（最小两场景 + [select] + entry 多路径） | PASS verify_template 4/4，package_game 打包成功 |
| [demo/tutorial/](demo/tutorial/) | 教程路径 **01–16**：从最小剧本到声明式补间 [tween] | PASS 引擎 tokenize/compile + Web 播放器双验证，ks_bake 通过 |
| [demo/showcase.ks](demo/showcase.ks) | Command Showcase：26 个命令全展示 | PASS 引擎 tokenize/compile + Web 播放器（DONE:53 + ending 解锁） |
| [demo/galgame_demo.ks](demo/galgame_demo.ks) | 核心 VN 流程演示（bg/ch/playbgm/voice/sprite/ending） | PASS Web flow 集成测试（round 109 部署默认 game） |
| [demo/full_pipeline_demo.ks](demo/full_pipeline_demo.ks) | 全管线流程（资产+脚本+播放） | PASS ks_bake bundle |
| [demo/sma_demo.ks](demo/sma_demo.ks) | SMA 骨骼动画命令演示 | PASS ks_bake bundle |

---

## Platform Support / 平台支持

| 平台 | 渲染 | 构建 | CI | 备注 |
|----------|----------|:----:|:---:|------|
| Windows (MSVC) | D3D11 | Yes | Yes | 主开发平台；D3D11 + OpenGL 4.3 真机像素验证 |
| Linux (GCC) | OpenGL | Yes | Yes | SDL3 源码构建 |
| macOS (Clang) | Metal | Yes | Yes | Metal 引擎侧完整，运行时验证需 macOS 硬件 |
| Web | Canvas (wasmoon) | — | Yes | 纯前端播放器，`web/` + ks_bake bundle |

CI 工作流：`.github/workflows/ci.yml`（Windows MSVC Debug+Release、macOS Clang、Linux GCC；Linux 跑耦合计数；Release 经 CPack 打包 ZIP）。
CI workflow: `.github/workflows/ci.yml` (Windows MSVC Debug+Release, macOS Clang, Linux GCC; Linux runs the
coupling count; Release is packaged as ZIP via CPack).

## Tech Stack / 技术栈

| 层 | 技术 |
|-----|------|
| 语言 Language | C++20 |
| 构建 Build | CMake 3.25+ |
| 渲染 Rendering | bgfx（D3D11 / OpenGL / Metal） |
| 窗口 Windowing | SDL 3.4 |
| 音频 Audio | SoLoud（BGM / Voice / SE 总线） |
| 脚本 Scripting | Lua 5.4 + 沙箱 + 指令预算（web: wasmoon） |
| 文本 Text | FreeType + CJK（NotoSansCJKsc） |
| 加密 Crypto | BCrypt/OpenSSL；AES-256-GCM + Ed25519 |
| 网络 Networking | cpp-httplib、nlohmann/json |
| 视频 Video | pl_mpeg (MPEG-1) + FFmpeg（可选） |
| Live2D | Cubism 5 SDK（可选，thirdparty/） |
| 归档 Archive | CARC（压缩 + 加密 + 签名） |
| 测试 Testing | doctest + CTest + Lua + vitest |
| 编辑器 Editor | HTTP :9876 / stdio JSON-RPC |

---

## 社区与支持（Community & Support / 社区与支持）

主阵地是 **GitHub Discussions**（对话）与 **Issues**（Bug / 功能追踪）。完整入口、话题分类与
学习路径见 [docs/guides/community.md](docs/guides/community.md)。
The main homes are **GitHub Discussions** (conversations) and **Issues** (bug/feature tracking). Full
entry points, topic categories and learning paths are in [docs/guides/community.md](docs/guides/community.md).

- **提问 / 求助（Ask / Help）** — Discussions「提问」（先看 [getting-started](docs/guides/getting-started.md) 与已有讨论）
- **作品展示（Showcase）** — Discussions「作品展示」：用引擎做的游戏 / 场景 / 立绘
- **引擎开发（Engine development）** — Discussions「引擎开发」：接口 / 渲染 / 脚本 / 构建
- **内容创作（Content creation）** — Discussions「内容创作」：剧本写作 / 美术音频 / 经验
- **发布与打包（Release & packaging）** — Discussions「发布与打包」：package_game.sh / itch / GitHub Pages
- **Bug / 功能请求（Bug / feature requests）** — Issues（[bug_report](.github/ISSUE_TEMPLATE/bug_report.md) / [feature_request](.github/ISSUE_TEMPLATE/feature_request.md) 模板）
- **发布作品（Publish your game）** — `bash scripts/package_game.sh` 一键打包为静态站 → itch.io / GitHub Releases / GitHub Pages

> **想参与贡献？** 从 [CONTRIBUTING.md](CONTRIBUTING.md) 开始：Fork → 分支 → 语义提交 → PR
> （合并门禁：全量构建零错误 + C++ / Lua / Web / Editor 四套件测试全绿 + 耦合门禁 + 索引守卫）。
> 贡献方式不仅限于代码——优质教程、示例游戏、美术/音频素材同样是贡献。
> **Want to contribute?** Start with [CONTRIBUTING.md](CONTRIBUTING.md): Fork → branch → semantic commits →
> PR (merge gates: zero-error full build + all four test suites green (C++ / Lua / Web / Editor) + coupling
> gate + index guard). Contributions are not limited to code — tutorials, example games and art/audio assets count too.

---

## 项目状态与路线图（Project Status & Roadmap / 项目状态与路线图）

### 里程碑（Milestones）

- **round 1–100（100 轮冲刺）**：引擎核心从零到功能完整（权威轮次记录：[ROADMAP-100.md](docs/plans/audit/ROADMAP-100.md)）
- **round 101 起「阶段 G：产品化」**：真机验证 / 后处理特效栈 / SMA 骨骼动画 / 声明式布局与补间 / 示例游戏《单程回信》/ 一键打包分发 / 项目模板 / KAG3 存量资产（xp3/tlg）/ 社区文档 / v1.0.0 发布准备（[ROADMAP-200.md](docs/plans/audit/ROADMAP-200.md)）
- **v1.0.0 发布准备（round 113）**：CMake 版本对齐 1.0.0、CHANGELOG v1.0.0 段、发布清单。
- **round 1–100 (100-round sprint)**: engine core from zero to feature-complete (authoritative round log:
  [ROADMAP-100.md](docs/plans/audit/ROADMAP-100.md))
- **round 101+ "Stage G: Productization"**: real-hardware validation / post-processing FX stack / SMA
  skeletal animation / declarative layout & tween / example game "The One-Way Reply" / one-click packaging /
  project template / legacy KAG3 assets (xp3/tlg) / community docs / v1.0.0 release prep
  ([ROADMAP-200.md](docs/plans/audit/ROADMAP-200.md))
- **v1.0.0 release prep (round 113)**: CMake version aligned to 1.0.0, CHANGELOG v1.0.0 section, release checklist.

### 当前基线（阶段 G 门禁实测 / Current baseline — stage-G gate measurements)

| 指标 | 数值 / Value |
|------|------|
| C++ 接口 | **34** 纯虚接口 / **412** 纯虚方法 |
| KAG 命令契约 | **134**（多类别，自动生成权威） |
| Lua 绑定函数 | **159**（11 个绑定文件） |
| RPC 表面 | **36** HTTP 端点 + **29** stdio JSON-RPC 方法 |
| Lua 运行时脚本 | **78**（scripts/，不含 demo/check） |
| 能力矩阵 | **82** 项 / 6 域 |
| 测试 · C++ | **1,120** 用例（385,790 断言，doctest 全绿） |
| 测试 · Lua | **143** 主套件 + **24** 孤儿套件（全绿） |
| 测试 · Web | **368** 用例（27 文件全跑全过；story bundle 与 web/dist 齐备，2026-08-28 实测） |
| 测试 · Editor | **615** 用例（vitest 全绿） |
| 耦合门禁 | PASS（entry/di/script ≤14，其余 ≤4） |
| CI | 三平台绿（Windows/macOS/Linux，Release + CPack ZIP + Android APK/AAB + iOS probe） |
| 示例库 | tutorial 01–16 + showcase + galgame/full_pipeline/sma + example_game + template + first_vn |
| 移动端真机验证 | 小米 11 (alioth / Snapdragon 888 / Adreno 660 / Android 14) 全链路 100% 闭环 |

> 以上数字为最近一次全量门禁实测（Sprint 4，2026-08-28）；实时口径见
> [api-stats.md](docs/api/api-stats.md)（API 表面）与 ROADMAP-200 各轮门禁列。若发现不一致，
> 请运行 `python scripts/api_stats.py` 重新生成 API 普查。
> The numbers above are from the latest full gate run (stage G rounds 108–113); the live view is
> [api-stats.md](docs/api/api-stats.md) (API surface) and the per-round gate columns in ROADMAP-200. If you
> spot any inconsistency, re-run `python scripts/api_stats.py` to regenerate the API census.

---

## License / 许可证

Caesura (AmeKAG) — Copyright (c) 2025-2026 AiliasDesu. [MIT License](LICENSE).

第三方库保留其原始版权：bgfx (BSD-2)、SDL3 (zlib)、SoLoud (zlib)、Lua 5.4 (MIT)、FreeType (FTL)、zstd (BSD)、nlohmann/json (MIT)、ed25519 (CC0)、stb (MIT/PD)、pl_mpeg (MIT)、cpp-httplib (MIT)、doctest (MIT)。
Third-party libraries retain their original licenses: bgfx (BSD-2), SDL3 (zlib), SoLoud (zlib), Lua 5.4 (MIT),
FreeType (FTL), zstd (BSD), nlohmann/json (MIT), ed25519 (CC0), stb (MIT/PD), pl_mpeg (MIT), cpp-httplib (MIT), doctest (MIT).

**Live2D Cubism SDK 为 Live2D Inc. 的专有软件**——需单独下载，不随仓库分发。
**The Live2D Cubism SDK is proprietary software of Live2D Inc.** — download it separately; it is not distributed with the repository.
