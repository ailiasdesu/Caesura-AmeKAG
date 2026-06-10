---
problem_type: known_issue
category: runtime-errors
severity: medium
status: open
date: 2026-06-10
branch: master
files:
  - web-editor/src/components/StageView.jsx
tags:
  - editor
  - stage
  - ux
---

# 舞台界面不支持鼠标拖拽平移

## 症状
舞台视口无法通过鼠标拖拽来平移（pan）画面。用户只能通过缩放和 fit 按钮调整视图，但无法移动视口位置。

## 根因分析
StageView.jsx 当前未实现鼠标拖拽事件处理。舞台区域的 `onDragOver`/`onDrop` 仅用于素材拖入，未处理视口平移逻辑。

## 修复方向
1. 在 `stage-area` 上监听 `mousedown`/`mousemove`/`mouseup`
2. 拖拽时通过 CSS `transform: translate()` 偏移视口位置
3. 使用 `useRef` 追踪拖拽起点和当前偏移量
4. 在 EditorContext 中新增 `stagePanX`/`stagePanY` 状态或使用本地 ref

## 优先级
中 — 不影响核心功能，但影响编辑体验。
