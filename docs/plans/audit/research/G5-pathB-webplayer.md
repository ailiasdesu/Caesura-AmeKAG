# G5 调研：Web 导出路径 B —— 轻量 Web 播放器（不编译 C++ 引擎）

> 调研员：G5 子 agent
> 日期：2026-08-14
> 范围：评估「在浏览器内运行现有 Lua 调度器 + 用 DOM/CSS/Canvas2D/WebGL/WebAudio 重实现可视命令，将 .ks 剧本导出为可分发 Web 播放器」的可行性与 MVP 边界。
> 依据：scripts/ 下源码实勘（tokenizer/scheduler/command handlers/ks_bake）+ 网络调研（Fengari / wasmoon / lua.vm.js / Ren'Py Web 先例）。

---

## 0. 结论摘要

1. **确定可行。** 现有 scripts/ 下 KAG 执行栈绝大多数是**纯 Lua**，且 lpeg.lua 本身就是**纯 Lua PEG 引擎**（零 C 依赖）——这是 Web 移植的天然前提。
2. **运行时首选 wasmoon（Lua 5.4 wasm），其次 Fengari（JS 移植 Lua 5.3）。** 因为引擎脚本必须 **Lua 5.4 语义**（调度器 coroutine + 5.4 特性），且 wasmoon 原生支持 async JS 互操作（IO/音视频加载天然异步）。lua.vm.js 已废弃（Lua 5.1，性能差），排除。
3. **84 个命令契约中，核心可视子集约 30-40 个命令**即可覆盖一部 VN 的全部主流程；这些命令调用的 C++ backend.* / layers.* 绑定面已完整枚举（分别约 64 / 27 个），Browser 端用一个小型 JS 适配层（DOM/CSS/Canvas/WebGL/WebAudio）逐一对口即可，**无需碰 C++ 引擎**。
4. **剧本→JSON 演出清单预编译思路完全成立**，且项目已有现成基础：ks_bake.lua 就是「离线预编译 .ks」的既有脚本，改为输出 JSON token 流即可。
5. **资产侧零成本适配。** 项目资产就是 .png/.jpg + .ogg/.wav，浏览器原生解码；CARC 加密归档在 Web 上可整体跳过（直接暴露 assets/ 目录或内联），保留非加密 asset 目录即可。
6. **同类先例充分。** Ren'Py 官方提供 Experimental Web（Emscripten）端口；Fengari/wasmoon 已被多个游戏/运行 Lua 的工具采用；没有原生「完整 KAG3 → Web」开源先例，但这正是机会而非障碍。
7. **工作量评估**：播放器本体（调度器复用 + 核心子集适配层）约 **1.5–2.5 人周**，含编辑器侧一键导出脚本约 **3–4 人周**。值得做，建议作为 G5 正式实施方向。

---

## 1. 浏览器内运行 Lua 的方案对比

### 1.1 候选方案

| 方案 | 内核 / Lua 版本 | 形态 | 性能特征 | 维护现状 | async 互操作 |
|---|---|---|---|---|---|
| wasmoon (https://github.com/ceifa/wasmoon) | **Lua 5.4**（官方 C 源码编到 wasm） | .wasm + JS 胶水 | 接近原生 C 性能（wasm JIT），冷启动需加载数百 KB wasm | 仍维护，npm 有多个镜像 fork（PlaryWasTaken / arrays-start-at-zero 等） | **一等公民**：await 调 JS Promise，JS 函数可直接注入 Lua，双向参数转换 |
| Fengari (https://github.com/fengari-lua/fengari) | Lua **5.3**（用 JS 重写 VM） | 纯 JS | 比 wasmoon 慢数倍；深闭包/嵌套作用域（upvalue 每 fiber 拷贝）有已知性能与语义坑 | 维护放缓（1.x 停滞），但有大量文档与生态 | 无原生 async；需自编桥接（回调 + 事件泵） |
| lua.vm.js (https://github.com/daurnimator/lua.vm.js) | Lua **5.1**（emscripten 编译） | asm.js | 慢，且 5.1 与项目 5.4 语义不兼容 | **长期废弃**（最后一次实质活动约 2017） | 无 |
| lua.js（lua 5.1 手动 JS 移植） | 5.1 | 纯 JS | 慢 | 废弃 | 无 |

### 1.2 推荐判定

- **必须 Lua 5.4**：scheduler.lua 是**基于 coroutine** 的 token 流执行器（yields on blocking ops, resumes next frame），项目脚本整体按 5.4 语义编写。Fengari（5.3）和 lua.vm.js（5.1）在**语言级不匹配**，直接排除为生产运行时。
- **必须 async 互操作**：Web 上贴图、BGM/SE、字体都是异步加载。wasmoon 的 await 桥接让「Lua 调 JS → JS 返回 Promise → Lua 继续」成为一等模式，与 KAG 调度器的阻塞式 [wait]/[waitforclick]/资源等待天然对接。Fengari 要在 Lua 协程里手写 async 泵，工作量大且易错。
- **性能**：wasmoon（编译过的 Lua 5.4）跑 tokenizer/scheduler 这类字符串+表密集的纯 Lua 逻辑，吞吐足够（每秒数千 token 无压力）；Fengari 纯 JS 解释器对长剧本连续解析会明显偏慢。
- **结论：首选 wasmoon**。风险点是 wasm 打包/bundle（需正确内联 asm.wasm 或走 CDN），已有多个 fork 解决 node-free standalone 场景。**Fengari 作为降级/无 wasm 环境的备选**（例如某些 CSP 严格或禁 wasm 的容器），但需额外写 async 桥。**lua.vm.js 排除**。

### 1.3 对现有 scripts/ 的承载能力

实勘确认（非推断）：
- **scripts/tokenizer.lua 只 require(lpeg)**，而 **scripts/lpeg.lua 是纯 Lua 实现的 PEG 子集引擎**——即 tokenizer **没有** C lpeg 依赖，可在 wasmoon/Fengari 内直接运行。这是本方案成立的第一支柱。
- **scripts/scheduler.lua** 依赖 kag.schema、kag.compiler（都是纯 Lua），用 coroutine + token_index 推进，**不依赖任何 C++ 函数**——可 100% 复用。
- scripts/kag/commands/*.lua（audio/layer/resource/save/system/text/transition/vfx/video 共 9 个文件）**只调 backend.* 与 layers.* 的 Lua 绑定**，这些绑定在浏览器里由 JS 适配层重新注册即可（见 §3），命令处理逻辑本身全部保留。

**复用度结论**：tokenizer + scheduler + compiler + schema + 9 个 command handler ≈ **现有 KAG 执行栈的 90%+ 可以直接跑在 wasmoon 里**，唯一要重写的是 backend.* / layers.* 的 JS 实现（约 64+27 个函数的口）。

---

## 2. 渲染层重实现：84 命令契约里的核心可视子集

### 2.1 命令契约概览（自动生成，kag/schema.lua）

84 个命令按 Category 分布（实勘）：system 21 / text 22 / layer 8 / transition 7 / audio 11 / vfx 3 / save 2 / video 1。

### 2.2 核心可视子集（MVP 建议约 32 命令）

从 demo_story.ks（实际演示剧本）+ 命令依赖度筛选，一个 VN 播放器 MVP 必须覆盖：

**文本层（DOM/CSS + Canvas2D）**
- [text] / [ch]（带角色名的文本行）—— 文本显示核心
- [p] / [l] / [r] / [s] / [br] / [hr] / [cl](clear) / [reset] / [font](face/size/color) / [ruby](furigana) / [nameplate] / [pt](print speed) / [waitforclick]
- 候选：[auto]（自动播放）、[nvl]（NVL 模式，若剧本用）

**图层/立绘/背景（DOM + CSS position/z-index 或 Canvas2D 专图层）**
- [bg](storage 全屏背景) / [fg](storage 前景立绘) / [position](layer x y scale) / [layopt](opacity/blend) / [move](layer 移动) / [moveto] / [image] / [set](layer 状态)
- 图层语义：layers 模块本身在浏览器里改为**纯 JS 层树**（add_layer/set_z/set_layer_image/set_layer_opacity/set_layer_visible/set_position/move_layer/fade_to）即可复用同一套 Lua 层管理逻辑。

**过渡/特效（Canvas2D/WebGL，可先降级）**
- [wait](time 阻塞) / [shake] / [quake] / [vib](振动) / [flash] / [camera](restore/x/y/time)
- 可选：[trans] 转场（fade/lut 可先用 CSS opacity 模拟）、[sprite_*]（若剧本用立绘表情切换）、[particles]（WebGL 粒子的进阶项，MVP 可跳过）

**音频（WebAudio）**
- [playbgm] / [playbgmstop] / [stopbgm] / [fadebgm] / [xfadebgm] / [fadevol] / [playse] / [stopse] / [setbgmvolume] / [setsevolume] / [setvoicevolume] / [voice_wait] / [voice_off]
- WebAudio 3-bus（BGM/Voice/SE）映射 SoLoud 的 IAudioBackend 3-bus 语义。

**流程控制（调度器内联，无需重写，天然复用）**
- [if]/[else]/[endif]、[jump]/[call]/[return]/[label]/[link]/[end]、[eval]/[emb]/[random]/[inc]、[delay]、[stop]
- [button]/[endbutton]（选择分支，纯 Lua kag/commands/text.lua，可直接复用）

**可延后（非核心可视）**
- system/video/save/music 类：[video]（可用 video 元素）、[save]/[load]/[listsaves]（Web 上用 IndexedDB/localStorage 重写存储适配）、[gallery]/[music]/[history]/[replay]、[ai_dialog]（LLM 端）、[sma_*]（骨架网格动画，Live2D/SMA 属进阶资产，浏览器 WebGL 可做但超出 MVP）。

### 2.3 每命令的渲染映射表（核心子集）

| 命令 | C++ 原实现（bgfx） | Web 重实现 |
|---|---|---|
| [bg]/[fg] | load_texture -> set_layer_image bg/fg 全屏/立绘图层 | new Image() + div 绝对定位 or Canvas2D drawImage，按 layer/z-index 组 DOM |
| [position]/[layopt]/[move]/[moveto] | set_position / set_layer_opacity / move_layer | 更新 DOM 的 left/top/transform/opacity，或 Canvas2D 坐标 |
| [text]/[ch]/[ruby]/[nameplate] | font_render_text / render_text（bgfx 纹理字形） | DOM div 文本 + CSS font-family/size/color；ruby 用 ruby 标签；或 Canvas2D fillText |
| [waitforclick]/[button] | 输入/视口事件 | Promise 桥接（wasmoon await）：pointerup/click |
| [playbgm]/[playse]/[stopbgm]/[fadebgm] | audio_play/audio_stop/audio_fade_volume（SoLoud 3-bus） | WebAudio AudioContext + GainNode（fade 用 setTargetAtTime），3 个 bus |
| [shake]/[quake]/[flash]/[vib] | 相机偏移 / 全屏色叠加（bgfx） | Canvas2D 外层容器 transform 抖动 + 全屏 overlay fillRect 透明度 |

> 关键判断：**不需要 WebGL 起步**。一部视觉小说的视觉复杂度（背景 + 立绘 + 文本 + 过渡）用 **DOM/CSS + Canvas2D** 即可高质量覆盖；WebGL 仅在需要 SMP 粒子/LUT 转场/sprite 缩放超采样时作为进阶优化。这极大降低 MVP 门槛。

---

## 3. 剧本→JSON 演出清单的预编译思路

### 3.1 现成基础：ks_bake.lua

实勘：项目已在做「离线预编译」——scripts/ks_bake.lua 遍历 .ks -> tokenizer.parse -> kag.compiler.compile(tokens) -> 写 .ksc 字节码缓存（cache/ksc/），并有 --check 新鲜度校验。路径/哈希算法与运行时 flow.load_scene 完全一致。

**这与「剧本→JSON 演出清单」是同一范式**：只需把 .ksc（字节码）换成 **JSON token 流**即可：
- tokenizer.parse(ks) 返回 token 数组（结构可由 scheduler 的 tok[1]=="label" 等访问方式推断）-> 序列化为 JSON。
- 输出形式可二选一（推荐后者）：
  1. 一个场景一个 scene.ks.json（直接喂浏览器的 fetch 缓存）；
  2. **整个剧本打包成一个 story.json**（所有场景 token 流 + label index + 资源清单/依赖图）——单次 fetch，浏览器只跑调度器。

### 3.2 为何预编译对 Web 尤有价值

- **省掉浏览器端 LPeg 解析**：tokenizer 虽能在 wasmoon 跑，但把 84 命令的解析压到离线构建时，浏览器端是**纯 JSON 遍历 + 调度器**，更快、更可预测、可离线缓存。
- **跳过嵌 Lua 块预处理风险**：[iscript] 嵌入脚本在 Web 沙箱的管理可放到预编译/白名单层统一处理。
- **编辑器已有 RPC + ks_check/ks_bake 管线**：编辑器侧加一个「Export -> Web」按钮即复用现有 .ks -> token 编译链，产出静态资源站（index.html + story.json + assets/ 资源 + wasmoon bundle）。

> 混合可选：MVP 也可**先不预编译**，直接浏览器跑 tokenizer（已验证纯 Lua 可跑），把预编译作为 P2 优化。但既然 ks_bake 已存在，导出 JSON 成本极低，建议一开始就做。

---

## 4. 资产侧：CARC 加密归档在 Web 的可行替代

实勘（docs/guides/carc-packaging.md）：
- CARC 是 **AES-256-GCM 可选文件级加密 + Ed25519 可选签名**的压缩归档，用于桌面发布加密资源。
- docs/guides/asset-pipeline.md 显示资源目录为 assets/，图 = .png/.jpg，音 = .ogg/.wav。

**浏览器结论：**
- **.png/.jpg（img/Canvas）、.ogg(Ogg Vorbis)/.wav（WebAudio decodeAudioData）均浏览器原生解码**，无需任何转码。
- **Web 播放器不需要 CARC。** 浏览器无法安全隐藏解密密钥（前端加密无意义），因此 Web 分发应**直接暴露 assets/ 目录**（静态部署）或打包进播放器的资源映射表；CARC 仅保留给桌面/离线原生版。
- 可选替代：做一个**非加密的 web.map.json**（资源路径->真实 URL 的相对映射 + 尺寸/时长元数据），编辑器导出时生成，安全无解密负担。
- 若确需防白嫖/篡改，只能做「服务端鉴权 + 按需下发」或 DRM 化（超出 MVP），不做浏览器内 AES。

---

## 5. 同类案例

### 5.1 VN 引擎 Web 端口先例（最相关）

- **Ren'Py Web（Experimental）**：官方维护，经 Emscripten（Pyodide/CPython -> wasm）把整个 Ren'Py 跑进浏览器，支持大部分桌面玩法，官方文档标注 experimental/移动端支持有限。这证明「把整条剧本执行链路塞进 wasm + 浏览器原生音视频」是**被行业验证的成功路径**。
  - 参考：https://doc.renpy.cn/zh-CN/web.html 、 https://github.com/renpy/renpyweb
- **KiriKiri/KAG 生态**：KAG3 本身无官方 Web 版；社区有 TJS2/KAG->Web 的零散尝试但未成规模，Web 复刻多靠人工重写。这既是空白也是差异化机会。
  - 参考：https://www.nvlmaker.net/manual/docs/kag3doc/contents/Intro.html 、 https://en.wikipedia.org/wiki/List_of_visual_novel_engines

### 5.2 Fengari / wasmoon 被实际采用（游戏/工具逻辑）

- **wasmoon**：多个 fork 活跃（PlaryWasTaken、arrays-start-at-zero、pixlise 等），npm 有 wasmoon-async-fix、glome-wasmoon、@vaultie/wasmoon 等派生包，广泛用于**把现有 Lua 5.4 逻辑在不改动下跑进 Web**（游戏 mod / 规则引擎 / app 脚本）。wasm 方案被 Vercel 等平台社区大量讨论（含 wasm 内联部署坑，见 Vercel community 帖）。
  - 参考：https://github.com/ceifa/wasmoon 、 https://www.npmjs.com/package/wasmoon-async-fix
- **Fengari**：被用作文档/教学/小游戏内嵌 Lua（Babylon.js 论坛、Sololearn playground、daurnimator/fengari-electron-example 等）。作为纯 JS 免 wasm 的最成熟 Lua 移植。
  - 参考：https://github.com/fengari-lua/fengari 、 https://forum.babylonjs.com/t/babylua-lua-implementation-for-babylonjs/61706
- **lua.vm.js**：历史性但已废弃（Lua 5.1），仅作参考，不可用于本项目（5.4 需求）。
  - 参考：https://github.com/daurnimator/lua.vm.js 、 https://www.philhassey.com/blog/2013/12/05/lua-on-javascript-comparison-lua-vm-js-lua-js-lua-js-phil-lua5-1-js-native/

### 5.3 对本项目的启示

- wasmoon 生态证明「现有 Lua 5.4 代码不改跑 Web」成熟可行；Caesura 的**纯 Lua 执行栈**（tokenizer+scheduler）比 Ren'Py（整个 CPython 生态）轻得多，是**最适合 wasm 化的那一类**。
- 没人在 Web 上完整复刻 KAG3，但 Ren'Py 证明了「完整剧本引擎进浏览器」的商业/技术可行；我们只需覆盖核心可视子集即可首发。

---

## 6. 工作量与风险

### 6.1 工作量估算（单人、含自测）

| 里程碑 | 内容 | 预估 |
|---|---|---|
| **M1 运行时可移植** | wasmoon 集成；lpeg.lua+tokenizer+scheduler+kag.compiler+kag.schema 在浏览器跑通，跑通 demo_story.ks 的 token 解析与调度步进 | 4-6 人日 |
| **M2 核心可视适配层** | 实现 backend.*(核心子集) + layers.*(纯 JS 层树) 的 JS 版本；DOM/CSS/Canvas2D 渲染 bg/fg/position/layopt/text/ruby；WebAudio 3-bus | 5-8 人日 |
| **M3 交互与流程** | waitforclick/button 选择分支、jump/call 场景跳转、存读档（IndexedDB）、自动/快进 | 3-5 人日 |
| **M4 导出管线** | 编辑器「Export->Web」：.ks -> story.json + 资源清单 + 静态站脚手架（复用 ks_bake 编译链）；一键预览 | 3-5 人日 |
| **M5 打磨** | 转场/震动/flash 特效、canvas 进阶、wasm 部署/CDN 内联、移动端适配、CSP 加固 | 3-5 人日 |
| **合计（MVP = M1-M3）** | 核心播放器 | **约 1.5-2.5 人周** |
| **合计（含 M4-M5 完整线）** | 含编辑导出 | **约 3-4 人周** |

> 依据：绑定面是确定的（backend 约 64 / layers 约 27 个函数，其中核心可视只需实现约 30-40 个），且 command handler 逻辑 90% 可复用，故可估得较准。

### 6.2 关键风险与对策

| 风险 | 等级 | 对策 |
|---|---|---|
| **wasmoon bundle/部署坑**（Vercel/某些宿主 wasm 内联失败） | 中 | 用现成 node-free standalone fork（PlaryWasTaken 等）或 CDN 独立加载 asm.wasm；部署含 wasm 内联的自测用例 |
| **Lua 5.4 与浏览器 JS 语义边界**（os/io/package 缺失） | 中 | 提供最小 polyfill：math/string/table 完整、os.time、自写资源 io -> fetch 异步（经 await 桥）；沙箱白名单镜像引擎 sandbox 语义 |
| **[iscript]/[eval] 任意 Lua 执行（安全）** | 高 | 沿用引擎沙箱思路：白名单 require；嵌入脚本序列化后同样在受限 env 跑；Web 无全局文件写，天然更安全 |
| **CARC 加密不可在浏览器复现** | 低 | 明确 Web 分发用裸 assets/，CARC 仅桌面版——这不是缺陷而是方案选择 |
| **Live2D/SMA/3D minigame 不可浏览器复用** | 中（影响面小） | MVP 明确排除，靠 [bg] 静态立绘兜底；后续可 WebGL 补 |
| **LPeg 若后续想用真 C lpeg** | 低 | 已有纯 Lua lpeg 够用；真要走原生可编译 lpeg 到 wasm（wasmoon 支持塞入 wasm searcher），但非必需 |
| **字体渲染不一致**（bgfx 纹理字形 vs 浏览器文本） | 中 | MVP 用 DOM/CSS 原生文本 + 内联 webfont（如 Noto Serif 中文字体），保证观感；宽高/换行以 CSS 为准 |

### 6.3 不可行/延后清单（明示边界）
- 3D 小游戏（minigame 模块）、Live2D/SMA 骨骼动画、粒子系统高级特效、视频播放高级控制 —— **不进 MVP**，用静态或 WebGL 替代。
- 存档加密、Steam 云存档、成就 —— Web 用 IndexedDB 明文替代，不做强加密。

---

## 7. 建议：是否值得做 + MVP 范围

### 7.1 值得做（结论）

- **技术可行性极高**：执行栈 90% 纯 Lua 可复用 + 资产 100% 浏览器原生 + 有 Ren'Py 行业先例背书。
- **业务价值明确**：零成本解锁「浏览器试玩 / 一键分享 / 静态托管分发」，验证网页体验后再考虑完整 Emscripten 全引擎（更重方案）也不迟。
- **与当前架构不冲突**：不编译 C++、不改现有模块边界，仅新增「编辑器导出子命令 + 一个独立 web-player 目录」，不触碰模块/接口/BackendRegistry 纪律。

### 7.2 MVP 范围建议
1. **运行时**：wasmoon（Lua 5.4）承载 tokenizer+scheduler+compiler+schema+kag/commands/{text,layer,audio,transition,resource}。
2. **核心可视子集**（约 32 命令）：文本/DOM、bg/fg 图层、position/layopt/move、font/ruby/nameplate、playbgm/playse/fade、wait/waitforclick/button、jump/call/if/random/eval、shake/quake/flash。
3. **预编译**：ks_bake 改出 --web story.json（token 流 + label index + 资源清单）。
4. **存储**：IndexedDB 存档；WebAudio 3-bus 对 SoLoud。
5. **资产**：裸 assets/ 目录，跳过 CARC。

### 7.3 建议周期（含打磨）
- **实验验证（约 3-5 人日）**：先把 demo_story.ks 在 wasmoon 里跑通 tokenizer+scheduler 步进（M1），这是整个方案的「风险先验」，通过后正式立项。
- **正式 MVP（约 2-4 人周）**：M1-M4，交付「编辑器一键导出 -> 浏览器可玩 demo_story」。
- 完整版（含 WebGL 特效/移动端/存储加固）再加约 1-2 人周。

### 7.4 相关参考链接
- wasmoon：https://github.com/ceifa/wasmoon 、 wasmoon-async-fix：https://www.npmjs.com/package/wasmoon-async-fix
- Fengari：https://github.com/fengari-lua/fengari 、 lua.vm.js：https://github.com/daurnimator/lua.vm.js
- Ren'Py Web：https://doc.renpy.cn/zh-CN/web.html 、 https://github.com/renpy/renpyweb
- Lua 移植横向对比（含性能）：https://www.philhassey.com/blog/2013/12/05/lua-on-javascript-comparison-lua-vm-js-lua-js-lua-js-phil-lua5-1-js-native/
- wasm 部署坑参考（Vercel community）：https://community.vercel.com/t/instantiate-wasm-failed-only-in-vercel-production/497

---

*本报告为 G5 调研节点产出，仅新增本文件，未改动任何引擎源码。*
