# 编辑器 Bug 修复计划

> 日期: 2026-06-10 | 状态: 进行中

## 问题清单

| # | Bug | 症状 | 根因 |
|---|-----|------|------|
| B1 | 舞台黑色覆盖层 | 舞台上层叠加了黑色阴影，下层才能看到网格 | `.stage-viewport` 的 `box-shadow: 0 0 60px rgba(0,0,0,0.5)` 在视口周围产生大范围黑色光晕 |
| B2 | 舞台不支持鼠标拖拽 | 无法用鼠标拖动舞台平移视图 | StageView 未实现 mousedown/move/up 平移逻辑 |
| B3 | 视图切换隔离 | 代码界面与舞台界面可能重叠渲染 | 需验证 `activePanel` 条件渲染的互斥性 |

## 实施单元

### IU-1: 修复舞台黑色覆盖层 (B1)
**文件**: `web-editor/src/styles/editor.css`
**变更**:
- `.stage-viewport` 的 `box-shadow` 中移除 `0 0 60px rgba(0,0,0,0.5)`，替换为微弱的 `0 0 20px rgba(0,0,0,0.15)`
- `.stage-viewport::before` 的 `z-index: -1` 改为 `z-index: 0`（在有 `position:relative` 的父容器内正确定位）
- canvas 设置 `z-index: 1; position: relative` 确保在 pseudo-element 之上

### IU-2: 实现舞台鼠标拖拽平移 (B2)
**文件**: `web-editor/src/components/StageView.jsx`
**变更**:
- 新增 `panX`、`panY` 状态（useRef）
- 在 `stage-area` 上监听 `mousedown` → 记录起点；`mousemove` → 计算 delta 更新偏移；
  `mouseup` → 停止拖拽
- 中间滚轮点击 (button===1) 或按住 Space+左键触发平移模式
- `.stage-viewport` 的 `transform` 改为 `scale(${zoom}) translate(${panX}px, ${panY}px)`
- 在 EditorContext 中新增 `SET_PAN` action 持久化偏移

### IU-3: 验证视图切换隔离 (B3)
**文件**: `web-editor/src/App.jsx`, `web-editor/src/styles/editor.css`
**变更**:
- 确认 `{activePanel === "scene" && <StageView />}` 和 `{activePanel === "code" && <CodeEditor />}` 互斥
- `.editor-body` 设置 `position: relative`，子组件 `position: absolute; inset: 0`
  确保切换时上一个组件彻底卸载

## 测试验证

- `npm run dev` 启动编辑器
- B1: 舞台显示纯色背景 + 网格，无黑色光晕
- B2: 鼠标拖拽平移舞台，偏移量实时更新
- B3: 点击"舞台"/"代码"标签切换，无重叠，无残留 DOM
