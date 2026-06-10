---
title: 编辑器稳定化 + Electron 打包
type: feature
status: planned
date: 2026-06-10
origin: 引擎 ~96% 完成 → 编辑器需稳定化 → 可演示安装包
prerequisites: 引擎核心 (✅), RPC 桥接 (✅), 12 编辑器组件 (✅)
---

# 编辑器稳定化 + Electron 打包

## Summary

引擎核心 ~96% 就绪。编辑器 12 组件代码完整但从未端到端验证。本计划覆盖修复已知问题 + 启动验证 + Electron 打包生成可分发安装包。

## Implementation Units

### E1: 修复已知缺陷
- `main.cjs` 引擎路径 `Debug` → `Release`（与构建配置一致）
- 检查 `node_modules` 依赖完整性，必要时 `npm install`
- `extraResources` filter 路径修正（`scripts/**` / `assets/**` 存在性验证）

### E2: 编辑器启动 + 面板验证
- 启动 `npm run dev` → 确认 Electron 窗口打开
- 逐面板验证：StageView / CodeEditor / SceneList / AssetPanel / PropertyPanel / AIPanel / Live2DPanel / MiniGamePanel / LogPanel / SaveManager / SettingsPage
- 修复任何渲染崩溃或空白面板

### E3: 引擎 RPC 连通测试
- ping → run 测试脚本 → getFrame 帧预览 → eval 单行 Lua → getState
- 验证日志面板接收引擎日志

### E4: Electron 打包
- `npm run package:win` → 生成 NSIS 安装包
- 验证安装包结构（引擎内嵌、脚本目录、assets）
- 安装 → 启动 → 编辑器工作正常

## Key Decisions

- 引擎路径硬编码为 `build_nol2d/Release`（与 `cmake --build` 输出一致）
- Windows-only 打包（`package:win`），Linux/macOS 打包需对应平台环境
- 编辑器 UI 功能验证为主，不做视觉重设计

## Test Plan

- E1：`npm run dev` 启动无崩溃
- E2：12 个面板全部渲染，无 console error
- E3：ping < 100ms, run 执行 KAG 脚本, getFrame 返回 base64 PNG
- E4：安装包 < 200MB, 安装后编辑器可启动
