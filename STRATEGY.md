# Caesura (AmeKAG) 引擎战略

> 更新: 2026-06-09 | 引擎完成度: ~87% (Alpha+)

## 核心目标

为视觉小说创作者提供**现代化、跨平台、AI IDE 辅助开发**的 galgame 引擎。

## 定位

- **不是** KiriKiri 的克隆 — KAG 兼容是入口，不是终点
- **不是** 通用游戏引擎 — 聚焦视觉小说 + 轻量 3D 互动
- AI **不嵌入**引擎运行时 — AI 在 IDE 中协助创作

## 技术路线

### 已完成 (Alpha+)

1. **KAG 脚本兼容** — 61 命令 + 8 视频函数 + mini_game API
2. **D3D11 渲染管线** — 零 TDR，GpuMonitor 防护
3. **跨平台架构** — SDL3 + bgfx + Lua 5.4，8 纯虚接口
4. **核心约束合规** — Lua 仅走抽象接口 + BackendRegistry
5. **多线程 JobSystem** — 粒子/纹理/CARC/视频解码异步
6. **Live2D Cubism 5** — Windows D3D11，条件编译安全
7. **视频播放** — pl_mpeg + FFmpeg 双后端，Lua 全绑定
8. **DeltaCARC 差分更新** — AES-256-GCM 加密 delta
9. **3D 小游戏** — BgfxMiniGameBackend, cube + camera + Lua API
10. **沙箱安全** — require 白名单 + I/O 禁用 + 指令预算

### 移交共同开发者

| 任务 | 原因 |
|------|------|
| Live2D OpenGLShared | 需 Linux 实测 |
| Live2D Metal | 需 macOS 实测 |
| 移动端适配 | 需真机 |

### 中期目标 (Beta)

- Electron 可视化编辑器（JSON-RPC 后端已就绪）
- FFmpeg 默认启用（当前需 CAESURA_VIDEO_FFMPEG 定义）
- MiniGame shader 编译（当前 debug wireframe）

## 优先级矩阵

| 优先级 | 领域 | 状态 |
|--------|------|------|
| P0 | 引擎稳定性 + 核心约束 | ✅ |
| P1 | KAG 兼容性 (61 cmd) | ✅ |
| P1 | 技术债 (18/22 闭合) | ✅ |
| P2 | Live2D 全平台 | ⚠️ 移交 |
| P2 | 跨平台 CI | ✅ 已修复 |
| P3 | MiniGame 3D | ✅ |
| P3 | Electron 编辑器 | ⚠️ 后端就绪 |
| P3 | 移动端 | ❌ 存根 |