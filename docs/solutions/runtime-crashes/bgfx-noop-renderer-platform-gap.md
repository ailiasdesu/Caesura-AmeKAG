---
module: render
tags: [bgfx, noop, renderer, platform-port, cmake]
problem_type: runtime-crash
---

# bgfx 零渲染器 Noop 陷阱（平台移植双实证）

> 本文档基于 **t92 批 + t94 应修（均尚未推送）** 的工作树现状编写：external/bgfx/bgfx/CMakeLists.txt 的平台分支、src/render/BgfxDeviceCore.{h,cpp} 的 per-platform 默认后端、失配诊断行与 requestedBackendName 映射均在本地未提交；引用行号以当时工作树为准，推送后可能漂移。

## 场景

向新平台移植引擎（或打包某个平台产物）时，游戏**黑屏无输出**，但进程 exit=0、日志"干净"关闭——甚至 CI/verify 全绿。两次实证：

1. **Android（Track M）**：未加渲染器 define 时 bgfx 在 Android 上编译 **零后端**，OpenGL init 失败后渲染器回落到 Noop（external/bgfx/bgfx/CMakeLists.txt:33-38 注释原文）。
2. **Linux（round-4，2026-08-29）**：发布包游戏在 Linux 上 `requested=Direct3D 11` → `actual=Noop`；历史 exit-code 绿 = 从未真正渲染的**假绿**（external/bgfx/bgfx/CMakeLists.txt:39-47 注释 + src/render/BgfxDeviceCore.cpp:144-147 注释）。

## 症状

- 游戏黑屏、无任何渲染输出，但进程正常退出（exit=0 干净关机）。
- verify_release_package.sh §5 报 `renderdisabled=1`：`grep -c 'rendering disabled (BGFX_DEBUG_IFH)'` 命中（scripts/verify_release_package.sh:628），但其判定行（:629-631）要求 `RENDERDISABLED=0` 才 ok——降级标记把"假绿"变成"该红"。
- 引擎启动日志出现**两行失配**：
  - 请求行：`[BgfxRenderDevice] nwh=... backend=%s`（src/render/BgfxDeviceCore.cpp:122，打印的是**首选**后端名；自动回退时改为 :129 的 `backend=auto-select`）。
  - 实际行：`[BgfxRenderDevice] Renderer: %s (%s)`（src/render/BgfxDeviceCore.cpp:141-142，打印 bgfx caps 的**实际** rendererType，bgfx 自身命名）。
- t94 之后：比两行更快定位的失配行 `[RENDER] [ERROR] requested=%s actual=%s`（src/render/BgfxDeviceCore.cpp:148-151）——只有当 `caps->rendererType != s_preferredBackend` 时打印，Release 也在内；requested 侧经引擎映射表（:149 `requestedBackendName`），actual 侧保持 bgfx 名字。

## 机制

1. **vendored bgfx 由 CMake 决定编译哪些渲染器**：external/bgfx/bgfx/CMakeLists.txt:24-27 先给基础 defines，:29-48 按平台分支追加渲染器 define：
   - `if(WIN32)` → `BGFX_CONFIG_RENDERER_DIRECT3D11=1 BGFX_CONFIG_RENDERER_DIRECT3D12=0 BGFX_CONFIG_RENDERER_OPENGL=43`（:30）
   - `elseif(APPLE)` → `BGFX_CONFIG_RENDERER_METAL=1`（:32）
   - `elseif(ANDROID)` → `BGFX_CONFIG_RENDERER_OPENGLES=30`（:38，Track M 修复）
   - `elseif(UNIX)` → `BGFX_CONFIG_RENDERER_OPENGL=43`（:47，round-4 修复；**置于 ANDROID 之后**，因为 CMake 在 ANDROID 下同时设 UNIX=1——:45-46 注释）
   - 公共尾行 `BGFX_CONFIG_RENDERER_VULKAN=0`（:50）。
2. **无 define = 零渲染器**：某个平台没匹配到分支（或分支遗漏）时，bgfx 编译时所有渲染器 define 均为 0/未定义——库内**一个真实渲染器都没有**。
3. **bgfx auto-select 分数制**：`bgfx::init` 在 initParams.type=Count（自动选择）时对每个**已编译**的渲染器打分；零渲染器时全部 0 分 → **Noop 胜出**（Noop 总是"可用"）。bgfx 会在 caps->rendererType 里报告 Noop——引擎"启动成功"，实际什么都没渲染。
4. **引擎默认后端曾硬编码 D3D11**：t92 之前 BgfxDeviceCore 的默认是固定 `Direct3D11`（即使编译出的 bgfx 只有 GL/Metal 也请求 D3D11）→ 请求失败走 :123-136 的 auto-select 回退 → Noop。t92 改为 per-platform `platformDefaultBackend()`（src/render/BgfxDeviceCore.cpp:65-78：`_WIN32→Direct3D11`、`__APPLE__→Metal`、else→`OpenGL`；声明与注释在 BgfxDeviceCore.h:32-36）。
5. **t94 应修（名字映射）**：bgfx 会把**未编译**的渲染器槽位 stub 成 Noop 占位——`bgfx::getRendererName(RendererType::Metal)` 在非 Apple 构建上返回 "Noop"（:46-50 注释）。因此 requested 侧打印必须经引擎自带映射表 `requestedBackendName()`（:51-63），否则失配行整体失真（"requested=Noop" 而非 "requested=Direct3D 11"）；actual 侧保持 `bgfx::getRendererName(caps->rendererType)`——活渲染器总会自报正确名字。

## 诊断配方（顺序固定）

1. **先核两行，再谈着色器**：抓引擎启动日志，比对 `[BgfxRenderDevice] nwh=... backend=...`（:122，首选）与 `[BgfxRenderDevice] Renderer: ...`（:141-142，实际）。两者不一致或 `Renderer: Noop` → 直接命中本陷阱；一致但花屏/黑屏才进入着色器/资源链路排查。
2. **t92/t94 之后多一行哨兵**：`[RENDER] [ERROR] requested=X actual=Y`（:148-151）。注意 grep 用单 token 带空格形（`'[RENDER] [ERROR]'`），连写 `'[RENDER][ERROR]'` 必漏（日志分段带空格——round34 教训）。
3. **verify §5 renderdisabled 断言**：scripts/verify_release_package.sh:628-631——`grep -c 'rendering disabled (BGFX_DEBUG_IFH)'` 必须为 0；失败块（:632-641）已带 t81 forensics（打印 $RL 内 `[RENDER]` 行 + backend/renderer 识别行）供 CI 抓因。
4. **确认编译面**：若怀疑 define 缺失，直接看 `external/bgfx/bgfx/CMakeLists.txt` 的平台分支是否命中（ANDROID 先于 UNIX！）。

## 修复模式（t92 落地的三件套）

1. **CMake 平台分支加 renderer define**（external/bgfx/bgfx/CMakeLists.txt:29-48）：每个新平台在 if/elseif 链里追加自己的 `target_compile_definitions(bgfx PRIVATE BGFX_CONFIG_RENDERER_XXX=N)`；平台链顺序**不可颠倒**（UNIX=1 会遮住 ANDROID/APPLE 之外的平台）。
2. **引擎侧匹配默认后端**：`BgfxDeviceCore::platformDefaultBackend()`（BgfxDeviceCore.cpp:65-78）为同一平台返回与 define 一致的 RendererType；`--backend` 显式覆盖（`setPreferredBackend`，:22-44，支持 vulkan/dx12/dx11/metal/webgpu/opengl/gles）**不受影响**——它是运行时首选，CMake define 只是编译面。
3. **失配诊断行**：init 完成后比对 `caps->rendererType != s_preferredBackend` 打 `[RENDER] [ERROR] requested=... actual=...`（:148-151），把"静默降级"变成第一类诊断（Release 也打）；requested 侧配 `requestedBackendName`（:51-63）防 bgfx stub 名字失真（t94）。

## 新平台移植检查清单

移植新平台时三件套缺一不可：

- [ ] **renderer define**：external/bgfx/bgfx/CMakeLists.txt 平台分支命中并编译目标渲染器（查 if/elseif 链与 CMake 平台变量重叠性——ANDROID 同时置 UNIX=1，simulator 平台可能同时置 APPLE 等）。
- [ ] **默认后端 case**：`BgfxDeviceCore::platformDefaultBackend()` 返回与该 define 一致的枚举；若该平台默认后端 init 可能失败，auto-select 回退与 Noop 的区分靠失配诊断行。
- [ ] **CI 探针红绿**：新平台的 compile/run 探针必须断言**实际渲染器**（日志内 `Renderer:` 行为目标后端 + verify `renderdisabled=0`），仅 exit-code 绿不算绿（round-4 假绿教训：exit 0 + renderdisabled=1 的"干净"才是陷阱本体）。

## 判据行原文（工作树快照）

external/bgfx/bgfx/CMakeLists.txt:34-37（Track M 注释节选）：

    # Track M: without a renderer define bgfx defaults to NO backend on
    # Android (OpenGL init fails, renderer falls back to Noop). GLES 3.0
    # matches the device OpenGLES driver; context comes from SDL (see
    # SDL3PlatformBackend::createGLContext).

external/bgfx/bgfx/CMakeLists.txt:40-46（round-4 注释节选）：

    # Sprint6-L1 (t92/round-4): same trap as the Android note above -- with no
    # renderer define bgfx compiles ZERO backends on desktop Linux, auto-select
    # scores everything 0 and Noop wins, so the packaged game never really
    # rendered (round-4 evidence: requested=Direct3D 11, actual=Noop). ...
    # Placed after ANDROID because CMake sets UNIX=1 there too.

src/render/BgfxDeviceCore.cpp:46-50（t94 名字映射注释节选）：

    // t94 must-fix: bgfx stubs renderer slots that are NOT compiled in as noop
    // placeholders, so bgfx::getRendererName(RendererType::Metal) returns "Noop"
    // on a non-Apple build. REQUESTED-side prints therefore map the enum through
    // the engine's own table; the ACTUAL side keeps bgfx's name (a live renderer
    // always names itself correctly).

src/render/BgfxDeviceCore.cpp:144-147（t92 失配行注释）：

    // t92: requested/actual mismatch must be loud on every build (Release
    // included). Noop landing here is the classic "fake green" (round4 Linux:
    // requested D3D11, actual Noop, renderdisabled=1) -- this line turns the
    // silent degradation into a first-class diagnostic.
