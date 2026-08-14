# live2d 模块审计

> 审计范围：`src/live2d/`（api/ 接口 + Live2DBackend + 4 条渲染路径 + NullAnimationBackend + PathConfinement）+ `tests/cpp/test_live2d.cpp` + `docs/guides/live2d-setup.md`
> 依赖宪章：`AGENTS.md` §1§2§3§4§7§8§9
> 审查方式：只读静态审查（未编译、未运行）

## 概述

live2d 模块提供动画后端，对外仅暴露 `api/IAnimationBackend.h`。两条实现路径：

- **Cubism SDK 路径**（`CAESURA_HAS_LIVE2D` 编译宏开启，`Live2DBackend`）：Windows 用 `D3D11NativeRenderPath`，macOS 用 `MetalNativeRenderPath` 或 OpenGL 路径，Linux 用 `OpenGLSharedRenderPath`/`OpenGLReadbackRenderPath`。每个模型渲染到独立离屏目标，再经 `bgfx::updateTexture2D`/`bgfx::overrideInternal` 交给 bgfx 合成。
- **PNG 降级路径**（无 SDK，`NullAnimationBackend`）：把 PNG/JPG/BMP 作为静态纹理加载并 blit 到主视图。

模块清晰、路径拆分明朗、接口纯净；**未发现 P0 级违反 AGENTS.md 铁律之处**。最严重问题是 Cubism 路径的 `playMotion()` 因 motionCache 从未填充而**完全失效**（P1 高风险），以及设备丢失/GPU 恢复未处理（P1）。

## P0 关键问题

逐项核对模块边界（include 范围、BackendRegistry 访问、组合根、接口纯净性、循环依赖、`../../../`/绝对路径 include），**未发现违反 AGENTS.md 铁律的条目**：

- `api/IAnimationBackend.h`：纯虚类、无数据成员、无第三方具体类型（仅 `std::string`/`cstdint`）——符合 §2。
- 跨模块 include 仅两类：(a) `render/api/ITextureManager.h`、`render/api/IRenderDevice.h`（api 头）；(b) `di/BackendRegistry.h`（§3 指定的唯一全局访问点，属 sanctioned 例外）。无 include 其他模块具体实现头文件。
- 组合根：`src/entry/Engine_Backends.cpp:103-125` 通过 `make_unique<Live2DBackend>()`/`make_unique<NullAnimationBackend>()` 创建具体后端、`dynamic_cast<Live2DBackend*>` 注入 render device——均在 entry（组合根）内，符合 §4。引擎内其他模块（script/rpc）仅通过 `IAnimationBackend` 接口 + BackendRegistry 访问。
- 无 `../../../`、无绝对路径 include：live2d 源码 include 最大深度为 `../../render/api/IRenderDevice.h`（自 `src/live2d/Live2D/` 上溯，合法）。
- 无循环依赖：live2d → render（api）/di，方向单纯。

**接近违规（值得记录、非 P0）的边界观察：**

| 文件:行 | 说明 |
|---|---|
| `src/live2d/Live2D/ILive2DRenderPath.h:4,26,32` | 该接口暴露 `bgfx::TextureHandle` 与 `CsmRendering::CubismRenderer*` 两个第三方具体类型。位于模块**内部**（非 `api/`），不跨模块边界，故不算 §7 违规；但若未来被提到 `api/` 对外暴露，会立即违反接口纯净性，需用引擎自有不透明句柄包裹。 |

## P1 重要问题

### P1-1 ｜ Cubism 路径 `playMotion()` 功能完全失效（motionCache 从未填充）
- **文件:行**：`src/live2d/Live2D/Live2DBackend.cpp:466-472,460-488`；声明 `Live2DBackend.h:73`
- **问题**：`motionCache` 成员声明了（Live2DBackend.h:73）并在 `playMotion()` 中只读查询（:466-472），但**全文件从未写入**——`loadModelInternal()` 只在 :383-392 填充了 `expressionCache`，未填充 motion 缓存。因此 `model.motionCache` 恒为空，`playMotion()` 永远走不到 `LoadMotion`/`StartMotion`，总是打印 "Motion not found" 并返回 false，动画控制核心能力不可用。RPC 层 `/api/live2d/*`（EditorServer.cpp:484 起）经此接口派发，故通过 RPC 播放动作同样失效。
- **修复建议**：在 `loadModelInternal()` 中仿照 expression 缓存，遍历 `model.setting->GetMotionCount()`，用 `GetMotionGroupName/`GetMotionFileName` 填充 `model.motionCache[motionName]`；`playMotion()` 直接 `motionCache.find(name)`（去掉 :468-471 有缺陷的子串扫描）。同时补一条能触达该路径的测试。
- **工作量**：M

### P1-2 ｜ `render()` 把 Live2D 输出画进 view 0（VIEW_RTT），与 NullAnimation 的 VIEW_MAIN 不一致
- **文件:行**：`src/live2d/Live2D/Live2DBackend.cpp:448`
- **问题**：`blitTexture(0, ...)` 硬编码视图 id 0（= `VIEW_RTT`，见 `IRenderDevice.h:11`：VIEW_RTT 最先渲染、VIEW_MAIN 次之合成 UI）。而 `NullAnimationBackend::render()` 用 `VIEW_MAIN`（NullAnimationBackend.cpp:132）。两条路径行为分叉：SDK 路径把角色画进"最先渲染、最后被 UI 遮罩"的 RTT 层，PNG 路径画进主合成层，层级语义不一致且硬编码魔法数字。若业务方期望 Live2D 角色在 UI 之下是刻意设计，也应在命名常量上统一表达。
- **修复建议**：统一为 `VIEW_MAIN`（或显式注释 RTT 层意图），改用命名常量而非裸 0；补测试断言两条后端路径 viewId 一致。
- **工作量**：S

### P1-3 ｜ `setOpacity()` 不夹紧/不校验有限性，渲染时 `uint8` 转换可能回绕
- **文件:行**：`src/live2d/Live2D/Live2DBackend.cpp:590-593,452`
- **问题**：`setOpacity` 直接 `model->opacity = opacity`，无 `std::isfinite`/clamp。渲染处 `static_cast<uint8_t>(model->opacity * 255.0f)`（:452）。`opacity < 0` 或多帧 `>1` → 乘以 255 后截断回绕到错误区间；NaN/Inf 传入则 UB。`NullAnimationBackend::setOpacity`（:106-112）做了 `isfinite + clamp(0,1)`，两条路径行为不一致。
- **修复建议**：`setOpacity` 内复用与 NullAnimation 一致的夹紧逻辑（`isfinite ? clamp(0,1) : 0`）。
- **工作量**：S

### P1-4 ｜ GPU 设备丢失/恢复（recoverDevice / IDeviceLostListener）未接入，Cubism 路径在渲染器重置后持过期资源
- **文件:行**：`src/live2d/Live2D/Live2DBackend.h:63-65,84-93`；`Live2DBackend.cpp:166-179,414-419`；`D3D11NativeRenderPath.cpp:42-48,61-73`
- **问题**：`Live2DModel` 持有的 `bgfx::TextureHandle bgfxTex`、`textures` 向量、D3D11 的 render target/SRV、以及 Cubism 静态 device-info 表（`CubismDeviceInfo_D3D11`）在渲染器丢失/重建（`IRenderDevice::recoverDevice`）后都会失效。live2d 未注册 `IDeviceLostListener`，缓存句柄不重建 → 重置后把过期 `bgfx.idx`/D3D11 指针喂给 bgfx → 花屏或崩溃。渲染模块已有完整 `IDeviceLostListener` 机制（`BackendRegistry::registerDeviceLostListener`）但 live2d 未使用。
- **修复建议**：让 `Live2DBackend`（或每个 render path）注册 `IDeviceLostListener`；收到 device-lost 后置空/销毁 bgfxTex、D3D11 target/SRV 与 Cubism device-info，在 `recoverDevice` 后按新设备重建 `SetConstantSettings` 与各模型离屏目标。
- **工作量**：L

### P1-5 ｜ 每帧热路径存在堆分配（OpenGL/Metal 读回路径）
- **文件:行**：`OpenGLReadbackRenderPath.cpp:111`（每帧 `std::vector<uint8_t> flipped`）；`MetalNativeRenderPath.cpp:121-122,128`（每帧 `bgfx::copy` + pixels vector）
- **问题**：`endFrame()` 每帧为翻转行序分配新的 `std::vector`，并每帧 `bgfx::copy` 分配一次 `bgfx::Memory`。1280×720×4 ≈ 3.6MB 的 CPU 拷贝 + 多次堆分配/帧，60fps 下每秒 ~440MB 读回流量与大量 malloc。文档承认这是 "GPU→CPU→GPU"，但未做任何复用。
- **修复建议**：复用 `flipped` 成员作为翻转缓冲；读回路径考虑双缓冲/行序交换避免 `bgfx::copy`；优先推进 OpenGLShared/D3D11 的 GPU 侧零拷贝路径（已实现）。
- **工作量**：M

### P1-6 ｜ OpenGLSharedRenderPath 每帧无条件 `overrideInternal`，未像 D3D11 路径按纹理变化门控
- **文件:行**：`OpenGLSharedRenderPath.cpp:139`；对比 `D3D11NativeRenderPath.cpp:219-222`
- **问题**：D3D11 路径用 `m_lastOverriddenTex` 避免每帧重复 `overrideInternal`（注释明确"overrideInternal recreates the SRV every call, so only do it when the texture actually changed"），而 OpenGLShared 路径每帧无条件调用。跨路径策略不一致，GL 路径白白增加每帧内部 SRV 重建开销。
- **修复建议**：为 OpenGLSharedRenderPath 增加同样门控（或与 D3D11 路径共用统一节流策略）。
- **工作量**：S

## P2 建议

- **P2-1｜CMake 命名不一致**：选项 `CAESURA_LIVE2D`（CMakeLists.txt:257）与编译宏 `CAESURA_HAS_LIVE2D`（:399-400）两套命名。虽然 `docs/guides/live2d-setup.md:108-111` 已记录对应关系、属有意为之，但代码里 grep 不到 `CAESURA_LIVE2D`（全用 `CAESURA_HAS_LIVE2D`），易混淆。建议同名或统一注释。（S）
- **P2-2｜陈旧/乱码注释**：`Live2DBackend.cpp:305-308` 注释声称 "model.setting was set to nullptr by cubismLog"——`model.setting` 实际在 :310 才被赋值，cubismLog 与 setting 无关，注释误导；:296、:426、:457 出现 "`→?`Cubism 5 API"/"`→?`bgfx" 乱码。建议清理。（S）
- **P2-3｜playMotion 子串扫描逻辑缺陷**：`Live2DBackend.cpp:468-471` 对 `motionCache` 键做子串匹配，循环内 `mit = model.motionCache.find(key)` 冗余（`key` 本身就是 map 键，find 恒命中当前键），且 `data` 解构未用。即使修好 P1-1 也应重写。（S）
- **P2-4｜`ILive2DRenderPath::createRenderer()` 是死接口方法**：三个实现（D3D11/Metal/OpenGLShared/Readback）全部 `return nullptr`（renderer 实际由 `CubismUserModel::CreateRenderer` 创建），从未被调用。建议从接口移除。（S）
- **P2-5｜live2d-setup.md 文档与代码漂移**：`:120,137` 仍称 Metal 为 "stub（init() 恒失败）"，但 `MetalNativeRenderPath.cpp` 已是完整实现（离屏 + 同步读回）；`:135` 称 OpenGLShared 路径"从未编译"，但 CMake 现已在 Apple/Linux 加入该源。建议更新文档状态表并标注 Metal 待 macOS 实机验证。（S）
- **P2-6｜测试缺口**：`test_live2d.cpp` 对 `NullAnimationBackend` 与 `PathConfinement` 覆盖很好（纹理生命周期、failure 不分配句柄、shutdown 幂等、路径穿越防护均有单测），但**没有任何 Cubism 路径测试**——受限于 SDK 可选、CI 无 SDK/GPU，可接受；值得注意的是：若不将 Cubism 路径纳入带 SDK 的验证，P1-1 的 motionCache 缺陷会被长期掩盖（文档记录的 2026-08-01 D3D11 手工验证只跑通 load+render，未验证 playMotion）。（M）
- **P2-7｜loadModel 失败哨兵不一致**：`Live2DBackend::loadModel` 失败返回 `-1`（Live2DBackend.cpp:543,553），`NullAnimationBackend::loadModel` 失败返回 `0`（:49,54,64）。两者都满足接口注释"non-positive"，但若调用方按 `==0` 判断失败会把 -1 当成功。建议统一哨兵或文档化。
- **P2-8｜日志不统一**：`NullAnimationBackend.cpp` 用 `printf`/`fprintf`（:28,53,62,73），绕过引擎 DEBUG 宏/日志系统；`Live2DBackend` 用 `SDL_Log*`。建议统一走引擎日志层。（S）

## 耦合分析

- **跨模块依赖（编译期 include）**：
  - `render`（经 api：`render/api/IRenderDevice.h`、`render/api/ITextureManager.h`）
  - `di`（`di/BackendRegistry.h`——§3 指定唯一访问点，属 sanctioned 例外）
  - 合计 **2 个模块**。
- **依赖方向**：live2d → render、di；无反向、无循环。
- **BackendRegistry 访问**：`NullAnimationBackend::init()` 经 `BackendRegistry::instance()` 取 textureManager/renderDevice（NullAnimationBackend.cpp:24-26）；`Live2DBackend::setRenderDevice` 由组合根注入。二者均未绕过注册表直接 new 单例。✓
- **对照耦合预算（AGENTS.md §9）**：live2d 属"其他"类，目标 ≤4。实际 2（render + di）≤ 4。**合规，余量充足。**
- **注**：`entry` 组合根为构造 live2d 依赖 render/di/live2d 具体头（Engine_Backends.cpp:2,20-21 等），属组合根特权，不计入 live2d 自身耦合。

## 审查结论

live2d 模块整体架构质量良好：对外接口纯净、双路径降级清晰、路径穿越防护（PathConfinement）与资源释放（~Live2DModel、D3D11 target 释放）写得扎实、NullAnimation 与路径防护测试完备，且**未发现任何 P0 级违反 AGENTS.md 铁律的问题**，耦合（2 ≤ 4）合规。

但 Cubism 主路径存在两处高风险功能性硬伤，必须优先修复：
1. **`playMotion()` 因 `motionCache` 从未填充而完全失效**（P1-1，需补模型遍历填充 + 测试）；
2. **GPU 设备丢失/恢复未接入，重置后持过期 bgfx/D3D11/Cubism 资源**（P1-4）。

另有两处低风险一致性问题（P1-2 view id、P1-3 setOpacity 夹紧）、两处热路径/跨路径不一致（P1-5、P1-6）与若干 P2 清理项。修复优先级建议：P1-1 → P1-4 → P1-3/P1-2 → P1-6 → P1-5。P1-1/P1-2/P1-3/P1-6 均为小到中工作量，可在后续迭代随带修完。
