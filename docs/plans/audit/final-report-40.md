# Caesura (AmeKAG) 引擎 —— 40 轮迭代最终报告

> 日期：2026-08-14（迭代轮次 20-40 完成）· 分支：master
> 前置：16 模块架构审计（docs/plans/audit/g0_*.md，P0×2 / P1×8 / P2×23）→ 修复轮 A/B（round 20-22）→ 17 轮持续迭代（round 23-39）→ 本报告（round 40）
> 配套素材：docs/plans/audit/market-comparison.md（11 引擎市场对比，本报告 §5 引用）

## 1. 总体结论

Caesura (AmeKAG) 是一个 C++20 视觉小说引擎（bgfx 渲染 / SDL3 窗口 / SoLoud 音频 / Lua 5.4 脚本），
16 个静态模块库以 15 个 API-only CMake 目标 + 30 个纯虚接口隔离（实时普查：`python scripts/api_stats.py`）。

**40 轮迭代达成三项目标：**
1. ✅ 逐模块审计（16 模块，P0×2/P1×8/P2×23）并全部修复/优化/评估闭环
2. ✅ 完成 21+ 轮迭代（实际 20-40 共 21 轮，达到 40 轮总目标）
3. ✅ 输出本报告（§5 含 11 引擎市场对比分析）

## 2. 架构审计与修复闭环（目标 1）

### 2.1 审计基线（round 19 前）
- 16 模块逐模块审计文档：g0_archive/audio/debug/di/entry/input/job/live2d/minigame/platform/render/resource/rpc/script/steam/storage
- 发现 P0×2（render 第三方嵌入编译、IVideoDecoder 接口位）、P1×8（playMotion 失效、pendingCount 记账、统计持久化、
  minigame View 配置、di 承载 Lua 绑定、debug 头越界、setRenderDevice 耦合、KAG 热路径）、P2×23

### 2.2 修复轮（round 20-22）
- P0-1/P0-2 + P1-1/P1-2/P1-3 完成（CI 绿）
- P1-4/P1-5/P1-6/P1-8 完成；P1-7 评估接受现状（前置声明无 include 依赖）
- P2 批量 8 项代码修复 + 18 处注释编码

### 2.3 持续迭代收口（round 23-39）—— 按模块
| 轮次 | 模块/主题 | 交付 |
|---|---|---|
| 23-25 | 编辑器/RPC/性能 | /api/pick 命中测试、SMA 骨骼编辑器、引擎性能基线 + CPU 基准套件 |
| 26-28 | SMA 资产管线/统计 | SMA 校验+保存、骨架画布、引擎 stats 端点（纹理预算/网格/任务/Lua 堆）+ IDE 面板 |
| 29 | 音频生命周期 | IAudioBackend suspend/resume + Engine 生命周期接线（MobileAdapter TODO 落地） |
| 29 | minigame 分发 | luaCall 15 分支链 → 类内 constexpr 分发表 |
| 29 | RPC 解码 | JSON \u 代理对（surrogate pair）+ readJsonString 实况修复 |
| 29 | live2d 命名 | CAESURA_HAS_LIVE2D → CAESURA_LIVE2D（14 文件 18 处） |
| 30 | render/文档 | sceneFromJson 拆分、live2d 注释/死接口/文档漂移 |
| 31 | 单源命令 | MiniGameCommands.h 双参 X-macro（后端表+绑定 stub+luaL_Reg 三处单源） |
| 31 | 日志统一 | live2d 全模块 DEBUG_* + playMotion 冗余扫描修复 |
| 32 | render 日志/P1-2 | render 63 处错误路径 DEBUG_*；shutdown 清 solid/path 缓存 + 回归测试 |
| 33 | 日志第三波 | storage/resource/archive 63 处 DEBUG_*（SubSys 扩至 12 值） |
| 34 | 日志第四波 | script/audio/minigame/rpc 24 处 DEBUG_*（业务模块错误路径全部统一） |
| 35 | minigame P1 | P1-3 线程契约收紧、P1-4 碰撞缓冲复用、P1-5 输入接线（registerGameCallback 首次有调用者）、P1-6 handle 初始化 |
| 36 | script P1 | AI epoch 取消（cancel 闩锁 bug 修复）、死字段清理、KAG API 计数自动推导 |
| 37 | steam 修复 | cloudFileNameAt 成员缓冲、STEAM_CALLBACK include 顺序、墙钟节流、cloud_read 64MB、storeStats 节流 |
| 38 | resource 收口 | XP3 64 位 seek、审计标记批量补齐 |
| 39 | 测试缺口 | GpuMonitor 状态机 7 单测 + bgfx 未 init 崩溃修复（setGpuAvailable 门禁） |
## 3. 迭代过程与门禁纪律（目标 2）

### 3.1 迭代规模
- 目标：21+ 轮迭代（round 19 后完成 40 轮总数）
- 实际：round 20-40 共 21 轮，全部完成；每轮保持全量门禁
- 提交：round 26 推送后本地累积 37 个语义提交（最终统一推送一次）

### 3.2 每轮门禁（全部通过）
| 关卡 | 基线 → 终态 |
|---|---|
| Debug 全量构建 | 0 错误（每轮） |
| C++ 测试套件 | 628 → **638 用例**（+10：stats 契约、sceneFromJson、shutdown 缓存、GpuMonitor 状态机 ×7） |
| Lua 套件 | 120/120（含修复 pick 边界断言、新增代理对转义断言） |
| ctest | 11/11（AI smoke 跳过） |
| 耦合度 --ci | PASS（全程，预算内） |
| editor tsc | 0 错误 |

### 3.3 踩坑沉淀（docs/solutions + 记忆）
1. PowerShell `>` 默认 UTF-16LE → markdown 写坏（用 python/Set-Content -Encoding utf8）
2. main.cpp if-constexpr 分支链保留 return 收尾（C4715 信号）
3. 引擎沙箱 io.open 白名单（scripts/assets/tests/demo 前缀）
4. kag 模块必须登记 kag/init.lua 预加载清单
5. **MSVC constexpr 无法访问含函数指针数组元素（C2131）** → X-macro 直接生成代替 static_assert 守卫
6. **bgfx::getStats() 未 init 时内部解引用崩溃**（非返回 null）→ setGpuAvailable 外部门禁
7. **子代理机械替换大函数易吞闭合括号** → 验收对照 git show 逐函数核对（5 次同型教训）
8. **test_source_encoding 扫描含注释** → Engine.cpp 注释禁用 bgfx:: 字样
9. 接口加方法需同步 3 处实现（GpuMonitor/NullGpuMonitor/test mock）

## 4. 终态能力盘点

### 4.1 模块健康度（16 模块）
- **全部通过边界铁律**：api/ 纯虚接口、BackendRegistry 唯一访问、组合根创建、耦合预算、无循环依赖
- **日志一致性**：8 个业务模块（render/live2d/minigame/audio/storage/resource/archive/script）错误路径全部 DEBUG_*；
  有意保留：main.cpp CLI、debug 系（日志系统自身）、ErrorUI（崩溃容错）、di/rpc/platform 状态消息
- **剩余记录项**（可接受）：live2d P2-6 / steam P2-5（SDK 测试缺口）、UnifiedBinding（测试在用保留）、
  g0_render P2-10 部分（RTTManager/TextRenderer/QuadBatch 需 GPU 或注入重构）

### 4.2 关键能力（54 项能力矩阵）
- 渲染：D3D11/OpenGL/Metal 多后端、图层合成、纹理管线（异步/预算/LRU）、粒子、视频、GPU 监控（自适应降级，本轮可测）、文字（FreeType/CJK/Ruby）、转场、RTT、批协议
- 脚本：Lua 5.4 沙箱、KAG Neo-Genesis 81 命令、流控、指令预算、热重载、错误恢复、条件等待/选择
- 音频：3 总线（BGM/Voice/SE）、交叉淡化、3D 空间、按句柄音量、**生命周期挂起/恢复（round 29）**
- 内容：Live2D（Cubism 5/PNG 降级）、3D 小游戏（**round 35 输入接线**）、存档（JSON/AES-256-GCM）、模式迁移 v1→v5、CARC 归档（加密/签名）、Steamworks、资产管线
- 编辑器/RPC：stdio JSON-RPC + HTTP 编辑器（/api/pick/state/sma/stats/...，Bearer 门禁）

### 4.3 开发者体验
- 命令契约自动生成（78 命令）、API 普查自动生成、耦合门禁脚本、三平台 CI（Windows/MSVC、macOS/Clang、Linux/GCC）
## 5. 市场对比（目标 3）—— 与主流 galgame 引擎对比

> 完整逐引擎分析见 docs/plans/audit/market-comparison.md（11 引擎、数据可信度标注、待核实清单）。
> 数据基准：Ren'Py 8.5.0（2025 "In Good Health" 版本核对）、各引擎官方站点/文档（2026-08 复核）。

### 5.1 对比总结表（11 引擎）

| 引擎 | 语言/许可 | 脚本模型 | 渲染 | 音频 | 存档 | 编辑器 | 平台 | 定位差异 |
|---|---|---|---|---|---|---|---|---|
| **Caesura (AmeKAG)** | C++20 / 私有 | KAG Neo-Genesis（81 命令）+ Lua 5.4 混编 | bgfx 多后端（D3D11/GL/Metal） | SoLoud 3 总线 | JSON+AES-256-GCM 加密 | 内置 HTTP/stdio 编辑器（pick/stats/SMA） | Win/macOS/Linux | 引擎级 C++ 性能 + 沙箱脚本；Live2D/Steam 可选 |
| Ren'Py | Python/多许可（MIT+LGPL 部分） | Python + Ren'Py 声明式 | OpenGL（软件回退） | 自研 | JSON 明文 | Ren'Py 内置 GUI | 全平台 | 生态最大、社区成熟、Android/iOS 官方支持 |
| KiriKiri2/KAG3 | C++（XP3）/自由 | KAG3 标签 + TJS | Direct3D | 自研 | 私有 | 无官方 | Win 为主 | 历史 KAG 语法兼容（Caesura 继承其命令模型） |
| 吉里吉里Z | C++/自由 | KAG3 兼容 + TJS | D3D9/11 | 自研 | XP3 | 无 | Win | 日系同人主流，XP3 打包生态 |
| NScripter/ONScripter | C++/自由 | NScripter 脚本 | 2D 软件 | 自研 | 私有 | 无 | 多平台（ONScripter） | 经典日系引擎，老旧 |
| TyranoBuilder/Script | JS 运行时/自由 | 可视化节点 + 标签 | WebGL/DOM | HTML5 | localStorage | 可视化节点编辑器 | Web/Win/mac/iOS/Android | 无代码可视化，Web 分发 |
| Visual Novel Maker | C#/私有 | 事件命令 | Unity 渲染 | Unity 音频 | Unity | 内置可视化编辑器 | Win/mac/iOS/Android | Unity 生态，商店素材 |
| Unity+Naninovel | C#/私有 | C# + 可视化 | Unity | Unity | Unity | Naninovel 编辑器 | 全平台 | 全功能引擎内嵌，性能强 |
| Godot+DialogueManager | GDScript/MIT | 对话资源 + GDScript | Godot | Godot | Godot | Godot 编辑器 | 全平台 | 免费开源、强编辑器 |
| WebGAL | TS/MPL-2.0 | WebGAL 脚本 | WebGL | WebAudio | IndexedDB | WebGAL 官方编辑器 | Web 为主 | 中文社区、网页即玩 |
| Monogatari | JS/MIT | Monogatari 脚本+条件 | DOM/Canvas | WebAudio | localStorage | 无 | Web | 轻量 Web 叙事框架 |

### 5.2 Caesura 差异化优势
1. **引擎级 C++ 性能与脚本沙箱并存**：KAG 命令层声明式 + Lua 5.4 完整编程能力 + 指令预算防死循环 + io 白名单沙箱——大多数竞品要么纯脚本（Ren'Py 性能受限）要么纯引擎（无沙箱脚本）
2. **双端编辑器内建**：stdio JSON-RPC + HTTP 编辑器（预览帧命中测试 /api/pick、引擎状态 /api/stats、SMA 资产校验保存）——Ren'Py/NScripter 无等价物，Tyrano 仅可视化节点
3. **加密存档 + CARC 加密归档 + Ed25519 签名**：完整内容保护链（同人商业化的差异化）
4. **Live2D/Steam/云存档可选集成**：模块化 DI，开箱即用
5. **跨平台渲染抽象**：bgfx 统一 D3D11/OpenGL/Metal，非 Web 引擎中少见的强多后端

### 5.3 相对短板（按差距排序）
1. **生态与内容市场**：Ren'Py 社区/教程/模板远超；WebGAL 中文社区活跃
2. **移动端/Web 分发**：Ren'Py（Android/iOS 官方）、Tyrano（Web）、VNM（移动）均占优；Caesura 桌面为主
3. **可视化工具链**：Tyrano 节点编辑器、VNM 可视化、Godot 全功能编辑器——Caesura 编辑器为技术型（面向开发者）
4. **成熟度验证**：Ren'Py/吉里吉里经大量商业作品验证；Caesura 需更多实机作品

### 5.4 追赶路径建议（按杠杆排序）
1. 补 Web 导出（emscripten）或移动运行时 → 覆盖最大分发缺口
2. 编辑器增强：可视化场景树/时间线（在现有 /api/pick+stats+SMA 基础上）
3. 示例作品库 + 教程（降低上手门槛，对标 Ren'Py 教程体系）
4. 继续验证 Live2D/Steam SDK 路径（当前 CI 无 SDK，P2-6 测试缺口）

## 6. 后续建议（round 40 之后）
- 将 37 个本地提交统一推送 + 三平台 CI 验证（本报告提交后执行）
- SDK 路径验证：Live2D（P2-6）、Steam（P2-5）需带 SDK 的手工/CI 验证
- P2-10 剩余测试缺口：RTTManager 池复用、TextRenderer rebuildCache、BgfxQuadBatch 合并组
- UnifiedBinding 迁移（测试在用，需先迁移测试）

---
*报告生成：Caesura 40 轮迭代自动汇总（门禁数据来自每轮实测）*
