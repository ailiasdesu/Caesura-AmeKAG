# minigame 模块审计

> 审计对象：src/minigame/ 全量源码（api 接口 + 实现）+ tests/cpp/test_minigame.cpp + cmake/CaesuraModules.cmake
> 审查依据：AGENTS.md §1–§9（模块边界铁律、接口规范、BackendRegistry 唯一访问点、组合根、耦合预算）
> 审计日期：2026-08-18　审查员：C++ 架构审查子代理

## 概述

minigame 模块承载引擎内嵌的 3D 小游戏子系统，生命周期为
`KAG scene → mini_game:enter → active loop → mini_game:leave → KAG`。
对外仅通过 `api/IMiniGameBackend.h`（纯虚接口）暴露 symbol；提供两个实现：
`NullMiniGameBackend`（no-op 默认后端）与 `BgfxMiniGameBackend`（bgfx 原生渲染 + 程序化几何 + PBR-lite 光照 + AABB 碰撞 + 简易刚体重力物理）。

整体架构遵守了模块边界铁律：**api 接口为纯虚类、不含数据成员、无第三方具体类型泄漏（bgfx、lua_State 均通过前置声明规避）；具体后端对象只在组合根 `src/main.cpp`（Bgfx）与 `src/entry/Engine_Backends.cpp`（Null 兜底）构造；跨模块依赖均为 api 头或唯一访问点 BackendRegistry，无 `../../../` 或绝对路径 include**。未发现硬 P0 违规，主要风险集中在渲染生命周期正确性（view 未配置）、线程契约与实现矛盾、以及若干测试覆盖缺口。

## P0 关键问题

**结论：未发现违反 AGENTS.md 铁律的硬性 P0。** 以下为最接近 P0 边界、需要优先复核的项：

### P0-1　接口 setRenderDevice 将 render 模块接口拉入 minigame api（跨模块强耦合）
- **位置**：`src/minigame/api/IMiniGameBackend.h:82`（`virtual void setRenderDevice(class IRenderDevice* dev) = 0;`）
- **问题**：接口以参数形式暴露 `IRenderDevice*`，令 minigame 的 api 携带对 render 模块接口类型的**编译期强依赖**，任何仅想用 minigame api 的调用方也必须能解析 `IRenderDevice`。这形成跨模块 api 级耦合（边界内），且与 AGENTS §3「BackendRegistry 是唯一访问点」的哲学略有张力——渲染设备本应通过 `BackendRegistry::getRenderDevice()` 获取，而非由调用方注入到接口。
- **修复建议**：删除接口中的 `setRenderDevice`，改由实现类内部通过 `BackendRegistry::instance().getRenderDevice()` 惰性获取（Bgfx 实现已 include BackendRegistry，可统一）；或保留但将 `IRenderDevice` 改为不透明句柄。耦合从「接口注入」收敛为「实现内部查找」。
- **工作量**：S

### P0-2　模块内支持头文件泄漏 bgfx 具体类型（非 api，但污染模块对外 helper 面）
- **位置**：`src/minigame/MiniGeometry.h:4`（`#include <bgfx/bgfx.h>`）及对外返回 `bgfx::VertexLayout / bgfx::VertexBufferHandle / bgfx::IndexBufferHandle`
- **问题**：`MiniGeometry.h` 在模块内作为几何工具头，直接向外返回 bgfx 具体句柄类型。虽不在 `api/` 目录（不构成 api 边界违规），但任何 include 它的内部文件都被迫引入 bgfx，导致 bgfx 成为 minigame 模块的**传递性硬依赖**。若未来引入第二个实现（Godot/Unity 桥），此头无法复用。同时 bgfx 句柄是 16 位下标，泄漏到本模块公共工具脸面易在跨后端重构时出错。
- **修复建议**：将 `MiniGeometry` 的几何生成（纯 CPU 数学）与 bgfx 包装（`posNormLayout/createVB/createIB`）拆分；前者的 CPU 部分不 include bgfx，bgfx 绑定收敛到 Bgfx 实现内部。若不拆分，至少明确其定位为「bgfx 私有支持头」并加注释，勿在本模块外引用。
- **工作量**：M

## P1 重要问题

### P1-1　render() 未配置 View（setViewRect/setViewClear），3D 视图可能不渲染
- **位置**：`BgfxMiniGameBackend.cpp:435-446`（`render()`）
- **问题**：render() 只调用了 `bgfx::setViewTransform(MINIGAME_VIEW,...)` + 提交，**从未调用 `bgfx::setViewRect(MINIGAME_VIEW,0,0,w,h)` 与 `bgfx::setViewClear`**。bgfx 中 view 必须在提交前设定 view rect（否则 view 的 output 尺寸未定义/可能为 0），且不清屏共用 `MINIGAME_VIEW` 可能与 KAG 主 pass 发生深度/颜色残留叠加。这是 mini-game 实际能否出图的关键正确性问题。
- **修复建议**：在 render() 内（或 enter 时）为 `MINIGAME_VIEW` 设置 `setViewRect`（宽高取自 `m_renderDevice` 的实际帧缓冲尺寸，而非硬编码）与 `setViewClear`（若需要独立清屏）；并确认 render 设备的 view 容量 > 10。
- **工作量**：S

### P1-2　渲染宽高比硬编码 1280/720，非 16:9 窗口畸变
- **位置**：`BgfxMiniGameBackend.cpp:439`（`float aspect=1280.0f/720.0f;`）
- **问题**：投影矩阵宽高比硬编码为 16:9，未查询实际窗口/帧缓冲尺寸，任何非 16:9 窗口下 mini-game 场景被横向/纵向拉伸变形。与 P1-1 关联（应统一取实际分辨率）。
- **修复建议**：从 `BackendRegistry::getRenderDevice()`（或 render 接口）获取当前输出尺寸计算 aspect；无设备时回退到默认 16:9。
- **工作量**：S

### P1-3　线程契约自相矛盾：update() 声明 JobSystem 安全却又调用 Lua
- **位置**：`api/IMiniGameBackend.h:20-22`（注释声明 `update() may be dispatched to JobSystem workers`）与 `BgfxMiniGameBackend.cpp:429-433 / 203-210`（`update()`→`runCollisionDetection()` 内 `lua_pcall` 调用 Lua 全局 `on_collision`）
- **问题**：接口契约声称 update() 可被分发到工作线程（纯 CPU），但实现中 update() 会执行 Lua 回调 `on_collision`。Lua 状态非线程安全，一旦未来真的把 update() 交给 JobSystem 就会数据竞争/崩溃。当前 Engine.cpp:739 在主线程顺序调用，是**潜在**而非现役 bug，但契约与实现矛盾必须消除。
- **修复建议**：二选一——(a) 收紧契约：声明 update() 仅主线程调用，删除「JobSystem 安全」注释；(b) 若确实要并行物理，则把碰撞产生的配对事件**出队**到主线程再调 Lua（提供 `drainCollisionEvents` 或让 update 返回冲突列表由 Engine 主线程回调）。推荐 (a) 先行（现状即主线程）。
- **工作量**：S（选 a）/ M（选 b）

### P1-4　热路径每帧多次分配（update/碰撞检测）
- **位置**：`BgfxMiniGameBackend.cpp:205-208`（`runCollisionDetection` 每帧 `vector<uint32_t>` + 6 个 float vector 重新分配）及 `:430`（update 每帧遍历 + 清空 3 个 accel）
- **问题**：`runCollisionDetection` 每帧重建 1 个 id vector + 6 个坐标 vector + sort，对象多时（几十上百）为热路径带来可观的分配与 cache 抖动。`findCollisions` 内部又 `vector<Item>` 二次分配。
- **修复建议**：在成员中复用预分配的 vector（`m_colIds/px/py/...` reserve 后 clear 复用）；或直接让碰撞遍历 unordered_map 内联成数组避免二次拷贝。改为对象数组时注意与 `m_objects`（unordered_map）的迭代一致性。
- **工作量**：S

### P1-5　processEvent 恒返回 false，小游戏实际收不到输入，与「D9.4 焦点切换」意图不符
- **位置**：`BgfxMiniGameBackend.cpp:461`（`bool processEvent(const void* e){(void)e;return false;}`）
- **问题**：enter()（414-416）把输入焦点切到 GAME、leave() 切回 KAG，但 `processEvent` 完全空实现、恒返回 false（不消费任何事件），也没把 SDL 事件转发给游戏逻辑。接口文档（IMiniGameBackend.h:57-60）明确 processEvent「forward SDL events to the mini-game when active」，现状使输入路由形同虚设。
- **修复建议**：实现 processEvent（按需解析 SDL 事件类型，转发给场景/回调，命中则返回 true）；或在留下空实现前明确标注「暂未接入输入」避免误导。与 P1-3 的 focus 切换逻辑配套验证。
- **工作量**：M

### P1-6　bgfx handle 初始化用 `= {}`（idx 0）与 `BGFX_INVALID_HANDLE`（kInvalidHandle）不一致
- **位置**：`BgfxMiniGameBackend.h:59-60`（`bgfx::VertexBufferHandle m_geoVB[...] = {};` 等）
- **问题**：bgfx 句柄是 {idx}；`BGFX_INVALID_HANDLE` 的 idx 为 `kInvalidHandle`（0xffff），而空花括号 `{}` 给 idx=0，而 idx 0 是**合法句柄的起始值**。`bgfx::isValid` 靠比对 kInvalidHandle 判断，因此初始化后未创建的句柄会被误判为「有效」。当前靠 `shutdown(){ if(!m_gpuReady) return; }`（:165）与 `ensureGpuResources` 的 gpuReady 门闩避免了误 destroy，但**惯用法危险**：任何绕过 m_gpuReady 的路径都可能 destroy 一个 idx=0 的假资源。
- **修复建议**：所有句柄成员统一初始化为 `BGFX_INVALID_HANDLE`（显式 `{BGFX_INVALID_HANDLE}`），与 bgfx 惯例一致。
- **工作量**：S

## P2 建议

### P2-1　函数过长 / 单一巨型分发
- **位置**：`BgfxMiniGameBackend.cpp:467-484`（`luaCall` 一串 if-chain，17 个分支）与 `:224-314`（`sceneFromJson` 近 90 行）
- **问题**：luaCall 靠 strcmp 链派发 17 个命令，与 `script/bindings/MiniGameBinding.cpp` 里另外 20 个 `luaL_Reg` 绑定重复维护命令名清单，易脱节；sceneFromJson 层级深（>4 层嵌套）。
- **修复建议**：luaCall 用静态 `unordered_map<string, handler>` 或 switch（对常量字符串）；sceneFromJson 拆出 `parseCamera/parseLight/parseObject` 辅助。绑定侧命令名应与后端命令集中定义共享。
- **工作量**：M

### P2-2　几何缓存重复生成 CPU 数据
- **位置**：`BgfxMiniGameBackend.cpp:57-66`（`initGeometryCache`）
- **问题**：每个几何类型把 createXxxGeometry() **连续生成两次**（一次喂 createVB、一次喂 createIB），如 `createVB(createCubeGeometry())` + `createIB(createCubeGeometry())`。CPU 顶点数据被算两遍，且几何 generator 内的 `printf` 会刷两倍日志。
- **修复建议**：先生成一个 `GeometryData` 临时量，再分别 `createVB(geo)/createIB(geo)`。
- **工作量**：S

### P2-3　日志用 printf/fprintf 而非 DebugManager，且伴生噪声
- **位置**：`MiniGeometry.cpp:83,99,132`（geometry generator 内 `printf`）、`BgfxMiniGameBackend.cpp` 多处 fprintf/printf
- **问题**：模块直接使用 printf/stderr 输出，未走 `DebugManager`（AGENTS §7 允许 DEBUG_* 宏作为唯一例外直连 DebugManager，但这里用的是 printf）。几何生成这种每加载一次就打行的日志在运行时是噪声；也不具备分级/缓冲/性能剖析能力。
- **修复建议**：统一接入 `BackendRegistry::getDebugManager()` 或 DEBUG_* 宏；几何 generator 内的 printf 降级为调试级或删除。
- **工作量**：S

### P2-4　重复魔法值
- **位置**：`BgfxMiniGameBackend.cpp:150,452-453`（默认材质 `{0.5f,0,0.5f,0}`）vs `MiniMaterial.h` 默认字段（roughness 0.5/metallic 0/specular 0.5）
- **问题**：默认 PBR 参数散落多处硬编码，与 `MiniMaterial` 默认值含义重复，改一处漏一处的风险。
- **修复建议**：抽一个默认材质 uniform 的命名常量在实现文件内统一。
- **工作量**：S

### P2-5　测试覆盖缺口
- **位置**：`tests/cpp/test_minigame.cpp`（115 行，9 个 TEST_CASE）
- **现状**：仅覆盖 Null 后端生命周期/接口 upcast、BackendRegistry round-trip、Bgfx 的 loadScene JSON 解析（GPU-free 部分）。
- **缺口**：
  - `MiniCollision` 的 `computeAABB/aabbOverlap/findCollisions`（含 sweep-prune 正确性、重叠/相离/边缘相接）**完全无测试**。
  - `MiniScene.h / MiniLight / MiniMaterial` 的默认值与结构体无构造测试。
  - `sceneFromJson` 缺少畸形输入分支测试（缺字段、数组越界、scale 为 number 与 array 混用、未知 type 落到 Cube）——虽然当前用 try/catch 兜底，但语义分支未验证。
  - `BgfxMiniGameBackend` 的 enter/leave/update 物理（重力/速度积分正确性）无单测（依赖 GPU 的 render 已有注释说明留给 smoke）。
  - Null 后端 `luaCall`/更新路径无 lua_State 冒烟。
- **修复建议**：为 MiniCollision 与 scene 解析补纯 CPU 单测；物理积分抽成可测的纯函数（不吃 GPU 也可测）。渲染相关维持「默认构造 + 访问器」约定。
- **工作量**：M

### P2-6　接口文档与实现的 `MiniGameEvent/MiniGameTransition` 枚举未接线
- **位置**：`api/IMiniGameBackend.h:86-109`（`MiniGameEvent`、`MiniGameTransition` 枚举）标注 reserved，但引擎/绑定中无任何使用点（grep 未命中）。
- **问题**：预留协议常量无消费代码，属死协议面；容易误导读者以为已支持 transition 事件。
- **修复建议**：要么实现并接线，要么移除/加「未启用」注释避免误用。
- **工作量**：S

### P2-7　无 TODO/FIXME 遗留
- **结论**：全模块源码 grep 未发现 `TODO`/`FIXME` 字面量。良好。

## 耦合分析

### minigame 跨模块依赖清单
| 被依赖模块 | 依赖方（文件） | 引用形式 | 合规性 |
|---|---|---|---|
| input | `BgfxMiniGameBackend.h:9` | `../input/api/IInputRouter.h`（api 头，含 InputFocus 枚举） | ✅ api 头 |
| render | `BgfxMiniGameBackend.h:16` | `../render/api/IRenderDevice.h`（api 头） | ✅ api 头 |
| di | `BgfxMiniGameBackend.cpp:2` | `../di/BackendRegistry.h` | ⚠️ 具体头，但为 AGENTS §3 唯一访问点，合规 |
| script | —— | `script/bindings/MiniGameBinding.cpp` include `minigame/api/IMiniGameBackend.h`（反向：script→minigame） | ✅ api 头 |

- 跨模块依赖**计数 = 3（input、render、di）**，对照 AGENTS §9 预算「其他模块 ≤4」，**未超预算**。
- 第三方库（bgfx、bx/math、bx/readerwriter、nlohmann_json、lua）为直接依赖，不计入模块耦合，但 `bgfx/bx` 作为 render 的底层库被 minigame 直连，属于「共享第三方」而非模块耦合，合规。
- 无 `../../../`、无绝对路径、无循环（minigame→render/input/di 均为单向；script→minigame 单向）。di/BackendRegistry.h 通过前置声明 + 单头方式对外，未造成接口头传递性泄漏。

### 与接口规范的符合度
- `api/IMiniGameBackend.h` 纯虚类、无数据成员 ✅；`lua_State`、`IRenderDevice` 均前置声明，无第三方具体类型泄漏 ✅（见 P0-1 关于 IRenderDevice 注入的权衡）。
- 命名：模块目录小写 `minigame`、接口 `I` 前缀、实现 PascalCase ✅；命名空间 `Caesura::` ✅。

## 审查结论

minigame 模块在**结构上符合 AGENTS.md 模块边界铁律**：api 接口纯净、具体对象只在组合根构造、依赖仅经 api 头或唯一访问点、耦合数 3 在预算内、无循环/无越界 include。**不存在硬 P0 违规**。主要风险集中在实现层的**渲染正确性**（render() 未配置 view rect→3D 视图可能不输出，且宽高比硬编码）与**线程契约矛盾**（update 声明 JobSystem 安全却调 Lua）——这两条（P1-1、P1-3）建议在接入真实 KAG 游戏循环前优先修复；其次为热路径每帧分配（P1-4）与输入未接线（P1-5）。测试对碰撞/物理/畸变场景覆盖明显不足（P2-5），建议补齐纯 CPU 单测后再扩展 GPU 冒烟。整体可标记为「边界合规、可运行、渲染与线程语义待加固」的中间态模块。
