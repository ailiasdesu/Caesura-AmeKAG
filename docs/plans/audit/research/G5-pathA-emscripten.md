# G5-Path A：Web 导出 —— emscripten/WASM 编译原生引擎 可行性调研

> 调研员：DSH 子 agent 调研会话
> 日期：2026-08（以 2026 视角检索，资料截至 Emscripten 6.x / SDL3 现网）
> 引擎：Caesura (AmeKAG) — C++20 + bgfx + SDL3 + SoLoud + Lua 5.4，CMake，目标 Windows/macOS/Linux
> 结论：条件性推荐（作为"技术可行性验证 / 长期支线"，不推荐作为近期主力或唯一 Web 路径）

---

## 0. 结论摘要（TL;DR）

| 维度 | 判定 |
|---|---|
| **总体结论** | 条件性推荐：技术上可实现 Demo 级 WASM 导出，但受"浏览器单线程回写受限 + 引擎多线程架构 + 大包体/加密归档 + Live2D/视频缺失"四大硬约束，不推荐作为近期（≤8 周）主打路径 |
| 预估工作量（首个可玩 Demo） | 6–10 周（单人，含 CMake 工具链接入、主循环改造、无头后端替换、资源预加载、存档适配、HTML 壳） |
| 预估工作量（生产级导出） | 12–18 周（补齐线程代理、CARC 流式、视频回退、Live2D 决策、性能调优、多浏览器 QA） |
| **最大风险** | 1) bgfx 渲染线程在浏览器上无法自由回写 / 主线程独占渲染（结构性）；2) CARC 加密归档 + 数 GB 资源无法整体预加载（部署性）；3) 多线程要求 COOP/COEP 头 + SharedArrayBuffer，部署与托管受限（环境性） |
| 类比参照 | Ren'Py Web 是"官方踩坑史"：多线程不可用、Live2D 不支持、视频走浏览器内建、存档需适配、大文件 50MB 缓存上限 |

> 特别注意：本项目是独特的不利组合——Ren'Py 之所以能勉强 WASM，是因为它引擎单线程且用 Python + Emterpreter；而 Caesura 是多线程 C++ 引擎（JobSystem + 渲染线程），这两点在浏览器上恰好是最难移植的。可行性结论必须基于此差异。

---

## 1. bgfx 的 WASM 支持现状

**结论：官方一级支持，WebGL1/2 + WebGPU(Dawn) 后端齐备，示例可跑，但示例本身印证"多线程受限"。**

**证据（来自 bgfx master README 与 genie.lua）：**

- **渲染后端**：WebGL 1.0、WebGL 2.0、WebGPU (Dawn Native only) 均在官方"Supported rendering backends"列表中。
- **平台**：Wasm/Emscripten 官方列入"Supported platforms"。
- **构建脚本**（scripts/genie.lua）显式有 "wasm*" 配置分支，说明整套工程（bgfx/bx/bimg/示例）就是为 wasm 构建设计的。
- **关键线索（多线程）**：genie.lua 注释——
  > "17-drawstress requires multithreading, does not compile for singlethreaded wasm"
  这直接印证：bgfx 官方认为某些功能在"单线程 wasm"上不可编译/不可用。17-drawstress 是压测示例，需要独立渲染线程。
- **社区实践**：已有第三方演示（如 ImGUI-bgfx-Web-Demo，用 emscripten + glfw3 把 bgfx + ImGui 跑进浏览器），证明基础渲染管线可通。

**对 Caesura 的直接含义：**

- 现有 render 模块走 bgfx，**渲染后端 Switch（选 WebGL2）基本可行**，着色器（bgfx shader）不受影响（GLES/WebGL 同源）。
- 但 Caesura 的 render 很可能依赖**独立渲染线程**（bgfx 推荐模式"render thread + 每帧 renderFrame"），这在浏览器上不能像桌面那样自由跑。详见 §7 风险。

---

## 2. SDL3 的 emscripten 支持

**结论：比 SDL2 显著成熟，是官方在 web 平台的主推支持，但线程/渲染约束与桌面明显不同。** SDL 官方 wiki 有专门 README-emscripten 文档（现网稳定版），逐项覆盖：

**状态（官方"state of things"，2024/10 快照，仍在更新）：**
- 现代浏览器（Chrome/FF/Edge/Safari，含 iOS/Android）都已具备：WebAssembly、WebGL（表现为 GLES2/3）、线程（有 caveat）、游戏手柄、自动更新。
- 官方论断："you don't have to make a lot of concessions to get even a fairly complex SDL-based game up and running"。

**关键约束（官方原文要点）：**
- **主循环**：主流模型必须从 while(1) 改为回调式 mainloop（emscripten_set_main_loop 或 SDL main callbacks 自动处理）。**SDL3 的 main-callbacks 机制能免掉平台分支**——这恰是 Caesura 需要的。
- **线程（重点）**：
  - "Rendering must be done on the main thread. This is a hard requirement on the web."
  - "You have to decide to either build something that uses threads or something that doesn't; you can't have one build that works everywhere."
  - 用线程**必须**配 COOP/COEP 头 + SharedArrayBuffer，否则程序**根本启动不了**。
  - 预处理宏 __EMSCRIPTEN_PTHREADS__。
- **音频**：API 层"如预期"，但浏览器强制"用户交互后才出声"；SDL 在未授权时丢弃音频数据（对依赖 audio callback 推进的 App 是好事）。SDL3 可在 build 时用 SDL_EMSCRIPTEN_PERSISTENT_PATH 自动把存档挂到 IDBFS。
- **渲染**：SDL 2D renderer → GLES2 → WebGL；WebGL2 可建 GLES3 上下文；**默认 vsync OFF，必须显式设置 vsync=1 用 requestAnimationFrame**，否则帧率不稳/耗电。
- **文件系统**：默认 MEMFS（内存）；可用 IDBFS 持久化；--embed-file 把整个目录/文件预打包进 App（数据在 main() 前就绪）——但"hundreds of MB / thousands of files"不推荐。
- **构建**：emcmake cmake + emmake make；Emscripten 4.0.x 现行，**强调用最新稳定版**（老版本"silently broken"）。

---

## 3. SoLoud 在 WASM 上的音频后端

**结论：SoLoud 是可移植到 WASM 的（基线支持存在），但**不是"开箱零改动"——**依赖所选后端与 Emscripten 音频栈的适配。**

**依据：**
- SoLoud（jarikomppa/soloud）是纯 C/C++ 音频引擎，官方定位"portable"，后端分层（miniaudio / opensles / sdl / wasapi / xaudio2 等），本身无平台限制。
- 第三方强证据——**flutter_soloud**：这是一个把 SoLoud 编译到 Web/WASM 的成熟项目（其 Wiki 有专门"Web Support"章节，且有 wasm 运行时问题单，说明真实在用）：它用 **miniaudio** 后端 + Emscripten 音频（AudioWorklet / HTML5 Audio）跑通浏览器。这证明"SoLoud + miniaudio 后端 → wasm 浏览器音频"是已被业界验证的可行路径。
- Emscripten 现代音频栈：Web Audio；Emscripten 4.x 新增 Wasm Audio Worklets，浏览器侧音频由浏览器混音，SoLoud 做 DSP/混音逻辑。
- **已知坑（来自 flutter_soloud 的 wasm 问题单）**：Web 音频在 JS/WASM 边界有类型回调问题，需要适配层。

**对 Caesura（audio → SoLoud）的含义：**
- 引擎的 audio 模块通过 IAudioBackend 抽象 SoLoud。若后续加一个"Emscripten 后端"实现（在组合根注入），理论上能复用现有 3-bus（BGM/Voice/SE）逻辑。
- **工作量中低，风险中低——是本调研中最顺的一环**。
- 但需注意：浏览器"用户交互前不发声"，SoLoud 初始化本身没问题，**声音要等第一次鼠标/触摸后才解锁**（SDL3 §2 也强调），需要封面/标题页交互开机。

---

## 4. Lua 5.4 编译为 WASM 的成熟度

**结论：成熟。** Lua 是纯 ANSI C，几乎零依赖，编译成 WASM 是**业界成熟做法**，性能与原生相当（WASM 接近原生速度，无 GC 顾虑，Lua 5.4 又是纯手写 VM）。

**依据：**
- 多起公开实践把 Lua 5.4 VM 编进浏览器（OSCHINA/CSDN 教程、lua-ffi-wasm、各类 C++/Lua 游戏引擎 wasm 移植）。
- 官方 Lua 源码无平台特定代码，CMake + emscripten 交叉编译即可。
- 项目现有 Lua 5.4 是 vendored（external/lua/），可直接用 emcc 编成 lib。
- **性能**：脚本层只做游戏状态/调度/命令分派，非渲染热路径，WASM 下的 Lua 性能足够。

**对 Caesura 含义：**
- **可行性高、工作量低**。现有 script 模块（ILuaManager + KAG 绑定 + Lua 沙箱）逻辑可原样编译。
- **注意沙箱与 io 白名单**（项目记忆：引擎沙箱 io.open 白名单 scripts/assets/tests/demo 前缀）——WASM 下文件系统是虚拟 MEMFS，白名单判断需适配到虚拟路径。

---

## 5. 业界同类 VN 引擎 Web 导出案例

| 引擎 | Web 路径 | 语言/策略 | 对我们的启示 |
|---|---|---|---|
| **Ren'Py** | emscripten/WASM 把整个 Python+C 引擎编译进浏览器（官方 Web/Beta 出口） | C + Python，Emterpreter + 线程补丁 | **最直接对标的"踩坑史"**。官方文档明示限制：多线程不可用、视频用浏览器内建播放器、Live2D 暂不支持、不支持网络请求、50MB 以上文件不被浏览器 cache、Web 端后台预加载缺失导致加载掉帧 |
| **WebGAL** | 纯浏览器原生（Web-first），Pixi.js 渲染，MPL-2.0 | TypeScript/JS，一次编写处处运行 | **主流新兴 VN 引擎直接用 Web 技术栈重写**，而非 wasm 化；说明"为 web 重写"在 VN 领域是被验证的更省力路径 |
| **Monogatari** | Web-first，HTML/JS/CSS + TypeScript，可 PWA/Electron 打包桌面 | 原生 web | 同上：VN 引擎的 web 化主流是**原生 web（DOM/Canvas）**而非 wasm |
| **yuzu (lyco-engine)** | WASM-based VN 引擎，Rust 编译 wasm + 资源流式后端 | Rust → wasm | 近期（2026-08 仍在更新）**纯 wasm VN 引擎**最新案例，证明"wasm VN 引擎"有意愿者在做 |
| **通用 C++ 引擎** | Godot/Unity（官方 Web 导出）、doom/ffmpeg 等 wasm 移植 | 各自 wasm 适配 | C++ 引擎 wasm 化是成熟工程，但都做**深度适配**（平台抽象 + 线程代理），非"重新编译即可" |

**关键对比结论：**
- **Ren'Py Web 的政治事实**：官方把整个引擎 wasm 化，但付出了多线程不可用、Live2D 砍掉、视频换浏览器播放器的代价，且 Ren'Py 引擎本质**单线程**（其代码主循环递归，曾靠 Emterpreter 打断点）。
- **Caesura 是"多线程 + 加密归档 + Live2D + 视频 + 大资源"的 C++ 引擎**——比 Ren'Py 的 wasm 迁移更难，因为最难移植的几项（线程、加密 fs、Live2D）它全占了。
- 主流 VN 引擎（WebGAL/Monogatari）选择"**Web-first 重写**"，侧面说明对大多数 VN 需求，**wasm 原生引擎迁移并非最优**。

---

## 6. CMake 集成 emscripten 工具链的要点

**核心路径：emcmake cmake 自动注入 Emscripten toolchain file（Emscripten.cmake），无需手写 toolchain。**

**要点清单：**

1. **toolchain 接入**：
   - emsdk install latest && emsdk activate latest，source emsdk_env.sh，然后 emcmake cmake -B build-wasm -DCAESURA_WEB=ON。
   - emcmake 会设置 CMAKE_TOOLCHAIN_FILE=Emscripten.cmake、CMAKE_SYSTEM_NAME=Emscripten、编译器为 emcc/em++。
   - 现有 16 个内部静态库 + 15 个 API-only target 的 CMake 结构无需改动即可被 emcc 编译（静态库同样支持）。

2. **多线程（pthreads）Gating**：
   - 开线程：-s USE_PTHREADS=1 -s PTHREAD_POOL_SIZE=N（链接期）。
   - **必须从构建期就决定"线程版 or 单线程版"**（SDL 官方：不能一个构建通吃）。
   - 线程版必须配 **COOP/COEP HTTP 头 + SharedArrayBuffer**，否则浏览器拒绝启动。
   - 预览/开发服务器需自行加头（现有本地 dev server 需要改）。

3. **内存配置**：
   - 链接 -s ALLOW_MEMORY_GROWTH=1 -s MAXIMUM_MEMORY=1gb（SDL 官方：较大程序必须；线程版必须 1gb 否则 iOS 浏览器启动失败）。

4. **文件系统预加载（关键决策点）**：
   - 资源打包选项：
     - --embed-file <dir>@/path：把整个资源树打进 wasm，main() 前即就绪、同步可读——但**全部驻留内存 + 每次都全量下载**，数 GB 资源不可行。
     - --preload-file（XHR 后填入 MEMFS，按需异步）。
     - 或自定义：CARC 归档**逐个文件**加载进 MEMFS，但需自己写 loader。
   - 存档持久化：SDL_EMSCRIPTEN_PERSISTENT_PATH=/storage + -lidbfs.js，走后端 IDBFS 自动持久化。
   - **浏览器限制**：>50MB 单文件不被缓存（Ren'Py 文档）；总包体受限。

5. **二进制输出**：
   - 链接 -o index.html（自带壳）或 -o index.js（自定义 HTML，产品级推荐，canvas 不能有 border/padding）。
   - Debug 用 -gsource-map。

6. **现有构建开关**：
   - 应在引擎顶层 CMake 加 CAESURA_WEB=ON 分支；关闭非 web 平台后端（Live2D 原生 SDK、FFmpeg、Steam、HTTP RPC 编辑器在 web 上需要特殊处理/裁剪）。

---

## 7. 风险清单（详细）

### RISK-1（高风险，结构性）— bgfx 渲染线程在浏览器上的多线程回写受限
- **现状**：bgfx 原生推荐"独立 render thread + 每帧 bgfx::renderFrame()"。浏览器**主线程独占渲染**（SDL 官方硬性要求），JobSystem 的多线程也依赖 pthreads(SharedArrayBuffer)。
- **影响**：Caesura 的 job（JobSystem 多线程）、render 渲染线程、audio 回调等与 web 的单线程/代理模型冲突。
- **应对**：① 全部线程代理到主线程（Emscripten proxying 机制，性能损失）；② 降级为"单线程构建"（舍弃 JobSystem 并行，退回渲染主线程 + 帧内任务）；③ 重构成浏览器友好的 event 模型。前两者改造成本高，后者等于重写主循环。

### RISK-2（高风险，部署性）— CARC 加密归档 + 大资源无法整体预加载
- **现状**：CARC 是 AES-256-GCM + Ed25519 的自定义加密归档。wasm 的 --embed-file 只能放明文树且全驻内存/全量下载；**CARC 解钥逻辑 + 加密封装在浏览器端无法原样用**（浏览器无原生桌面 fs，且密钥放 wasm 可被提取——加密意义存疑）。
- **影响**：要么放弃 CARC 的防篡改价值（在 web 端明文打包），要么在 wasm 内实现自定义解包器（把 CARC 当"流的容器"，逐文件解密进 MEMFS）。
- **应对**：低风险做法=**Web 导出用明文资源文件夹 + 可选混淆**；或实现 wasm 版 CARC reader（工作量 +）。另外**数 GB 资源被 50MB 缓存/包体限制**卡死，必须支持"按章节流式加载/分段 CDN"。

### RISK-3（中高风险，环境性）— 多线程要求 COOP/COEP + SharedArrayBuffer
- wasm 线程版必须由服务器下发 COOP/COEP 头 + 浏览器支持 SAB；**公共托管（itch.io 等）/CDN/CORS 环境常不满足**，导致线程版**无法部署**。
- 因此往往被迫选"单线程构建"（能力阉割）。这与 RISK-1 叠加。

### RISK-4（中风险）— ffmpeg / 视频解码在 wasm
- **CAESURA_ENABLE_FFMPEG**（硬解）依赖系统/平台 FFmpeg，wasm 无系统原生解码。
- **浏览器内建视频播放器**（Ren'Py 采用）是 VN 的可行替代（HTML5 video），但格式兼容受限（H.264/VP9 不等价）。
- 或 ffmpeg.wasm（纯 wasm 软解，CPU 重，不推荐 VN 全片替换）。引擎 video 模块有 pl_mpeg 软解回退——wasm 上大概率走 soft-dec 路径，性能是风险。

### RISK-5（中高）— Live2D 在 wasm 的可用性
- **Ren'Py Web 官方文档明确："暂不支持 Live2D"**。
- Live2D Cubism **Cubism SDK for Web** 是独立于 Native 的产品线（JS API + WebGL），**不能直接复用**引擎当前的 **Cubism SDK for Native**（live2d 模块，CAESURA_LIVE2D=ON）。
- 若 Live2D 是内容刚需，wasm 路径需**引入 Cubism SDK for Web 并写独立适配层**，或降级为 PNG 回退（引擎已有 PNG fallback）。**额外工作量显著**。

### RISK-6（中风险）— 主循环改造 + 无头后端
- 现有 main.cpp + src/entry/Engine::init() 是"Win32 原生 while 循环"组合根；浏览器需改成 SDL3 main-callbacks / emscripten mainloop。
- 需新增"Web 平台后端"实现 IPlatformBackend、IAudioBackend（Emscripten）、ISaveProvider（IDBFS）并注入组合根——**不破坏模块边界，但要动 main.cpp 的 if-constexpr 分支链**（按项目记忆，此链改动有踩坑教训）。

### RISK-7（中风险）— 编辑器/RPC/Steam 在 web 无意义
- rpc 编辑器服务器、steam（CAESURA_HAS_STEAM）在浏览器不可用，需裁剪/条件编译，避免污染 wasm 构建。

### RISK-8（低-中）— 浏览器沙盒内容限制
- 网络请求受限（Ren'Py 官方文档确认 socket 不可用）；大文件 50MB 缓存上限；跨浏览器/移动端兼容性需多端 QA。

---

## 8. 综合评估与建议

### 结论：条件性推荐（作为远期技术验证/特定分发场景），不作为近期主力路径

**适合做 WASM 导出的前提（均需满足才值得投入）：**
1. 目标 = **Web 演示 / 试玩 / 特定客户部署**，而非全平台主力发行；
2. 可接受**降低渲染线程与 JobSystem 并行**（浏览器单线程代理或降级构建）带来的性能代价；
3. 可接受 **Web 端明文/混淆资源包**（放弃 CARC 防篡改的绝对性）或投入 wasm 版自定义解包器；
4. **Live2D 不是内容刚需**（否则多一笔独立 SDK for Web 适配成本）；
5. 具备**能下发 COOP/COEP 头 + 能承载分片大资源**的托管/CDN 环境（或接受单线程构建）。

**不推荐场景（若命中其一则放弃 A 路径）：**
- 需要**无缝多线程/高性能渲染**（浏览器给不了）；
- 需要**CARC 加密防拆包的强保证**（wasm 内密钥可被提取，加密名存实亡）；
- **Live2D 首发必备**；
- 目标是**itch.io 等通用托管**且资源 > 数百 MB（无法线程 + 包体受限）。

### 预估工作量（单人）

| 阶段 | 内容 | 预估（周） |
|---|---|---|
| P0 工具链与骨架 | emcmake 接入、16 库编译通过、一个最小 wasm 应用出画面 | 1–2 |
| P1 平台后端替换 | Web IPlatformBackend + 主循环、IAudioBackend(Emscripten)、IDBFS 存档、组合根接入 | 2–3 |
| P2 资源与渲染 | 渲染后端 WebGL2、着色器验证、CARC/明文资源加载策略、预加载方案 | 2–3 |
| P3 交互闭环 | 音频解锁、输入、存档读写、基础 VN Demo 可玩 | 1–2 |
| **首个可玩 Demo 合计** | | **6–10** |
| P4 生产化 | 视频回退（HTML5 video / soft-dec）、大包分片流式、多线程/单线程构建矩阵、多浏览器 QA、Live2D（如需要） | +6–8 |
| **生产级合计** | | **12–18** |

### 最大风险排序
1. **RISK-1 渲染线程/多线程在浏览器结构性受限**（决定成败，需先做 1–2 周 PoC 验证 bgfx 在 web 单线程下渲染管线能否闭环）。
2. **RISK-2 CARC 加密 + 大资源部署**（决定"导出什么"，直接影响解包/流式工作量）。
3. **RISK-5 Live2D wasm 支持（决定内容是否可带）**。
4. RISK-3 COOP/COEP + 托管环境（决定能否上线线程版）。
5. RISK-4 视频软解性能、RISK-6 主循环改造、RISK-7/8 裁剪与沙盒限制。

---

## 9. 参考文献（均以 2026 视角检索到的官方/一手来源）

- bgfx 官方 README（后端/平台含 WebGL1/2 + WebGPU(Dawn) + Wasm/Emscripten）：https://github.com/bkaradzic/bgfx
- bgfx genie.lua（wasm* 配置 + "17-drawstress requires multithreading, does not compile for singlethreaded wasm"）：https://github.com/bkaradzic/bgfx/blob/master/scripts/genie.lua
- SDL3 官方 Emscripten 文档（线程须主线程渲染 / 构建线程或非线程二选一 / COOP-COEP / SDL_EMSCRIPTEN_PERSISTENT_PATH / 主循环 / 内存）：https://wiki.libsdl.org/SDL3/README-emscripten
- Emscripten Pthreads 文档（SAB 需 COOP/COEP、pthreads 稳定）：https://emscripten.org/docs/porting/pthreads.html
- SoLoud（可移植音频引擎）：http://soloud-audio.com ；flutter_soloud Web/WASM 支持（SoLoud+miniaudio→wasm 实证）：https://deepwiki.com/alnitak/flutter_soloud/6.2-web-support
- Ren'Py Web / HTML5 官方文档（多线程不可用、视频浏览器内建、Live2D 暂不支持、50MB 缓存限制、无网络请求）：https://doc.renpy.cn/zh-CN/web.html ；RenPyWeb 构建环境 README（Emterpreter + 线程补丁路径、性能说明）：https://github.com/renpy/renpyweb
- WebGAL（Web-first 平台，Pixi.js）：https://github.com/OpenWebGAL/WebGAL ；Monogatari（Web-first，TS/PWA）：https://github.com/Monogatari/Monogatari
- yuzu / lyco-engine（WASM-based VN 引擎，Rust）：https://github.com/lilyco-42/lyco-engine
- ffmpeg.wasm（wasm 软解参考）：https://github.com/ffmpegwasm/ffmpeg.wasm
- Live2D Cubism SDK（Native 与 Web 系独立的产品线）：https://docs.live2d.com/en/cubism-sdk-manual/platform/

---
*本报告仅做文献/资料调研，未改动任何源码。仅新建本文档。*
