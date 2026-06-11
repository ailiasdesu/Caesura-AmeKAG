---
problem_type: known_issue
category: runtime-errors
severity: medium
status: fixed
date: 2026-06-10
fixed_date: 2026-06-10
branch: master
files:
  - web-editor/src/components/StageView.jsx
  - web-editor/src/context/EditorContext.jsx
  - web-editor/src/styles/editor.css
tags:
  - editor
  - stage
  - ux
---

# 舞台界面不支持鼠标拖拽平移

## 症状
舞台视口无法通过鼠标拖拽来平移（pan）画面。

## 根因
StageView.jsx 未实现鼠标拖拽事件处理。`onDragOver`/`onDrop` 仅用于素材拖入。

## 修复 (2026-06-10)
1. 在 `stage-area` 上添加 `onMouseDown`/`onMouseMove` 监听
2. 中键点击 (button===1) 或 Alt+左键触发平移模式
3. 拖拽 delta 通过 `SET_PAN` action 更新 `stagePanX`/`stagePanY`
4. `.stage-viewport` transform 改为 `scale(zoom) translate(panX, panY)`
5. 适配按钮同时执行 `RESET_PAN` 归位
6. 全局 `mouseup` 监听防止在区域外释放时状态卡住
7. 同步修复：CSS box-shadow 从 60px 黑色光晕缩小为 12px 微弱阴影
