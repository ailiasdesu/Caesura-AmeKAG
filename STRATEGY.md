---
name: Caesura (AmeKAG)
last_updated: 2026-06-12
---

# Caesura (AmeKAG) 策略

## 目标问题

Galgame/视觉小说创作者缺乏一个将 Live2D、3D MiniGame、AI 辅助作为一等公民内建的现代开源引擎——现有方案要么是日系闭源引擎（功能现代化不足），要么是通用游戏引擎（太重、需学多个技术栈）。

## 当前架构状态

已完成大规模模块化解耦重构：
- **16 个独立模块**：archive, Audio, debug, di, entry, input, job, Live2D, MiniGame, platform, render, Resource, rpc, script, Steam, storage
- **Core 目录已消除**——拆分为 di（依赖注入）、entry（引擎入口）、platform（平台后端）、input（输入路由）、job（任务系统）
- **Scripting→script 重命名**——内部分为 vm/state/bindings 子目录
- **Carc→archive、System→storage**——模块名对齐职责
- **主循环依赖已消除**：Core↔Render 双向依赖已断开，Audio→Scripting 越层引用已修复

## 破局方案

将现代能力（Live2D、3D、AI 辅助）作为架构层的一等公民，同时**以接口隔离重建模块边界**——让每个子系统只通过抽象接口互相通信。稳定架构是交付 galgame 创作工具的物质前提。

## 接口覆盖状态

每个子系统通过 `api/` 子目录暴露纯虚接口，BackendRegistry 只依赖接口指针：

| 子系统 | 接口 | 状态 |
|---|---|---|
| Render | IRenderDevice | ✅ `render/api/` |
| Render | IGpuMonitor | ✅ `render/api/` |
| Render | IVideoDecoder | ✅ `render/` |
| Audio | IAudioBackend | ✅ `Audio/api/` |
| Platform | IPlatformBackend | ✅ `platform/api/` |
| MiniGame | IMiniGameBackend | ✅ `MiniGame/api/` |
| Animation | IAnimationBackend | ✅ `Live2D/` |
| Steam | ISteamBackend | ✅ `Steam/` |
| Asset | IAssetProvider | ✅ `Resource/api/` |
| Save | ISaveProvider | ✅ `storage/` |
| Debug | IDebugManager | ✅ `debug/api/` |

全部 11 个子系统接口已完成。

## 目标用户

**首要用户：** 小型 galgame/VN 开发社团——需要 Live2D + 3D 小游戏但不想维护多个技术栈。他们使用本引擎用一个统一工具交付包含动态立绘和 3D 小游戏的现代 galgame。

## 关键指标

- **跨平台 CI 绿率** — Windows/macOS/Linux 三端 CI 同时通过构建和测试的比率。当前 85/90 单元测试通过，5 个失败待修。
- **架构耦合度** — 跨模块 #include 的数量。Core 已消除，Scripting 耦合已修复；持续监控。
- **新人上手到 Demo 可跑时间** — 从下载引擎到成功跑出 Demo 场景的耗时（分钟计）。

## 赛道

### 架构解耦：接口隔离与模块边界重建 ✅

消除循环依赖、越层引用和上帝模块。以 BackendRegistry 的纯虚接口为锚点，所有 11 个子系统接口已就位，模块间通过抽象接口通信。

_状态：完成。后续维护：防止新增跨模块直接依赖。_

### 跨平台 CI 与测试稳定性

修复剩余 5 个失败单元测试，补齐关键路径的集成测试，确保三端 CI 稳定绿。涵盖 bgfx 多渲染后端（D3D11/OpenGL/Metal）的一致性。

_状态：进行中。85/90 通过，5 个预存失败（音频加载、图片解码、Lua 脚本、bgfx null 崩溃）。_

### 创作者上手体验与文档

Demo 场景、web-editor 开箱即用体验、84 个 KAG API 的文档和示例——让创作者从下载到"能跑出第一个 galgame 场景"的路径尽可能短。

_状态：待启动。_
