---
title: Electron 可视化编辑器 + AI 辅助面板 — 完整架构实现
date: 2026-06-09
category: docs/solutions/architecture-patterns
module: Editor
problem_type: architecture_pattern
component: development_workflow
severity: high
applies_when:
  - 为 C++ 游戏引擎构建可视化编辑器前端
  - 需要代码双向同步（可视化操作 ↔ 脚本代码）
  - 集成 AI 代码生成到创作工具中
  - Electron 打包 C++ 引擎为独立桌面应用
tags:
  - electron-editor
  - ai-assisted-development
  - kag-lua
  - visual-novel-engine
  - rpc-architecture
  - canvas-2d
  - vibe-coding
related_components:
  - RpcServer
  - KAGParser
  - BackendRegistry
  - UnifiedBinding
---

# Electron 可视化编辑器 + AI 辅助面板 — 完整架构实现

## 当前状态: ✅ 已完成

- **11 组件**: StageView, SceneList, CodeEditor, AssetPanel, PropertyPanel, AIPanel, Live2DPanel, MiniGamePanel, LogPanel, SaveManager, SettingsDialog
- **3 AI 后端**: OpenAI / Codex CLI / Custom — 真实 Electron IPC 桥接 (非 mock)
- **RPC 方法**: ping, run, eval, stop, assets, getState, getFrame, logs
- **ES 模块修复**: main.js→main.cjs, preload.js→preload.cjs
- **KAG 系统提示词**: AI 面板注入引擎上下文

## 架构

### 整体架构

```
┌─────────────────────────────────────────────────────────┐
│                    Electron Shell                        │
│  ┌───────────────────────────────────────────────────┐  │
│  │              React SPA (Vite)                      │  │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────────────┐   │  │
│  │  │StageView │ │SceneList │ │  PropertyPanel   │   │  │
│  │  │(Canvas2D)│ │(场景排序) │  │(属性↔代码同步)  │   │  │
│  │  └──────────┘ └──────────┘ └──────────────────┘   │  │
│  │  ┌──────────┐ ┌──────────────────────────────┐    │  │
│  │  │AssetPanel│ │         AIPanel              │    │  │
│  │  │(场景+素材)│ │ (对话+多后端+@generate/@fix) │    │  │
│  │  └──────────┘ └──────────────────────────────┘    │  │
│  │  ┌──────────────┐ ┌──────────┐ ┌──────────┐      │  │
│  │  │ CodeEditor   │ │LogPanel  │ │Live2D/   │      │  │
│  │  │ (语法高亮)    │ │(实时日志) │ │MiniGame  │      │  │
│  │  └──────────────┘ └──────────┘ └──────────┘      │  │
│  │         ↕ EditorContext (状态管理中心)             │  │
│  │         ↕ kagParser (正则 AST 解析器)              │  │
│  └───────────────────────────────────────────────────┘  │
│         ↕ IPC (contextBridge)                           │
│  ┌───────────────────────────────────────────────────┐  │
│  │  main.cjs: 引擎管理 + JSON-RPC + AI Chat Bridge   │  │
│  └───────────────────────────────────────────────────┘  │
│         ↕ stdin/stdout (JSON-RPC)                      │
│  ┌───────────────────────────────────────────────────┐  │
│  │        CaesuraAmeKAG.exe (--headless)             │  │
│  │        RpcServer + Engine + BackendRegistry       │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

## 关键决策

### KTD1: Canvas 2D 舞台（非 bgfx 回读）
用 HTML Canvas 2D 绘制素材缩略图近似布局，不做 bgfx 帧回读。bgfx 帧回读延迟 50-200ms, Canvas 2D <5ms。真正预览走 RPC `run` 在引擎窗口渲染。

### KTD2: 正则 KAG 解析器（覆盖 80% 场景）
支持 `KAG.show_image`, `KAG.show_text`, `KAG.wait_click`, `KAG.play_bgm`, `KAG.transition` 等模式。不支持嵌套表达式和条件分支。

### KTD3: AI 上下文预算管理
- 当前脚本（截断至 4000 字符）
- KAG API 摘要（~2000 字符）
- 最近 3 条引擎日志
- 总计 ~7000 字符

### KTD4: Provider 模式 AI 后端
- `openai.js`: OpenAI REST API
- `codex.js`: 本地 Codex CLI (真实 IPC)
- `custom.js`: OpenAI-compatible 端点

### KTD5: electron-builder 跨平台打包
Win NSIS / macOS DMG / Linux AppImage。引擎以 `extraResources` 内嵌。

## Integration Points

### RPC 接口
```
ping       → {}                                   → {pong: true}
run        → {script: string}                     → {status: "running"|"done"}
eval       → {code: string}                       → {result: any}
stop       → {}                                   → {status: "stopped"}
assets     → {}                                   → {images: [...], audio: [...], scripts: [...]}
getState   → {}                                   → {currentScene: string, variables: {...}}
getFrame   → {}                                   → {png: base64} (引擎帧预览)
logs       → {}                                   → {entries: [...]}
```

### 与 BackendRegistry 的关系
编辑器操作 → KAG Lua 代码 → UnifiedBinding → BackendRegistry → 具体后端。保证编辑器生成的操作经过 KAG 脚本层。

## Known Limitations

- Canvas 2D 舞台预览不显示透明度/混合模式效果
- KAG 解析器不支持嵌套函数调用和条件分支
- AI 上下文不含素材文件内容（仅文件名）
- Codex provider 需要项目在 Codex IDE 中打开
- 打包产物需要引擎二进制已构建且路径匹配

## Related

- `docs/solutions/architecture-patterns/minigame-3d-pipeline.md` — 3D 小游戏管线架构
- `docs/solutions/runtime-errors/kag-backend-circular-delegation-stack-overflow.md` — KAG 后端循环委托问题
- `docs/strategy/ENGINE_ANALYSIS.md` — 引擎完整度分析
- `docs/strategy/STRATEGY.md` — 技术路线与优先级
