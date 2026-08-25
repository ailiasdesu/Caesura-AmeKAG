# Caesura (AmeKAG) 长期项目记忆

> **加载规则**：每轮开发或审查开始前，必须首先阅读本文件；若本文件内容与 `AGENTS.md` 存在冲突，一律以 `AGENTS.md` 为准（宪法最高权威）。  
> **更新规则**：每轮结束时，必须把本轮的"新增纪律 / 踩坑 / 架构决策 / 状态变化"追加或修订到对应小节（保持幂等、带日期与提交来源）。

---

## 1. 铁律（不可违反，违反即返工）

1. **环境与 Shell 规范**：
   - Windows 下开发环境 Shell 必须使用 **git-bash**。命令必须使用 bash 语法 + 正斜杠路径（如 `external/lua/lua.exe`）。反斜杠路径会被转义吃掉（`external\lua\lua.exe` $\rightarrow$ `externallualua.exe`）。
   - PowerShell `&&` 无法作为语句分隔符；跨命令调用必须使用 `bash -c "cmd1 && cmd2"`。
   - PowerShell `>` 重定向默认写入 UTF-16LE，会损坏源码/markdown/Lua 文件，禁止在写文件时使用裸重定向。
   - 禁止在 `git commit -m` 中包含未转义的 `${...}` 插值，避免 bash 参数展开（bad substitution）。
2. **工具操作与文件保护**：
   - **大文件（60KB+）禁止全量覆写**（如 `src/entry/Engine.cpp` 1500+ 行），必须使用增量替换/局部编辑工具，严防工具阶段截断导致 C1075 语法错误。
   - **绝对禁止盲目终止 node 进程**：严禁执行 `taskkill /F /IM node.exe`（会杀掉 DSH 宿主与 IDE 后台）。清理残留 node 进程前必须检查 PID 及命令行参数。
3. **真实性与零伪造红线（Iron Rule）**：
   - **禁止捏造或修饰任何验证证据**：凡无法提供真实可复核证据（命令输出、日志、哈希、时间戳）的平台状态，一律诚实标注为 `claimed / device-unverified (hardware-gated)`，严禁用文档声明替代实测。
   - **真实设备硬件档案严禁编造**：真机档案唯一权威为 `Redmi K40 (M2012K11AC, haydn, Snapdragon 870 / Adreno 650 / Android 13)`。严禁标注为不存在的其他设备。
   - **日志字符串必须与源码对齐**：文档中引用的日志必须与引擎实际输出格式（如 `[BgfxRenderDevice]`、`[BackendRegistry]`、`[FirstVN]`）完全一致。
4. **仓库卫生与产物出清**：
   - **禁止将非源码产物入库**：ZIP 压缩包、`.wasm` 胶水二进制、构建中间件及 dist 产物一律移出 Git 索引并写入 `.gitignore`（如 `/artifacts/`）。发布产物统一由 GitHub Actions / Release Artifact 承载。
5. **门禁闭环与 CI 规则**：
   - **提交前全量门禁必须全绿**：C++ doctest（1052 用例）、Lua 主套件（136 文件）、Lua 孤儿套件（24 文件）、Web Vitest（24 文件 323 测试）、架构耦合预算（16/16 模块）必须 100% 通过。
   - **CI 红灯必修**：提交后若 CI 变红，必须第一时间定位修复，严禁在 CI 红灯状态下继续堆砌新功能。
   - **新增 KAG 模块预加载**：新增的 `scripts/kag/*.lua` 模块必须在 `scripts/kag/init.lua` 中注册预加载。
   - **接口变更普查同步**：修改 `src/*/api/I*.h` 接口后，必须重新运行 `python scripts/api_stats.py` 并同步更新 `docs/api/api-stats.md`。

---

## 2. 工程规范（代码 / 测试 / 提交）

### 2.1 架构与模块边界（16 模块铁律）
- **模块目录全小写**：`archive/`, `audio/`, `debug/`, `di/`, `entry/`, `input/`, `job/`, `live2d/`, `minigame/`, `platform/`, `render/`, `resource/`, `rpc/`, `script/`, `steam/`, `storage/`。
- **符号隔离**：每个模块只能通过 `api/` 子目录对外暴露符号（`src/<module>/api/I<ModuleName>.h`），接口必须为纯虚类（`= 0`），禁止包含数据成员。
- **唯一访问点**：所有后端访问必须通过 `BackendRegistry::instance().get*()`；禁止直接访问底层单例。
- **组合根唯一性**：`src/main.cpp` 与 `src/entry/` 是唯一创建具体后端对象的地方。
- **架构耦合预算**：
  - `entry` ≤ 14, `di` ≤ 14, `script` ≤ 14；
  - 其他 13 个业务模块必须 ≤ 4。执行 `python scripts/count_coupling.py --ci` 检验。

### 2.2 测试门禁与度量基线
| 测试套件 | 运行命令 | CWD 要求 | 基线指标 |
|---|---|---|---|
| **C++ Doctest** | `./CaesuraTests.exe` | `build/tests/Debug` | **1052 cases, 385,299 assertions, 0 failed** |
| **Lua 主套件** | `external/lua/lua.exe tests/scripts/run_lua_tests.lua` | 仓库根目录 | **136 suites, 0 failed** |
| **Lua 孤儿套件** | `external/lua/lua.exe tests/scripts/run_orphan_tests.lua` | 仓库根目录 | **24 suites, 0 failed** |
| **Web Vitest** | `npm --prefix web test` | 仓库根目录 / `web` | **24 files, 323 tests, 0 failed** |
| **RC 对抗突变** | `python tests/scripts/test_rc_adversarial_mutations.py` | 仓库根目录 | **42/42 mutations caught (100%)** |
| **平台矩阵对抗** | `python tests/scripts/test_platform_matrix_adversarial.py` | 仓库根目录 | **31/31 assertions pass (100%)** |
| **Android 回归**| `python scripts/verify_android_regression.py` | 仓库根目录 | **88/88 checks passed** |
| **架构耦合度** | `python scripts/count_coupling.py --ci` | 仓库根目录 | **16/16 compliant** |

### 2.3 提交规范与代码格式化
- **提交格式**：约定式提交 `type(scope): description`（如 `feat(kag)`、`fix(render)`、`docs(status)`、`test(input)`）。
- **提交内容**：纯净提交，不包含签名表情或外部注入元信息。
- **代码格式化**：C++ 代码统一遵守 `.clang-format`（WebKit 风格，C++20，120 列宽，4 空格缩进，指针左对齐 `PointerAlignment: Left`）。

---

## 3. 开发流程（每轮标准流程）

```
      ┌────────────────────────────────────────────────────────┐
      │ 1. 契约先行 (Contract-First)                           │
      │    主 Agent 明确接口 (src/*/api/I*.h) 与数据协议      │
      └──────────────────────────┬─────────────────────────────┘
                                 │
      ┌──────────────────────────▼─────────────────────────────┐
      │ 2. 子代理扇出 (Subagent Fan-out)                        │
      │    拆分为 4~8 个互不重叠文件集，并行编码与编写测试用例 │
      └──────────────────────────┬─────────────────────────────┘
                                 │
      ┌──────────────────────────▼─────────────────────────────┐
      │ 3. 主 Agent 汇聚与本地门禁 (Local Verification)         │
      │    运行 C++ / Lua / Web / 对抗性全量门禁确保全绿        │
      └──────────────────────────┬─────────────────────────────┘
                                 │
      ┌──────────────────────────▼─────────────────────────────┐
      │ 4. 累积语义提交与推送 (Semantic Commit & Push)          │
      │    细粒度 commit，节点统一推送并监控 CI 状态            │
      └────────────────────────────────────────────────────────┘
```

1. **三位一体交付要求**：任何新功能或修复必须满足 **Code + Test + Verification Evidence**，严禁只改文档或声明"已完成"而不附带真实可运行代码与测试。
2. **Release Candidate 验证流程**：
   - 组装证据包：`python scripts/verify_release_candidate.py --generate-bundle`；
   - 验证快照自洽：`python scripts/verify_release_candidate.py --check`；
   - 指定目标基线：`python scripts/verify_release_candidate.py --check --commit <ref>`；
   - 对抗性校验：`python tests/scripts/test_rc_adversarial_mutations.py`（42 用例全拦截）。

---

## 4. 平台与环境事实

### 4.1 Windows 开发机
- **工具链**：Visual Studio 2022 (MSVC v143), CMake 3.25+, vcpkg (`C:/vcpkg`), Python 3.12+, Node 18+。
- **构建输出**：桌面引擎 `build/Debug/CaesuraAmeKAG.exe`，测试程序 `build/tests/Debug/CaesuraTests.exe`。

### 4.2 Android 移动端实机
- **硬件档案**：`Redmi K40 (M2012K11AC, haydn, Snapdragon 870 / Adreno 650 / Android 13)`，Magisk Root。
- **本地工具链**：NDK r27.3 (`/d/green/ndk/27.3.13750724`)，SDL3 3.2.4 安卓源码，OpenSSL 3.3.2，JDK 17，Gradle 8.9，Ninja，Android SDK 35。
- **真机交互铁律**：
  - `adb push` 必须使用 Windows 绝对路径；
  - `su` 权限写入 `/data/user/0/com.caesura.app/files/caesura_root` 后，**必须执行 `su -c chown -R u0_a242:u0_a242`**，防止应用权限不足无法读取资源；
  - MIUI 拒绝无 `INJECT_EVENTS` 权限的普通 adb 触控，模拟点击必须使用 `su -c input tap <x> <y>`；
  - 日志读取：`adb logcat -d | grep engine-stderr`。
- **渲染与生命周期**：
  - 分辨率适配：物理屏幕（如 2320×956）通过 `SDL_GetWindowSizeInPixels` 注入 `IRenderDevice::setPresentSize`，逻辑保持 1920×1080；
  - 横屏强制锁定：窗口创建前必须调用 `SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft,LandscapeRight")`；
  - 外部 EGL 上下文：SDL3 创建 GLES 3.0 上下文并绑定到 bgfx 平台数据。
- **存储系统**：
  - 快速存档：`slot=-1` $\rightarrow$ `save_quick.json`；
  - 自动存档：`slot=-2` $\rightarrow$ `save_auto.json`；
  - 截图两阶段：`requestScreenShot` 挂钩下一帧，避免当前帧内重复调用 `bgfx::frame()` 产生双 present。

### 4.3 渲染引擎（Render Architecture）
- **设计分辨率**：全平台统一 1920×1080，UI 布局基于 `scripts/viewport.lua` 计算相对坐标。
- **每帧提交**：`scripts/layers.lua` 对可见节点每帧提交 quad（`dirty` 标记仅用于 RTT 懒分配，不可用于跳过顶点提交）。
- **字形图集**：`TextRenderer` 采用 **2048×2048 RGBA8** 动态图集（`r=g=b=255, a=coverage`），启动时预载 `NotoSansCJKsc-Regular.otf` 约 8074 个常用字符，完美兼容移动端 GLES `fs_texture` 单一贴图直通采样。

### 4.4 Web 端（WebAssembly / Wasmoon）
- **路径归一化**：引擎资源路径统一归一化为 `/assets/<path>`，防止出现 `/assets/assets/` 404。
- **索引与胶水文件**：
  - `web/scripts-index.json` 是提交工件，新增 `scripts/*.lua` 后必须运行 `node web/gen-index.mjs`；
  - `glue.wasm` 必须本地 Pin（`__CAESURA_WASM_FILE__`），严禁依赖 unpkg 在线 CDN。
- **打包与冒烟**：`bash scripts/package_game.sh` + `node scripts/web_browser_smoke.mjs`。

### 4.5 CI 与自动化门禁
- **工作流配置**：`.github/workflows/ci.yml`（Windows MSVC、Ubuntu GCC/Clang、macOS AppleClang）。
- **探针策略**：无物理设备的 macOS/iOS 编译步骤配置为 continue-on-error / `HARDWARE-GATED`。

---

## 5. 状态与决策索引

### 5.1 当前版本状态
- **最新 HEAD**：`9f5a022f`
- **发布候选决策**：`1.0.0-rc.1`（**RC-GO**）
- **全量门禁状态**：C++ 1052/1052、Lua 160/160、Web 323/323、Coupling 16/16 全部通过。

### 5.2 状态划分（实测 vs Gated）
- ✅ **已完成实测验证**：
  - Windows x64 完整引擎运行、编辑器 RPC 与 1052 C++ 测试；
  - Linux WSL/Ubuntu 无头与窗口化验证；
  - Web 播放器 PWA 离线缓存、DOM 渲染、Wasmoon 323 项 Vitest；
  - Android 真机（Redmi K40）全链路闭环：启动、渲染、字形、分支、存档、生命周期。
- ⏳ **声明 / 硬件受限项（Honest Gating）**：
  - **IME 输入法候选词真机输入**：C++ 底层接口与 Lua 绑定已闭环，真机实体输入法弹出与实际打字处于 `claimed / device-unverified (pending)`；
  - **iOS 真机实测**：12/12 Metal Shaders 与 Xcode 流程完备，物理真机处于 `hardware-gated`；
  - **低内存压力中断（onLowMemory）**：真机物理触发未覆盖；
  - **多指手势注入**：单测覆盖，真机物理多指注入未实测。

### 5.3 关键历史决策与评估
- **KAG3 生态兼容决策**：
  - 决策：通过 `scripts/kag3_import.lua`、`.xp3` 归档解析器与 TLG5/6 解码器提供 KAG3 导入管道；
  - 《LimeLight Lemonade Jam》商业作品移植评估：引擎核心能力 100% 就绪，移植阻塞点为官方加密 XP3（需通过提取后的 `unencrypted.xp3` 载入）。
- **统一语义层（KAG Semantic Layer）**：
  - 决策：在 `scripts/kag/semantic.lua` 建立统一 AST / CFG 模型，作为 Story Flow、i18n、LSP、CLI 统一真相源，根除正则表达式解析分歧。

---

## 6. 踩坑库（按模块持续沉淀）

| 模块 | 问题现象 | 根本原因 | 解决方案 | 验证与来源 |
|---|---|---|---|---|
| **Render** | Android GLES 下文字全黑或不可见 | TTF 原使用 R8 单通道贴图，GLES `fs_texture` 采样丢失 Alpha | 重构为 RGBA8（`r=g=b=255, a=cov`）2048×2048 图集，预载 Noto 8074 字符 | Commit `c6170e39` / Round 028 |
| **Render** | QuadBatch 多纹理立绘丢失 | 单批次仅分配单个 TransientBuffer，首个 `bgfx::submit` 后被 GPU 丢弃 | 改为每个 `MergeGroup` 独立分配 TransientBuffer 并重设 `bgfx::setState` | Round 028 / `BgfxQuadBatch.cpp` |
| **Render** | 对话框 RTT 被立绘拉伸覆盖 | Lua 层 RTT ID 与普通纹理 ID 小整数重叠，`resolveTexture` 误命中立绘 | 解耦 `tex` 与 `rt` 查询分支，`rt` 严格走 `getViewportTexture` | Round 028 / `RenderBinding.cpp` |
| **Input** | 手机屏幕点击分支按钮无响应 | Android 物理像素（2320×956）与 1920×1080 逻辑坐标未换算 | 在 `Engine.cpp` 中增加物理到逻辑视口比例缩放映射 | Round 028 / `Engine.cpp` |
| **Platform** | Android 手机旋转时画面错位 | Manifest 横屏配置被 SDL3 默认系统旋转行为覆盖 | 窗口创建前调用 `SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft,LandscapeRight")` | Commit `c6170e39` |
| **Android** | `adb push` 脚本后应用报 Module not found | su 写入的文件所有者为 `root:root (600)`，应用无读取权限 | 写入后必须执行 `su -c chown -R u0_a242:u0_a242` | Plan 027 / Android 指南 |
| **Android** | `adb shell input` 模拟点击被 MIUI 拦截 | MIUI 限制 uid2000 调用 `INJECT_EVENTS` 接口 | 必须通过 root 通道执行 `su -c input tap <x> <y>` | Plan 027 / Android 指南 |
| **Audio** | 存档截图时音频微顿与双重 Present | `captureThumbnailPNG` 帧内同步调用 `bgfx::frame()` | 改为挂钩下一帧两阶段异步捕获，消除帧内重复 present | Round 028 / `SaveManager.cpp` |
| **Script** | 帧回调中 `require` 报 not preloaded | 沙箱清除了 searchers，仅允许 `package.loaded` | 所有运行时子模块必须在 `scripts/kag/init.lua` 中预载 | Plan 027 / `sandbox.lua` |
| **Web** | Web 玩家静态资源 404 报错 | 引擎资源默认带 `assets/` 前缀，Web 侧拼接导致 `/assets/assets/` | 在 `web/main.mjs` 中增加路径归一化剥离多余前缀 | Plan 027 / Track W |
| **Toolchain** | Windows Python 子进程乱码与管道卡死 | Windows 默认使用 GBK 编码导致 UTF-8 字符解码崩溃 | 子进程显式声明 `encoding="utf-8", errors="replace"` | Commit `31e2fb32` / CLI |
| **QA / RC** | 后续日常提交导致 RC `--check` 误报红 | Verifier 将 HEAD Commit 与 Manifest 写死比对 | 改为以 Manifest 基线为权威，`--check` 校验内部自洽性 | Commit `9f5a022f` / Verifier |
| **Matrix** | 平台矩阵 `test: null` 绕过校验 | `str(None)` 转换为字符串 `"None"` 导致空判定失效 | 显式拦截 None 值并增强 probe 状态下引用文档存在性校验 | Commit `5a87ca86` / Matrix |
