# Caesura (AmeKAG) 下一步计划

> 更新: 2026-06-09 | 引擎完成度: ~87% (Alpha+)

## 已完成 (2026-06-09 会话)

| 任务 | 提交 | 状态 |
|------|------|:--:|
| A1: TD-17 submit_batch RTT→tex | 已就位 | ✅ |
| A2: TD-12 CryptoEngine 跨平台 | 已就位 | ✅ |
| D1: TD-15 FFmpeg 视频解码 + Lua 绑定 | c701cb1 | ✅ |
| D2: TD-14 DeltaCARC 差分更新 | 1f696cd | ✅ |
| D3: TD-21 BgfxMiniGameBackend 3D | 9f031dd | ✅ |
| C1: TD-22 跨平台 CI | db2964f | ✅ |
| 核心约束合规 + 文档重写 | c1e84c6~1bd5812 | ✅ |
| ce-compound-refresh | 1d8e1f7 | ✅ |

## 移交共同开发者

| 任务 | 原因 |
|------|------|
| Live2D OpenGLSharedRenderPath | 需 Linux 环境实测 |
| Live2D MetalNativeRenderPath | 需 macOS 环境实测 |
| OpenGLReadbackRenderPath | 回退路径，Linux 实测 |
| MobileAdapter 移动端适配 | 需真机环境 |
| MiniGame 3D shader 编译 | 需 shaderc 工具链 |

## 长期 (Beta+)

- Electron 可视化编辑器前端（JSON-RPC 后端已就绪）
- FFmpeg 默认编译启用
- CARC chain trust 完善
- nlohmann/json 评估引入