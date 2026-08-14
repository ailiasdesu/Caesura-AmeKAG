# G5 — Web 导出路径调研（决策收敛）

> 状态：**调研完成（round 30）**。三路径并行子 agent 调研，产出：
> - [G5-pathA-emscripten.md](./G5-pathA-emscripten.md) — 原生引擎 emscripten/WASM 编译（244 行）
> - [G5-pathB-webplayer.md](./G5-pathB-webplayer.md) — 轻量 Web 播放器（wasmoon 复用 Lua 调度器）（48 行，实勘级）
> - [G5-pathC-ecosystem.md](./G5-pathC-ecosystem.md) — 生态对比 + 移动运行时备选（236 行）

## 决策：路径 B（轻量 Web 播放器）作为近期实施方向，路径 A 中期 PoC 跟进

**核心逻辑**：Caesura 的 KAG 执行栈（tokenizer/scheduler/compiler/schema/9 个命令 handler）
**90%+ 是纯 Lua 且零 C 依赖**（lpeg.lua 是纯 Lua PEG 引擎；scheduler 基于 coroutine 不碰 C++ 绑定），
用 wasmoon（Lua 5.4 wasm，async 互操作一等公民）即可原样运行；唯一要重写的是
backend.* / layers.* 的浏览器 JS 适配层（约 64+27 个函数的口，但 VN 核心可视子集仅 30-40 命令）。

| 路径 | 方案 | 工作量 | 风险 | 结论 |
|---|---|---|---|---|
| **B** | 轻量 Web 播放器（wasmoon + 调度器复用 + JS 适配层） | 1.5–2.5 人周播放器 / 3–4 人周全流程 | 低：纯前端，不碰 C++；CARC 可跳过暴露 assets/ | **首选（近期）**：最大杠杆/成本比 |
| **A** | emscripten 编译原生引擎 | 6–10 周 Demo / 12–18 周生产级 | 高：渲染线程结构性受限（bgfx wasm 单线程）、COOP/COEP 托管限制、Live2D 需 Cubism Web 独立产品线、ffmpeg 缺失 | 中期演进：先做 1–2 周 bgfx-wasm 单线程渲染 PoC 验证 |
| **C** | 移动运行时（SDL3 Android/iOS） | > 路径 A | 中高：iOS 审核、CARC 密钥需 Keystore/Keychain 注入、JNI 宿主集成 | 第二分发曲线，排在 A 之后 |

## 仓库现状基线（round 30 实测）

- Lua 脚本：76 文件 / 23,187 行；kag 命令模块 9 个（audio/layer/resource/save/system/text/transition/vfx/video）
- tokenizer 依赖：仅 lpeg.lua（纯 Lua PEG 子集引擎，零 C 依赖）——Web 移植第一支柱
- scheduler：基于 coroutine 的 token 流执行器，100% 纯 Lua，可直接复用
- 资产：14 文件 / 32.80 MB（png×3, ogg×3, wav×3, otf×1, lua×3, txt×1）——浏览器原生解码，零成本适配
- 脚本→JSON 预编译基础已存在：ks_bake.lua（离线预编译 .ks，改输出 JSON token 流即可）
- 渲染：bgfx GPU（多后端）——Web 播放器用 DOM/CSS/Canvas/WebAudio 重实现核心可视子集

## 关键调研发现（三路径共识）

1. **Ren'Py web 是路径 A 的行业标杆**（Emscripten→WASM，IDBFS 存档、渐进下载），但其代价 = 多线程不可用、Live2D 砍掉、视频换浏览器播放器——与 A 报告的风险清单一致。
2. **WebGAL（10k+ stars）走 Web-first 重写**，中文社区活跃；TyranoScript 验证了移动 Web VN 市场；Monogatari 老牌纯 Web（IndexedDB 存档）。
3. **itch.io HTML5 是 VN Web 主阵地**，zip 约 1000 文件数上限——CARC 单归档天然规避（架构红利）。
4. **wasmoon（Lua 5.4 wasm）为运行时首选**：async 互操作一等公民，与 KAG 阻塞式 [wait]/[waitforclick] 天然对接；Fengari（5.3）语言级不匹配、lua.vm.js（5.1）废弃。
5. **SDL3 emscripten 比 SDL2 成熟**，但渲染必须主线程 + 线程版需 COOP/COEP + SharedArrayBuffer。
6. 中文字体需 woff2 子集化（cn-font-split/pyftsubset）；WebAudio 需手势解锁。

## MVP 建议（路径 B）

1. **运行时**：wasmoon（Lua 5.4）加载 scripts/ 全套 KAG 执行栈
2. **适配层**：JS 实现 backend.* / layers.* 核心子集（bg 全屏图、chara 立绘、文本层、BGM/SE、[wait]/[jump]/[if] 流程）
3. **导出链**：ks_bake.lua 扩展输出 JSON token 流 + 资产清单（png/ogg/woff2）
4. **存档**：IndexedDB；**分发**：itch.io HTML5 zip / 静态页嵌入
5. **验证路径**：demo 剧本（4 个 .ks）导出 → 浏览器可玩

## 落地路线（映射到 ROADMAP-100.md）

- **Phase C（round 41-55）**：G5 实施开始——wasmoon 播放器脚手架 + tokenizer/scheduler 复用验证 + 核心子集适配层
- 中期（G5 后续）：bgfx-wasm 单线程渲染 PoC（决定路径 A 是否值得全投）
- 移动端（G6 之后备选）：SDL3 Android/iOS 模板调研已就绪（zraz/sdl3-gpu-starter 等）

---
*收敛：round 30，三子 agent 并行调研 + 主 agent 决策*
