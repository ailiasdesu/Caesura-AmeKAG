---
title: Alpha→Beta 引擎升级 — FFmpeg默认 + PBR shader + Demo + AI优化
date: 2026-06-09
category: docs/solutions/best-practices
module: Engine
problem_type: best_practice
component: development_workflow
severity: medium
applies_when:
  - 引擎完成度 ~90% 进入可演示版本冲刺
  - 需要补齐功能缺口（视频解码、shader、Demo、文档）而非重构
  - 多轨道并行推进（引擎 + 编辑器 + 文档）
tags:
  - beta-sprint
  - ffmpeg
  - pbr-shader
  - demo-scene
  - ai-context
  - documentation
related_components:
  - VideoPlayer
  - MiniGame
  - AIPanel
  - KAG-API
---

# Alpha→Beta 引擎升级 — 最小可行填补策略

## Context

引擎 Alpha+ 完成度 ~90%：核心架构、8 纯虚接口、61 KAG 命令、Electron 编辑器均已就位。进入 Beta 可演示版本冲刺时面临 4 类缺口：

1. **功能缺口**: FFmpeg 默认 OFF，MiniGame 只有 debug wireframe
2. **体验缺口**: 无 Demo 场景展示引擎能力，AI 面板上下文不完整
3. **文档缺口**: KAG API 只覆盖 35/61 命令，无用户教程
4. **工程缺口**: Electron 打包路径错误，shaderc 未构建

## Guidance

### 原则: 先补洞，不加新

Alpha→Beta 阶段的核心原则是补齐已有体系，不引入新架构：

```
Alpha+ (90%)                         Beta (95%)
+-----------------+                 +-----------------+
| 8 接口 OK        |                | 8 接口 OK        |
| 61 KAG OK        |   补齐缺口 ->   | 61 KAG OK + 文档 |
| 编辑器 OK        |                | 编辑器 OK + AI   |
| FFmpeg OFF       |                | FFmpeg ON        |
| Shader wireframe |                | Shader PBR       |
| Demo NONE        |                | Demo 4场景       |
| 打包路径 BUG     |                | 打包路径 OK      |
+-----------------+                 +-----------------+
```

### FFmpeg: QUIET fallback 模式

```cmake
# 关键: 从 REQUIRED 改为 QUIET，默认 ON
option(CAESURA_VIDEO_FFMPEG "Enable FFmpeg" ON)
if(CAESURA_VIDEO_FFMPEG)
    find_package(FFmpeg QUIET ...)
    if(FFmpeg_FOUND)
        # 启用 FFmpeg
    else()
        message(STATUS "FFmpeg NOT found - pl_mpeg only")
    endif()
endif()
```

**决策理由**: QUIET 保证未安装 FFmpeg 时构建不中断，pl_mpeg 作为零依赖 fallback。

### Shader: 源文件先行，二进制跟随

MiniGame 面临 GLSL .sc 仍为 Phong、无 Metal shader、只嵌入 DXBC 三个问题。Beta 阶段的务实策略：

1. **立即**: 升级 GLSL .sc 为 PBR-lite（与 DX11 HLSL 对齐）
2. **延后**: shaderc 构建 + 跨平台二进制编译（需 bgfx genie 工具链）
3. **现状**: DX11 嵌入式 DXBC 已工作，Windows 平台不受阻

**教训**: shaderc 假设"工具链已配置"是错误的 — 仓库无预编译 shaderc.exe，需从源码构建。

### Demo: 展示所有引擎能力

4 场景设计覆盖引擎核心卖点：
- 场景 1: KAG 文本 + 立绘 + BGM（基础能力）
- 场景 2: Live2D（条件编译 + fallback 静态立绘）
- 场景 3: MiniGame 3D（PBR 渲染 + 碰撞 + 光照）
- 场景 4: 视频 + 存档（FFmpeg/pl_mpeg + AES-256-GCM）

**关键**: 场景 2 的条件分支确保无论是否编译 Live2D，Demo 都能完整运行。

### AI 上下文: token 预算 + 完整 API 参考

```javascript
const TOKEN_WARNING_THRESHOLD = 6000;  // ~1500 tokens
const KAG_API_SUMMARY = `...`;           // 61 commands, 6 sub-modules
```

**改进点**:
- KAG API 摘要从 14 个命令扩展到 61 个（6 子模块全覆盖）
- 脚本截断 4000 字符 + API 参考 ~2000 字符 = ~1500 tokens
- 超预算时前端警告
- 代码插入前语法验证

## Why This Matters

Alpha→Beta 是最危险的阶段 — 引擎"基本完整"，容易陷入两种陷阱：
1. **永不完结**: 不断追加新功能，永远到不了可演示版本
2. **草率演示**: 跳过 Demo 和文档，演示时才发现体验断层

本模式在 1 个提交周期内完成了 8 项填补，全部是对已有体系的补齐，零新架构引入。

## When to Apply

- 引擎完成度 >=85%，核心架构稳定
- 有明确的"可演示"目标
- 缺口可枚举且互不阻塞

## Related

- `docs/solutions/architecture-patterns/electron-editor-ai-panel.md` — 编辑器架构
- `docs/solutions/architecture-patterns/minigame-3d-pipeline.md` — MiniGame 3D 管线
- `docs/plans/2026-06-09-beta-v1-full-plan.md` — Beta 冲刺完整计划
