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

## Context

Caesura (AmeKAG) 是一个 C++ 视觉小说引擎，使用 KAG Lua 脚本驱动场景。最初的编辑器只是一个带有 Run/Stop 按钮的文本编辑器和素材浏览器。创作者需要：

- 可视化地看到素材在场景中的排列，而不是对着坐标数字盲调
- 直观编排素材出现顺序和时间线
- 在不记忆全部 KAG API 的情况下生成代码
- 快速的"描述意图 → AI 生成代码 → 预览效果"闭环

编辑器需要在 Electron 中运行，通过 JSON-RPC 与 C++ 引擎通信，最终打包为跨平台桌面应用。

## Architecture

### 整体架构

```
┌─────────────────────────────────────────────────────────┐
│                    Electron Shell                        │
│  ┌───────────────────────────────────────────────────┐  │
│  │              React SPA (Vite)                      │  │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────────────┐   │  │
│  │  │StageView │ │ Timeline │ │  PropertyPanel   │   │  │
│  │  │(Canvas2D)│ │(事件块)  │ │  (属性↔代码同步) │   │  │
│  │  └──────────┘ └──────────┘ └──────────────────┘   │  │
│  │  ┌──────────┐ ┌──────────────────────────────┐    │  │
│  │  │AssetPanel│ │         AIPanel              │    │  │
│  │  │(拖拽源)  │ │ (对话+多后端+@generate/@fix) │    │  │
│  │  └──────────┘ └──────────────────────────────┘    │  │
│  │         ↕ EditorContext (状态管理中心)             │  │
│  │         ↕ kagParser (正则 AST 解析器)              │  │
│  └───────────────────────────────────────────────────┘  │
│         ↕ IPC (contextBridge)                           │
│  ┌───────────────────────────────────────────────────┐  │
│  │  main.js: 引擎管理 + JSON-RPC + AI Chat Bridge    │  │
│  └───────────────────────────────────────────────────┘  │
│         ↕ stdin/stdout (JSON-RPC)                      │
│  ┌───────────────────────────────────────────────────┐  │
│  │        CaesuraAmeKAG.exe (--headless)             │  │
│  │        RpcServer + Engine + BackendRegistry       │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

### 组件职责

| 组件 | 文件 | 职责 |
|------|------|------|
| **StageView** | `src/components/StageView.jsx` | Canvas 2D 画布渲染当前场景素材布局，支持拖入素材、网格吸附、安全区显示 |
| **Timeline** | `src/components/Timeline.jsx` | 水平时间轴，事件块按时间排列（素材、对话、音频、转场），拖拽排序 |
| **PropertyPanel** | `src/components/PropertyPanel.jsx` | 选中元素后显示属性编辑器（位置、缩放、透明度等），编辑实时触发代码同步 |
| **AssetPanel** | `src/components/AssetPanel.jsx` | 素材列表按类型分组，支持拖拽源（drag source）到舞台/时间线 |
| **AIPanel** | `src/components/AIPanel.jsx` | AI 对话面板：消息列表 + 流式渲染 + 后端切换 + `@generate`/`@fix` 指令 |
| **EditorContext** | `src/context/EditorContext.jsx` | 全局状态管理：当前脚本、选中元素、素材列表、引擎状态、AI 设置 |
| **kagParser** | `src/parser/kagParser.js` | 正则 KAG Lua 解析器：`parseScript()` → AST、`generateCode()` → Lua 字符串 |
| **AI Providers** | `src/providers/{openai,codex,custom}.js` | AI 后端抽象：`chat(messages)` → `{content}`，支持 OpenAI / Codex / 自定义端点 |
| **SettingsDialog** | `src/components/SettingsDialog.jsx` | AI 设置持久化：API key、端点 URL、模型选择、temperature |
| **main.js** | `electron/main.js` | Electron 主进程：引擎生命周期管理 + JSON-RPC 通信 + AI Chat IPC Bridge |

### 数据流

```
用户拖入素材到舞台
    → EditorContext.dispatch({type: "ADD_EVENT", ...})
    → Timeline 更新事件列表
    → StageView 重新渲染 Canvas
    → kagParser.generateCode(events)
    → 文本编辑器更新 KAG Lua 代码
    → 用户点击 "预览"
    → RPC run {script} → 引擎渲染 → RPC getState 轮询
```

## Key Technical Decisions

### KTD1: Canvas 2D 轻量舞台预览

**决策**: 舞台用 HTML Canvas 2D 绘制素材缩略图近似布局，不做 bgfx 帧回读。

**理由**:
- bgfx 帧回读需要 GPU→CPU 传输，延迟 50-200ms，编辑体验差
- Canvas 2D 绘制缩略图延迟 <5ms，足够编辑用
- 真正预览走 RPC `run` 在引擎窗口渲染，所见即所得

**实现**: `StageView.jsx` 监听 `events` 变化，对每个 `show_image`/`show_char` 事件绘制占位矩形或缩略图。

### KTD2: 正则 KAG 解析器（覆盖 80% 场景）

**决策**: 用正则匹配 KAG Lua 函数调用提取参数，不引入完整 Lua AST 解析器。

**支持的模式**:
- `KAG.show_image(layer, file, x, y, opts)` → `{type: "show_image", layer, file, x, y, opacity, ...}`
- `KAG.show_text(name, text)` → `{type: "show_text", name, text}`
- `KAG.wait_click()` → `{type: "wait_click"}`
- `KAG.play_bgm(file, volume, loop)` → `{type: "play_bgm", file, volume, loop}`
- `KAG.transition(type, duration)` → `{type: "transition", type, duration}`

**限制**: 不支持嵌套表达式、条件分支内的 KAG 调用、自定义 Lua 函数包装的 KAG 调用。这些场景保留文本编辑。

### KTD3: AI 上下文预算管理

每次对话注入:
- 当前脚本（截断至 4000 字符）
- KAG API 摘要（~2000 字符，包含所有 KAG.* 函数签名和参数说明）
- 最近 3 条引擎日志（错误/警告级别）

总计控制在 ~7000 字符内，适配 GPT-4o 128K 上下文窗口但有充足余量做连续对话。

### KTD4: Provider 模式 AI 后端

```
interface AIProvider {
  chat(messages: Message[], onToken?: (token: string) => void): Promise<{content: string}>
}
```

三种实现:
- `openai.js`: 直接调用 OpenAI REST API (`/v1/chat/completions`)
- `codex.js`: 通过 PowerShell 调用本地 Codex agent
- `custom.js`: 自定义 OpenAI-compatible 端点（Ollama、vLLM 等）

通过 `EditorContext` 的 `provider` 状态切换，设置持久化到 `app.getPath('userData')/settings.json`。

### KTD5: electron-builder 跨平台打包

```json
"win":  { "target": ["nsis"] },
"mac":  { "target": ["dmg"] },
"linux": { "target": ["AppImage"] }
```

引擎以 `extraResources` 内嵌，打包后路径 `process.resourcesPath/engine/CaesuraAmeKAG.exe`。开发模式下 spawn 本地构建产物。

## Integration Points

### 与引擎的 RPC 接口

```
┌─────────────┐     JSON-RPC (stdin/stdout)     ┌──────────────┐
│   Editor    │ ───────────────────────────────→ │   Engine     │
│  (Electron) │ ←─────────────────────────────── │  (C++ bgfx)  │
└─────────────┘                                  └──────────────┘

Methods:
  ping       → {}                                   → {pong: true}
  run        → {script: string}                     → {status: "running"|"done"}
  eval       → {code: string}                       → {result: any}
  stop       → {}                                   → {status: "stopped"}
  assets     → {}                                   → {images: [...], audio: [...], scripts: [...]}
  getState   → {}                                   → {currentScene: string, variables: {...}}
```

引擎侧 `RpcServer` 通过 `stdin` 读 JSON-RPC 请求，`stdout` 写 JSON 响应。Push 事件（日志）无 `id` 字段直接推送。

### 与 BackendRegistry 的关系

编辑器不直接操作 C++ 后端。所有操作通过 KAG Lua 脚本间接驱动引擎：
```
编辑器操作 → KAG Lua 代码 → UnifiedBinding → BackendRegistry → 具体后端
```

这保证了编辑器生成的所有操作都经过 KAG 脚本层，与手写脚本行为一致。

## Verification

- **E1-E4**: 打开编辑器 → 拖入素材到舞台 → 调整属性 → 代码区自动更新 → 点预览 → 引擎渲染 ✅
- **E5-E7**: 打开 AI 面板 → 输入描述 → AI 返回 KAG 代码 → 插入编辑器 → 预览运行 ✅
- **E8**: `npm run package` → 生成安装包 → 双击启动 → 编辑器正常工作（exe 生成，需引擎二进制就位）✅

## Known Limitations

- Canvas 2D 舞台预览不显示透明度/混合模式效果（需真正渲染才能看到最终合成效果）
- KAG 解析器不支持嵌套函数调用和条件分支内的标签
- AI 上下文不含素材文件内容（仅文件名），不感知素材实际外观
- Codex provider 需要项目在 Codex IDE 中打开，不是纯独立模式
- 打包产物需要引擎二进制已构建且路径匹配

## Related

- `docs/solutions/architecture-patterns/minigame-3d-pipeline.md` — 3D 小游戏管线架构
- `docs/solutions/runtime-errors/kag-backend-circular-delegation-stack-overflow.md` — KAG 后端循环委托问题
- `ENGINE_ANALYSIS.md` — 引擎完整度分析
- `STRATEGY.md` — 技术路线与优先级
