---
title: "Caesura G8: 视觉验证 + 着色器补全"
type: plan
status: draft
date: 2026-06-06
origin: docs/superpowers/plans/2026-06-06-caesura-gap-analysis.md
execution: code
---

## Summary

基于 G7 修复完成后的差距分析 S1-S7 项，修复阻塞性验证问题并补全着色器管线。

S6 (GPU 帧监控) 已完整实现。剩余 6 项按依赖顺序分 5 个实施单元。

## Gap Status

| ID | 项 | 状态 | 规格 |
|----|-----|------|------|
| S6 | GPU 帧监控 + 自适应降级 | ✅ 已实现 | GpuMonitor.cpp 完整 |
| S5 | Lua 256MB 硬上限 | ❌ | [10.2.60] |
| S4 | 显式 GC 点 (场景切换/存档后) | ❌ | [10.2.29] |
| S1 | [trans] 过渡槽预加载 | ❌ | [10.2.32] |
| S2 | 异步加载队列流控 | ❌ | 高水位+取消 |
| S3 | 对象池 | ❌ | [10.2.29] |
| S7 | CJK 静态图集 fallback | ❌ | [10.2.54] |

## Implementation Units

### G8-U1: Lua 内存硬上限 + 显式 GC 点 (S4+S5)

**规格:** [10.2.60] Lua 256MB hard limit + [10.2.29] explicit GC

**Files:**
- `src/Core/Engine.cpp` — 每帧末 lua_gc(L, LUA_GCCOUNT) 检查
- `scripts/system.lua` — collect_step / collect_full 辅助
- `scripts/kag/commands/transition.lua` — [trans] 后 GC step
- `scripts/kag/commands/save.lua` — 存档后 GC collect
- `scripts/kag/commands/flow.lua` — [jump] 前 GC collect

**Approach:**
- **lua_Alloc 钩子**: Engine::init() 中设置 lua_setallocf，每次 Lua 分配后检查 lua_gc(L, LUA_GCCOUNT)，超 256MB 立即抛 Lua error
- **每帧末检查**: Engine::run() 调用 lua_gc(L, LUA_GCCOUNT)，三级响应:
  - 80% (204MB): lua_gc(L, LUA_GCSTEP, 50) 增量步进
  - 95% (243MB): lua_gc(L, LUA_GCCOLLECT) 全量回收 + 清理音频缓存
  - 100% (256MB): 触发 ErrorUI 错误界面
- **显式 GC 点**: [trans] 后 GC step, [save] 后 GC collect, [jump] 前 GC collect, 每 300 帧 GC step (在 scheduler 中计数)
- Lua 侧 system.lua 提供 collect_step/collect_full 辅助函数

**Files (updated):**
- `src/Core/Engine.cpp` — lua_Alloc 钩子 + 每帧末三级检查 + 300帧计数器
- `scripts/system.lua` — collect_step / collect_full
- `scripts/kag/commands/transition.lua` — [trans] 后 GC step
- `scripts/kag/commands/save.lua` — [save] 后 GC collect
- `scripts/kag/commands/flow.lua` — [jump] 前 GC collect
- `scripts/kag/scheduler.lua` — 300 帧 GC step 计数

**Risks:** 低 — 纯增量

### G8-U2: 过渡槽预加载 (S1)

**规格:** [10.2.32] — [trans] 前隐性 wait_for_preload，过渡槽纹理不替换当前图层

**Files:**
- `scripts/kag/commands/resource.lua` — 过渡槽 + promote_transition_slot
- `scripts/kag/commands/transition.lua` — [trans] 前等待所有 pending 完成

**Approach:**
- resource.lua 增加 _transitionSlot 表
- ResourceCommands.preload_transition(paths) 加载到过渡槽
- ResourceCommands.promote_transition_slot() 提升到主缓存
- [trans] 调用前检查 ResourceCommands.has_pending() 循环 yield 等待
- 超时保护 5 秒防止死锁

**Risks:** 中 — 等待循环需超时防止死锁

### G8-U3: 异步加载队列流控 (S2)

**规格:** 高水位告警 (MAX_PENDING=16) + 取消排队加载

**Files:**
- `scripts/kag/commands/resource.lua` — load_texture_async + cancel_all_pending
- `src/resource/AsyncLoader.h` — **新建** 异步加载器头文件
- `src/resource/AsyncLoader.cpp` — **新建** 后台线程 + SDL_PushEvent 派发
- `src/script/BackendAPI.cpp` — 注册 load_texture_async / cancel_async_loads API
- `src/Core/Engine.cpp` — AsyncLoader 初始化/关闭集成

**Approach:**
- C++ AsyncLoader 单例，后台线程队列 + SDL_PushEvent 回调主线程
- Lua 侧待处理队列 MAX_PENDING=16，超出告警并拒绝
- cancel_all_pending() 清空队列并通知后台线程
- 主线程独占 bgfx 调用 (纹理创建在回调中通过 SDL 事件延迟)

**Risks:** 中 — 涉及后台线程，严格主线程独占 bgfx

### G8-U4: 对象池 (S3)

**规格:** [10.2.29] — render_batch / CancelToken / 事件表复用，减少 GC 压力

**Files:**
- `scripts/pool.lua` — **新建** 通用对象池模块
- `scripts/kag/scheduler.lua` — 集成 pool 到 render_batch 分配

**Approach:**
- Lua 侧 Pool.new(ctor, reset, maxSize) 通用工厂
- 预建 renderBatchPool / eventTablePool
- scheduler 中 render_batch 走 pool:acquire/pool:release
- CancelToken 池化 (高周转对象)

**Risks:** 低 — 纯 Lua，无 C++ 依赖

### G8-U5: CJK 静态图集 fallback 集成 (S7)

**规格:** [10.2.54] — 扩容期间优先 CJK 静态图集 → 其次 8x16 位图 → U+FFFD

**Files:**
- `src/Render/FontRenderer.h` — +m_cjkFallbackAtlas + m_expanding 标记
- `src/Render/FontRenderer.cpp` — renderGlyph 内扩容降级链

**Approach:**
- FontRenderer 初始化时可选加载 CJK 静态图集 (4096² RGBA8, ~64MB)
- 扩容期间标记 m_expanding=true
- renderGlyph: 扩容时优先查 CJK 图集 → 内置位图 → U+FFFD
- 扩容完成清除标记恢复 FreeType 栅格化
- 若 CJK 图集加载失败，直接走位图降级

**Risks:** 中 — 需要预生成或运行时创建 CJK 图集，内存额外 ~64MB

## Execution Order

```
U1 → U2 → U3 → U4 → U5
```

U1 先做 (全局稳定)，U2/U3 连续 (同模块 resource.lua)，U4 独立，U5 最后 (视觉验证依赖)。

## Test Scenarios

- U1: 填充 Lua 表至 204MB (80%)，验证 GC step 触发；至 243MB (95%)，验证 collect+音频清理；至 260MB，验证 ErrorUI 弹出
- U2: [trans] 前 [preload] 未完成纹理，验证等待不超时
- U3: 提交 >16 个异步加载，验证高水位拒绝
- U4: pool:acquire/release 循环 1000 次，验证无泄漏
- U5: 中文字符渲染在扩容期间正常显示

## Verification

- cmake --build build 无错误
- 26 项集成测试 + 3 项全量解析测试全部通过
- Lua 内存超过 256MB 时触发 ErrorUI 而非崩溃