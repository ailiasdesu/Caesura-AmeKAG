---
title: 引擎模块接口隔离实施规划
type: refactor
status: active
date: 2026-06-11
origin: docs/brainstorms/2026-06-11-interface-isolation-requirements.md
---

## Summary

以组合根模式逐模块切断跨模块 #include 违规：main.cpp 集中创建具体实现，通过 EngineConfig 聚合结构体注入上层。6 步解耦按风险排序，每步独立可构建、三端 CI 全绿。

## Problem Frame

Caesura 引擎 11 个模块存在 Core↔Render 循环引用、Audio→Scripting 越层、Scripting 绑定 7 模块、CARC↔Resource 双向依赖、BgfxRenderDevice 52KB 巨石等耦合问题。BackendRegistry 已有 9 个纯虚接口但未强制用作通信边界。修复任意模块 Bug 可能连锁触发其他模块回归。

## Requirements

Traceable to `docs/brainstorms/2026-06-11-interface-isolation-requirements.md` (R1–R17).

### Composition Root Pattern

- R1. main.cpp 是所有具体子系统实现的唯一创作点。
- R2. 具体实现以抽象接口指针注入消费者，消费者不引用具体实现头文件。
- R3. Engine.h 只包含 EngineConfig 所需的抽象接口头文件。
- R4. 每层模块公共头文件只暴露抽象接口。

### Engine Construction

- R5. Engine 构造函数接受 `const EngineConfig&`，EngineConfig 为聚合结构体，每个子系统一个抽象接口指针字段。
- R6. 可选子系统字段默认 `nullptr`，不在构造函数参数列表中参与条件编译。
- R7. main.cpp 使用 C++20 指定初始化器构造 EngineConfig。

### Decoupling Steps (risk-ordered)

- R8. Step 1 Core↔Render: Engine.h 不再 include BgfxRenderDevice.h。
- R9. Step 2 Audio→Scripting: SoLoudAudioEngine 不再 include LuaManager.h 或 Engine.h。
- R10. Step 3 Render→Core: BgfxRenderDevice 不再 include Engine.h。
- R11. Step 4 Scripting de-spiderweb: 各 binding 模块跨模块 include 替换为 BackendRegistry 接口访问。
- R12. Step 5 CARC↔Resource: 消除双向依赖。
- R13. Step 6 BgfxRenderDevice 拆分为 5 个编译单元。
- R14. 每步独立可构建、可测试，不破坏已完成步骤。

### Verification

- R15. 每步后 Win/Mac/Linux 三端 CI 构建通过，零新增警告。
- R16. 每步后全部 138 个测试继续通过。
- R17. 维护耦合度计数：Core 目标从跨 9 模块降到跨 ≤3 模块。

## Key Technical Decisions

- **EngineConfig 独立头文件。** EngineConfig 定义在 `src/Core/EngineConfig.h` 而非 Engine.h 内——Engine.h 仍会被许多文件 include，EngineConfig 抽离为独立头文件后，上层 include EngineConfig 不会引入 Engine.h 的依赖链。

- **SoLoud 事件回调接口。** `IAudioBackend` 新增 `setEventCallback(std::function<void(int,const char*)>)`，Engine 在 main.cpp 层注册回调将音频事件转发到 Lua。SoLoud 不再知道 LuaManager 的存在。

- **BgfxRenderDevice 服务注入。** 当前 BgfxRenderDevice.cpp include Engine.h 主要为获取帧时间戳（TDR 保护）和沙箱配额。将这两个值通过 `BgfxRenderDevice::setFrameTick(uint64_t)` 和已有的 `SandboxQuota` 接口传入，不再直接依赖 Engine.h。

- **Scripting 统一走 BackendRegistry。** 当前 KAGBinding/RenderBinding/VFXBinding 各自直接 include 具体模块头文件——改为通过 `BackendRegistry::instance().getRenderDevice()` 等方法获取接口指针，消除对 Render/Audio/MiniGame/Resource 的直接依赖。

- **CARC↔Resource 通过 ProviderChain 解耦。** ProviderChain 已经实现多级 IAssetProvider 链——将 CARC 对 Resource 的引用改为通过 ProviderChain 间接获取，消除双向 include。

- **BgfxRenderDevice 拆分顺序：ShaderManager → DeviceCore → Draw → DebugCallback。** ShaderManager 为纯资源管理无反向依赖，先抽；DeviceCore 持有画布尺寸和 RTT 表，依赖 ShaderManager；Draw 依赖前两者；DebugCallback 独立。最后 BgfxRenderDevice 退化为门面类。

## Implementation Units

### U1. EngineConfig extraction

- **Goal:** 定义 EngineConfig 聚合结构体，重构 Engine 构造函数签名。
- **Files:** `src/Core/EngineConfig.h` (new), `src/Core/Engine.h`, `src/Core/Engine.cpp`, `src/main.cpp`
- **Pattern:** C++20 聚合结构体 + 指定初始化器。参考 `src/Core/IPlatformBackend.h` 的接口定义风格。
- **Test Scenarios:**
  - 默认构造的 EngineConfig 所有字段为 nullptr，Engine 可正常处理。
  - 部分填充的 EngineConfig（仅 render+audio+platform）正常初始化，其余子系统为 nullptr 不崩溃。
  - main.cpp 使用指定初始化器构造 EngineConfig，编译通过且行为等价于当前代码。
- **Verification:** 构建通过，138 测试全绿。Engine.h 的 include 列表减少（移除至少 2 个具体实现头文件引用）。

### U3. Audio→Scripting decoupling (Step 2)

- **Goal:** SoLoudAudioEngine.cpp 不再 include `LuaManager.h` 和 `Engine.h`。音频事件通过 `IAudioBackend::setEventCallback` 回调上报，回调注册在 main.cpp 完成。
- **Files:** `src/Core/IAudioBackend.h`, `src/audio/SoLoudAudioEngine.h`, `src/audio/SoLoudAudioEngine.cpp`, `src/main.cpp`
- **Pattern:** 回调接口模式。IAudioBackend 新增纯虚方法 `setEventCallback(ScriptCallback cb)` 或通过 `std::function` 实现。参考 BackendRegistry 中已有的事件通知模式。
- **Test Scenarios:**
  - 音频播放结束触发回调，Lua 层接收到 `audio_end` 事件且行为不变。
  - 回调未注册时 SoLoud 正常工作（回调调用被跳过）。
  - headless 模式下 NullAudioBackend 的 setEventCallback 为空实现。
- **Verification:** 构建通过，138 测试全绿。SoLoudAudioEngine.cpp 的 include 列表中无 LuaManager.h 和 Engine.h。

### U2. Core↔Render decoupling (Step 1)

- **Goal:** `Engine.h` 不再 include `BgfxRenderDevice.h`。BgfxRenderDevice 创建移至 main.cpp，通过 `EngineConfig.render` 注入。
- **Files:** `src/Core/Engine.h`, `src/Core/Engine.cpp`, `src/main.cpp`, `src/render/BgfxRenderDevice.h`
- **Pattern:** main.cpp 中 `auto render = std::make_unique<BgfxRenderDevice>()`，init 后传入 EngineConfig。Engine 通过 `IRenderDevice*` 持有，不再直接构造 BgfxRenderDevice。这是组合根模式的首次应用，后续模块套用同一模板。
- **Test Scenarios:**
  - `main.cpp` 构造 BgfxRenderDevice 并注入 Engine，渲染行为不变（Demo 场景正常显示）。
  - Engine.h 编译依赖中移除 BgfxRenderDevice.h；任何源文件 include Engine.h 不需再编译 BgfxRenderDevice.h。
  - headless 模式下 EngineConfig.render 为 nullptr，Engine 安全降级。
- **Verification:** 构建通过，138 测试全绿。`rg '#include.*BgfxRenderDevice' src/Core/Engine.h` 返回空。

### U4. Render→Core decoupling (Step 3)

- **Goal:** `BgfxRenderDevice.cpp` 不再 include `Engine.h`。渲染层所需运行时数据通过注入接口获取。
- **Files:** `src/render/BgfxRenderDevice.cpp`, `src/render/BgfxRenderDevice.h`, `src/Core/Engine.h`
- **Pattern:** 当前 BgfxRenderDevice 依赖 Engine.h 获取帧时间戳（TDR 检测）和沙箱配额。将帧时间戳通过 `setFrameTick(uint64_t)` setter 传入，沙箱配额已通过 SandboxQuota 接口可用。移除 include 后若发现其他隐蔽依赖逐个注入。
- **Test Scenarios:**
  - TDR 检测仍正常工作（长时间帧触发 GPU 重置）。
  - 沙箱配额检查行为不变。
  - 编译边界验证：`rg '#include.*Engine.h' src/render/` 返回空。
- **Verification:** 构建通过，138 测试全绿。

### U5. Scripting de-spiderweb (Step 4)

- **Goal:** 消除 Scripting 模块对 Render/Audio/MiniGame/Resource/Steam/Debug 的直接 include。所有 binding 统一通过 BackendRegistry 获取接口引用。
- **Files:** `src/Scripting/KAGBinding.cpp`, `src/Scripting/RenderBinding.cpp`, `src/Scripting/VFXBinding.cpp`, `src/Scripting/UnifiedBinding.cpp`, `src/Scripting/DevCoreBinding.cpp`, `src/Scripting/DebugBinding.cpp`, `src/Scripting/SteamBinding.cpp`
- **Pattern:** 当前每个 binding 直接 include 具体模块头文件（如 KAGBinding → `Render/IRenderDevice.h` + `Core/IAudioBackend.h` + `Core/InputRouter.h`）。改为通过 `BackendRegistry::instance().getRenderDevice()` 等方法获取——BackendRegistry 已经提供 getRenderDeviceFromLua 等 Lua C 函数辅助方法。IRenderDevice.h 和 IAudioBackend.h 仍可保留（它们是抽象接口），但具体实现头文件必须移除。
- **Test Scenarios:**
  - 84 个 KAG API 全部可调用，行为不变。
  - MiniGame 15 个 Lua API 全部可用。
  - VFX、transition、video API 可用。
- **Verification:** 构建通过，138 测试全绿。`rg '#include.*(TextureManager|VideoPlayer|ParticleSystem|Engine\.h)' src/Scripting/` 返回空。耦合度计数：Scripting 从跨 7 模块降到跨 ≤3 模块（仅接口）。

### U6. CARC↔Resource bidirectional decoupling (Step 5)

- **Goal:** 消除 CARC（`CARCReader`, `CarcAssetProvider`）与 Resource（`AssetManager`, `IAssetProvider`）之间的双向 include。
- **Files:** `src/CARC/CarcAssetProvider.h`, `src/CARC/CarcAssetProvider.cpp`, `src/resource/AssetManager.h`, `src/resource/AssetManager.cpp`, `src/resource/ProviderChain.h`
- **Pattern:** CarcAssetProvider 当前 include `Resource/IAssetProvider.h`（通过 ProviderChain 已桥接），AssetManager include `CARC/CARCReader.h` 和 `CARC/CarcAssetProvider.h`。将 AssetManager 对 CARC 的具体依赖替换为通过 IAssetProvider 抽象接口和 ProviderChain 间接访问。ProviderChain 作为中介持有 CarcAssetProvider 实例并向上暴露 IAssetProvider 接口。
- **Test Scenarios:**
  - CARC 归档文件正常加载（Reader/Writer 功能不变）。
  - 资产链（Dir → XP3 → CARC）正常解析。
  - DeltaCARC 增量更新功能不变。
- **Verification:** 构建通过，138 测试全绿。`rg '#include.*CARC' src/resource/ | rg -v 'CarcAssetProvider'` 返回空（仅 ProviderChain 引入 CarcAssetProvider.h）。Resource 模块不再直接 include CARC 头文件。

### U7. BgfxRenderDevice split (Step 6)

- **Goal:** 将 1170 行的 BgfxRenderDevice 拆分为 5 个编译单元。
- **Files:** `src/render/BgfxDeviceCore.h/.cpp` (new, ~300 行), `src/render/BgfxShaderManager.h/.cpp` (new, ~150 行), `src/render/BgfxDraw.h/.cpp` (new, ~450 行), `src/render/BgfxDebugCallback.h/.cpp` (new, ~50 行), `src/render/BgfxRenderDevice.h/.cpp` (修改，退化为门面)
- **Pattern:** 按抽离顺序执行：
  1. BgfxShaderManager — 内嵌 shader 编译 (`initEmbeddedShaders`, `buildBgfxShader`) 和所有 ProgramHandle/UniformHandle 存储。纯资源管理，无正向依赖。
  2. BgfxDeviceCore — init/shutdown/resize/frame loop/view 设置/RTT 管理。持有画布尺寸和 m_rttMap。依赖 BgfxShaderManager。
  3. BgfxDraw — 批量四边形、纹理 blit、stretchBlt/affineBlt、全屏特效（submitBlend/submitTransition/submitVFX/fillViewport）。依赖前两者。
  4. BgfxDebugCallback — 全局 Debug 回调单例，setBgfxShuttingDown。独立。
  5. BgfxRenderDevice 退化为门面——持有上述 4 个组件 unique_ptr，IRenderDevice 虚方法委托到对应组件。
- **Test Scenarios:**
  - 每步抽离后编译通过且渲染行为不变（Demo 三场景正常）。
  - 门面类所有 IRenderDevice 方法正确委托，无需空指针检查。
  - CMakeLists.txt 新增源文件后构建正常。
- **Verification:** 构建通过，138 测试全绿。BgfxRenderDevice.cpp 从 1170 行缩减到 ≤200 行（门面委托代码）。

### U8. Verification automation

- **Goal:** 实现耦合度计数脚本和 CI 门禁集成，确保解耦效果可量化、可回归。
- **Files:** `scripts/count_coupling.py` (new), `.github/workflows/ci.yml` (modify)
- **Pattern:** Python 脚本扫描 `src/` 下所有 `.h` 和 `.cpp` 文件，按模块统计跨模块 `#include` 计数，输出 Core 和 Scripting 的跨模块引用数。CI 在构建步骤后运行该脚本，若 Core 跨模块引用数上升则报警（不阻断 CI）。
- **Test Scenarios:**
  - 脚本输出当前 Core 跨 9 模块（基线值），Scripting 跨 7 模块。
  - 完成 U2 后 Core 跨模块数 ≤8。
  - 完成全部 6 步后 Core 跨模块数 ≤3，Scripting 跨模块数 ≤3。
- **Verification:** 脚本可独立运行，输出格式为 `Core: N modules, Scripting: M modules`。CI yml 包含该步骤。

## Scope Boundaries

- Live2D 和 Steam 的条件编译隔离不在此范围——已通过 `CAESURA_LIVE2D`/`CAESURA_HAS_STEAM` 宏实现干净隔离。
- web-editor 的 RPC 边界不在此范围——编辑器已是 JSON-RPC 接口通信。
- 功能级 Bug 修复不在此范围——只动模块边界，现有行为不改变。
- 性能优化不在此范围——不引入额外的虚函数调用或间接层开销评估。

## Risks & Dependencies

- **风险：编译时间增加。** 拆分后文件数增加，全量重编译时间可能上升。缓解：拆分出的新编译单元粒度合理（每个 50–450 行），增量编译受益。
- **风险：EngineConfig 字段遗漏。** 若某个子系统当前在 Engine::init() 中被构造但未被显式识别为 EngineConfig 字段，运行时崩溃。缓解：U1 阶段逐个对照 Engine::init() 中的构造调用点确保纳入 EngineConfig。
- **风险：回调注册顺序。** U3 的音频回调若注册时机晚于音频初始化，可能丢失早期事件。缓解：main.cpp 中先创建 SoLoud → 注册回调 → 再 init。
- **依赖：BgfxRenderDevice 拆分（U7）需在 Core↔Render 解耦（U2）之后执行——拆分前的 #include 清理使拆分后的依赖更清晰。**
- **依赖：全部 6 步解耦完成后才能达到 R17 耦合度目标（Core ≤3 模块），中间步骤的耦合度计数仅作进度指示。**

## Sources / Research

- `src/Core/Engine.h` — 当前 include 列表和子系统持有方式
- `src/Core/Engine.cpp` — Engine::init() 构造序列
- `src/Core/BackendRegistry.h` — DI 容器模式
- `src/Core/IPlatformBackend.h` — 抽象接口参考风格
- `src/render/BgfxRenderDevice.cpp` — 关注点拆分分析（1170 行，按函数边界 4 路切分）
- `src/main.cpp` — 当前 Engine 构造模式
- `src/audio/SoLoudAudioEngine.cpp` — Audio→Scripting 越层引用点
- `docs/brainstorms/2026-06-11-interface-isolation-requirements.md` — 上游需求文档
