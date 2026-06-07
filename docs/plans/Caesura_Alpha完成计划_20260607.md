# Caesura Alpha 完成计划

> 基于: `docs/Caesura_功能实现规格说明书_整合版.md` (75项决策) + 2026-06-07 代码审计
> 修订: 四路并行审查 (ce-doc-review) — 2026-06-07
> 目标: 关闭 Alpha 阶段所有遗留项, 进入 Beta 路线图

---

## 当前状态

| 维度 | 状态 |
|------|:---:|
| P0 决策 | 17/17 ✅ |
| P1 决策 | 32/32 ✅ |
| P2 决策 | 8/23 (已实现 8, 15 项 Beta 推迟) |
| C++ 测试 | 110/110 ✅ |
| Lua 测试 | 33/33 ✅ |
| Debug 构建 | 通过 ✅ |
| Release 构建 | 通过 ✅ |

---

## 摘要

审查结论: **取消 MobileAdapter (死代码)**、**推迟 RTT spinlock (Beta 随 Job System)**。新增 RTTManager 死代码清理、IVideoDecoder 接口定义、FontRenderer Beta 提醒。共 6 个 Phase (~70 行 C++ + ~15 行文档)。

---

## Phase 1: 代码修复

### 1.1 RenderBinding 视频播放 TODO

- **文件**: `src/Scripting/RenderBinding.cpp:428`
- **现状**: `(void)path; // TODO: implement video playback`
- **方案**: 调用 `luaL_error(L, "Video playback not available in Alpha (FFmpeg planned for Beta)")`，删除 `dynamic_cast<BgfxRenderDevice*>` 无用分支
- **风险**: 低

### 1.2 RTTManager `m_handleToPoolIndex` 死代码清理 ← 新增

- **文件**: `src/Render/RTTManager.h`, `src/Render/RTTManager.cpp`
- **现状**: `std::unordered_map<bgfx::FrameBufferHandle, int32_t> m_handleToPoolIndex` 声明但从未填充
- **方案**: 填充该 map（`create()` 时插入、`destroy()`/`destroyCanvas()` 时移除）
- **风险**: 低

### 1.3 `destroyCanvas` 立即 flush 修正 ← 新增

- **文件**: `src/Render/RTTManager.cpp:277-281`
- **现状**: `destroyCanvas()` 内部调用 `bgfx::frame()` 立即 flush
- **方案**: 移除 `bgfx::frame()` 调用，改为纯入队销毁请求
- **风险**: 低

### 1.4 IVideoDecoder 纯虚接口定义 ← 新增

- **文件**: `src/Render/IVideoDecoder.h` (新建)
- **方案**: 纯虚接口 `open()` / `decodeFrame()` / `close()` (~15 行)
- **风险**: 低

### 1.5 FontRenderer 图集扩容 Beta 提醒 ← 新增

- **文件**: `src/Render/FontRenderer.cpp`
- **方案**: 添加 `// @Beta: Replace hardcoded threshold with configurable atlas growth policy`
- **风险**: 零

---

## Phase 2: 文档更新

### 2.1 CONTEXT.md 最终更新
- P2 状态: `8/23 (15 项 Beta 推迟)`
- Alpha 完成日期

### 2.2 ALPHA_NOTES.md 最终更新
- P2 推迟列表完整枚举 (15 项，含 [10.2.55] RTT spinlock + [10.2.64] MobileAdapter)
- 测试数: 110 C++ + 33 Lua

---

## Phase 3: 集成验证

### 3.1 Debug 构建 + 全量测试
### 3.2 Release 构建
### 3.3 引擎启动冒烟测试 (无 abort)
### 3.4 性能基线检查 (GPU <1.0ms, 内存 ~30MB, 二进制 ~2.5MB)

---

## 推迟到 Beta (15 项 P2)

| 决策 | 领域 |
|------|------|
| [10.2.16] CARC 差分原子回滚 | CARC |
| [10.2.17] 视频解码器扩展接口 | 视频 |
| [10.2.23] TextureArray 扩容策略 | 字体 |
| [10.2.24] FFmpeg 视频解码器 | 视频 |
| [10.2.36] Lua 5.4 新特性 | 脚本 |
| [10.2.44] FreeType 图集扩容锁 | 字体 |
| [10.2.48] 热重载静态分析 | 开发 |
| [10.2.55] RTT 池自旋锁 | 渲染 |
| [10.2.64] MobileAdapter 触摸翻译 | 平台 |
| [10.2.72]-[10.2.78] Job System | 架构 |
| 其余 4 项 | — |

---

## 执行顺序

```
Phase 1.1 + 1.2 + 1.3 + 1.4 + 1.5  (独立, 不重叠文件)
       |
Phase 2.1 + 2.2  (文档)
       |
Phase 3.1 → 3.2 → 3.3 → 3.4  (顺序验证)
```

## 修改量

| Phase | 文件 | 行数 | 风险 |
|:--:|:--:|:--:|:--:|
| 1.1 | 1 | ~10 | 低 |
| 1.2 | 2 | ~20 | 低 |
| 1.3 | 1 | ~5 | 低 |
| 1.4 | 1 | ~15 | 低 |
| 1.5 | 1 | ~3 | 低 |
| 2.1-2.2 | 2 | ~15 | 低 |
| 3.1-3.4 | 0 | 0 | — |

---

## 修订历史

| 版本 | 日期 | 变更 |
|------|------|------|
| v1 | 2026-06-07 | 初版: MobileAdapter + RTT spinlock + 文档 + 验证 |
| v2 | 2026-06-07 | 审查修订: 取消 MobileAdapter, 推迟 RTT spinlock, 新增 4 项修复 |