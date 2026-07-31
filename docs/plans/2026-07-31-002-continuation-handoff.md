# Caesura (AmeKAG) — 交接文档（2026-07-31）

> 面向后续 agent 的完整上下文。先读 `AGENTS.md`（模块边界铁律），再读本文档与
> `docs/design/engine-architecture-topology.md`。

## 1. 当前状态

- **分支**：`master`（所有工作已合并并推送；另有本地分支 `codex/engine-audit-hardening` 与 master 同步）
- **GitHub**：`git@github.com:ailiasdesu/Caesura-AmeKAG.git`
- **CI**：`master` 最近运行 **三平台全绿**（Windows MSVC ×2、macOS Clang、Linux GCC；GitHub Actions，`gh run list` 可查）
- **本地验证基线**：doctest **537/537**、断言 2616/2616；ctest **9/9**；`python scripts/count_coupling.py --ci` PASS
- **工作树**：干净（仅一个未跟踪 `.reasonix/` 目录，非本项目产物，勿提交）

### 最近提交（自 7/31 起，均含在 master）
```
77334975 fix(ci): prefer vcpkg SDL3 when toolchain active, TYPE-guard runtime copy commands
a33de14d fix(ci): force SDL3 CONFIG mode with shim fallback, guard install generator expression
c365c083 fix(ci): normalize SDL3::SDL3 target alias for package-manager SDL3
4c214772 fix(ci): guard SDL3 copy behind if(TARGET), adapt job-system concurrency test to worker count
df6b3b9e fix(ci): Linux <cstring> include, audio-device skip for bare-init tests, harden job-system timing test
25ecce60 test(render/script): VideoPlayer missing-file open, transition Bezier math coverage
88cc355b feat(storage): cloud provider mock tests, Path A/B save convergence, remove shadowing Lua wrappers
93054252 feat(minigame): JSON scene loader — loadScene parses descriptors, enter spawns objects, camera/lights applied
81752b10 fix(ci): Linux <limits> include, SDL3 imported-target CMake guards, audio-device test skips
30b32263 feat(render): embed GLSL+Metal shaders for all programs, per-renderer bytecode selection, uniform/sampler name alignment
d29d6ab7 feat(rpc): managed-coroutine run + eval via owner-thread pump, fix extractField quoted-value parsing
97db4b26 feat(render): real font face switching — IRenderDevice::loadTTF + text_set_font binding + NotoSansCJK asset
b47b4113 feat(script): layer opacity fade — Layers.fade_to + [layfade] command with tests
bd0af504 feat(demo): full-pipeline KAG demo, tokenizer [/endscript] compat, startup budget windows
cafcf3b6 chore(git): stop tracking CMake build directories (build-*-verify)
7cbc1feb fix(render/script): VFX uniform layout, embedded DXBC uniform metadata, ms-based vfx timing, submit_vfx signature
0a976fea fix(tests): align quicksave/quickload expectations with pause-guarded input loop
```

## 2. 已完成的执行计划（阶段 1–4 主体）

### Phase 1 — 核心可玩性（已完成并合入 master）
- `demo/full_pipeline_demo.ks` + `demo/entry_full.lua`：覆盖 bg/fg/ch/p/ruby/音频/if-jump/eval/iscript/按钮/存读档/转场/vfx 的全流程 demo；`demo/minigame_scene.json` 是 3D 场景示例
- tokenizer 兼容 KAG3 标准 `[/endscript]`（`scripts/tokenizer.lua`）
- 指令预算 500k→2M（`src/script/vm/LuaManager.h`）+ 启动阶段预算窗口重置（`src/main.cpp`）
- P1-2 文字换行：审计发现**已实现**（`scripts/kag/text_layout.lua` + `add_wrapped`），无需开发
- `Layers.fade_to` + `[layfade]` 图层渐变（`scripts/layers.lua`、`scripts/kag/commands/layer.lua`）
- 字体切换：`IRenderDevice::loadTTF` 接口（按 AGENTS.md 流程修改 I*.h → 实现 → 绑定）+ `text_set_font` 真实实现 + `assets/fonts/NotoSansCJKsc-Regular.otf`（OFL，16MB）+ LICENSE
- 编辑器 `run`/`eval`：owner-thread managed coroutine（`src/main.cpp` EngineRpcDispatcher）+ `extractField` 引号解析修复 + `tests/headless_rpc_smoke.py`（8 项端到端）

### Phase 2 — 跨平台渲染（已完成）
- 构建 bgfx `shaderc`（GENie vs2022 生成，`external/bgfx/bgfx/.build/win32_vs2022/bin/shadercRelease.exe`，**该工具是本地产物不入库**）
- 编译 OpenGL GLSL + Metal MSL 字节码并内嵌：`src/render/EmbeddedShaders_GL.cpp` / `EmbeddedShaders_Metal.cpp`（~108KB 各，由 `shaders/embed_to_c.py` 再生成）
- `BgfxShaderManager::initEmbeddedShaders` 按渲染器选择字节码（Vulkan→SPIRV、D3D→DXBC、GL→GLSL、Metal→MSL），消除 "Debug text only" 降级
- 统一 uniform/sampler 命名对齐 C++：`BlendParams`/`TransParams`/`VFXParams`/`StretchParams` + `s_texture`/`s_texture1`/`s_texture2`（`src/render/BgfxShaderManager.h` getSampler1/2）
- 修复 `fs_blend.sc` 的 `bVividLight` float/vec3 调用 bug
- CI Test 步骤改为 `ctest`（三平台）

### Phase 3 — 内容系统（部分）
- **P3-1 ✅** MiniGame JSON 场景加载：`src/minigame/MiniScene.h`（MiniObject 已迁入此头）+ `loadScene`/`enter`/`unloadScene` 真实实现 + 测试；enter 需真实 bgfx（GPU），测试仅覆盖解析管线
- **P3-2 ❌ 未做** Live2D Cubism：本地无 Cubism SDK（需手动下载），Metal 渲染仍是 STUB（`src/live2d/MetalNativeRenderPath.h`）
- **P3-3 ✅** CloudSaveProvider mock 测试（`tests/cpp/test_storage.cpp`，MockSteamBackend + 小文件/分块/空后端 3 项）

### Phase 4 — 工程质量（部分）
- **P4-1 ✅** 补齐 minigame/video/transition/cloud 测试（VideoPlayer 缺失文件、transition Bezier 数学、云存档）
- **P4-2 ✅** Save Path A/B 收敛：移除 `kag.lua` 中遮蔽性 Lua 包装（C 绑定 `KAG.save_game/load_game` 为准），`System.save/load` 标记弃用，迁移 `scripts/test_demo/main.lua`；`game_logic.lua` 在 .gitignore 中（本地脚本，迁移未入库）
- **P4-3 ❌ 未做** MobileAdapter：仍是占位 stub，需真实实现或文档化降级

## 3. 后续工作建议（按优先级）

1. **确认 CI 绿**（已完成 ✅，如需复查 `gh run list --workflow=ci.yml --limit 1`）
2. **P3-2 Live2D**：下载 Cubism SDK（`CAESURA_LIVE2D=ON -DCUBISM_SDK_ROOT=...`），验证 Windows D3D11 路径，修复 Metal STUB。无 SDK 则文档化状态。
3. **P4-3 MobileAdapter**：实现或降级文档化。
4. 可选：DeltaCARC 增量归档（当前是占位拷贝 `src/archive/DeltaCARC.cpp:216`）；SaveManager 最终清理。
5. 可选：将 `codex/engine-audit-hardening` 分支删除（已合入 master）。

## 4. 关键技术上下文（新 agent 必读）

### 构建与测试（Windows 主开发机）
```powershell
cmake -S . -B build-repro-verify            # 复用现有构建目录
cmake --build build-repro-verify --config Debug --parallel
cd build-repro-verify/tests/Debug && ./CaesuraTests.exe   # 全量 doctest（CWD 必须在此）
ctest -C Debug --test-dir build-repro-verify --output-on-failure
python scripts/count_coupling.py --ci
```
- 测试二进制在 `build-repro-verify/tests/Debug/`；构建目录不入库（.gitignore `/build-*/`）
- `apply_patch` 工具在本环境不可用（WindowsApps ACL），**用 PowerShell/Python 脚本做文件编辑**；PowerShell here-string 嵌套与 `\"` 转义易错，长文本用 Python `io.open` 读写

### 模块边界（铁律，见 AGENTS.md）
- 模块间只允许 include `src/<module>/api/I*.h`；具体实现头只在 `src/entry/` + `src/main.cpp` 使用
- `BackendRegistry`（`src/di/BackendRegistry.h`）是唯一后端访问点；新增后端按「接口 → 实现 → set/get → Engine::init 注册」流程
- 改接口流程：I*.h → 实现 override →（需要时）BackendRegistry → Engine::init → 全量构建 → 测试全绿

### CI 调试经验（重要，避免重蹈覆辙）
- **CI 三平台此前长期全红**，本次修复后全绿。关键坑：
  - Windows：仓库内置 `external/SDL3/SDL3-3.2.0/`（配置+头文件入库，但 **DLL/LIB 被 .gitignore 排除**）→ CI 检出缺二进制 → `SDL3::SDL3-shared` 不创建 → 生成器表达式 `$<TARGET_FILE:SDL3::SDL3>` 报 "No target"。修复：vcpkg 工具链激活时跳过内置 SDL3（`if(WIN32 AND NOT SDL3_DIR AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)`），复制命令用 `get_target_property(TYPE)` 守卫（仅 SHARED/STATIC/MODULE_LIBRARY 才执行）
  - Linux：`CloudSaveProvider.cpp` 缺 `<limits>`、`BgfxDeviceCore.cpp` 缺 `<cstring>`（MSVC 传递包含掩盖，GCC 报错）→ 已补
  - macOS/Linux CI 无音频设备 → 音频测试改为 init 失败时 `MESSAGE` + `return` 跳过（`tests/cpp/test_audio*.cpp`）
  - macOS job 系统：`computeWorkerCount` 在 hw<4 时仅 1 worker → 并发断言按 `js.workerCount()>=2` 自适应（`tests/cpp/test_job_system.cpp`）
- **测试用例数只增不减**（AGENTS.md）；当前 537/537

### 着色器工具链（P2 遗留知识）
- 重编译内嵌着色器：`external/bgfx/bgfx/.build/win32_vs2022/bin/shadercRelease.exe`（本地已构建）
- 流程：`shaderc -f shaders/glsl/<name>.sc -o ... --platform linux --profile 130`（或 `--platform osx --profile metal`），`-i external/bgfx/bgfx/src`（bgfx_shader.sh 所在）；GL 用 profile 130（120 不支持 switch/位运算；glsl-optimizer 最高 150）；然后 `shaders/embed_to_c.py` 再生成 C 数组
- `shaders/compiled/` 是中间产物（已 gitignore）

### 编辑器/RPC
- `CaesuraAmeKAG.exe --headless` 走 stdin/stdout JSON-RPC（每行一个 JSON）；`tests/headless_rpc_smoke.py` 是端到端测试（ctest 注册为 CaesuraHeadlessRpcSmoke）
- `run` 是异步 managed coroutine（逐帧 resume），`eval` 同步返回字符串化结果

## 5. 给新 agent 的续接提示（可直接粘贴）

```
继续 Caesura (AmeKAG) 引擎开发。先读 AGENTS.md 与 docs/plans/2026-07-31-002-continuation-handoff.md。
当前状态：master 分支、CI 三平台全绿、本地 537/537 测试全过。
任务：按交接文档第 3 节继续——优先 P3-2 Live2D Cubism 验证（无 SDK 则文档化）、P4-3 MobileAdapter 降级文档化。
约束：模块边界铁律（接口隔离）、测试只增不减、全量构建+测试全绿后提交推送。
环境提示：apply_patch 不可用，用 PowerShell/Python 编辑文件；构建/测试用 build-repro-verify 目录。
```

## 6. 附带说明
- `docs/` 分类规范：api/design/guides/plans/solutions；新执行计划放 `docs/plans/YYYY-MM-DD-NNN-描述.md`
- 耦合度目标：entry/di/script ≤14，其他 ≤4（`scripts/count_coupling.py --ci`）