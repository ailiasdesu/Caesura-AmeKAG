# Caesura (AmeKAG) 引擎战略

> 更新: 2026-06-09 | 引擎完成度: ~82% (Alpha+)

## 核心目标

为视觉小说创作者提供**现代化、跨平台、AI IDE 辅助开发**的 galgame 引擎。

## 定位

- **不是** KiriKiri 的克隆 — KAG 兼容是入口，不是终点
- **不是** 通用游戏引擎 — 聚焦视觉小说 + 轻量 3D 互动
- **是** 首个支持 AI IDE（Codex/Copilot/Cursor）辅助开发的 galgame 引擎
- AI **不嵌入**引擎运行时 — AI 在 IDE 中协助创作，不影响游戏性能

## 技术路线

### 已完成 (Alpha+)

1. **KAG 脚本兼容** — 61 个命令，6 个子模块 (layer/text/audio/system/transition/video)
2. **D3D11 渲染管线** — 零 TDR，稳定 1-2ms GPU/帧，GpuMonitor 防护
3. **跨平台架构** — SDL3 + bgfx + Lua 5.4，8 个纯虚接口
4. **资源管理** — 异步加载 + XP3/CARC 加密包 + 6 级纹理预算
5. **多线程 JobSystem** — 粒子/CARC/纹理异步，主线不阻塞
6. **Live2D Cubism 5** — Windows D3D11 ✅，条件编译，MIT 法律安全
7. **沙箱安全** — Lua require 白名单 + I/O 禁用 + 指令预算
8. **核心约束合规** — 所有 Lua 绑定仅走抽象接口 + BackendRegistry（c1e84c6）

### 当前阶段（修复技术债 + 补齐平台）

1. 跨平台 CI 修复 (Linux/macOS) — 全局约束排最后
2. Live2D 渲染路径补齐 (OpenGLShared + Metal)
3. submit_batch RTT view→tex 键名修复 (TD-17)

### 中期目标 (Beta)

4. Electron 可视化编辑器（JSON-RPC 后端已就绪）
5. FFmpeg 视频解码替代 pl_mpeg
6. MiniGame 3D 小游戏后端实现
7. CARC 差分更新 (Delta CARC)
8. 移动端生命周期适配

## 架构原则

- **AI 不嵌入引擎** — AI 在 IDE 中辅助开发，不在运行时
- **主线剧情单线程** — 保证流程确定性
- **多核用于辅助** — 资源加载/解析/解码/粒子/物理
- **Lua 只走抽象接口** — 禁止直接引用具体 C++ 类，后端可替换、沙箱安全、AI 可理解
- **Lua 沙箱安全** — require 白名单 + I/O 禁用 + 指令预算
- **条件编译外部 SDK** — Live2D 等专有 SDK 不提交仓库

## 优先级矩阵

| 优先级 | 领域 | 状态 |
|--------|------|------|
| P0 | 引擎稳定性（零 TDR） | ✅ 已达成 |
| P0 | 核心约束合规（抽象接口） | ✅ 已达成 |
| P1 | KAG 兼容性 (61 cmd) | ✅ 已达成 |
| P1 | 技术债修复 (TD 1-22) | ✅ 12/22 闭合 |
| P2 | Live2D 全平台 | ⚠️ 1/4 路径完成 |
| P2 | 跨平台 CI | ❌ 全局约束最后 |
| P3 | MiniGame 3D | ❌ 接口占位 |
| P3 | Electron 编辑器 | ⚠️ 后端就绪 |
| P3 | 移动端 | ❌ 存根 |