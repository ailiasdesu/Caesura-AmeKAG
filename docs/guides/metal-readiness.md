# Metal 后端就绪度审计（Metal Backend Readiness Audit）

> 生成轮次：round 102 之后 · 渲染工程师审计
> 范围：Metal 后端初始化路径、round 102 后处理链降级、D3D 硬编码假设、
> CI macOS job 覆盖度、真机验证清单。**未改任何 src/render 实现，未 git 提交。**
> 结论性质：静态审计（读代码+构建配置+CI 配置）。真实 GPU 行为需 macOS 真机验证。

---

## 0. 结论速览

| # | 审计点 | 结论 | 风险 |
|---|--------|------|------|
| a | Metal 初始化路径完整 | OK 完整（--backend metal 可选 + 自动回退到 Metal） | 默认未显式选 Metal，靠 D3D11 失败到 auto-select |
| b | PostFx 4 program 在 Metal 恒等降级 | OK 明确且不崩（identity copy + fallbackProgram 双保险） | 仅视觉降级，功能正常 |
| c | D3D 硬编码假设 | WARN 无致命 D3D 假设，但默认后端、shader 再生成有隐患 | 见 4 |
| d | CI macOS job 验证覆盖 | WARN 仅编译级（Metal cpp 会被编译），无 Metal 运行级验证 | 见 5 |

**总体判定：Metal 后端在代码/编译层面完全就绪，缺一块实机验证拼图。** 恒等降级路径设计良好、不会崩溃；唯一实质缺口是「从未在 macOS 真机跑过」这一事实本身。

---

## 1. 相关文件清单

| 文件 | 角色 |
|------|------|
| src/render/EmbeddedShaders_Metal.cpp | 内嵌 Metal (MSL) 字节码的 10 个 VS/FS 数组+size |
| src/render/EmbeddedShaders.h | 10 组 Metal 符号的 extern 声明 |
| src/render/EmbeddedShaders_GL.cpp | 对照：GL 内嵌字节码（亦无 postfx） |
| src/render/BgfxDeviceCore.cpp/.h | bgfx::init、后端选择、RTT/视图管理 |
| src/render/BgfxShaderManager.cpp/.h | program 创建、按后端选字节码、postfx 降级 |
| src/render/BgfxRenderDevice.cpp/.h | 渲染设备、runPostFxChain、PostFx 生命周期 |
| src/platform/SDL3PlatformBackend.cpp | 提供 native window handle（nwh） |
| cmake/CaesuraModules.cmake | render 模块源文件清单（含 Metal cpp） |
| shaders/embed_to_c.py | 从 shaderc 产物再生成 GL/Metal 内嵌数组 |
| shaders/compiled/macos/*.metal.bin | 预编译 Metal 字节码（10 个，无 postfx） |
| .github/workflows/ci.yml | CI（build-macos job 等） |
| src/main.cpp | --frames / --backend CLI 参数解析 |

---

## 2. 审计清单逐项结论

### a. Metal 后端初始化路径 — OK 完整

**后端选择：**
- BgfxDeviceCore::setPreferredBackend()（BgfxDeviceCore.cpp:16）识别 metal / Metal -> bgfx::RendererType::Metal。
- CLI --backend metal（main.cpp:939）-> EngineConfig.renderBackend -> Engine.cpp:282 调用 setPreferredBackend。
- 未指定时默认 s_preferredBackend = Direct3D11（BgfxDeviceCore.cpp:12 静态默认）。

**bgfx::init：**
- BgfxDeviceCore::init（:58-85）填充 bgfx::Init{ .type=backend, .nwh=nativeWindowHandle, .resolution=wxh }。
- macOS 上默认路径：Direct3D11 初始化必然失败 -> RendererType::Count（auto-select）-> bgfx 在 Apple 平台 auto-select 选 Metal。链路成立但「靠失败回退」不够显式。

**Metal 平台数据关键点（已验证 bgfx 源码 renderer_mtl.cpp:4005-4030）：**
- bgfx Metal 后端接受 NSWindow / NSView / CAMetalLayer。传 NSWindow 会走 contentView.layer 取出 CAMetalLayer。
- 引擎 SDL3PlatformBackend::getNativeWindowHandle()（:86）在 macOS 返回 SDL_PROP_WINDOW_COCOA_WINDOW_POINTER（即 NSWindow），正好是 bgfx Metal 接受的形态。nwh 接线与 Metal 兼容。

**shader 字节码加载：**
- BgfxShaderManager::initEmbeddedShaders（:121）按 bgfx::getCaps()->rendererType 选字节码；isMetal 分支（:197-207）取 kEmbeddedMetal_*。
- EmbeddedShaders_Metal.cpp 已在 cmake/CaesuraModules.cmake:214 编入 CaesuraRender，编译级保证存在。

### b. round 102 的 4 个 PostFx program 在 Metal 的行为 — OK 明确且不崩

**情况：** 4 个 postfx PS（vignette / LUT grade / soft blur / bloom）**只编译了 DXBC**（shaders/dx11/*.dxbc），Metal 无对应 .metal.bin，故 EmbeddedShaders_Metal.cpp 不含 kEmbeddedMetal_fs_postfx_* 符号（GL 亦无，天然一致）。

**降级路径（三重，层层兜底）：**
1. BgfxShaderManager.cpp:156 -> isMetal 分支不填 fsPostfx*，保持 size=0。
2. BgfxShaderManager.cpp:218-221：if (fsPostfxX.size == 0) fsPostfxX = fsTexture; -> postfx PS 变恒等 texel 复制（identity copy）。
3. BgfxRenderDevice.cpp:616-617：if (!isValid(prog)) prog = fallbackProgram; — 即使某个 program 构建失败，也用 fallback 兜底，绝不空 program 崩溃。

**结论：** Metal 上后处理链照常走（RTT 到 backbuffer 合成），只是 4 个效果退化为「无视觉效果的复制」——符合 round 102 设计注释（BgfxShaderManager.cpp:153-155 写明 graceful degradation）。功能链路（createPostFx -> runPostFxChain -> setViewFrameBuffer -> submitFullscreenQuad）完全后端无关，Metal 上不崩溃。

### c. 未实现/未接线/硬编码 D3D 假设 — WARN 少量非致命

| 项 | 结论 |
|----|------|
| view clear | OK 后端无关（setupDefaultViews 用标准 bgfx API，:201-232） |
| texture format | OK RGBA8 统一，无 D3D 专属格式假设 |
| RTT 路径 | OK createRenderTarget/getScratchRt 用标准 createTexture2D(BGFX_TEXTURE_RT)+createFrameBuffer，Metal 兼容 |
| 默认后端 | WARN s_preferredBackend 静态默认 = Direct3D11。macOS 依赖「D3D11 init 失败 -> auto-select -> Metal」。建议 macOS 显式走 Metal（见 4.1） |
| shader 再生成 | WARN embed_to_c.py 的 FILES 列表已含 4 个 postfx 名，但当前提交的 Metal/GL cpp 与 compiled/{macos,linux} 均无 postfx 产物——文件是 postfx 引入前生成的，与脚本 FILES 列表不同步（见 4.2） |

没有发现会阻止 Metal 起跑的实现缺陷。

### d. CI macOS job 当前验证什么、缺什么 — WARN 仅编译级

build-macos（ci.yml:155-188）：
- 编译：cmake --build 全量构建含 EmbeddedShaders_Metal.cpp（已编译则 Metal 字节码文件可编译）。
- Lua 套件 / web gen-index / ctest（无 GPU 的单元测试）。
- 缺：无 Metal 运行级验证——不跑 engine 可执行文件、无 --frames 冒烟、无 --backend metal 启动、无 macOS 真机/虚拟化 GPU 渲染。
- ctest 用 NullRenderDevice/默认构造，不触达任何 GPU 路径。

**minigame 对照**：cmake/CaesuraModules.cmake:187 也编入 EmbeddedShaders_MiniGame_Metal.cpp，同样只有编译级覆盖。

---

## 3. 真机验证清单（步骤化，macOS 上执行）

> 前置：macOS 真机或有 Metal 的 VM（Apple Silicon / Intel 均可）；已按 docs/guides/getting-started.md 构建。

### 3.1 构建（含 Metal 字节码）
    cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -DCAESURA_ENABLE_FFMPEG=OFF
    cmake --build build --config Debug --parallel
预期：构建零错误；build/Debug/CaesuraAmeKAG 生成（编译级 Metal 就绪已证，此步应为绿色）。

### 3.2 显式 Metal 后端冒烟（--frames）
    ./build/Debug/CaesuraAmeKAG --backend metal --frames 60
验证点（看 stdout/日志）：
- 启动无 Fatal: bgfx::init failed；
- [BgfxRenderDevice] Renderer: Metal（确认走 Metal，而非 auto-select 意外落到 GL）；
- [BgfxShaderManager] initEmbeddedShaders: renderer=Metal；
- Fallback program READY / Blend READY / Transition READY / VFX READY / StretchBlt READY / AffineBlt READY；
- 跑满 60 帧无崩溃、无 0xC0000005。

### 3.3 postfx 链手测（恒等降级）
在 Lua/KAG 侧启用任意 postfx（如 vignette）后启动，例如：
    ./build/Debug/CaesuraAmeKAG --backend metal --frames 120
脚本内 backend.create_postfx('vignette', ...) 触发 runPostFxChain：
- 帧不崩、不黑屏；PostFxVignette program 不打印 READY（因无字节码降级为 fsTexture），但合成仍走、场景照常显示；
- 确认视觉上「无效果」但图片正常（恒等复制生效），无黑屏/白屏。

### 3.4 关键手动回归（Metal 风险区）
1. 窗口缩放/全屏：触发 resizeWindow -> BgfxDeviceCore::resize，确认 CAMetalLayer drawableSize 跟随（bgfx 处理，验证无花屏）。
2. RTT（draw_viewport / render_to_target）：用任意生成 RTT 的脚本，确认 createRenderTarget -> getViewportTexture 往返正常。
3. 着色器 uniform：BlendTransition / VFXParams / StretchParams / AffineParams 各发一帧，确认 uniform 绑定正常（Metal 的 buffer 布局与 D3D 不同，是最高风险点）。
4. Debug text HUD：确认 BGFX_DEBUG_TEXT 叠加正常。

### 3.5 已知限制
- 未显式默认 Metal：默认启动不走 Metal（默认 D3D11 到失败到 auto-select）。审计不推荐改默认（避免破坏 Windows 主平台），建议用 --backend metal 显式跑 macOS。
- postfx 在 Metal 无实机效果（恒等降级）：要上真实 vignette/LUT/blur/bloom 需补 shaders/metal/fs_postfx_*.metal.bin 并重跑 embed_to_c.py、接上 BgfxShaderManager 的 Metal 分支——超出本次审计（不动实现）。
- 未在真机验证 uniform 布局：MSL 的 constant buffer 对齐与 DXBC 不同，3.4-3 是最可能暴露问题的手测项。

---

## 4. 低风险附件（建议，未改动文件）

### 4.1 macOS 显式 Metal（可选）
macOS 下想让默认即 Metal，可考虑在 Engine::init 或 CMake 里、仅当 Apple 平台且未显式指定 backend 时默认 metal。未改（涉及共享耦合点，需主代理定夺）。

### 4.2 shader 再生成脚本同步（建议，属文档/脚本侧）
shaders/embed_to_c.py:17-20 FILES 含 postfx，但 compiled/{macos,linux} 与 EmbeddedShaders_{Metal,GL}.cpp 均无 postfx 产物。若有人重跑脚本且无 postfx bin，open() 会直接抛错（非静默出坏文件）——风险有限但仍建议：要么补 postfx 的 metal/gl bin，要么从 FILES 移除 postfx，保持脚本与产物一致。

### 4.3 CI 编译级 Metal 强化（低风险，见 5）

---

## 5. CI macOS job 建议

**当前**：build-macos 仅编译（EmbeddedShaders_Metal.cpp 已被编译，Metal 字节码文件可编译性已隐性覆盖）。

**可加的编译级 Metal 强化步骤（低风险，不改 src/render）**：

1. 断言 Metal 字节码被编译（编译级 guard）：在 cmake/CaesuraModules.cmake 或用 nm 检查 EmbeddedShaders_Metal.cpp 目标文件包含 kEmbeddedMetal_vs_sprite 符号已被链接。最简：CI 步 grep 编译日志确认 EmbeddedShaders_Metal.cpp 被编译（当前无显式断言，靠编译产物是否成功隐含）。

2. 检查 embed_to_c 产物与脚本同步（防 stale）：CI 步校验 src/render/EmbeddedShaders_Metal.cpp 是否由当前 shaders/embed_to_c.py + compiled/macos 再生成零 diff（类比现有 api_stats.py / gen-index --check 的 docs-freshness 守卫）。这能把「Metal 字节码与源 shader 脱节」变成 CI 失败。

3. （更强的）Metal 运行级冒烟：在 build-macos 追加 ./build/Debug/CaesuraAmeKAG --backend metal --frames 30（需 macOS runner 有可用 GPU/虚拟化渲染；GitHub macos-latest 通常可用软件/虚拟 Metal）。风险高于编译级，建议先本地真机验证 3.2-3.4 通过后再加。

**注意**：改 .github/workflows/ci.yml 属共享文件，需主代理验收后落。本审计未改动。

---

## 6. 附：本审计未改动的文件
- src/render/*（全部）——未发现需改的实现 bug；postfx 补 Metal 字节码属功能增强，不在审计范围。
- .github/workflows/ci.yml —— CI 建议在 5，落地需主代理验收。
- shaders/embed_to_c.py —— 建议在 4.2。

> 引用行号以本次审计读取时的 HEAD 为准（git dce73ee3 round 102 提交之后）。
