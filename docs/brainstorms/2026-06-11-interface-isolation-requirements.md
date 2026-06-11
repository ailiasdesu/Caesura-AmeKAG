---
date: 2026-06-11
topic: interface-isolation
---

# 引擎模块接口隔离路线图

## Summary

以组合根模式逐模块切断跨模块 #include 违规，从最危险的 Core↔Render 循环依赖开始按风险排序推进。每个步骤独立可构建、可测试，完成后下一个模块套用同一模板。

## Problem Frame

Caesura 引擎当前 11 个模块之间存在以下耦合伤疤：Core 依赖全部 9 个子系统、Core↔Render 循环引用、Audio→Scripting 越层引用、Scripting 同时绑定 7 个模块、CARC↔Resource 双向依赖、BgfxRenderDevice 52KB 巨石。修任何一个模块的 Bug 都可能触发其他三个模块的连锁回归。

BackendRegistry 已经定义了 9 个纯虚接口，具备接口隔离的基础设施——但 `Engine.cpp` 大量直接 `#include` 具体实现头文件，接口未被强制用作通信边界。

## Key Decisions

- **组合根模式。** main.cpp 集中创建所有具体实现，通过抽象接口指针注入上层。每个解耦步骤都是"提取具体创建到 main.cpp → 上层只持有接口指针"。
- **EngineConfig 聚合结构体。** Engine 构造函数接受一个包含所有子系统接口字段的结构体，而非 9 个独立参数。可选子系统（Live2D、Steam、FFmpeg）字段默认 nullptr，加新子系统不破坏已有调用点。
- **按风险排序。** Core↔Render（最危险循环依赖）第一步切，逐级向叶子模块推进。每个步骤完成后 CI 必须全绿。
- **不在此范围：** Live2D/Steam 的条件编译隔离（已足够干净）；web-editor 的 RPC 边界（已是 JSON-RPC 接口通信）；功能级 Bug 修复。

## Requirements

### 组合根模式

- R1. main.cpp 是所有具体子系统实现的唯一创作点（composition root）。
- R2. 所有具体实现以抽象接口指针注入消费者，消费者不引用具体实现头文件。
- R3. Engine.h 的 `#include` 列表仅包含 EngineConfig 所需的结构体和抽象接口头文件，不含任何具体实现头文件。
- R4. 每一层模块的公共头文件只暴露抽象接口，具体类声明移入 `.cpp` 或不暴露给上层。

### Engine 构造

- R5. Engine 构造函数接受 `const EngineConfig&` 参数，EngineConfig 为聚合结构体，每个子系统一个字段，类型均为抽象接口指针。
- R6. EngineConfig 中可选子系统（Live2D、Steam、FFmpeg）字段默认值为 `nullptr`，不在 Engine 构造函数参数列表中参与条件编译。
- R7. main.cpp 中使用 C++20 指定初始化器构造 EngineConfig 实例，使每个子系统以 `.field = value` 形式可读。

### 解耦路线（按风险排序）

- R8. **第 1 步：Core↔Render。** `Engine.h` 不再 `#include` `BgfxRenderDevice.h`。Engine 通过 `IRenderDevice*` 获得渲染能力，BgfxRenderDevice 的创建移至 main.cpp。
- R9. **第 2 步：Audio→Scripting。** `SoLoudAudioEngine.cpp` 不再 `#include` `LuaManager.h` 或 `Engine.h`。音频层通过 `IAudioBackend` 回调接口向上通知事件，回调注册在 main.cpp 中完成。
- R10. **第 3 步：Render→Core。** `BgfxRenderDevice.cpp` 不再 `#include` `Engine.h`。渲染层所需的服务通过其构造函数注入（或以 main.cpp 级别的依赖注入在创建时提供）。
- R11. **第 4 步：Scripting 解耦。** 重审 LuaManager 的所有 binding 模块——将每个 binding 的跨模块 `#include` 替换为通过 BackendRegistry 获取抽象接口引用，消除 Scripting 对 7 个模块的直接 include。
- R12. **第 5 步：CARC↔Resource 双向解耦。** 通过 `IAssetProvider` 和 `ProviderChain` 细化，消除 CARC 模块与 Resource 模块之间的互相直接引用。
- R13. **第 6 步：BgfxRenderDevice 拆分。** 将 52KB 的单文件按关注点拆分为设备初始化、渲染循环、截图、ImGui 集成等独立编译单元，每个单元只导入其真正需要的头文件。
- R13. **第 6 步：BgfxRenderDevice 拆分。** 将 52KB/1170 行的单文件按关注点拆为 5 个编译单元：`BgfxRenderDevice`（门面类，委托调用）、`BgfxDeviceCore`（初始化/帧循环/视图/RTT 管理，~300 行）、`BgfxShaderManager`（内嵌 shader 编译与 ProgramHandle/UniformHandle 生命周期，~150 行）、`BgfxDraw`（批量四边形/纹理 blit/stretchBlt/affineBlt/全屏特效，~450 行）、`BgfxDebugCallback`（bgfx 调试回调与关闭协调，~50 行）。抽离顺序：ShaderManager（叶子）→ DeviceCore（依赖 ShaderManager）→ Draw（依赖前两者）→ DebugCallback。每步抽离后编译通过。
- R14. 每一步独立可构建、可测试。完成一步后，已完成的步骤不被后续步骤破坏。

### 验证

- R15. 每一步完成后，Windows/macOS/Linux 三端 CI 构建必须通过，零新增警告。
- R16. 每一步完成后，全部 138 个已有测试必须继续通过。
- R17. 维护一个耦合度计数：当前 Core 跨 9 模块 include，目标 Core 跨不超过 3 个模块（只跨抽象接口模块）。

## Scope Boundaries

- **不在此范围：** Live2D 和 Steam 的条件编译隔离——这两个模块已经通过 `CAESURA_LIVE2D`/`CAESURA_HAS_STEAM` 宏实现干净的条件编译隔离。
- **不在此范围：** web-editor 的 RPC 边界。编辑器已通过 stdin/stdout JSON-RPC 与引擎通信，这一层已经满足接口隔离。
- **不在此范围：** 功能级 Bug 修复。此路线图只聚焦模块边界重建，现有功能行为不改变。

## Dependencies / Assumptions

- 依赖 BackendRegistry 已有的 9 个纯虚接口保持稳定，无需新增顶层抽象接口类别。
- 假定 CI 在 Windows（MSVC）、macOS（Clang）、Linux（GCC）三端的编译器和标准库行为足够一致，不会因接口重构引入平台特有编译错误。
- 假定 web-editor 的 JSON-RPC 协议不需要变更——RpcServer 和 EditorServer 的接口边界已足够干净，不参与本次重构。

## Outstanding Questions
- **Deferred to Planning:** 是否需要为耦合度计数自动化一个脚本（扫描 `#include` 以报告当前 Core 跨模块引用数量），在每步完成后自动输出。
- **Deferred to Planning:** BgfxRenderDevice 各拆分文件的头文件依赖如何收敛——拆分后每个新 `.cpp` 应只导入自己真正需要的 bgfx 头，不与 BgfxRenderDevice 共享同一组 include。
- **Deferred to Planning:** 是否需要为耦合度计数自动化一个脚本（扫描 `#include` 以报告当前 Core 跨模块引用数量），在每步完成后自动输出。
