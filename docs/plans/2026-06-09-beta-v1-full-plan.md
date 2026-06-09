---
title: 引擎 Beta → v1.0 全量推进计划
type: feature
status: pending
date: 2026-06-09
origin: 完成度审查 (~90% Alpha+) — 冲刺可演示版本 + 编辑器打磨 + 文档
prerequisites: Alpha+ 引擎 (✅), Electron 编辑器 (✅), 技术债 19/23 闭合 (✅)
---

# 引擎 Beta → v1.0 全量推进计划

## Summary

引擎 Alpha+ 已完成核心架构、KAG 兼容、渲染管线、编辑器框架。本计划覆盖三条并行轨道：

1. **引擎 Beta 冲刺** — MiniGame shader + FFmpeg 默认 + Demo 场景
2. **编辑器打磨** — AI 上下文优化 + RPC 帧预览 + Electron 打包
3. **文档生态** — 用户教程 + API 完善 + 发布准备

12 个实施单元，3 组可并行推进。

## Implementation Units

### 轨道 A: 引擎 Beta 冲刺

| IU | 内容 | 关键文件 | 优先级 |
|----|------|----------|:---:|
| A1 | FFmpeg 默认启用 + 跨平台 CMake 检测 | `CMakeLists.txt`, `VideoPlayer.cpp` | P0 |
| A2 | MiniGame PBR shader 编译 (替换 debug wireframe) | `MiniGame/`, `shaders/` | P0 |
| A3 | Demo 场景 — 完整演示 (Live2D + MiniGame + 视频 + 存档) | `scripts/demo/` | P1 |

### 轨道 B: 编辑器打磨

| IU | 内容 | 关键文件 | 优先级 |
|----|------|----------|:---:|
| A4 | 编辑器 AI 上下文优化 (token 预算 + KAG API 提示词) | `web-editor/electron/main.js`, `AIPanel.jsx` | P1 |
| B1 | RPC 帧预览 — 引擎渲染截图回传编辑器 | `EditorServer.cpp`, `StageView.jsx` | P1 |
| B2 | 编辑器 UI 打磨 (主题/布局/快捷键) | `web-editor/src/` | P2 |
| B3 | Electron 打包 — 生成 Win/Mac/Linux 安装包 | `web-editor/package.json`, `electron-builder` | P1 |

### 轨道 C: 文档 + 生态

| IU | 内容 | 关键文件 | 优先级 |
|----|------|----------|:---:|
| C1 | 用户教程 — "30 分钟制作第一个视觉小说" | `docs/tutorials/` | P1 |
| C2 | KAG API 参考完善 (所有 61 命令 + 示例) | `docs/api/KAG-API.md` | P1 |
| C3 | MiniGame API 文档 (15 函数 + 示例) | `docs/api/MiniGame-API.md` | P2 |
| C4 | 构建与部署指南 (三平台 + 打包) | `docs/BUILD.md` | P2 |
| C5 | README 发布就绪 (徽章/GIF/路线图) | `README.md` | P2 |

## Key Decisions

### KD1: FFmpeg 默认启用策略
- `find_package(FFmpeg QUIET)` — 找到就默认启用，未找到保持 pl_mpeg
- 不强制要求 FFmpeg 安装，保持 pl_mpeg 作为 fallback
- 添加 `CAESURA_VIDEO_FFMPEG=ON` 为默认值（当前 OFF）

### KD2: MiniGame Shader 编译
- 使用 bgfx `shaderc` 编译 D3D11/OpenGL/Metal 三平台 shader
- PBR shader: vertex (transform + normal) + fragment (Cook-Torrance BRDF)
- 预编译为 `.bin` 嵌入，不运行时编译

### KD3: RPC 帧预览
- 引擎侧：`getFrame` RPC 方法 → bgfx 截帧 → base64 PNG → stdout
- 编辑器侧：轮询 `getFrame`（~200ms 间隔）→ Canvas 渲染
- 可选：仅在"实时预览"模式启用，正常编辑不轮询

### KD4: Demo 场景设计
- 场景 1: 古典教室 (KAG 文本 + 立绘 + BGM + 转场)
- 场景 2: Live2D 动态角色展示 (呼吸 + 表情切换)
- 场景 3: MiniGame 3D 迷宫探索 (立方体 + 碰撞 + 光照)
- 场景 4: 视频过场 + 存档/读档演示
- 全程可通过编辑器 AI 面板生成和修改

### KD5: Electron 打包
- Windows NSIS + macOS DMG + Linux AppImage
- 引擎以 `extraResources` 内嵌
- 自动检测开发/打包模式引擎路径

## Test Plan

- **A1**: `cmake -DCAESURA_VIDEO_FFMPEG=ON` → 构建 → 视频播放测试
- **A2**: 构建 MiniGame → 不再显示 wireframe → PBR 材质可见
- **A3**: `--demo` → 4 场景顺序播放 → 无崩溃/无 ErrorUI
- **A4**: AI 面板输入 "添加一个金发女孩在教室" → 生成有效 KAG 代码 → 预览运行
- **B1**: 编辑器开预览模式 → 引擎渲染帧出现在 Canvas → 帧率 ≥ 5fps
- **B2**: 快捷键 Ctrl+S 保存 → Ctrl+R 运行 → F11 全屏
- **B3**: `npm run package:win` → 安装 CaesuraAmeKAG.exe → 双击启动正常
- **C1-C5**: 按教程操作 → 30 分钟内完成第一个场景

## Scope Boundaries

- **不做**: Live2D OpenGLShared/Metal（移交共同开发者）
- **不做**: 移动端适配（需真机）
- **不做**: 跨平台 CI 测试启用（需非 GPU 环境）
- **不做**: bgfx 完整帧回读管线（RPC 帧预览用截图方案）
- **不做**: 实时协作编辑

## Assumptions

- Windows D3D11 构建环境可用
- bgfx shaderc 工具链已配置
- FFmpeg 开发库可通过 vcpkg/choco 安装
- Electron + Node.js 环境可用
- macOS 和 Linux 打包需对应平台构建环境（或由共同开发者构建）



## Open Questions (从审查中提炼)

### 实现前需明确
- **OQ1**: bgfx shaderc 需先构建 — 加入 CMake 子项目还是手动预编译？
- **OQ2**: GLSL minigame shader 需升级为 PBR (Cook-Torrance + multi-light)，还是接受跨平台 Phong fallback？
- **OQ3**: getFrame RPC 走 HTTP (EditorServer) 还是 JSON-RPC？需考虑 1-3MB 帧数据
- **OQ4**: MiniGame PBR 光照环境 — 环境光 IBL? 方向光? 几个点光源？纯 uniform 还是纹理？
- **OQ5**: Demo Live2D 场景的 fallback — 无 Live2D 时跳过还是替换为静态立绘？
- **OQ6**: B2 编辑器 UI 打磨具体范围 — 暗色主题？面板布局？快捷键完整列表？
- **OQ7**: AI 面板 token 预算上限值与超预算降级策略
- **OQ8**: MiniGame Metal shader — 需重写还是 GLSL→Metal 转换？

### 工作量校准
- **OQ9**: KAG API 文档需补 ~25 个未文档化命令（当前 36/61）
- **OQ10**: Demo 场景完全从零构建（scripts/demo/ 不存在）

### 可延后
- **OQ11**: 无障碍（a11y）— Beta 阶段不做，v1.0 再评估
- **OQ12**: 跨轨道集成测试 — 各 IU 独立验证后补一个端到端烟雾测试

