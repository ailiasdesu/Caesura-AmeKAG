# Caesura (AmeKAG) — 交接后续执行总结（2026-07-31）

> 依据 `docs/plans/2026-07-31-002-continuation-handoff.md` 第 3 节第 2、3 项执行：
> **P3-2 Live2D**（无 SDK → 文档化）与 **P4-3 MobileAdapter**（实现可验证核心 + 降级文档化）。

## 1. P3-2 Live2D Cubism — 审计与文档化

### 结论
本地与 CI 均无 Cubism SDK（`CubismSdkForNative-5-r.5` 不存在），`CAESURA_LIVE2D` 默认 OFF，
所有 `#if defined(CAESURA_HAS_LIVE2D)` 守卫内的代码在 CI/本地构建中**从未被编译**。
因此选择交接文档允许的「无 SDK 则文档化状态」路径，不引入无法验证的构建配置。

### 审计结果（逐路径）
| 路径 | 平台 | 状态 |
|------|------|------|
| `D3D11NativeRenderPath` | Windows | 代码就绪·未验证（共享 bgfx D3D11 设备 + RTV/SRV 共享纹理 + `CopyResource` + `bgfx::overrideInternal`） |
| `OpenGLSharedRenderPath` / `OpenGLReadbackRenderPath` | Win/Linux/macOS | 代码就绪·未验证（CMake 仅 Apple/Linux 编译 OpenGL 路径，Windows 仅 D3D11） |
| `MetalNativeRenderPath` | macOS/iOS | **STUB**（`name()` 返回 `"MetalNative(STUB)"`，`init()` 恒失败） |
| `NullAnimationBackend`（PNG 降级） | 全部 | ✅ 已测试默认路径（`tests/cpp/test_live2d.cpp`） |

### 产出
- `docs/guides/live2d-setup.md`：新增「当前实现状态（2026-07-31 审计）」章节（状态表 + 要点 + 验证路线图），
  修正渲染路径选择表使其与 `Live2DBackend.cpp` 实际逻辑一致（macOS 依赖 bgfx 渲染器类型；
  Metal stub 失败后由引擎层整体回退 `NullAnimationBackend`，而非文档原先声称的 OpenGL 回退）。
- `docs/design/engine-capability-matrix.md` C1 行：细化状态并指向 live2d-setup.md。

### 后续（有 SDK 权限者）
按 live2d-setup.md「验证路线图」：下载 SDK → `-DCAESURA_LIVE2D=ON` 编译 → Windows D3D11 加载 `.moc3`
→ 逐路径验证 → 回填状态表与 C1 行。**Metal 路径需要 macOS 开发者实现。**

## 2. P4-3 MobileAdapter — 实现 + 降级文档化

`MobileAdapter`（`src/platform/`）原为半实现：生命周期 Lua 回调与 touch→mouse 事件注入已有，但
存在状态机缺陷，`onPinch` 为空，无行为级测试。

### 修复与实现（`src/platform/MobileAdapter.cpp` / `.h`）
1. **触摸计数状态机**：`onFingerDown` 越界/重复 down 不再重复计数；`onFingerUp` 对越界或
   从未 down 的手指不再递减（消除下溢）；`onFingerMotion` 仅处理已跟踪手指。
2. **`onPinch` 真实实现**：首次调用建立基线（`m_lastPinchScale`），后续将 scale 增量映射为
   `SDL_EVENT_MOUSE_WHEEL`（`wheel.y = delta * 100`，光标位置写入 `mouse_x/mouse_y`）；
   新增 `resetPinch()` 结束手势、`getLastPinchScale()` 查询基线。
3. **`onLongPress`**：保持右键点击合成，注释明确按压计时（>500ms）由平台层负责。
4. 头文件注释更新为真实状态（不再声称 "placeholder stubs"）。

### 测试（`tests/cpp/test_mobile_adapter.cpp`，+11 用例）
利用 SDL3 `SDL_PushEvent` 在入队前**同步**调用 `SDL_AddEventWatch` watcher 的机制，无需
`SDL_Init`/窗口即可确定性验证事件注入（watcher 以 RAII guard 管理，REQUIRE 失败时安全清理）：
- 多指计数、重复 down 不重复计数、越界/负数 fingerId 忽略、up 未 down 不下溢
- 注入事件坐标按 `displayScale` 缩放（down/motion/up 全链路），`button.down` 语义正确
- 重复 down 与未跟踪手指 motion 不注入事件；长按注入右键 down+up 对
- pinch 基线→增量→回缩→无增量不注入；`resetPinch` 结束手势；onResume 在 null Lua 状态下携带 savedData 参数安全（Lua 回调路径未在单测覆盖）
- NaN/Inf 输入拒绝（5 个事件入口 + setDisplayScale，防 pinch 基线被毒化）

### 降级文档化
- capability matrix 新增 **P7** 条目：核心映射已实现并测试（18 用例）；**未接入 Engine 生命周期、
  无移动平台原生集成**（接入时需按 AGENTS.md 流程：`I*` 接口 → 实现 → BackendRegistry → Engine::init 注册）。

## 3. 验证结果（Windows 本地）

| 检查 | 结果 |
|------|------|
| `cmake --build build-repro-verify --config Debug --parallel` | ✅ 零错误 |
| `CaesuraTests.exe`（build/tests/Debug CWD） | ✅ **548/548**（537 + 11 新增），断言 2679/2679 |
| `ctest -C Debug --test-dir build-repro-verify` | ✅ 全部通过（13.45s） |
| `python scripts/count_coupling.py --ci` | ✅ PASS（platform 0/4 跨模块） |

## 4. 提交

- `fix(platform): harden MobileAdapter touch state machine, implement pinch→wheel mapping`
  （含测试扩展）
- `docs: live2d status audit (P3-2), MobileAdapter closeout (P4-3), capability matrix updates`

## 5. 遗留事项

1. **Live2D**：需 SDK 才能编译验证全部 Cubism 路径；Metal 需 macOS 开发者实现。
2. **MobileAdapter**：未接线（无 I* 接口、无 Engine 生命周期挂钩）；SoLoud 暂停逻辑未实现；
   pinch 映射系数（`kPinchToWheelScale = 100`）为经验值，接入真实设备后可调。
3. 交接文档第 3 节第 4/5 项（DeltaCARC、SaveManager 清理、删除 `codex/engine-audit-hardening` 分支）未处理，仍为可选后续。
