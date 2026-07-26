# 2026-07-21 当前状态校正与架构迁移执行计划

> 状态: Phase 0-2 已在工作区执行并验证，待形成 Git 提交闭包
>
> 适用分支: `codex/engine-audit-hardening`
>
> 基线提交: `c091e71c08fe31079571dd26c53d40ada3941726`
>
> 本计划取代 `2026-06-28-next-steps-plan.md` 作为当前执行基线。旧文件保留为历史记录。

## 1. 结论

当前首要任务不是继续追加 P2 功能，而是恢复“源码、构建描述、测试产物”三者一致的可复现基线。

`2026-06-28-next-steps-plan.md` 中的功能方向大体合理，但它仍使用 76 项旧审计、旧测试数量、旧提交号和旧实现状态。更关键的是，当前 HEAD、工作区未跟踪文件以及 `stash@{2026-07-21 21:15}` 分别保存了迁移前代码、部分迁移文件和大批迁移改动。直接按旧计划先清理 include 或实现 P2，会扩大冲突面，并无法建立可信的完成度。

架构迁移本身可行，目标也合理: 内部模块使用独立静态库表达边界，最终仍统一链接成单个可执行文件。但必须先完成一次受控恢复，再继续调试器生产桥接和功能开发。

## 2. 2026-07-21 恢复前实测基线

| 检查项 | 实测结果 | 判断 |
|---|---|---|
| 当前 HEAD | `c091e71c` | 今日提交只更新旧计划，没有纳入迁移改动 |
| 工作区 | 存在 `cmake/`、Null 后端、资源 generation API 等未跟踪项 | 迁移文件尚未形成可提交闭包 |
| WIP 状态 | `stash@{2026-07-21 21:15}` 含 131 个文件、约 4987 行新增/2988 行删除 | 主要架构成果仍在 stash，不能盲目丢弃或覆盖 |
| 重新生成/构建 | 失败 | `CMakeLists.txt` 仍引用已迁移的 `src/render/EmbeddedShaders_MiniGame.cpp` 与 `src/storage/SaveBinding.cpp` |
| 旧构建目录 CTest | `7/8` 通过 | `CaesuraUnitTests` 失败，旧二进制不可代表当前源码 |
| 旧构建目录 doctest | `468/480` 用例、`1963/2047` 断言通过 | 12 个用例失败，主要是源码门禁检测到迁移前后不一致 |
| 耦合门禁 | `python scripts/count_coupling.py --ci` 通过 | 只说明当前文本依赖计数未越界，不能替代构建和测试 |

因此，恢复前的发布/合并状态应标记为 **Blocked by reproducibility**。这不是代码能力归零，而是现有成果尚未落在一个可从 HEAD 重建并验证的状态中。

### 2.1 Phase 0-2 执行结果

执行时间: `2026-07-21 21:49 +08:00`

| 检查项 | 实测结果 | 判断 |
|---|---|---|
| 现场恢复 | `stash@{0}` 无冲突应用，131 个已跟踪改动恢复；原 stash 保留 | 恢复源仍可回溯，没有覆盖未跟踪文件 |
| 构建闭包 | 顶层只引入 `cmake/CaesuraModules.cmake`；MiniGame shader、SaveBinding、ThreadAssert 与 Null 后端路径均对齐 | 16 个模块静态库、API 目标、应用与测试共享同一生产源清单 |
| SDL3 发现 | Windows 未传 `SDL3_DIR` 时自动使用仓库内随附包；显式 `SDL3_DIR` 仍可覆盖 | 计划中的 fresh configure 命令不再依赖旧 CMake 缓存或本机绝对路径 |
| Fresh configure/build | `build-repro-verify`，`CAESURA_LIVE2D=OFF`，Debug 全量构建零错误 | 应用、工具、模块库和测试均由当前源码重新生成 |
| CTest | `8/8` 通过，`0 failed` | 分组测试与 headless CLI smoke 全绿 |
| doctest | `480/480` 用例、`2066/2066` 断言通过，`0 failed, 0 skipped` | 当前源码与测试二进制一致 |
| 耦合门禁 | `python scripts/count_coupling.py --ci` 通过 | 所有模块均在预算内，API 边界检查通过 |
| 补丁门禁 | `git diff --check`、`git diff --cached --check` 通过 | 未发现空白或补丁格式错误 |

工作区层面的 **Blocked by reproducibility 已解除**。合并状态仍需保持阻塞，直到 9 个关键未跟踪文件与 131 个恢复改动形成同一 Git 提交闭包；本次执行按约束未暂存、未提交，也未删除原 stash。

## 3. 对今日旧计划的合理性审查

### 3.1 可以保留的方向

- A1-A3 的焦点切换、PNG 立绘和 Voice 回调仍值得做真实运行验证。
- A4-A6 的图层淡入淡出、文字效果和自动换行属于视觉小说核心体验，应保留。
- C 的全流程 demo 是合适的系统验收载体。
- D 的 Registry 空值、Lua 错误传播和 GPU 生命周期审计是必要的安全工作。

### 3.2 必须调整的内容

| 旧计划内容 | 问题 | 调整 |
|---|---|---|
| `P0 阻塞 = 0` | 当前无法从源码重新生成构建 | 增加 P0“恢复可复现构建” |
| 先做 B 类 include 清理 | 会改动迁移冲突热点，收益低 | 放到基线全绿之后，且每批清理后编译 |
| A1-A3 各 15 分钟 | 只覆盖手工观察，不含测试环境和资源准备 | 每项按 0.25-0.5 天评估，先补可自动化断言 |
| A4/A5 直接使用 Lua coroutine | 当前 scheduler 与 debugger 的 resume 所有权尚未统一 | 先完成 scheduler-aware resume adapter |
| A6 直接修改 `IRenderDevice::renderText` | 会影响接口、实现、绑定和调用方，默认参数也不能降低虚接口变更风险 | 先定义文本布局参数对象或独立布局 API，再迁移调用方 |
| A7 仅“检查 submit” | submit 成功不代表像素结果正确 | 增加 GPU 截图/像素 smoke，headless 只做协议测试 |
| C 预计 2 小时 | 未计入 8 个资源的制作、许可、路径和 GPU 验证 | 拆为脚本、可分发资产、自动化 smoke、人工视觉验收四部分 |
| D 统一放在最后 | 高风险线程/Lua 生命周期问题会影响前面所有实现 | 将静态边界与线程审计前置，GPU 深度审计保留在后段 |
| 3 人 2-3 天完成全部 | P0 恢复具有串行关键路径，不能完全并行 | 基线恢复完成后再并行，整体按 4-7 个工程日评估 |

### 3.3 可行性结论

- 技术可行性: 高。迁移代码和验证产物已经存在，主要工作是恢复、对齐和收口，不是从零设计。
- 当前直接执行旧计划的可行性: 低。构建描述与源码不一致，任何功能完成度都无法可靠验证。
- 受控执行本计划的可行性: 高。阶段间有明确依赖和退出门槛，可逐步缩小风险。
- 最大风险: stash/未跟踪文件恢复时发生覆盖或遗漏。必须保留原引用，禁止使用 `reset --hard`、`checkout --` 或清理未跟踪文件。

## 4. 目标架构

采用“内部独立静态库 + 最终统一链接”的混合方案:

1. 16 个模块由独立 CMake 静态库目标表达编译和依赖边界。
2. 普通模块只通过 `api/I*.h` 互相依赖。
3. `src/entry/` 与 `src/main.cpp` 继续作为唯一组合根。
4. `BackendRegistry` 只保存后端接口的非拥有指针。
5. `CaesuraAmeKAG` 最终仍交付一个可执行文件，不引入 DLL ABI 和部署负担。
6. 测试链接与生产相同的模块库，不维护第二套生产源码清单。
7. `DebugProtocol` 由组合根显式持有，不进入 `BackendRegistry`。
8. RPC/HTTP 只投递 DTO/命令，不在传输线程直接访问 `lua_State`。

## 5. 执行阶段

### Phase 0: 保护现场与确定唯一真源 (P0, 0.25-0.5 天)

状态: **已完成（工作区）**。stash、HEAD、未跟踪文件与旧构建产物已审计，恢复过程无路径交集和冲突，原 stash 保留。

目标: 在不丢失任何现有成果的前提下，确定迁移代码的恢复来源。

- 记录 HEAD、stash 引用、未跟踪文件清单和现有构建产物时间戳。
- 将 `stash@{2026-07-21 21:15}` 视为只读恢复源，先做文件级差异审计。
- 对比 stash、未跟踪文件与 HEAD，建立“采用/舍弃/需合并”清单。
- 在隔离分支或 worktree 中恢复，不直接覆盖当前工作区。
- 明确 CMake 模块文件、Null 后端、测试夹具和接口头必须作为同一批次落地。

退出条件:

- 每个迁移文件都有明确来源和归属。
- 无文件仅存在于临时构建目录。
- 原 stash 和当前未跟踪文件仍可回溯。

### Phase 1: 恢复模块化构建闭包 (P0, 1-1.5 天)

状态: **已完成（工作区）**。模块目标和迁移路径已闭合，fresh Debug configure/build 通过；9 个关键新增文件仍须在提交时一并纳入。

目标: 从干净构建目录完整生成并编译。

- 让顶层 `CMakeLists.txt` 只保留模块入口，不再维护旧 `ENGINE_SOURCES` 大列表。
- 纳入 `cmake/CaesuraModules.cmake`，校对 16 个模块和 15 个 API 目标。
- 对齐 MiniGame shader、SaveBinding、ThreadAssert 等已迁移路径。
- 确认生产源文件只编译一次，测试通过 `Caesura::Engine`/`Caesura::Rpc` 复用生产库。
- 确认所有新增、移动、删除文件均被 Git 正确表达，Windows 大小写与索引一致。
- 从新的空构建目录执行配置与 Debug 构建。

退出条件:

- CMake configure 成功。
- `cmake --build <fresh-build> --config Debug --parallel` 零错误。
- 不依赖旧 `.lib`、`.obj`、DLL 或已生成测试二进制。

### Phase 2: 恢复架构门禁与测试基线 (P0, 0.5-1 天)

状态: **已完成（工作区）**。CTest、完整 doctest、耦合门禁和补丁检查全部通过，结果见 2.1 节。

目标: 让测试真正对应当前源码。

- 先运行 source-encoding/architecture boundary 测试，修复源码门禁与实现不一致。
- 再运行模块级测试、CTest 和完整 doctest。
- 核对 Engine 生命周期回滚、Registry 注销、Null 后端和资源关闭顺序。
- 复核能力矩阵与 README，只记录实际可复现结果。

退出条件:

- CTest `0 failed`。
- doctest `0 failed, 0 skipped`，用例数和断言数以新构建实际输出为准。
- `python scripts/count_coupling.py --ci` 通过。
- `git diff --check` 与 `git diff --cached --check` 通过。

### Phase 3: 调试器生产桥接 (P1, 1-1.5 天)

状态: **已完成（工作区，尚未提交）**。执行时间: `2026-07-21`。

目标: 在不阻塞 Lua owner thread 的前提下，让 KAG 协程可被可靠暂停和恢复。

- 恢复实例化、非阻塞的 `DebugProtocol` 状态机和 command mailbox。
- 由组合根在可调试协程创建前挂载，在 Lua VM 关闭前按 owner-thread 顺序解绑。
- 为 `kag_runner.lua` 建立唯一 resume 入口，统一 `start/update/on_click/debug-resume`。
- debugger 为 `Paused` 时，普通 frame update 和点击批量推进不得自行 resume。
- Continue/Step 命令只在 owner thread pump，并保留 Lua 错误、yield 结果和栈所有权。
- 定义 canonical source-id: 斜杠、绝对/相对路径、`.`/`..`、Windows 大小写策略。
- RPC 首阶段只接 command DTO/callback；HTTP worker 不直接访问 Lua。

退出条件:

- breakpoint、continue、step into/over/out、过期 pauseId、并发 shutdown 均有测试。
- KAG 点击在暂停期间不会越过断点。
- scheduler 正常 yield、调试 yield 和错误返回能被区分并清理。
- Lua 指令预算 hook 在挂载、暂停、恢复和解绑后仍有效。

执行结果:

- `Engine` 显式持有 `DebugProtocol`，在 HotReload 之后挂载，在 HotReload/Lua 之前解绑；`BackendRegistry` 未新增调试器槽位。
- KAG 的 `start/update/on_click` 统一经过 resume scheduler，并通过只读 C 闭包实时查询暂停状态；C++ 恢复后的同一帧不会再次执行普通 Lua 回调。
- canonical source-id 已覆盖 Lua source 前缀、分隔符、`.`/`..`、绝对/相对路径及 Windows ASCII 大小写归一。
- `RpcServer` 与 `EditorServer` 已移除 `lua_State*` 和 Lua API；传输只提交 DTO，组合根 dispatcher 在 owner thread 执行并在关闭时取消等待请求。
- stdio RPC 已开放断点、Continue、Step、变量检查和调试状态命令；过期 `pauseId` 会被拒绝。
- `--editor` 已接入 HTTP `EditorServer`（9876），`--editor-stdio` 保留 GPU + stdio RPC；`--headless` 使用 stdio RPC。
- `run/eval` 尚未迁移到 managed coroutine，当前明确返回 `unsupported_yieldable_execution`，不再在不可 yield 的 Lua 主状态执行。
- 定向验证通过：DebugProtocol `14/14`、RPC/Editor `19/19`（含 Windows UTF-8 BOM 首行）、Engine 调试生命周期、KAG resume 仲裁、headless stdio 进程 smoke、D3D11 HTTP status/stop smoke。

残余边界:

- canonical source-id 不解析 symlink/junction；后续可由组合根显式注入 source root。
- HTTP 暂未提供调试路由，完整调试命令当前通过 stdio RPC 暴露。
- `run/eval` 的 managed coroutine 执行属于下一批独立工作，不应与本阶段完成状态混淆。

### Phase 4: P2 核心功能与全流程 demo (P2, 1.5-2.5 天)

目标: 在稳定架构上验证并补齐视觉小说核心体验。

建议顺序:

1. 焦点切换、Voice 完成回调、PNG fallback 立绘的自动化验证。
2. 图层透明度渐变与文字淡入淡出，复用统一的逐帧 operation/cancel token 模式。
3. 文本布局 API 设计与 CJK 自动换行，覆盖标点禁则、长英文单词、Ruby 和边界宽度。
4. VFX GPU smoke，验证最终像素而不只检查 submit 次数。
5. 建立最小可分发 demo；优先复用仓库已有且许可明确的资源，缺失资源不得用占位声明冒充完成。

退出条件:

- 每项至少有一个确定性自动化测试。
- demo 覆盖背景、立绘、对话、分支、音频、存读档、转场、VFX 和结束流程。
- Windows GPU 人工 smoke 通过；可自动化时增加截图像素检查。

### Phase 5: 安全、跨平台与发布验收 (P2, 1-1.5 天)

目标: 将“代码存在”提升为“可发布”。

- 审查所有 Registry getter 的空值和关闭期行为。
- 审查 `lua_pcall`/`lua_resume` 的错误栈清理和非 owner-thread 调用。
- 审查 GPU create/destroy、device loss/restored 和失败回滚路径。
- 在 Windows、Linux、macOS CI 重新配置并构建，不复用缓存判断成功。
- 对 Steam、Cubism、FFmpeg 等可选 SDK 分别记录“未启用、配置通过、真实运行通过”。
- 验证安装包包含运行库、脚本、demo 和许可文件。

退出条件:

- 三平台构建和非 GPU 测试全绿。
- Windows 真实 GPU smoke 全绿；其他平台未验证项明确标注，不写成完成。
- Release/Debug 均可从干净目录构建。

## 6. 关键依赖与并行策略

```text
Phase 0 现场保护
    -> Phase 1 构建闭包
        -> Phase 2 全绿基线
            -> Phase 3 调试桥接
            -> Phase 4 P2 功能和 demo
            -> Phase 5 安全/发布
```

Phase 0-2 是串行关键路径。基线全绿后可以并行:

- A 线: DebugProtocol + KAG scheduler + RPC DTO。
- B 线: 文本布局、渐变和 VFX。
- C 线: demo、资产许可、GPU/发布验证。

不要让多个执行者同时修改 `Engine.cpp`、`CMakeLists.txt`、`BackendRegistry` 或 `kag_runner.lua`；这些文件必须单一所有者串行合并。

## 7. 工期估算

| 阶段 | 单人估算 | 三人并行后的日历时间 |
|---|---:|---:|
| Phase 0-2 基线恢复 | 1.75-3 天 | 1.5-2.5 天，关键路径不可压缩 |
| Phase 3 调试桥接 | 1-1.5 天 | 1-1.5 天 |
| Phase 4 P2 + demo | 1.5-2.5 天 | 1-1.5 天 |
| Phase 5 发布验收 | 1-1.5 天 | 0.75-1.5 天 |
| 总计 | 5.25-8.5 工程日 | 4-7 个日历日 |

估算不包含 Cubism/Steam SDK 缺失、跨平台 CI 环境故障或新增美术资产制作时间。旧计划的“总计约 8 小时”不具备足够缓冲，不应作为交付承诺。

## 8. 风险登记

| 风险 | 概率/影响 | 应对 |
|---|---|---|
| stash 恢复覆盖当前文件 | 高/高 | 隔离 worktree，先审计后恢复，保留原引用 |
| 新旧 CMake 同时编译生产源 | 中/高 | 顶层只保留一个模块入口，检查对象和符号重复 |
| 旧测试二进制产生假绿 | 高/高 | 仅接受 fresh build 目录的结果 |
| Debugger 与 scheduler 双重 resume | 高/高 | 唯一 resume adapter + owner-thread 状态机测试 |
| HTTP worker 直接访问 Lua | 中/高 | DTO 队列与主线程 dispatcher，禁止传裸 `lua_State*` |
| source-id 在 Windows 不一致 | 高/中 | 先定义规范化契约，再开放编辑器断点 |
| GPU 功能仅有 headless 测试 | 高/中 | 增加截图/像素 smoke 和人工验收记录 |
| 文档完成度高于代码事实 | 中/中 | 能力矩阵只引用 fresh build/运行证据 |

## 9. 每批修改的统一验证命令

```powershell
cmake -S . -B build-current-verify -DCAESURA_LIVE2D=OFF
cmake --build build-current-verify --config Debug --parallel
ctest --test-dir build-current-verify -C Debug --output-on-failure
build-current-verify/tests/Debug/CaesuraTests.exe --no-skip
python scripts/count_coupling.py --ci
git diff --check
git diff --cached --check
```

只有上述命令来自同一个 fresh build 且全部通过，才能更新完成度、测试数量或“已完成”状态。

## 10. 下一步

先完成 Phase 3 的全量构建、CTest、doctest 与耦合门禁复验；通过后将关键未跟踪文件、本批恢复改动和 Phase 3 生产桥接形成一个原子 Git 提交闭包。在此之前不得删除 `stash@{0}`。下一批优先进入 Phase 4 的焦点切换、Voice 完成回调和 PNG fallback 自动化验证；`run/eval` managed coroutine 与 HTTP 调试路由作为可并行的编辑器增强项单独实施。
