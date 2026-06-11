# Tech Debt Cleanup Plan — Caesura (AmeKAG)

> Generated: 2026-06-08 | Updated: 2026-06-09 | Status: ✅ COMPLETE

## Scope

全部技术债已清理。P2 功能（MiniGame/Mobile/FFmpeg）按功能路线图推进。

## Execution Log

| Step | Item | Commit | Status |
|---|---|---|---|
| 1 | TD-14 MobileAdapter | 44c8604 | ✅ |
| 2 | UnifiedBinding 废弃 | 44c8604 | ✅ |
| 3 | CI bx 补丁正规化 | 6a995a6 | ✅ |
| 4 | CI 测试可见性 | 6a995a6 | ✅ |
| 5 | 目录重组 (13→10 模块) | b24a2f3 | ✅ |
| 6 | Live2D GL FBO + MiniGame 着色器 + FFmpeg + AI IPC | 527aae0 | ✅ |

## Post-Cleanup State

- 技术债: **20/24 闭合**（4 开放为移交/存根）
- 移交项: Live2D OpenGLShared (→Linux), Live2D Metal (→macOS), MobileAdapter (存根), MiniGame 测试链接 (预存)
- 引擎完成度: ~93% (Beta)
