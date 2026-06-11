---
title: 引擎 Beta → v1.0 全量推进计划
type: feature
status: in_progress
date: 2026-06-09
origin: 完成度审查 (~93% Beta) — 冲刺可演示版本 + 编辑器打磨 + 文档
prerequisites: Beta 引擎 (✅), Electron 编辑器 (✅), 技术债 20/24 闭合 (✅)
---

# 引擎 Beta → v1.0 全量推进计划

## Summary

引擎 Beta 已完成核心架构、KAG 兼容、渲染管线、目录重组、Live2D GL FBO、MiniGame 跨平台 shader、AI IPC 真实桥接。本计划覆盖余下三条并行轨道。

### 最近完成 (2026-06-09)
- ✅ 目录重组: 13→10 模块 (Carc→CARC, Animation→Live2D, Editor/Platform→Core, DebugManager→Debug)
- ✅ Live2D OpenGLReadback FBO + GL 状态保存恢复
- ✅ MiniGame 跨平台 shader (D3D11 DXBC / OpenGL GLSL / Metal MSL)
- ✅ FFmpeg CMake 集成 (条件编译, 需外部 ffmpeg-dev)
- ✅ AI Panel 真实 Electron IPC 桥接 (替代 setTimeout mock)
- ✅ ES 模块冲突修复 (main.js→main.cjs, preload.js→preload.cjs)
- ✅ Demo 脚本重写 (真实 layer/backend/mini_game API)

## Implementation Units

### 轨道 A: 引擎 Beta 冲刺

| IU | 内容 | 关键文件 | 优先级 | 状态 |
|----|------|----------|:---:|:---:|
| A1 | FFmpeg 默认启用 + 跨平台 CMake 检测 | `CMakeLists.txt`, `VideoPlayer.cpp` | P0 | ⚠️ |
| A2 | MiniGame PBR shader 编译 (shaderc 预编译) | `MiniGame/`, `shaders/` | P0 | ✅ |
| A3 | Demo 场景 — 完整演示 (Live2D + MiniGame + 视频 + 存档) | `scripts/demo/` | P1 | ❌ |

### 轨道 B: 编辑器打磨

| IU | 内容 | 关键文件 | 优先级 | 状态 |
|----|------|----------|:---:|:---:|
| A4 | 编辑器 AI 上下文优化 (token 预算 + KAG API 提示词) | `web-editor/electron/main.cjs`, `AIPanel.jsx` | P1 | ✅ |
| B1 | RPC 帧预览 — 引擎渲染截图回传编辑器 | `RpcServer`, `StageView.jsx` | P1 | ⚠️ |
| B2 | 编辑器 UI 打磨 (主题/布局/快捷键) | `web-editor/src/` | P2 | ✅ |
| B3 | Electron 打包 — 生成 Win/Mac/Linux 安装包 | `web-editor/package.json`, `electron-builder` | P1 | ⚠️ |

### 轨道 C: 文档 + 生态

| IU | 内容 | 关键文件 | 优先级 | 状态 |
|----|------|----------|:---:|:---:|
| C1 | 用户教程 — "30 分钟制作第一个视觉小说" | `docs/tutorials/` | P1 | ❌ |
| C2 | KAG API 参考完善 (所有 61 命令 + 示例) | `docs/api/KAG-API.md` | P1 | ⚠️ |
| C3 | MiniGame API 文档 (15 函数 + 示例) | `docs/api/MiniGame-API.md` | P2 | ⚠️ |
| C4 | 构建与部署指南 (三平台 + 打包) | `docs/BUILD.md` | P2 | ✅ |
| C5 | README 发布就绪 (徽章/GIF/路线图) | `README.md` | P2 | ✅ |

## Key Decisions

### KD1: FFmpeg 默认启用策略
`find_package(FFmpeg QUIET)` — 找到就默认启用，未找到保持 pl_mpeg。不强制要求 FFmpeg 安装。

### KD2: MiniGame Shader 编译
已实现运行时选择: D3D11 DXBC / OpenGL GLSL / Metal MSL。下一步: shaderc 预编译为 .bin 嵌入。

### KD3: RPC 帧预览
getFrame RPC 已实现 (base64 PNG)。编辑器轮询待完善。

### KD4: Demo 场景设计
- 场景 1: 古典教室 (KAG 文本 + 立绘 + BGM + 转场)
- 场景 2: Live2D 动态角色展示
- 场景 3: MiniGame 3D 迷宫探索
- 场景 4: 视频过场 + 存档/读档

### KD5: Electron 打包
Windows NSIS + macOS DMG + Linux AppImage。引擎以 `extraResources` 内嵌。配置就绪，需实际打包测试。

## Scope Boundaries

- **不做**: Live2D OpenGLShared/Metal（移交共同开发者）
- **不做**: 移动端适配（需真机）
- **不做**: 跨平台 CI 测试启用（需非 GPU 环境）
- **不做**: 实时协作编辑

## Related Knowledge

- docs/solutions/best-practices/alpha-to-beta-sprint.md — Alpha→Beta 升级模式
