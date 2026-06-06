# Caesura (AmeKAG) — AGENTS.md

## 开发前必读

每次开始编码工作前，Codex **必须**首先读取以下文件建立上下文：

1. `docs/superpowers/CONTEXT_ANCHOR.md` — 核心约束速查（架构不变式 + 64 项决策索引）
2. `docs/Caesura_功能实现规格说明书_整合版.md` — 完整功能规格说明书（Part 0-11）
3. `docs/superpowers/plans/2026-06-06-caesura-implementation.md` — 实现计划（Phase 0-9）

## 开发铁律

- **Lua 永远不直接调用 C 函数或 bgfx/SoLoud/SDL C API** — 所有调用走 `backend.xxx()`
- **主线程独占** — bgfx/SoLoud 仅主线程调用，后台线程用 SDL_PushEvent 派发
- **永久无锁** — 不引入互斥锁或自旋锁
- **不轮询** — 音频/视频同步用回调，不用忙等待
- **统一句柄** — 所有资源走 `{type, id, generation}` 的 ResourceHandle

## 项目结构

```
src/        — C++ 引擎核心 (render/, audio/, script/, resource/, carc/, system/, input/, debug/)
scripts/    — Lua 脚本层 (kag/commands/, layers.lua, audio.lua, backend.lua)
shaders/    — bgfx 着色器 (vs_*, fs_*)
tests/      — Catch2 (C++) + busted (Lua)
```

## 文档索引

| 文档 | 路径 |
|------|------|
| 上下文锚点 | `docs/superpowers/CONTEXT_ANCHOR.md` |
| 功能规格说明书 | `docs/Caesura_功能实现规格说明书_整合版.md` |
| 实现计划 | `docs/superpowers/plans/2026-06-06-caesura-implementation.md` |
| BackendAPI 规格 | 说明书 Part 0 → "Lua↔C++ 统一抽象接口层" |
| 设计决策索引 | 说明书 Part 11 → [10.2.1]~[10.2.66] |

## 当前状态

- 引擎代码: 已有基础骨架
- 说明书: 64 项决策，可进入编码阶段
- 实现计划: 完整 Phase 0-9 + Phase 0.5
- 下一步: 开始 Phase 0 编码 (CMake + SDL3 + bgfx 三角形 + Lua 嵌入)
