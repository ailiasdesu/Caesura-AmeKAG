# Caesura (AmeKAG) 下一步计划

> 更新: 2026-06-09 | 状态: 持续更新

## 当前状态

引擎完成度 ~90% (Alpha+)。Electron 编辑器 E1-E8 全部完成。

## 已完成（本轮）

| 阶段 | 内容 | 状态 |
|------|------|:--:|
| 存档系统升级 | SU-1~6: AES-GCM + ISaveProvider + 快存 + Schema v1→v5 | ✅ |
| 技术债务修复 | U1-U4: BackendRegistry/ParticleSystem/API去重/沙盒外抽 | ✅ |
| Electron 编辑器 | E1-E8: 舞台/时间线/属性/AI面板/RPC/多后端/打包配置 | ✅ |

## 下一步选项

| # | 方向 | 预估 |
|---|------|------|
| 1 | **Demo 场景建设** — 创建完整演示（Live2D + MiniGame + 视频 + 音频） | 中 |
| 2 | **MiniGame shader 编译** — 替换 debug wireframe 为真实 PBR shader | 中 |
| 3 | **FFmpeg 默认启用** — 提升视频兼容性 | 小 |
| 4 | **编辑器 AI 优化** — token 预算调优 + 提示词迭代 | 小 |
| 5 | **跨平台 CI 测试启用** — macOS/Linux CI 测试去 continue-on-error | 中 |
| 6 | **Electron 打包试运行** — 生成 Win NSIS 安装包验证流程 | 小 |
| 7 | **RPC 帧预览** — 编辑器 → 引擎渲染截图回传 | 大 |
| 8 | **用户文档** — 教程 + API 手册 | 大 |