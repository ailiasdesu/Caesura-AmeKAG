| P1-1 | live2d | Live2DBackend.cpp:466-472 | **playMotion() 失效**：motionCache 从未填充（只读查询） | loadModelInternal 遍历 GetMotionCount 填充 motionCache |
| P1-2 | resource | AsyncLoader.cpp:136-147 | **pendingCount 记账 bug**：缓存命中路径不 ++，poll/drain 误减 | 缓存命中分支对齐 ++ |
| P1-3 | steam | SteamBackend.cpp:119-161 | **统计持久化丢失**：setStatInt/Float 未置 m_statsDirty | 成功分支追加 m_statsDirty=true |
| P1-4 | minigame | BgfxMiniGameBackend.cpp:435-446 | render() 未 setViewRect/setViewClear，3D 视图可能不渲染 | render()/enter 时配置 View |
| P1-5 | script | di/BackendRegistry 213-355 | Lua 绑定代码（registerEngineBindings）落在 di 容器（分层错误 + di include lua.h） | 迁入 script/bindings/EngineBinding.* |
| P1-6 | render | GpuMonitor.cpp:2 / VideoPlayer.cpp:10 | 跨模块 include 具体 debug 头（有豁免但越界） | 提供 debug api 薄封装头，render 改 include api |
| P1-7 | minigame | api/IMiniGameBackend.h:82 | setRenderDevice 把 render 接口拉入 minigame api（跨模块强耦合） | 评估改为注册回调/由组合根注入 |
| P1-8 | script | KAGBinding.cpp:108-120 | 热路径 lua_getfield(registry) 未缓存（render_text 高频） | 复用 RenderBinding 缓存模式 |

## 4. P2 批量（23 项，见各报告；代表性）
- render：巨型注释噪声、重复死静态变量 s_preferredBackend、注释编码损坏（??→—）、ShaderCache \n 双反斜杠
- script：RenderBinding_Shutdown 悬空声明、UnifiedBinding 死代码、SteamBinding 注册位置不一致
- minigame：luaCall 17 分支 if 链 → unordered_map、sceneFromJson 90 行拆分
- live2d：CMake 命名 CAESURA_LIVE2D/HAS_LIVE2D 不一致、陈旧注释、createRenderer 死接口
- resource：缓存 key 分隔符碰撞（path+"|"+type）
- steam：SteamAchievement 死结构体
- archive：checkedRead 封装（3 项）
- 其余记录级

## 5. 修复排期（与迭代路线合并）

| 迭代轮（全局） | 主题 | 内容 |
|---|---|---|
| 20 ✅ | 架构修复轮 A | P0-1/P0-2 + P1-1/P1-2/P1-3 完成（CI 绿） |
| 21 ✅ | 架构修复轮 B | P1-4/P1-5/P1-6/P1-8 完成；P1-7 评估为**接受现状**（setRenderDevice 用前置声明 `class IRenderDevice*`，无 include 依赖，接口合规；耦合扫描 PASS） |
| 22 ⏳ | P2 批量 + 全量回归 | 全部 P2 + 复核 |
| 23-39 | 17 轮持续迭代 | 特性/深化轮（/api/pick、SMA 骨骼编辑器、IDE、性能、市场功能）按路线图/用户指示 |
| 40 | 最终报告轮 | 详细报告（含主流 galgame 引擎对比分析） |

> 每轮保持：全量重建零错误 → C++ 全测 → Lua 全测 → ctest → 耦合 PASS → api-stats → 分语义提交 → push → CI 三平台绿。