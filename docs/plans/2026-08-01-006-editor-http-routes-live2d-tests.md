# Caesura (AmeKAG) — closeout 006：编辑器 HTTP debug routes + Live2D 路径测试收尾（2026-08-01）

> 范围：handoff 004 §4 (b) 编辑器 HTTP debug routes + Live2D 安全加固测试收尾（abc 全做，用户确认）。

## (b) 编辑器 HTTP debug routes — D1 刷新为 Full

### 新增 8 条 HTTP 路由（`src/rpc/EditorServer.cpp`，10 → 18 条）
- `POST /api/eval`：Lua 代码求值，返回 `{result}`（运行时错误 → 500 带消息，编译错误 → 400）
- `GET /api/debug/getState`：当前场景
- `GET /api/debug/getFrame?w=&h=`：帧捕获 base64（范围校验 1..8192）
- `POST /api/debug/setBreakpoint` / `removeBreakpoint`：`{source, line}`
- `POST /api/debug/clearBreakpoints`
- `POST /api/debug/continue`：可选 `{pauseId}`；无活跃 pause → `stale_pause` 语义化错误
- `GET /api/debug/inspect?name=&frame=&global=`：局部/全局变量检查；无 pause → `inspection_unavailable`

全部复用 `dispatchRequest` + `setDispatchError` + `invalidDispatcherReply`，经 `IRpcDispatcher` 与 stdio 方法集对齐（18 条路由覆盖 dispatcher 全部 14 操作）。

### HTTP 端到端冒烟测试（新增 `tests/headless_http_smoke.py`，142 行，14 断言）
- `--editor` 模式启动真实引擎，HTTP 验证：ping / status / eval（成功 + 错误）/ getState / 断点生命周期 / continue / inspect / getFrame / live2d/load 路由 / engine-alive
- 注册 ctest `CaesuraHeadlessHttpSmoke`（LABELS entry;headless;cli;rpc;http，TIMEOUT 90，WORKING_DIRECTORY 指向 exe 目录）
- 验证：14/14 PASS；ctest 9 → 10 全过（原用例不减少）
- 修复过程中发现并纠正 CMake 注册块结构问题（RpcSmoke 块 WORKING_DIRECTORY 悬空）

## Live2D 路径测试收尾

### `confineToModelRoot` 可测化（`src/live2d/PathConfinement.h/.cpp`）
- 从 `Live2DBackend.cpp` 提取路径包含逻辑到独立文件，**无 Cubism SDK 依赖**，`cmake/CaesuraModules.cmake` `caesura_add_module(Live2D)` 始终编译
- `Live2DBackend.cpp` 改为调用（readFile/loadModel 两处），行为不变（Haru HTTP 回归 4/4 PASS 确认）

### 单元测试（`tests/cpp/test_live2d.cpp` 新增 6 用例，557 → 563）
- 绝对路径拒绝（C:/Windows/win.ini、UNC）；`..` 逃逸拒绝（3 形态）；dot 组件（`.`=根自身允许、`..`=父级拒绝）；根内通过；前缀 lookalike 边界（root+X 拒绝）；symlink 拒绝（temp 目录尽力创建，无权限跳过）

## 验证结果
- 双构建（build-repro-verify 无 SDK + build-live2d）Debug 零错误
- CaesuraTests 563/563 × 2（2760 assertions）；ctest 10/10 × 2；耦合度 PASS
- Haru HTTP 回归 4/4 PASS；HTTP 路由端到端 9/9 PASS

## 变更文件
- `src/live2d/PathConfinement.h`（新）、`src/live2d/PathConfinement.cpp`（新）
- `src/live2d/Live2D/Live2DBackend.cpp`（提取调用）
- `cmake/CaesuraModules.cmake`（Live2D 模块源列表）
- `src/rpc/EditorServer.cpp`（8 条新路由）
- `tests/cpp/test_live2d.cpp`（6 新用例）
- `tests/headless_http_smoke.py`（新）、`tests/CMakeLists.txt`（ctest 注册）
- `docs/design/engine-capability-matrix.md`（D1 → Full）

## 遗留
- HTTP 路由无鉴权（editor 本地 127.0.0.1，与 stdio 同信任边界；若需远程暴露应加 token——记录于 handoff 004 (d) 同类）
- `confineToModelRoot` junction 用例仅 Windows 显式覆盖；6to4/Teredo 嵌入式 IPv4 未标记（`_http_utils.py` 预存项，非本模块）
