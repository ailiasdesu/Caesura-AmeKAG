---
name: Caesura Product-Ready Plan
created: 2026-06-12
status: active
---

# Caesura (AmeKAG) 产品化计划

## 当前基线

架构解耦完成（26 api/ 接口，237/237 测试全绿），Engine 拆分完成，CI 三端就位，基础文档已有。

**但离"能被 galgame 创作者使用"还有距离。** 当前引擎是一个架构优美的空壳——Live2D/MiniGame/Steam 全是桩，Demo 只是一个冒烟测试，没有人真正用它做出过一个 galgame 场景。

## 目标

让引擎达到 **Alpha 可用** 状态：一个创作者下载后能写出 KAG 脚本、看到画面、听到声音、打包发布。

## 赛道

---

### R1: 编辑器集成 —— 创作者入口

> 当前 web-editor/ 是独立的前端项目，与引擎通过 RPC 通信。这个链路从未端到端验证过。

**IU R1.1:** 验证 web-editor ↔ Engine RPC 通信链路
- 启动 Engine（editorMode=true）→ 打开 web-editor → 确认 StageView 能收到渲染帧
- 修通 CodeEditor → KAG 脚本 → Engine 执行的闭环
- 文件: `web-editor/src/*`, `src/rpc/EditorServer.cpp`

**IU R1.2:** 编辑器中的 Live2D 预览面板
- Live2DPanel.jsx 当前是占位符
- 通过 RPC 加载 .moc 模型文件 → 在 StageView 中渲染
- 文件: `web-editor/src/components/Live2DPanel.jsx`, `src/live2d/`

**IU R1.3:** 一键打包（KAG 脚本 + 资源 → .carc 归档）
- 前端 "Build" 按钮 → RPC 调用 CARCWriter → 生成可分发的 .carc 文件
- 文件: `src/archive/CARCWriter.cpp`, `web-editor/src/components/AssetPanel.jsx`

---

### R2: Live2D 真实集成 —— 核心差异化能力

> 当前 Live2DBackend 编译条件受 Cubism SDK 限制。NullAnimationBackend 是桩。创作者无法在引擎中使用 Live2D 立绘。

**IU R2.1:** 无 SDK 环境下的 Live2D 降级方案
- 当 Cubism SDK 不可用时，NullAnimationBackend 应能加载静态 PNG 作为立绘
- 使 Demo 在有/无 SDK 的环境下都能展示角色立绘
- 文件: `src/live2d/NullAnimationBackend.cpp`

**IU R2.2:** Cubism SDK 集成文档
- 文档：如何获取 Cubism SDK license → 放置 SDK 文件 → 重新构建 → 验证 Live2D 可用
- 文件: `docs/guides/live2d-setup.md`

---

### R3: 资源管线文档 —— 创作者工作流

> 创作者需要知道：我的图片/音频/立绘素材放哪、什么格式、怎么在 KAG 脚本中引用。

**IU R3.1:** 资源格式规范文档
- 支持的图片格式（PNG/JPG/TGA/BMP）、音频格式（WAV/FLAC/MP3）、视频格式（MPEG1）
- 资源目录结构约定
- 文件: `docs/guides/asset-pipeline.md`

**IU R3.2:** CARC 打包指南
- 如何用 `tools/carc_pack` 打包资源
- CARC 格式说明（加密/压缩/签名）
- 文件: `docs/guides/carc-packaging.md`

---

### R4: 测试覆盖率补齐 —— 质量底线

> 237 测试中，script 和 entry 模块仍无直接的单元测试文件（test_script_*.cpp 和 test_entry_*.cpp 不存在）。

**IU R4.1:** script 模块单元测试
- 创建 `tests/cpp/test_script_bindings.cpp`：DevCoreBinding/RenderBinding/VFXBinding/SteamBinding 的独立测试
- 创建 `tests/cpp/test_game_state.cpp`：GameState push/pop/clear 测试
- 文件: `tests/cpp/test_script_bindings.cpp`, `tests/cpp/test_game_state.cpp`

**IU R4.2:** entry 模块单元测试
- EngineConfig 默认值验证
- Engine 构造/shutdown 生命周期
- 文件: `tests/cpp/test_entry.cpp`

---

### R5: CI 跨平台验证 —— 真正的三端绿

> macOS CI 的 `|| true` 已移除，但从未在远端真正跑过。Linux CI 也未验证。

**IU R5.1:** 触发一次完整 CI 运行
- Push 到 master → 观察三端 CI 结果
- 根据失败日志逐项修复
- 目标：三端 CI 全部自然绿（无 `|| true`、无 `continue-on-error`）

---

## 执行顺序

```
R5 (CI验证) ──→ R1 (编辑器) ──→ R2 (Live2D) ──→ R3 (文档) ──→ R4 (测试)
```

R5 先做——先确认三端 CI 的真实状态。R1/R2 可并行。R3/R4 收尾。

## 非目标

- 性能优化
- AI 辅助创作
- 多语言支持
- Steam 真实集成
- MiniGame 完整实现
- 移动端移植