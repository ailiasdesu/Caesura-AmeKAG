# Caesura (AmeKAG) 引擎战略

> 更新: 2026-06-09 | 引擎完成度: ~93% (Beta)

## 核心目标

为视觉小说创作者提供**现代化、跨平台、AI IDE 辅助开发**的 galgame 引擎。

## 定位

- **不是** KiriKiri 的克隆 — KAG 兼容是入口，不是终点
- **不是** 通用游戏引擎 — 聚焦视觉小说 + 轻量 3D 互动
- AI **不嵌入**引擎运行时 — AI 在 IDE 中协助创作

## 技术路线

### 已完成 (Alpha+)

1. **KAG 脚本兼容** — 61 命令 + 8 视频函数 + 15 MiniGame API
2. **D3D11 渲染管线** — 零 TDR，GpuMonitor 防护
3. **跨平台架构** — SDL3 + bgfx + Lua 5.4，8 纯虚接口
4. **核心约束合规** — Lua 仅走抽象接口 + BackendRegistry
5. **多线程 JobSystem** — 粒子/纹理/CARC/视频解码异步
6. **Live2D Cubism 5** — Windows D3D11，条件编译安全
7. **视频播放** — pl_mpeg + FFmpeg 双后端，Lua 全绑定
8. **DeltaCARC 差分更新** — AES-256-GCM 加密 delta
9. **3D 小游戏** — BgfxMiniGameBackend PBR-lite, 15 Lua API
10. **沙箱安全** — require 白名单 + I/O 禁用 + strict 模式
11. **存档系统** — JSON + AES-256-GCM + ISaveProvider + Schema v1→v5
12. **Electron 编辑器** — 舞台 + 时间线 + 属性 + AI 面板 + 多后端

### 移交共同开发者

| 任务 | 原因 |
|------|------|
| Live2D OpenGLShared | 需 Linux 实测 |
| Live2D Metal | 需 macOS 实测 |
| 移动端适配 | 需真机 |
| 跨平台 CI 测试 | 需非 GPU 测试环境 |

### 中期目标 (Beta)

- [ ] MiniGame shader 编译（当前 debug wireframe）
- [ ] FFmpeg 默认启用（当前 CAESURA_VIDEO_FFMPEG 定义）
- [ ] 编辑器 AI 上下文优化（token 预算 + 提示词调优）
- [ ] 可视化帧预览（编辑器 → RPC → 引擎渲染截图回传）
- [ ] Demo 场景（Live2D + MiniGame + 视频 完整演示）

### 远期目标 (v1.0)

- [ ] Electron 打包发布（Win/Mac/Linux 安装包）
- [ ] 用户文档 + 教程
- [ ] 移动端适配
- [ ] 社区协作工具

## 优先级矩阵

| 优先级 | 领域 | 状态 |
|--------|------|:---:|
| P0 | 引擎稳定性 + 核心约束 | ✅ |
| P1 | KAG 兼容性 (61 cmd) | ✅ |
| P1 | 技术债 (19/23 闭合) | ✅ |
| P1 | Electron 编辑器 + AI 面板 | ✅ |
| P2 | Live2D 全平台 | ⚠️ 移交 |
| P2 | 跨平台 CI | ✅ YAML |
| P3 | MiniGame 3D | ✅ |
| P3 | Electron 可视化编辑器 | ✅ |
| P3 | AI 辅助代码生成 | ✅ |
| P4 | Demo 场景 | ❌ |
| P4 | 移动端 | ❌ 存根 |
| P4 | 发行打包 | ⚠️ 配置就绪 |