# Caesura (AmeKAG) — closeout 005：Live2D D3D11 验证 + game_logic.lua 迁移（2026-08-01）

> 交接文档：`docs/plans/2026-08-01-004-delivery-handoff.md` §4 剩余清单 (a) 与 (c)。
> 范围：用户确认做 (a)+(c)，跳过 (b) 编辑器 HTTP debug routes。

## (a) Live2D Cubism SDK 验证 — Windows D3D11 路径首次真实编译+运行

### 背景
- 此前 `CAESURA_HAS_LIVE2D` 守卫内的 Cubism 代码从未被编译（无 SDK），capability matrix C1 标注 "code-ready but unverified"。
- 静态审查（子代理）预判：3 个编译硬错误 + 系列运行时缺陷。

### 编译修复（首次编译）
1. `Live2DModel` 补 `textures` 成员（`Live2DBackend.h`）。
2. 补 `createBgfxTexture` 本地辅助（替代不存在的 `ILive2DRenderPath::createTexture` 静态）。
3. `blitTexture(0, model->bgfxTex.idx, ...)`（接口形参为 `uint32_t`）。
4. 补 `#include "../../render/api/IRenderDevice.h"`（`Live2DBackend.cpp`）。

### 运行时修复（真实运行暴露）
1. `CubismRenderer_D3D11::SetConstantSettings(1, device)` — 模型加载前必须（消除 `ContextNum has not been set`）。
2. `beginFrame`：绑定共享纹理 RTV + `StartFrame/DrawModel/EndFrame` + 恢复 bgfx 渲染目标。
3. `endFrame`：Cubism 已画入共享纹理，直接 `bgfx::overrideInternal`（删除 `CopyResource` 通路）。
4. `cubismModel->Update()` 每帧顶点更新（此前模型静止）。
5. 模型纹理：D3D11 SRV（`createModelTexture`）+ `CubismRenderer_D3D11::BindTexture`（此前纹理从未绑定，画面空白）。
6. shader：`FrameworkShaders/*.fx` CMake 复制到输出目录 + `LoadFileFunction/ReleaseBytesFunction` 回调（消除 `File loader is not set` / `Fail Compile shader` / DXGI 设备丢失）。
7. renderer double-free 修复（去掉显式 `CubismRenderer::Delete`，由 `~CubismUserModel` 管理）。
8. `CubismFramework::Option` 栈对象悬垂 → static（`s_option` 长期持有指针）。

### 验证结果
- 配置：`cmake -B build-live2d -DCAESURA_LIVE2D=ON` → `Live2D: D3D11 renderer for Windows/AMD64`。
- 构建：Debug 零错误；`FrameworkShaders/` 复制到输出目录。
- 测试：`CaesuraTests` **557/557**（Live2D 构建与无 Live2D 构建均通过），`ctest` 9/9，耦合度 `--ci` PASS。
- 真实运行：`--editor` HTTP `POST /api/live2d/load` 加载 SDK `Haru.model3` 成功（`modelId 1`），渲染多帧无崩溃、无设备丢失、无 shader 编译错误、无 ContextNum 警告。
- 修复前基线：`Fail Compile shader` + `DXGI_ERROR_DEVICE_REMOVED`（设备丢失）→ 修复后全部消失。

### 遗留
- OpenGLShared / OpenGLReadback 路径未编译验证（无 Linux/macOS 环境）；Metal 维持 stub（需 macOS 开发者）。
- 画面像素级正确性未做视觉确认（管线全绿、无错误日志）。
- `getInternalData()`/`overrideInternal()` 依赖 bgfx 单线程渲染（`BGFX_CONFIG_MULTITHREADED=0`）——已确认当前配置安全；若未来启用多线程渲染需重新评估。
- 中文路径模型加载报 `No mapping for the Unicode character...`（Windows ACP 限制），验证使用 ASCII 路径副本。

## (c) scripts/game_logic.lua 迁移（本地未入库脚本）

- L1 断言 `System.save/System.load` 删除（`system.lua` 保留其他函数）；L3d 改为 `KAG.save_game(0, saved, "L3d", token)` + `data, meta = KAG.load_game(0)` + `meta.token_index` 断言（参照 a519a662 受跟踪 fixture 迁移模式）。
- 引擎实跑：**PASS 43 / FAIL 0 / RESULT: ALL PASSED!**（临时 `config.entry_script = "game_logic.lua"`，验证后还原）。
- 注意：脚本自退出受沙箱 lockdown 限制（`_autoQuitFrame` 全局创建被拒）——本地验证需 timeout 终止进程，属既有行为。

## 文档更新
- `docs/design/engine-capability-matrix.md` C1：D3D11 (Windows) 路径 **verified 2026-08-01**。
- `docs/guides/live2d-setup.md`：状态表 D3D11 行、审计横幅、要点、验证路线图步骤 2/3、新增「2026-08-01 验证记录」小节。

## 变更文件
- `src/live2d/Live2D/Live2DBackend.h/.cpp`
- `src/live2d/Live2D/D3D11NativeRenderPath.h/.cpp`
- `CMakeLists.txt`（FrameworkShaders 复制）
- `docs/design/engine-capability-matrix.md`、`docs/guides/live2d-setup.md`
- `scripts/game_logic.lua`（本地未入库，不提交）

## 测试与回归
- 无 Live2D 主构建（build-repro-verify 重建，修复 CMakeCache 旧路径）：零错误、557/557、ctest 9/9。
- Live2D 构建（build-live2d）：零错误、557/557、ctest 9/9。
- 耦合度：PASS。
