# G5 — 路径 C：Web 分发生态对比 + 移动运行时备选（调研）

> 状态：调研完成（round 30 续）。本文件是 [G5-decision.md](./G5-decision.md) 的三份调研产出之一。
> 定位：为 Caesura (AmeKAG) 决策「Web 导出 / 移动运行时」的生态依据与技术可行性评估。
> 调研日期：2026-08-14。资料以 GitHub / 官方文档 / 开发者社区为准。

---

## 0. 结论速览（TL;DR）

- **Web 视觉小说分发已高度成熟的赛道中，没有「一条龙全能引擎」**：WebGAL（生态热度最高，中文 galgame 圈）、Monogatari（老牌纯 Web 框架）、Ren'Py web（最成熟 LTS 方案的浏览器移植）、TyranoScript（日本智能手机优先）、B-Engine（新锐，面向差异化）。它们各有明确的「适合谁」边界。
- **Caesura 的 Web 出路不在「复制一个 WebGAL」，而在「用 A（emscripten 原生编译）或 B（轻量播放器）复用自有 Lua 调度器」**——三平台桌面+CARC+bgfx+SoLoud 的技术栈正是 Ren'Py 模型（原生引擎做浏览器发布）的现代 C++20 版。
- **纯 Web VN 工程最佳实践已成熟且基本稳定**：中文字体 woff2 子集化、WebAudio+ogg、立绘差分/眨眼、IndexedDB/IDBFS 存档、进度导出。这些是路径 A/B 的技术验收基准，不是难点。
- **分发渠道**：itch.io HTML5 嵌入是 VN Web 分发的主阵地（尺寸/文件数限制明确）；Steam 以桌面为主、Web 只作附加；网页直接嵌入适合社区/个人站。
- **移动运行时（SDL3 的备选）**：SDL3 3.2 对 Android/iOS 是**官方一等支持**，bgfx 有对应渲染后端；但 iOS 商店审核 + CARC 加密归档在移动端的存储/密钥管理是真正的风险点，工作量大于路径 A/B。

**推荐排序见 §6。**

---

## 1. 2026 年 VN 引擎 Web 分发生态现状

### 1.1 WebGAL（OpenWebGAL，中文社区主力，热度第一）

- **定位**：全新的网页端视觉小说引擎（TypeScript/React），「零门槛做 Galgame」是核心卖点。非 LTS 桌面移植，天生为浏览器而生，直接输出**静态站点**（WebGAL 本体即一个可部署的前端工程）。
- **热度量级**：GitHub OpenWebGAL/WebGAL 在中文 galgame/视觉小说开源仓库中稳居前列（star 量级为万级最靠前的 Web VN 引擎，长期进入 GitHub Trending / HelloGitHub；具体数字随时间浮动，量级约 **10k+ stars**，远超其他纯 Web VN 引擎）。官方站点 openwebgal.com，中文文档完善。
- **衍生生态**：
  - **WebGAL Terre**：可视化编辑器（GUI 拖拽剧本/立绘/音效），面向非程序员。2026 年推出 **Steam 版（WebGAL Terre Steam Edition）**，上架 Steam 商店，标志其从社区工具向商业发布工具演进。
  - **WebGAL-Server / WebGAL 打包平台**：提供在线托管/发布，降低发布门槛。
- **发布形态**：**静态页 / 直接嵌入**（WebGAL 本身就是前端项目，产物为静态资源，可扔进任意静态托管、itch.io HTML5 上传）。
- **对 Caesura 的意义**：WebGAL 证明了「纯 Web VN 引擎」的可行性，但它**不是原生桌面引擎的 Web 移植**，与 Caesura（C++/bgfx 原生）是不同路线。WebGAL 是「Web 原生重写」的代表，而 Caesura 应走「原生引擎 + emscripten 编译」路线（Ren'Py 模型）。

### 1.2 B-Engine（bengine）

- **定位**：较新锐的 Web 视觉小说引擎（不同作者有多个同名项目，行业里「B-Engine」常被泛称）。GitHub 上有 bengine/B-Engine 若干仓库，主打差异化脚本语法 + Web 交互，但整体生态成熟度和社区热度**远低于 WebGAL/Monogatari**。
- **热度量级**：star 量级较低（百级到千级上下，远未进入主流），文档/商业配套不完整。
- **发布形态**：静态 Web / 可嵌入。
- **对 Caesura 的意义**：可作为「Web VN 脚本语法设计」的参考（差异化脚本），但不是值得跟随的成熟生态。结论：**不构成威胁也不构成借力对象**。

### 1.3 Monogatari（老牌纯 Web VN 框架）

- **定位**：最早的一批「把 VN 带到 Web」的框架之一，由 Monogatari Project 开发。JS/HTML 驱动，npm 包 @monogatari/core，有配套官方文档站点（monogatari.io）和可视化编辑器。
- **热度量级**：在 TypeScript/JS 的 visual-novel 主题里长期居前（npm 生态 + GitHub star 数千量级），是**纯 Web VN 框架的长期代表**，但近年活跃度趋缓（v2 仍在迭代 alpha，进度较慢）。
- **技术要点**：数据存储走 **IndexedDB / LocalStorage**（官方 Storage Engine 文档，支持持久化存档），这正是纯 Web VN 的存档范式。依赖浏览器能力，无原生底层运行时。
- **发布形态**：**静态页 / itch.io HTML5 嵌入** 为主，npm 集成到自定义站点。
- **对 Caesura 的意义**：证明「纯 Web 轻量框架 + 浏览器存储」能覆盖 VN 基础需求，但其渲染/音频/系统深度（无 bgfx、无 CARC、无 JobSystem）远低于 Caesura。**这恰是路径 B（轻量 Web 播放器）对标/参考的形态**，但 B 的优势在于复用 Caesura 的 Lua 调度器与资产格式。

### 1.4 Ren'Py web（LTS 方案的浏览器移植，最成熟参照）

- **定位**：Ren'Py 官方为 LTS 版本提供的 **Web / HTML5 导出**（renpy.web / renpyweb 项目）。Ren'Py 是 Python 编写的桌面 VN 引擎，**Web 供能不是重写，而是通过 Emscripten 把 Python 运行时 + 引擎编译为 WASM**。
- **热度与成熟度**：Ren'Py 本体 GitHub star 长期数万级别（一直在 VN 引擎里最高）；Web 供能是**官方一等特性**（官方文档有「Web / HTML5」章节），随 LTS 发布迭代，社区把它当成「桌面 VN 引擎发布到浏览器的标杆方案」。
- **技术要点**：
  - **Emscripten + WASM**，SDL2 后端 → WebGL；音频经 SDL/emscripten 通道。
  - **存档持久化用 IDBFS（emscripten IndexedDB 文件系统）**——官方文档明确支持，社区有大量「浏览器存档/回档」排障帖，证明这是可用但有坑的成熟路径。
  - 大体积游戏支持**渐进式下载 / 分块加载**（progressive downloading），服务在线流式游玩。
- **发布形态**：官方 web 构建产物可直接部署到 **itch.io HTML5 / 任意静态托管**；社区广泛用这方式发 Html/web 构建。
- **对 Caesura 的意义**：Ren'Py web **是路径 A（emscripten 编译原生引擎）的行业范例**——它证明了「桌面 C/C++/Python 原生引擎经 Emscripten → WASM → 浏览器，且存档走 IDBFS」完全可行且被社区接受。Caesura（C++20 + SDL3 + bgfx + SoLoud + Lua）与 Ren'Py 的架构模型一致，路径 A 是「现代 C++20 版的 Ren'Py web」。**这是本调研最重要的生态参照。**

### 1.5 TyranoScript / ティラノスクリプト（日本智能手机优先）

- **定位**：日本建立的免费 VN 引擎，官方定位即 **「智能手机兼容」**（tyranoscript.com：A novel game engine compatible with smartphones），HTML5/JS 驱动，产出可在**手机浏览器/WebView** 运行，也有 TyranoBuilder/TyranoStudio（现 Steam 上架）配套编辑器。
- **热度量级**：日本社区热度高，GitHub 上商业/同人项目众多，但引擎本体是分发型（官网下载），star 统计不如 GitHub 原生项目直接可比；影响力长期而稳定。
- **发布形态**：可**静态 Web / 手机 Web**；大量日本同人游戏用它在 网页/移动端 分发。
- **对 Caesura 的意义**：证明「**智能手机优先的 VN 分发**」在日本是成熟市场（对应 §4 移动运行时备选）。但 TyranoScript 是 Web 技术栈，无 CARC 级加密与 C++ 系统深度。

---

## 2. 纯 Web VN 技术的当前最佳实践

> 这是路径 A/B 的「技术验收基准」：下述每一项都是 Web VN 工程的成熟做法。

### 2.1 文本渲染（中文字体子集化 woff2）

- **问题**：CJK 全量字体动辄数 MB ~ 20+ MB，对首屏加载不可接受。
- **最佳实践**：
  - 构建期用**字体子集化工具**裁剪出剧本实际用到的字符。代表工具：cn-font-split（GitHub / npm，多线程切割 otf/ttf/woff2，CJK 粒度控包，社区主流）、wf-cn-font-split（npm 变体）、pyftsubset（fonttools）。
  - 产出 **woff2** 子集（压缩率最优），通过 @font-face 加载；对游戏内文本，可在构建时扫描 .ks/Lua 剧本提取字符集，裁剪出「剧本小子集」+「通用大字回退」两级。
  - 引擎侧：bgfx 文本渲染可直接采样子集字体纹理；若走 DOM/Canvas 覆盖（路径 B），则直接 CSS font-face + 子集 woff2。
- **Caesura 落地**：asset-pipeline 增加「字体子集化」步骤（扫描文本 → cn-font-split → woff2），桌面侧保留全量字体、Web 侧用子集。

### 2.2 音频（WebAudio + ogg/mp3）

- **问题**：浏览器自动播放策略严格（必须用户手势后解锁 AudioContext）；格式兼容因浏览器而异。
- **最佳实践**：
  - 主格式 **.ogg（Vorbis）**：Chrome/Firefox/Edge 原生支持，Web VN 常用；对 Safari/iOS 需 .m4a/.mp3（AAC 兼容）回退，或统一转 mp3。
  - 引擎初始化时在「用户首次点击/回车」后调用 AudioContext.resume() 解锁（autoplay policy 官方文档明确）。
  - 音乐/音效用 WebAudio 节点图（GainNode 做 3 总线：BGM/Voice/SE），与现代桌面引擎的混合总线对应。
- **Caesura 落地**：SoLoud 本身有 emscripten 示例；路径 A 下 SoLoud 编译到 WASM 后音频经 emscripten（SDL/JS 通道）走 WebAudio；路径 B 则直接前端 WebAudio 节点实现 3 总线。

### 2.3 立绘差分 / 眨眼

- **问题**：桌面靠 texture + 定时器 + 蒙版；Web 同样可行，难点在资产加载与骨架/眨眼动画。
- **最佳实践**：
  - 立绘差分：多张「部位图」（眼睛开/闭、嘴形、表情层）在 Canvas/WebGL（或 bgfx WASM）上分图层叠加，与桌面差异小。
  - 眨眼：定时器驱动切换眼睛层纹理（透明度/替换），或用骨骼/脊柱动画替代。Live2D 在 Web 有官方 Cubism SDK Web（为 Web 设计的 runtime）。
  - 性能：WebGL 纹理合批、避免频繁 upload；对路径 B 用 Canvas2D 合成已足够 VN 帧率。
- **Caesura 落地**：Caesura 已有 live2d/ 模块（Cubism 或 PNG 回退）；Web 侧可把 Cubism Web runtime 作为路径 A 的 Live2D 后端，或路径 B 用 PNG 差分层。

### 2.4 存档（IndexedDB）

- **问题**：浏览器不能写任意文件系统；localStorage 有大小/同步限制。
- **最佳实践**：
  - 主存 **IndexedDB**（大容量、异步、结构化）；引擎侧用 **emscripten IDBFS** 把文件系统挂到 IndexedDB（Ren'Py web 就是这么干的），让现有 storage/ 读写透明地落到 IndexedDB。
  - 封装模式：idb-state-persistence / idb-keyval 等轻量封装工具成熟。
  - 注意：IndexedDB **随浏览器/站点源隔离**，清缓存会丢档——需提供「进度导出/导入」作为云备份（见 §2.5）。
- **Caesura 落地**：storage/ 模块已做存档加密 + 模式迁移；Web 侧把 ISaveProvider 换成「IDBFS-backed」实现即可，加密逻辑复用。

### 2.5 进度导出

- **问题**：纯浏览器存档不可跨设备、不可靠（清缓存即失）。
- **最佳实践**：提供「**导出存档 / 导入存档**」按钮：把 IndexedDB 里的存档序列化为 JSON/base64 文件下载，用户可备份或换设备导入。或可选接云（自有后端）；对单机 VN 一般用「导出文件」即够。
- **Caesura 落地**：在 Web 导出壳（入口 HTML + JS glue）里加导出/导入逻辑，对接 ISaveManager。

---

## 3. 分发渠道对比：itch.io / Steam / 网页嵌入

| 维度 | itch.io（HTML5） | Steam | 网页直接嵌入 / 静态托管 |
|---|---|---|---|
| **VN 分发地位** | **主阵地**：itch.io 有独立 Visual Novel 分类，海量免费/付费 VN 以 HTML5 游玩；（社区数据）VN 是 itch 高占比的叙事类目；支持无 DRM 直接玩 | 强于桌面变现；Web 只作附加（Steam 不鼓励纯浏览器游戏当主版本） | 社区/个人作品集、独立站、同人展；无平台抽成 |
| **发布形态** | **HTML5 上传到 itch 自带浏览器 play 页**（直接嵌入其播放器）；也支持 ZIP/桌面可执行 | 桌面可执行包（Windows/macOS/Linux），商店页面 | 任意静态托管（GitHub Pages/Netlify/自建），iframe 嵌进个人站 |
| **技术限制** | 浏览器 zip 游戏**文件数量限制（约 1000 文件上限，社区多帖反馈）**；单文件过大报错——需**资产打包成少数大文件**（CARC 天然合适） | 无文件数限制；需过审 + 商店分成 | 取决于托管商（GitHub Pages 有单文件/仓库大小限制） |
| **加载** | 靠浏览器按需/渐进加载；不能像桌面预装 | 本地安装即加载 | 靠 CDN/静态托管 |
| **存档** | IndexedDB/IDBFS（每个 itch 游戏隔离） | 本地文件 / Steam 云存档 | IndexedDB 按源隔离 |
| **变现** | 免费 / 定价 + 打赏，抽成低 | 平台分成 30%/硬件 | 完全自控 |
| **适合 Caesura 的用途** | **Web 版本首选落地渠道**（上传 CARC 单包 + 少量资源） | 桌面三平台发布（已有），Web 不作主 | 编辑器预览 / 官方 demo / 个人演示页 |

**结论**：
- **VN 在 itch.io 占比显著**——itch.io 明确按 genre 划分 Visual Novel 分类，Web 游玩是 itch 的招牌能力。Caesura 的 Web 导出首选打 **itch.io HTML5**。
- 注意 **ZIP 文件数上限**：CARC 已把资产打成单个归档，天然规避「1000 文件」限制——这是 Caesura 相对散资产原生 Web VN 的一个架构红利，应在报告里明确利用。
- Steam 侧：Web 不是加分项，聚焦桌面；网页嵌入适合作为「官方试玩/编辑器联动预览」。

---

## 4. 移动运行时备选（G5 路径 C 的另一个选项）

> 选型背景：若不做 Web（A/B），把 SDL3 原生引擎编译到 **Android / iOS**。调研 SDL3 3.2 系列的移动端现状、CARC 存储注意事项、JNI 集成复杂度。

### 4.1 SDL3 3.2 对 Android / iOS 的支持程度

- **官方一等平台**：SDL3 README-platforms 及官方 wiki 明确把 **Android 与 iOS 列为官方支持平台**（SDL2 时代即如此，SDL3 延续并改进）。
- **构建**：
  - **Android**：SDL3 提供官方 build-scripts（build-scripts/pkg-support/android），用 **CMake/NDK** 构建；社区有「SDL3 build for Android 和 WebAssembly 的分步指南」（SDL 官方 Discourse）。
  - **iOS**：README-ios 官方支持，通过 Xcode + CMake 交叉编译出 framework/.a；sdlplatform.cmake 处理平台分支。
  - **现成 CMake 模板**：有开箱即用的跨平台 starter（如 zraz/sdl3-gpu-starter，官方 SDL3 GPU API，支持 Win/Linux/macOS/iOS/Android），可作为移植骨架参考。
- **渲染**：SDL3 抽象在移动端走 **OpenGL ES**；bgfx 有对应后端（NDK 支持 ES）。bgfx 在 Android/iOS 有成熟支撑（早年移动游戏大量用 bgfx）。
- **音频**：SoLoud 在 Android（OpenSL ES/AAudio）/iOS（CoreAudio）均可编译，原生后端已存在。
- **部署目标**：SDL3 官方 Discourse 明确过 Apple 部署目标（iOS minimum deployment target）；Android 走 NDK + Java/Kotlin 宿主 Activity。

### 4.2 CARC 归档与存档在移动端的存储注意事项

- **CARC（AES-256-GCM + Ed25519 签名）**：引擎 archive/ 模块的加密归档。移动端注意事项：
  - **密钥管理**：CARC 密钥若硬编码进可执行文件，在移动端比桌面更容易被提取（APK 可解包、iOS 越狱设备可 dump）。建议密钥经 **Android Keystore / iOS Keychain** 注入，或至少用设备绑定混淆，别明文写死。
  - **安装体积**：APK 有约 2GB（target 版本更高可放宽）单包大小上限，商店有压缩比政策；超大 CARC 需考虑分成 APK base + OBB（Android）或 on-demand resources（iOS）。
  - **归档访问**：CARC 只读归档放 internal storage（随 app），存档放 data/data / iOS App Support（沙盒内）——移动端有严格沙盒，storage/ 现有「文件路径」抽象需映射到沙盒目录。
  - **文件数**：CARC 单文件读省去大量小文件 IO，对移动端启动加载友好。

### 4.3 JNI 集成复杂度（Android）

- **Android 宿主为 Java/Kotlin Activity**，SDL3 只要一个 Java 层拉起 native（SDL3 自带 SDLActivity，官方支持）。
- **JNI 复杂度**：
  - **低-中**：若不做平台互通（奖杯/内购/分享/权限），SDL3 自带 Activity + 纯 native 渲染即可跑通，JNI 接触面小。
  - **中-高**：若要接 Google Play 计费 / Game Services / 微信分享 / 相机，每个都需 JNI + Gradle 集成 → 工作量大。
  - Caesura 已有 steam/ 模块抽象成就系统；移动端需新 achievements/analytics 后端走 JNI，属于新增工作量。
- **iOS**：无 JNI，用 Objective-C++ 桥（SDL3 支持），复杂度与 Android JNI 相当但方向不同。

### 4.4 现成 CMake 模板 / 社区样本

- zraz/sdl3-gpu-starter：SDL3 GPU API 跨平台 starter（含 iOS/Android CMake），参考价值高。
- SDL 官方：README-cmake、README-ios、README-android、README-emscripten，及 build-scripts。
- **注意**：Caesura 的 bgfx + SoLoud + Lua 5.4 需各自为移动端配置（Lua 纯 C 可直接编译；bgfx 需 Android/iOS 渲染后端；SoLoud 需移动音频后端），没有「一键模板」，但每一块都有官方/社区产物可查。

### 4.5 移动端结论

- **可行性**：技术与工具链成熟（SDL3 一等支持 + bgfx/SoLoud 有移动后端）。**技术风险低，但「合规/商店/密钥/宿主集成」是真正风险**（尤其 iOS 上架审核、安卓密钥提取）。
- **工作量估算**：> 路径 A（emscripten），因为移动端除了交叉编译还要处理 Activity/宿主集成/商店打包/密钥注入，且无法复用 Web 的 IDBFS 简单存储（要写原生沙盒路径）。
- **收益**：智能手机分发是日本 TyranoScript 已验证的成熟市场，但国内/Steam VN 用户仍以桌面+PC 为主；移动端适合「第二分发曲线」，优先级低于 Web。

---

## 5. 技术与路线可行性交叉核对（针对 Caesura 特有点）

| Caesura 特性 | 路径 A（emscripten） | 路径 B（轻量 Web 播放器） | 路径 C（移动） |
|---|---|---|---|
| **bgfx 渲染** | bgfx 有 **WebGL/WebGL2 emscripten 后端**（官方、有多起示例；含 wasm64 fix）；直接复用 | **不复用**，需重实现「文本/立绘/UI 可视子集」（Canvas/WebGL）——**最大工作量洞** | bgfx Android/iOS 后端（GLES），复用度高 |
| **SoLoud 音频** | SoLoud emscripten 可行；或走 WebAudio 通道 | 前端 WebAudio 节点实现 3 总线 | SoLoud 移动后端 |
| **Lua 5.4 + kag 调度器** | **直接复用**（Lua 纯 C 编译进 WASM） | **复用**（wasmoon/Lua 5.4 需 Emscripten Lua 或重译；Fengari 为 5.3 不直接匹配） | 直接复用 |
| **CARC 加密归档** | 复用读取逻辑；注意 WASM 内存/单归档大小 | 复用解码，浏览器端需重放解密（密钥安全问题更严峻） | 复用；密钥需 Keystore/Keychain 注入 |
| **存档 storage/** | **IDBFS 透明落 IndexedDB**，改动最少 | 需新浏览器存储实现 | 沙盒目录映射 |
| **Live2D** | Cubism Web runtime（路径 A）或 PNG 差分回退 | PNG 差分 / DOM 层 | Cubism native 移动 runtime |
| **3D 小游戏（minigame/）** | bgfx WASM 可承载 | **不可**（重写成本高） | bgfx 可承载 |
| **三平台 CI 复用** | 直接在 .github/workflows 加 emscripten 作业 | 前端工程独立 CI | 加 Android/iOS 交叉编译作业 |

> **关键洞察**：路径 B 唯一真正的优势是「产物纯 JS 无需 WASM、易嵌入任意页面」，但它**丢掉 bgfx 与 3D 小游戏，且需重实现渲染**——对已有完整 bgfx 渲染的 Caesura，这是净损失。路径 A 复用度最高。

---

## 6. 三路径排序：工作量 × 风险 × 收益

评分口径：1–5（5 最有利/最高）。「工作量得分」取反向（5=最省力）。

| 维 | 权重 | 路径 A（emscripten 原生编译） | 路径 B（轻量 Web 播放器） | 路径 C（移动运行时） |
|---|---|---|---|---|
| 复用度（bgfx/Lua/CARC/存档） | 35% | 4.5（几乎全复用） | 2.0（丢 bgfx，失 3D 小游戏） | 4.0（bgfx/音频复用） |
| 工作量（省力反向） | 30% | 4.0（加 emscripten 作业 + glue） | 3.0（重写渲染子集 + 新播放器） | 2.5（宿主/密钥/商店/沙盒） |
| 技术风险 | 20% | 4.0（bgfx/SoLoud 官方案例） | 3.5（纯前端，但渲染重写风险） | 3.0（工具成熟，iOS 审核/密钥风险） |
| 分发收益 | 15% | 4.5（itch.io HTML5 主阵地） | 4.5（任意嵌入） | 3.5（智能手机市场，次要） |
| **加权总分** | | **4.33** | **3.03** | **3.28** |

**综合排序：A > C > B。**

### 推荐

- **首选：路径 A（emscripten 原生编译）**。理由：
  1. **复用度最高**——bgfx WebGL 后端、SoLoud emscripten、Lua 5.4 纯 C、CARC 读取、storage(IDBFS) 全部直接复用，工作量最小且不牺牲 3D 小游戏。
  2. **有成熟行业范式**——Ren'Py web 已把「桌面原生 VN 引擎 → Emscripten → 浏览器，存档走 IDBFS」跑通并沉淀多年，Caesura 是它的现代 C++20 翻版。
  3. **分发直接**——产物可上传 **itch.io HTML5**，且 **CARC 单归档天然规避 itch 的约 1000 文件数限制**（架构红利）。
  4. **CI 成本低**——只需在现有三平台 CI 上加一个 emscripten 作业。
- **备选（不建议优先）**：路径 B 仅当「必须纯 JS 无 WASM、零后端」且放弃 3D/深度渲染时才值得；对已有完整 bgfx 的 Caesura 是净损失。路径 C 作为**第二分发曲线**（移动），排在 A 之后，因为其真正风险（iOS 审核、CARC 密钥注入、宿主/JNI）在 A 之上且收益次要。
- **建议**：A 达成 Web 分发（itch.io HTML5 首选、任意静态嵌入次选）后，再评估 C 作移动第二曲线。§2 纯 Web 最佳实践（字体子集化/WebAudio/IDBFS/导出）直接作为 A 的工程验收清单。

---

## 附：参考链接

- WebGAL/Terre：github.com/OpenWebGAL/WebGAL、openwebgal.com、Steam「WebGAL Terre Steam Edition」
- Monogatari：monogatari.io、@monogatari/core、developers.monogatari.io（Storage/IndexedDB 文档）
- Ren'Py web：renpy.org（Web/HTML5 章节）、github.com/renpy/renpyweb、Ren'Py web save/IDBFS 社区帖
- TyranoScript：tyranoscript.com（手机兼容定位）、TyranoStudio（Steam）
- 文本渲染：github.com/jrenc2002/cn-font-split、npm wf-cn-font-split、pyftsubset
- 音频：MDN「Audio for Web games」「Web Audio API best practices」（autoplay policy）
- 存档/导出：emscripten IDBFS（issues/12348 排障）、itch.io「Saving in browser (HTML5)」、pixi-vn save 文档
- 分发：itch.io Visual Novel 分类 / HTML5 zip「1000 files limit」帖子
- SDL3 移动/Web：wiki.libsdl.org/SDL3/README-ios、README-android、README-cmake、README-emscripten、README-platforms、SDL3 Discourse「SDL3 → Android/WebAssembly 指南」
- bgfx Web：bkaradzic/bgfx（WebGL emscripten 后端、PR#3282 wasm64）
- CMake 模板：github.com/zraz/sdl3-gpu-starter
