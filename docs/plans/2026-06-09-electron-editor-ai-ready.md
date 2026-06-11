---
title: Electron 编辑器完善 — 可视化场景编辑 + AI 辅助面板
type: feature
status: complete
date: 2026-06-09
origin: 引擎完成度审查 + 用户需求 ("两者都做")
prerequisites: RPC Server (✅), Engine 85-88% (✅), Electron shell (✅)
---

# Electron 编辑器完善 — 可视化场景编辑 + AI 辅助面板

## Summary

在现有 Electron 编辑器骨架（脚本编辑 + 素材浏览 + Run/Stop + JSON-RPC）之上，新增两个核心功能：
1. **可视化场景编辑器**：拖拽式舞台视图 + 时间线 + 属性面板 + RPC 实时预览
2. **AI 辅助面板**：可切换后端的 AI 对话侧边栏，注入引擎 API 上下文，生成/补全 KAG 脚本

## Problem Frame

当前编辑器只有基础文本编辑功能。创作者需要：
- 可视化地看到素材如何排列在场景中
- 直观编排素材出现顺序和时间
- 不需要记忆所有 KAG API 就能生成代码
- 快速的 "描述意图 → AI 生成代码 → 预览效果" 闭环

引擎侧 RPC Server 已就位 (`ping/run/eval/stop/assets/getState`)，可直接驱动编辑器预览。

## Requirements

### 可视化场景编辑器

- R1. 舞台视图：渲染当前场景的素材布局（bg/fg/char），画布分辨率对应引擎分辨率
- R2. 素材面板：按类型分组（背景/立绘/UI/BGM/SE/语音），支持拖拽到舞台/时间线
- R3. 时间线视图：场景事件按时间顺序排列（显示素材、对话、音频、转场），可拖拽调整顺序和时长
- R4. 属性面板：选中舞台/时间线中的元素后显示属性（位置、缩放、透明度、混合模式等），编辑实时反映到代码
- R5. 代码生成：可视化操作自动生成对应 KAG Lua 代码，与文本编辑器双向同步
- R6. 实时预览：通过 RPC `eval` 推送当前场景到引擎，预览窗口显示引擎渲染结果（截图轮询）

### AI 辅助面板

- R7. 侧边栏 AI 对话面板，支持多后端切换（OpenAI API / 本地 Codex / 自定义端点）
- R8. 上下文注入：每次对话自动附加当前 KAG 脚本 + API 参考 + 引擎状态
- R9. 代码生成：AI 根据自然语言描述生成 KAG Lua 代码，可插入到编辑器
- R10. 错误诊断：引擎报错时自动将错误信息送入 AI，返回修复建议
- R11. 后端配置：API key/端点/模型选择持久化到本地设置文件

### 发行打包

- R12. Electron 应用打包为独立 exe/dmg/AppImage（跨平台分发）
- R13. 引擎以 headless 模式内嵌，用户无需单独安装引擎

## Key Technical Decisions

- KTD1. **舞台预览用 Canvas 2D 轻量实现** — 不做完整渲染回读。编辑模式下，舞台用 Canvas 绘制素材缩略图近似布局；"实时预览"按钮触发 RPC `run` 在引擎窗口渲染。避免 bgfx 帧回读的复杂性和延迟。
- KTD2. **代码双向同步** — 可视化操作 → 修改内部 AST → 序列化为 Lua 代码；文本编辑 → 解析 AST → 更新可视化。使用简单正则解析 KAG 脚本（不引入完整 Lua parser）覆盖 80% 场景。
- KTD3. **AI 后端抽象为 provider 接口** — `providers/openai.js`、`providers/codex.js`、`providers/custom.js`，通过 Vite 环境变量选择打包。设置面板运行时切换。
- KTD4. **AI 上下文 token 预算** — 每次对话注入当前脚本（截断至 4000 字符）+ KAG API 摘要（~2000 字符）+ 最近 3 条引擎日志。避免超出模型上下文窗口。
- KTD5. **Electron 打包使用 electron-builder** — 已有配置骨架，补齐 native 模块路径和引擎 exe 内嵌。

## Scope Boundaries

- **不做**: bgfx 帧回读到编辑器（性能开销大，Canvas 2D 预览足够编辑用）
- **不做**: 完整 Lua AST 解析器（正则解析覆盖标签级操作即可）
- **不做**: AI 模型训练/微调（只做 prompt engineering + RAG 注入）
- **不做**: 协作编辑/云端同步（v2 功能）
- **不做**: 移动端编辑器（桌面端优先）

## Implementation Units

### IU-E1: 舞台视图 + 素材面板重构

- 将当前左侧素材面板升级为双模式：素材列表 + 舞台画布
- 舞台用 HTML Canvas 渲染，支持拖入素材调整位置
- 舞台网格 + 吸附，分辨率可切换（1280×720 / 1920×1080）
- 素材面板支持拖拽源（drag source）

### IU-E2: 时间线视图

- 水平时间轴，事件块按时间排列
- 事件类型：素材出现/消失、对话气泡、音频播放、转场效果、等待点击
- 拖拽调整事件顺序和时间
- 点击事件块 → 属性面板显示该事件参数

### IU-E3: 属性面板 + 代码双向同步

- 选中元素后右侧显示属性编辑器
- 属性变更 → 生成 KAG Lua 代码 → 更新文本编辑器
- 文本编辑器变更 → 解析 KAG 标签 → 更新舞台/时间线
- 解析器：正则匹配 `KAG.show_image("bg", ...)` 等调用提取参数

### IU-E4: 实时预览（RPC 集成）

- "预览"按钮 → RPC `run` 发送当前脚本到引擎
- 引擎在独立窗口渲染（headless 模式关闭）
- 轮询引擎状态（RPC `getState`）反馈当前场景名
- 预览模式下编辑器保持响应，可编辑并重新推送

### IU-E5: AI 面板 — 基础框架

- 右侧或底部可折叠 AI 对话面板
- 消息列表（用户/助理角色） + 输入框 + 发送按钮
- 流式响应渲染（逐 token 显示）
- 后端切换下拉菜单

### IU-E6: AI 面板 — 上下文注入 + KAG 代码生成

- 每次对话前构建系统提示词：KAG API 参考 + 当前脚本摘要 + 引擎状态
- `@generate` 指令：AI 返回的代码块自动插入到编辑器光标位置
- `@fix` 指令：当前脚本 + 引擎错误日志 → AI 诊断建议
- 上下文缓存：连续对话共享相同系统提示词

### IU-E7: AI 后端抽象 + 设置持久化

- Provider 接口：`chat(messages, onToken)` → Promise
- OpenAI provider：使用 `node-fetch` 或 Electron 内置 fetch，支持自定义 base URL
- Codex provider：通过 IPC 调用本地 Codex agent
- 设置面板：API key、端点、模型名、temperature
- 设置存储：`electron-store` 或 `app.getPath('userData')` JSON 文件

### IU-E8: 发行打包 + 引擎内嵌

- engine-builder 配置：将 `bin/Release/CaesuraAmeKAG.exe` 打包为 extraResource
- 启动逻辑：开发模式 spawn 命令，打包模式解压内嵌引擎
- 图标资源：应用图标 + 安装程序图标
- Windows NSIS + macOS DMG + Linux AppImage

## Implementation Order

```
IU-E1 (舞台+素材) → IU-E2 (时间线) → IU-E3 (属性+同步) → IU-E4 (预览)
                                                              ↓
IU-E5 (AI框架) → IU-E6 (上下文+生成) → IU-E7 (后端+设置) → IU-E8 (打包)
```

两组可并行：E1-E4（可视化）与 E5-E7（AI）互不阻塞，E4 和 E6 共享 RPC 通道但无代码冲突。

## Verification

- **IU-E1~E4**: 打开编辑器 → 拖入素材到舞台 → 调整属性 → 代码区自动更新 → 点预览 → 引擎渲染
- **IU-E5~E7**: 打开 AI 面板 → 输入 "添加一个金发女角色在教室场景中" → AI 返回 KAG 代码 → 点击插入 → 代码出现在编辑器
- **IU-E8**: `npm run package` → 生成安装包 → 安装 → 双击启动 → 编辑器正常工作

## Deferred to Implementation

- 具体 React 组件拆分方案（根据实际 UI 复杂度决定）
- AI 提示词模板的精确措辞（需根据模型行为迭代）
- Canvas 2D 舞台渲染性能优化参数