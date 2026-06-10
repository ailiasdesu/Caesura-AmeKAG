---
title: 编辑器↔引擎 RPC 全连通 — stderr 转发 + 帧预览 + 渲染修复
date: 2026-06-10
category: docs/solutions/architecture-patterns
module: Editor, Engine
problem_type: architecture
component: RpcServer, Electron
severity: medium
tags:
  - rpc
  - editor
  - frame-preview
  - json-rpc
  - stderr-forwarding
---

# 编辑器↔引擎 RPC 全连通

## Context

编辑器 React UI 需要三大实时数据流：
1. **日志** — 引擎 stderr 输出实时显示在 LogPanel
2. **帧预览** — StageView 显示引擎渲染画面
3. **脚本执行** — Run 按钮触发引擎执行 KAG 脚本

## Solution

### 1. stderr → LogPanel

`web-editor/electron/main.cjs` 中 `engineProcess.stderr.on("data")` 改为：
- 按行拆分 stderr 输出
- 自动归类级别：含 ERROR/FATAL/FAIL → error，含 WARN → warn，其余 info
- 通过 `mainWindow.webContents.send("engine-log", ...)` IPC 事件发送到渲染进程
- 渲染进程通过 `window.caesura.onLog(callback)` 监听，App.jsx dispatch ADD_LOG

### 2. 帧预览

`Engine::captureFrameForRpc` 增强：
- 先调用 `engine_update` + `render()` 提交绘制调用
- 再通过 `captureFrameBase64` 调用 `bgfx::requestScreenShot` + `bgfx::frame()` 触发截图
- 读取 PNG 文件，编码为 base64，通过 JSON-RPC 返回

`StageView.jsx` 增强：
- Preview 按钮激活时，500ms 间隔轮询 `window.caesura.getFrame(VP_W, VP_H)`
- 接收 base64 帧数据，通过 `setFrameData` 触发 Canvas 重绘
- 取消 Preview 时清除帧数据，停止轮询

### 3. 脚本执行

已有实现：`RpcServer::handleRun` 将脚本写入临时文件，`luaL_dofile` 执行，`pushLog` 反馈结果。

## Key files

- `src/Core/Engine.cpp` — captureFrameForRpc 渲染前提交
- `src/Core/RpcServer.cpp` — JSON-RPC 8 方法
- `web-editor/electron/main.cjs` — stderr 转发 + 引擎 spawn
- `web-editor/src/components/StageView.jsx` — 帧轮询 + Canvas 渲染

## Verification

- 引擎 `--editor` 模式稳定运行 3+ 秒无崩溃
- RPC ping 往返正常
- stderr 日志正确转发到 LogPanel（级别自动归类）
- Preview 模式 getFrame 返回 base64 PNG 帧
