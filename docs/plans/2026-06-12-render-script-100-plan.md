# Render + Script 100% 完善计划

> 创建日期: 2026-06-12 | 基线: 316/316

## Render 模块分析

### 缺口精确扫描

```
src/render/api/ITextureManager.h:  bgfx::TextureHandle (6 处)
src/render/api/ILayerManager.h:    bgfx::TextureHandle (1 处) + bgfx::ProgramHandle (1 处)
src/render/api/IVideoPlayer.h:     bgfx::TextureHandle (1 处)
总计: 8 处 bgfx 类型暴露
```

### 消费者精确扫描（关键发现）

| 接口 | bgfx 方法数 | 外部消费者 | 影响文件数 |
|------|-----------|-----------|-----------|
| ITextureManager | 6 | RenderBinding.cpp | **1 个文件** |
| ILayerManager | 2 | 无（仅 render 内部） | **0** |
| IVideoPlayer | 1 | 无（仅 render 内部） | **0** |

**实际影响面：1 个文件（RenderBinding.cpp）+ render 模块内部重构。**

之前估计"5+ 消费者"是错误的——经过精确扫描，ILayerManager 和 IVideoPlayer 的 bgfx 类型方法全部是模块内部调用，外部消费者只通过 BackendRegistry 拿到指针后调用非 bgfx 方法（open/close/update/isPlaying 等）。

### 实施方案

**IU R1:** 创建不透明句柄类型
- `src/render/api/RenderTypes.h`: `struct TextureId { uint32_t id; };`, `struct ProgramId { uint32_t id; };`
- 包含 `valid()`, `operator bool()`, `operator==` 等辅助方法

**IU R2:** 替换 ITextureManager 接口（唯一影响消费者的接口）
- 6 个方法的 `bgfx::TextureHandle` → `TextureId`
- BgfxRenderDevice 内部维护 `std::unordered_map<uint32_t, bgfx::TextureHandle>` 映射
- 新增 `resolveTexture(TextureId) → bgfx::TextureHandle` 内部方法

**IU R3:** 替换 ILayerManager 接口（零外部消费者）
- `setTexture(LayerType, bgfx::TextureHandle)` → `setTexture(LayerType, TextureId)`
- `render(..., bgfx::ProgramHandle)` → `render(..., ProgramId)`
- LayerManager 内部做 TextureId → bgfx::TextureHandle 转换

**IU R4:** 替换 IVideoPlayer 接口（零外部消费者）
- `getTexture(VideoHandle)` → `getTextureId(VideoHandle)` 返回 `TextureId`

**IU R5:** 更新 RenderBinding.cpp（唯一外部消费者）
- `getTextureHandle(id)` 返回 `TextureId`，需要 bgfx handle 时调用 `resolveTexture()`
- `loadTexture(path)` 返回 `uint32_t`（已经是），不改
- `createSolidTexture(...)` 返回 `TextureId`
- 约 8 处调用点

**IU R6:** 测试验证
- 构建零错误
- 316 测试全绿（零回归）


## Script 模块分析

### 当前覆盖

| 测试文件 | 用例数 | 覆盖范围 |
|---------|--------|---------|
| test_lua_manager.cpp | 5 | VM 生命周期，安全锁定 |
| test_kag_binding.cpp | 9 | KAG C++ 绑定 API |
| test_kag_execution.cpp | 12 | tokenizer/parser/scheduler |
| test_kag_integration.cpp | 1 | KAG 集成 |
| test_game_state.cpp | 5 | GameState push/pop |
| test_script_bindings.cpp | 9 | DevCore/Render/VFX/Debug 绑定 |
| test_script_boundary.cpp | 9 | KAG 错误恢复 + GameState + 绑定校验 |
| **总计** | **50** | |

### 剩余缺口

1. **jump 到不存在 label** — conductor.lua 已有 `labelMap`，但 @jump 到未知 label 时的行为未测试。需在 scheduler 执行层添加 pcall 保护。
2. **RenderBinding 空路径保护已加** ✅（F4 已完成）
3. **VFX create_emitter 负参数** — 需在 VFXBinding.cpp 添加参数校验
4. **DevCore 非法参数** — 已部分覆盖

### 实施方案

**IU S1:** conductor.lua jump 保护
- `@jump` 执行前检查 labelMap，未知 label 时 pcall 包装 + 打印警告

**IU S2:** VFXBinding 参数校验
- `create_emitter` 检查 rate/speedMin/speedMax 合法性
- 非法参数返回 -1

**IU S3:** 测试补齐
- +3 用例：jump 未知 label、VFX 负参数、GameState 跨协程隔离


## 执行顺序

```
R1→R2→R3→R4→R5→R6 (render)  →  S1→S2→S3 (script)
```

Render 先做（影响面已量化，仅 1 个外部文件）。Script 后做（纯增量，零风险）。
