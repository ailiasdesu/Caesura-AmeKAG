# Caesura (AmeKAG) 引擎全量性能基准与架构测算规范

> **版本**：v1.0.0-rc.1 / 2026-08-25 全面更新  
> **适用平台**：Windows (Direct3D 11/12, Vulkan), Linux/WSL (Vulkan, OpenGL), macOS/iOS (Metal), Android (OpenGLES, Vulkan), Web (WebAssembly / Wasmoon)  
> **入口套件**：`bash scripts/run_benchmarks.sh --web` | `CaesuraTests.exe -tc="Perf:*"` | `python scripts/count_coupling.py`  
> **核心原则**：所有热路径（分词、调度、图层遍历、音频混音、字体渲染、存档序列化）均具备确定性性能守卫与宽松 CI 守护上限，严防性能回退。

---

## 1. 架构性能设计总览

Caesura (AmeKAG) 作为下一代高保真 Visual Novel / Galgame 引擎，在 60 FPS (16.67 ms/frame) 与 120 FPS (8.33 ms/frame) 的实时渲染约束下，对 CPU 热循环与内存开销制定了严格的分级预算体系：

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       16.67 ms (60 FPS) Frame Budget                        │
├──────────────────┬──────────────────┬─────────────────────┬─────────────────┤
│ Lua Logic & Walk │ Render Device &  │ GPU Render & Blend  │ Headroom Margin │
│ (< 500 μs / 3%)  │ Dispatch (< 1ms) │ (5 ~ 8 ms / 30-50%) │ (> 40% Safety)  │
└──────────────────┴──────────────────┴─────────────────────┴─────────────────┘
```

| 核心子系统 | 关键热路径技术选型 | 目标帧时间 / 吞吐上限 | 典型实测值 | 安全余量 (Headroom) |
|---|---|---|---|:---:|
| **1. 字体图集与排版** | FreeType 2048² 动态图集 + 4096² CJK 预烘焙 + `MessageLayerCache` | < 1.0 s / 4096 瓦片核算 | **1.0 ~ 3.0 ms** | > 99.7% |
| **2. 音频并发混音** | SoLoud 3-Bus (BGM/Voice/SE) + 32-Entry LRU + 句柄环形复用 | < 2.0 s / 80k 次分配释放 | **63 ~ 123 ms** | > 93.8% |
| **3. 脚本分词与调度** | LPeg 紧凑分词器 + AOT 预编译 + 协程调度器 + Label 哈希索引 | < 10.0 s / 9600 Token 场景 | **946 ms 解析 / 42 ms 推进** | > 90.5% |
| **4. Backlog 与序列化** | 500 页环形上限 + 增量差异备份 + JSON 紧凑快照 (<50 KB) | < 4096 KB 堆增量 | **934.7 KB** | > 77.2% |
| **5. 图层遍历与渲染** | 7 角色场景图 DFS + 对象池化 (`pool.lua`) + 2048 Quad 批处理 | < 500 μs / 帧遍历 | **274 ~ 350 μs / 帧** | > 45.0% |

---

## 2. 子系统 1：字体字形图集光栅化与查找速度

### 2.1 架构与技术实现 (`src/render/TextRenderer.h`, `TextRenderer.cpp`)
- **FreeType 2 动态图集 (`loadTTF()`)**：
  - 初始化 FreeType 2 面孔（默认 24px/32px），在内存中动态维护单张 2048×2048 RGBA8 纹理图集（16 MB 显存）。
  - 采用行扫描打包算法（`m_ttf->penX`, `m_ttf->penY`, `m_ttf->maxRowH`），按需光栅化 Unicode 范围：ASCII (32–126)、标点符号 (0x2000–0x206F)、CJK 符号 (0x3000–0x303F)、平假名 (0x3040–0x309F)、片假名 (0x30A0–0x30FF)、全角字符 (0xFF00–0xFFEF) 及 CJK 统一表意文字 (0x4E00–0x9FFF)。
- **预烘焙 CJK 静态图集 (`loadCjkAtlas()`)**：
  - 针对大字符集发行版，支持加载 4096×4096 RGBA8 预渲染静态图集（64 MB 显存）与二进制元数据表，将常用 7000+ CJK 汉字以 `CjkGlyph` 结构体（`{x, y, w, h, advance, offsetX, offsetY}`）常驻内存。
- **四级字形查找级联 (`getTTFGlyph()`)**：
  1. 动态 TTF 内存哈希表：`m_ttf->glyphs[codepoint]`
  2. 预烘焙 CJK 静态图集哈希表：`m_cjkGlyphs[codepoint]`
  3. 内置 ASCII 8×16 等宽位图字体回退
  4. Unicode 缺失字符替换符号 `0xFFFD`
- **对话框批缓存与脏矩形增量更新 (`MessageLayerCache`)**：
  - 支持多达 2048 字形的常驻动态顶点/索引缓冲（`bgfx::DynamicVertexBufferHandle`, `bgfx::DynamicIndexBufferHandle`）。
  - `computeDirtyRange()` 针对打字机式字符流式输出，只更新新增字形顶点（`dirtyStart` 到 `dirtyEnd`），完全避免每一帧重复 UTF-8 解码与全文本重新光栅化排版。

### 2.2 性能实测与守卫指标
- **4096 瓦片巨型图集预扫描核算 (`test_scale_stress.lua` Section A)**：
  - 4096×4096 图集划分为 4096 个 64×64 瓦片（16,777,216 纹素），执行完整空间预算分配与边界核算。
  - **实测耗时**：**1.0 ~ 3.0 ms**
  - **守护上限**：`< 1.0 s`
  - **安全余量**：`> 99.7%`

---

## 3. 子系统 2：音频 3-Bus 混音器与高并发句柄池

### 3.1 架构与技术实现 (`src/audio/SoLoudAudioEngine.h`, `SoLoudAudioEngine.cpp`)
- **3-Bus 独立总线拓扑**：
  - 采用 SoLoud 音频中间件构建三级独立总线：`m_bgmBus` (背景音乐)、`m_voiceBus` (角色语音)、`m_seBus` (音效与环境音)。
  - 各总线独立控制音量、淡入淡出 (`fadeVolume()`)、暂停与音效处理，避免全局音量互相干扰。
- **智能语音避让 Ducking 机制**：
  - 维护最大 4 路语音并发池（`kVoicePoolSize = 4`）。当角色语音触发播放时，BGM 总线自动降音避让；语音播毕后，BGM 在 0.3s 内平滑淡入恢复原音量。自然播放结束由 `m_voiceCompletionsPending` 帧末回收。
- **32 项 O(1) LRU 波形缓存 (`m_waveCache`)**：
  - 内部使用 `std::list<std::string>` (`m_waveLRU`) + `std::unordered_map` 迭代器索引 (`m_waveLRUMap`) 实现 O(1) 访问与剔除。
  - 严格锁定正在播放的音频源（`m_soloud.countAudioSource() > 0`），绝不剔除活跃声源。
- **句柄生命周期与环形复用**：
  - 活跃音效句柄记录于 `m_activeSE`，淡出退役句柄记录于 `m_retiringBGM` / `m_retiringVoice`。
  - 每帧 `cullFinishedHandles()` 扫除已停止句柄，并清理 `m_rawWaveCache` 中的临时 PCM 内存。

### 3.2 性能实测与守卫指标
- **80,000 次高频句柄分配与释放流转 (`test_scale_stress.lua` Section B)**：
  - 设定 128 最大并发池，执行 80,000 次连续 `alloc()` 与 `release_oldest()` 压力测试。
  - **实测耗时**：**63.0 ~ 123.0 ms**
  - **复用效率**：79,872 次句柄成功重用，活跃句柄数稳定在 120，单调递增 ID 严格封顶于 129（零句柄 ID 逃逸）。
  - **守护上限**：`< 2.0 s`
  - **安全余量**：`> 93.8%`

---

## 4. 子系统 3：大规模脚本分词、AOT 编译与调度吞吐

### 4.1 架构与技术实现 (`scripts/tokenizer.lua`, `scripts/kag/compiler.lua`, `scripts/scheduler.lua`)
- **LPeg 紧凑无回溯分词器**：
  - 基于 LPeg (Parsing Expression Grammar) 构建解析器，覆盖全部 167 个 KAG3 标签及 Neo-Genesis 扩展指令，具备线性 O(N) 解析复杂度，杜绝正则表达式指数级回溯爆炸。
- **AOT 预编译与指令元表 (`compiler.compile()`)**：
  - 脚本加载时将原始 Token 流一次性编译为 `_compiled` 附着结构：
    - `_compiled.exprs`：预翻译 Lua 表达式
    - `_compiled.params`：预校验与类型提升的参数表
    - `_compiled.handlers`：直接函数指针绑定，消除运行时反射
    - `_compiled.flow` / `_compiled.labels`：跳转分支目标静态索引
- **协程调度热循环与 O(1) 跳转索引**：
  - 调度器 `scheduler.run()` 在 Lua 协程内以紧凑循环执行编译后指令，仅在帧边界指令（`[p]`, `[wait]`, 动画阻塞）产生 `coroutine.yield()`。
  - `build_label_index()` 将全文标签构建为哈希表，`find_label()` 达到 O(1) 跳转寻址，彻底取代 O(N) 线性遍历。

### 4.2 性能实测与守卫指标
- **万词大场景分词与连续推进 (`test_scale_stress.lua` Section C)**：
  - 4800 行 `[ch][p]` 剧本（~388 KB 源码，9600 Token）。
  - **分词耗时**：**946.0 ~ 1474.0 ms**（守护上限 `< 10.0 s`，安全余量 `> 85.3%`）。
  - **调度推进**：9601 帧连续状态推进仅耗时 **42.0 ~ 62.0 ms**（**155 ~ 229 tok/ms**，即 15.5万~22.9万 token/s，守护上限 `< 10.0 s`）。
- **标准 2000 行纯文本基准 (`test_benchmark.lua`)**：
  - 4000 Token 解析耗时 **249 ~ 269 ms** (**62.25 ~ 67.25 ms / 1000 tok**，守护上限 `< 3.0 s`)。
  - 4001 次协程唤醒耗时 **22.0 ~ 24.0 ms**。
- **编译态指令极速分发 (`test_bench_dispatch.lua`)**：
  - 2000 个编译态 `[ch]` 指令热循环分发耗时 **6.0 ms**，等效吞吐达 **333,333 tokens/sec**。
- **Label 哈希索引查找 (`test_label_bench.lua`)**：
  - 1500 个标签构建与 3000 次查找，哈希查找相比线性查找提速 **> 300×**。
- **3000 行嵌套条件叙事流翻译 (`test_scale_stress.lua` Section E)**：
  - 3000 行含嵌套三元表达式与空值合并运算符的复杂逻辑（408.9 KB），翻译耗时 **1226.0 ms** (**408.7 μs / 行**，守护上限 `< 10.0 s`）。

---

## 5. 子系统 4：Backlog 内存控制与增量序列化

### 5.1 架构与技术实现 (`scripts/kag/commands/text.lua`, `scripts/kag/commands/save.lua`, `scripts/history_ui.lua`)
- **500 页桌面级环形缓冲 (`TextCommands.push_backlog()`)**：
  - 每一条对话记录结构体包含：`{name, text, voice, time, timestamp, scene, token_index, src}`。
  - 强制执行 `ctx.backlog_max or 500` 环形上限，超额时通过 `table.remove(ctx.backlog, 1)` 弹出最旧记录，防止长篇阅读导致 Lua 虚拟机内存无限增长。
- **轻量化增量存档序列化 (`save.lua`)**：
  - 存档写入时仅截取最近 100 条记录（`math.max(1, #ctx.backlog - 99)`）写入 JSON 存档文件，使单次存档体积严格受控在 **< 50 KB**。
  - 读档恢复时重新应用 500 条上限校验，防止恶意篡改存档导致 UI 遍历死锁。

### 5.2 性能实测与守卫指标
- **500 页超长历史记录内存增量 (`test_scale_stress.lua` Section D)**：
  - 累积 500 页历史记录（共计 2500 条复杂角色对话数据与坐标快照）。
  - **Lua 堆增量**：**934.7 KB**（GC 采样：起始 224.5 KB → 最终 1159.2 KB）。
  - **守护上限**：`< 4096.0 KB` (4 MB)。
  - **安全余量**：`> 77.2%`。

---

## 6. 子系统 5：帧渲染时间、CPU 分发与骨骼蒙皮预算

### 6.1 架构与技术实现 (`scripts/layers.lua`, `src/render/BgfxQuadBatch.cpp`, `src/render/SmaSkinner.h`)
- **Lua 7 图层场景树 DFS 遍历 (`layers.render()`)**：
  - 场景图包含 7 大标准图层角色：`base` (背景), `layer0` (立绘0), `layer1` (立绘1), `fore` (前景), `ui` (界面), `message` (文本), `effect` (特效)。
  - 逐帧执行深度优先遍历、Z-Order 排序、RTT 视口映射、脏矩形裁剪、震屏偏移叠加。
  - 采用 `pool.lua` 循环复用批处理指令表，做到每帧渲染遍历 **0 新增垃圾回收分配 (Zero-GC Churn)**。
- **C++ 动态 Quad 批处理 (`BgfxQuadBatch`)**：
  - 内部常驻 2048-Quad 动态顶点/索引缓冲，同一纹理与混合状态的绘制自动合并为单次 GPU Draw Call。
- **SMA 骨骼蒙皮双路径架构 (`SmaSkinner.h`)**：
  - **CPU 软蒙皮**：纯 C++ 纯函数实现，对双骨骼权重进行线性插值与世界变换矩阵计算。
  - **GPU Compute Shader 蒙皮**：通过 `packBonePoses()` 将骨骼变换打包为 `vec4(cos*scale, sin*scale, ox, oy)` 传入计算管线。

### 6.2 性能实测与守卫指标
- **每帧图层遍历开销 (`test_frame_bench.lua`)**：
  - 5 个全屏活跃图层连续遍历 5000 帧。
  - **实测均值**：**274.6 ~ 350.0 μs / 帧**。
  - **守护上限**：`< 500.0 μs / 帧`。
  - **预算占比**：仅占 60 FPS 单帧预算 (16.67 ms) 的 **1.6% ~ 2.1%**，为 GPU 渲染与复杂逻辑预留超过 **97%** 裕量。
- **C++ 8k 顶点软蒙皮吞吐 (`test_perf_bench.cpp` / `test_render_integration.cpp`)**：
  - 8192 顶点、64 骨骼网格蒙皮计算。
  - **CPU 软蒙皮实测**：**0.75 ~ 0.92 ms / 帧**（守护上限 `< 10.0 ms`）。
  - **GPU Compute 蒙皮**：**~0.08 ms / 帧**（相比 CPU 实现提速 **~11.5× ~ 15.8×**）。
- **C++ Lua 虚拟机热路径操作 (`test_perf_bench.cpp`)**：
  - 10,000 次字符串格式化与表格追加：**19.05 ms**（守护上限 `< 800.0 ms`）。
  - 10,000 次表格字段访问：**0.50 ms**（守护上限 `< 400.0 ms`）。
- **小游戏 200 物体碰撞检测 (`test_mini_collision.cpp`)**：
  - 采用 Sweep-and-Prune (SAP) 算法与稀疏集合，200 个 AABB 包围盒碰撞检测保持在 O(N log N) 极低开销，无 O(N²) 退化。

---

## 7. Web 播放器 (Wasmoon Wasm) 性能基准

针对 Web/HTML5 端，Caesura Web Player 运行于 Wasmoon WebAssembly 沙箱环境：

| Web 性能维度 | 测试用例 | 守护上限 / 目标 | 实测基线数值 | 状态 |
|---|---|---|---|:---:|
| **Wasm 渲染帧推进吞吐** | `perf-baseline.test.js` | `> 1.3 帧 / ms` | **1.83 帧 / ms** | 🟢 PASS |
| **预烘焙包直接加载加速** | `perf-bundle.test.js` | `≥ 0.8× 源吞吐` | **1.97× 源脚本吞吐** | 🟢 PASS |
| **Wasmoon 内存驻留** | `perf-baseline.test.js` | `< 32 MB` | **~14.2 MB** | 🟢 PASS |

---

## 8. 全维度性能指标汇总矩阵

| 序号 | 维度分类 | 测量路径与测试文件 | 守护上限 / CI 预算 | 典型实测值 | 裕量 (Headroom) |
|:---:|---|---|---|---|:---:|
| 1 | **图层合成渲染** | `tests/scripts/test_frame_bench.lua` | `< 500 μs / 帧` | **274.6 μs / 帧** | **45.1%** |
| 2 | **混合表达式求值** | `tests/scripts/test_frame_bench.lua` | `< 2.0 s / 1000次` | **54.0 ms** | **97.3%** |
| 3 | **指令链式分发** | `tests/scripts/test_frame_bench.lua` | `< 2.0 s / 1000次` | **4.0 ~ 7.0 ms** | **99.6%** |
| 4 | **巨型图集核算** | `tests/scripts/test_scale_stress.lua` | `< 1.0 s / 4096瓦片` | **1.0 ~ 3.0 ms** | **99.7%** |
| 5 | **音频句柄池回收** | `tests/scripts/test_scale_stress.lua` | `< 2.0 s / 80k次` | **63.0 ~ 123.0 ms**| **93.8%** |
| 6 | **万词大剧本分词** | `tests/scripts/test_scale_stress.lua` | `< 10.0 s / 9600tok` | **946.0 ~ 1474 ms**| **85.3%** |
| 7 | **调度器帧推进** | `tests/scripts/test_scale_stress.lua` | `> 50 tok / ms` | **155 ~ 229 tok/ms**| **> 200%** |
| 8 | **500页Backlog内存**| `tests/scripts/test_scale_stress.lua` | `< 4096 KB 增量` | **934.7 KB** | **77.2%** |
| 9 | **长叙事翻译流** | `tests/scripts/test_scale_stress.lua` | `< 3000 μs / 行` | **408.7 μs / 行** | **86.4%** |
| 10 | **分词器基准吞吐** | `tests/scripts/test_benchmark.lua` | `< 750 ms / 1000tok`| **62.25 ~ 67.25 ms**| **91.0%** |
| 11 | **调度器热分发** | `tests/scripts/test_bench_dispatch.lua` | `> 100,000 tok / s` | **333,333 tok / s** | **> 230%** |
| 12 | **Label哈希查找** | `tests/scripts/test_label_bench.lua` | `≤ 线性查找时间` | **300× 优于线性** | **极显著** |
| 13 | **CPU 8k顶点蒙皮**| `tests/cpp/test_perf_bench.cpp` | `< 10.0 ms / 帧` | **0.75 ~ 0.92 ms** | **90.8%** |
| 14 | **Lua 10k文本格式化**| `tests/cpp/test_perf_bench.cpp` | `< 800.0 ms` | **19.05 ms** | **97.6%** |
| 15 | **Lua 10k表格读取** | `tests/cpp/test_perf_bench.cpp` | `< 400.0 ms` | **0.50 ms** | **99.8%** |
| 16 | **架构跨模块耦合** | `scripts/count_coupling.py` | `0 违规 / 16 模块` | **0 违规** | **100% 合规**|

---

## 9. 性能验证与回归复现指南

所有开发者及 CI 流水线均可通过以下标准化指令复现性能测量：

### 9.1 执行全量 Lua 性能套件（秒级门禁）
```bash
bash scripts/run_benchmarks.sh
```
*预期输出*：`Total: 5/5 PASS, 0 FAIL`，详细耗时写入 `tmp/bench-latest.txt`。

### 9.2 执行全量性能套件（含 Web Vitest Wasm 性能测试）
```bash
bash scripts/run_benchmarks.sh --web
```
*预期输出*：包含 `perf-baseline.test.js` 与 `perf-bundle.test.js` 在内的全套件通过。

### 9.3 执行 C++ 纯 CPU 热路径微基准
```powershell
build\tests\Debug\CaesuraTests.exe -tc="Perf:*" -s
```
*预期输出*：`3 passed, 0 failed, 1049 skipped`，输出 Lua 格式化、表字段读取与 8k 软蒙皮耗时。

### 9.4 验证 16 模块跨模块耦合隔离约束
```powershell
python scripts/count_coupling.py
```
*预期输出*：全部 16 个核心模块的 include 依赖严格处于限额以内，0 违规。

