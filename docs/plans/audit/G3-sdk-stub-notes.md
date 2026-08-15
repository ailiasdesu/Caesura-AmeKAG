# G3 — Live2D / Steam SDK 路径验证文档 + 条件编译测试桩

> 素材池 G3（见 docs/plans/audit/ROADMAP-100.md 第 20 行）：
> **Live2D/Steam SDK 路径验证文档 + 条件编译测试桩**，对应 g0_live2d.md 的 P2-6 与 g0_steam.md 的 P2-5。
> 目标：让「无 SDK 构建」也能测试两个可选 SDK 模块的所有可触达代码路径，并为 SDK 分支提供静态守卫与验证路线图。

## 1. 模块现状与架构

### 1.1 live2d（IAnimationBackend）

- **接口**：src/live2d/api/IAnimationBackend.h —— 纯虚，无数据成员（符合 AGENTS.md §2）。
- **双实现**：
  - NullAnimationBackend（**无 SDK 默认**）：静态 PNG/JPG/BMP 立绘降级。NullAnimationBackend.cpp + PathConfinement.cpp **无条件编译**（cmake/CaesuraModules.cmake 的 caesura_add_module(Live2D ...)）。
  - Live2DBackend（**需 Cubism SDK**）：整文件以 #ifdef CAESURA_LIVE2D 包裹，仅在 CMake option(CAESURA_LIVE2D) 开启且探测到 CubismSdkForNative-* 时经 target_sources(CaesuraLive2D ...) 加入构建，并定义 CAESURA_HAS_LIVE2D。
- **4 条渲染路径**：D3D11Native（Windows，已实机验证）、MetalNative（macOS，已实现待验证）、OpenGLShared/OpenGLReadback（Linux/macOS，代码就绪未实机运行）。

### 1.2 steam（ISteamBackend）

- **接口**：src/steam/api/ISteamBackend.h —— 纯虚，无数据成员。
- **双实现**：
  - NullSteamBackend：全 no-op 桩（init→false，其余→false/0/"")。
  - SteamBackend：真实 Steamworks 集成。**SteamBackend.cpp 无条件编译**（cmake/CaesuraModules.cmake 的 caesura_add_module(Steam ...)），每个方法以 #ifdef CAESURA_HAS_STEAM / <SDK 调用> / #else / <返回 false/0/""> / #endif 守卫；SDK 缺失时整体退化为「可编译但不可用」的空操作，与 NullSteamBackend 同语义。

## 2. 无 SDK 时的降级行为

### 2.1 Live2D 无 SDK → NullAnimationBackend（静态 PNG 立绘）

| 能力 | 有 SDK（Live2DBackend） | 无 SDK（NullAnimationBackend） |
|---|---|---|
| init | CubismFramework 初始化 | true（无需外部依赖） |
| loadModel(*.moc3) | 加载 Cubism 模型 | 0（非图片路径拒绝） |
| loadModel(*.png/.jpg/.bmp) | — | 加载为静态纹理，返回句柄 |
| playMotion / setExpression / setParameter | 播放动作/表情/参数 | 均 no-op（playMotion→false） |
| render | 离屏渲染合成 | blitTexture 到 VIEW_MAIN |
| 设备丢失恢复 | 未接入（g0_live2d P1-4） | 不涉及 |

### 2.2 Steam 无 SDK → SteamBackend 空操作 / NullSteamBackend

| 能力 | 有 SDK（SteamBackend） | 无 SDK（SteamBackend #else / NullSteamBackend） |
|---|---|---|
| init | SteamAPI_Init() | **false**（不初始化） |
| runCallbacks | SteamAPI_RunCallbacks + 统计批处理 flush | no-op |
| achievements / stats | 真实读写 | **false / 0 / 0.0f** |
| storeStats | 置脏、节流 flush | **false** |
| cloud* | 远程存储读写 | **false / 0 / ""** |
| overlay 轮询 | 回调驱动 | **false** |
| shutdown | SteamAPI_Shutdown | no-op（幂等） |


## 3. CI 覆盖状态

- **CI 恒为「无 SDK」构建**：三个平台（Windows/MSVC、macOS/Clang、Linux/GCC）均未配置 CAESURA_LIVE2D 与 CAESURA_HAS_STEAM。
- 因此 Live2DBackend（SDK 分支）与 SteamBackend 的 **SDK 分支**在 CI 中 **从不编译/执行**；其真实 SDK 缺陷（如 g0_steam 的 P1-1 统计脏标记、P1-3 STEAM_CALLBACK include 顺序）只能靠 **静态源码断言** 或 **带 SDK 的本地验证** 捕获。
- SteamBackend.cpp 的 **#else 分支**（每个方法的空降级）**在 CI 中被编译并链接执行** —— 本文档新增的测试桩覆盖了它。
- NullAnimationBackend + PathConfinement（Live2D 无 SDK 路径）**在 CI 中被完整测试**。

## 4. 条件编译测试桩覆盖了什么

### 4.1 tests/cpp/test_steam.cpp（已注册于 tests/CMakeLists.txt:68）

| 分组 | 用例 | 覆盖路径 |
|---|---|---|
| NullSteamBackend 桩（原有 8 例） | init/name/overlay/achievements/stats/cloud/runCallbacks/shutdown | Null 桩全部纯 no-op 语义 |
| SteamBackend 无 SDK 降级（G3 新增） | init→false、name=Steam、feature gates、stats、cloud、析构+runCallbacks+幂等 shutdown | **SteamBackend.cpp 每个方法的 #else 分支**（真实类在无 SDK 构建下的优雅降级） |
| SDK 守卫静态断言（G3 新增） | SteamAPI_Init/RunCallbacks/UserStats/RemoteStorage 符号存在；#ifdef CAESURA_HAS_STEAM 守卫存在 | 防止 SDK 符号泄漏到无 SDK 构建（链接失败） |

### 4.2 tests/cpp/test_live2d.cpp（已注册于 tests/CMakeLists.txt:69）

| 分组 | 用例 | 覆盖路径 |
|---|---|---|
| NullAnimationBackend（原有） | init/识别图片/加载失败不分配句柄/无尺寸拒绝/无纹理服务可选/渲染 PNG/关停幂等与句柄重置/接口访问器 | NullAnimationBackend.cpp 全路径（PNG 降级） |
| PathConfinement（原有） | 绝对路径/..爬升/点分量/根内路径/前缀仿冒/符号链接 | PathConfinement.cpp 路径穿越防护 |
| SDK 守卫静态断言（G3 新增） | Live2DBackend.h/.cpp 以 #ifdef CAESURA_LIVE2D 包裹、SDK include 仅在守卫内；NullAnimationBackend+PathConfinement 无条件入 Live2D 模块；组合根含 Null 与 Live2D 双分支 make_unique | 防止 SDK 泄漏到无 SDK 构建；锁定「无 SDK→Null」组合根回退 |

### 4.3 覆盖到的代码路径汇总

- **可运行**（无 SDK 构建真实执行）：
  - NullAnimationBackend 整个 PNG 降级生命周期（加载/释放/渲染/关停）。
  - PathConfinement 所有路径穿越防护。
  - SteamBackend 全部方法的 **#else 降级分支**（真实类，非桩）。
  - NullSteamBackend 全部 no-op 语义。
- **静态守卫**（SDK 分支源码级断言，非运行）：
  - Live2DBackend / SteamBackend 的 #ifdef 包裹、SDK 符号隔离、组合根回退接线。
- **无法在此环境覆盖**（需真实 SDK + 对应硬件）：
  - SteamBackend 的 SDK 分支真实逻辑（成就/统计/云存档/overlay）。
  - Live2DBackend 全路径 + D3D11/Metal/OpenGL 渲染路径。

## 5. SDK 集成方式

### 5.1 Live2D

见 docs/guides/live2d-setup.md。要点：

- CMake：cmake -B build -DCAESURA_LIVE2D=ON [-DCUBISM_SDK_ROOT=...]。
- SDK 探测顺序：CUBISM_SDK_ROOT 显式 → thirdparty/CubismSdkForNative-5-r.5 → 项目根目录同名目录。
- 找到后定义 CAESURA_HAS_LIVE2D，编译 Live2DBackend 替代 NullAnimationBackend。
- Windows 需 MSVC；macOS Metal 已实现待实机验证；OpenGL 路径代码就绪未验证。

### 5.2 Steam

- CMake：caesura_add_module(Steam src/steam/SteamBackend.cpp) 总是编译；当 CAESURA_HAS_STEAM 被定义时，SteamBackend.cpp 内的 SDK 分支启用（root CMakeLists.txt:249-254 配置 CaesuraSteam/CaesuraEntry 的编译定义、include、链接）。
- **CAESURA_HAS_STEAM 由 toolchain 设置**（如 SteamWorks SDK 提供 toolchain），不是普通 CMake 选项（CMakeLists.txt:167 默认 OFF）。
- Steam SDK 不入库（专有许可），需自行下载 Steamworks。

## 6. 验证路线图（带 SDK 时）

1. **Steam**：配置 CAESURA_HAS_STEAM，确认 SteamBackend 编译通过（含 P1-3 已修的 STEAM_CALLBACK include 顺序）。运行所有 test_steam.cpp 用例——SDK 分支启用后，本文档新增的「无 SDK 降级」断言语义反转（init 不再必然 false），但 **静态守卫断言仍应通过**（它们只查 #ifdef 结构与符号存在性）。
2. **Live2D**：-DCAESURA_LIVE2D=ON 构建并运行，确认 Live2DBackend 编译；Windows D3D11 已实机验证（见 live2d-setup.md §2026-08-01 记录）；其余平台按 live2d-setup.md §验证路线图 逐路径验证并更新能力矩阵。
3. 每次带 SDK 验证后，回填本文件 §3/§4 的覆盖状态表。
