# Survivors — 2026-06-11 Surprise-Me Ideation

## Grounding Context
**Codebase:** Caesura v1.0-rc C++20引擎 (68 cpp + 71 h, 10模块) + 0.1.0-alpha Electron/React web-editor (12组件)。9个抽象接口 + BackendRegistry。JSON-RPC桥接 (8方法)。84 KAG API via Lua沙箱。
**关键发现:** AI Panel 用 setTimeout 模拟回复（假的）；provider框架已搭建未接线。编辑器版本滞后引擎一大截。docs/ 目录为空。Live2D仅Win端确认。3/138测试失败。仅3个Demo场景。
**策略赛道:** AI深入编辑器 · 跨平台稳定性 · 创作者上手体验

## Survivors (7)

### 1. AI Panel 接线计划 — 把模拟响应替换为真实 LLM 调用
**Description:** AIPanel.jsx 当前用 setTimeout 返回硬编码文本。将已有的 provider 框架 (openai.js/codex.js/custom.js) 真正接线，实现 @generate (自然语言→KAG脚本) 和 @fix (调试分析) 的实际 LLM 调用。保留 provider 选择器，让用户切换后端。
**Basis:** `direct:` AIPanel.jsx 第22-28行使用 `setTimeout` 模拟响应；`providers/openai.js` 已存在但未被引用
**Rationale:** 策略首要赛道"AI深入编辑器"的核心界面当前是假的——接线是让赛道成立的最低门槛动作。已有的 provider 架构意味着这是接线工作而非从零搭建。
**Downsides:** API key 管理、网络错误处理、token 成本——这些是接线后立刻需要面对的现实问题
**Confidence:** 95%
**Complexity:** Medium

### 2. AI 上下文感知 — 让 AI 看到当前项目的完整状态
**Description:** AI 生成 KAG 代码时需要知道当前项目有什么资源（图层、音频、角色），否则生成的代码引用不存在的资源名。将 EditorContext 的完整状态（openFiles、assets、scenes、项目结构）注入 AI prompt，让 AI 生成"知道自己在做什么"的代码而非随机猜测。
**Basis:** `direct:` EditorContext.jsx 持有 openFiles/activeFileIdx/assets/scenes 等完整状态，AI Panel 当前无任何上下文传入
**Rationale:** 没有上下文的 AI 生成等于随机生成——上下文感知是从"AI玩具"到"AI工具"的分界线。目标用户说"在我的教室场景里加一个角色对话"，AI 需要知道"教室场景"用了哪些图层才能生成正确代码。
**Downsides:** prompt 长度会随项目变大而增长，需要上下文窗口管理策略（压缩、摘要）
**Confidence:** 90%
**Complexity:** Low-Medium

### 3. KAG API 文档自动生成 — 从 Binding 源码提取并生成文档
**Description:** 84 个 KAG API 的文档应从 C++ Binding 源码 (KAGBinding.cpp 等) 中自动提取函数名、参数、注释，生成结构化的中英文 API 参考文档。用脚本/模板引擎做代码生成，而非手写维护 84 个 API 的文档。
**Basis:** `direct:` KAGBinding.cpp 包含所有 lua_KAG_* 函数实现及注释；docs/ 目录当前为空
**Rationale:** 策略赛道"创作者上手体验与文档"——84 个 API 手写文档不可持续。自动生成后，文档可随代码变更自动更新，解决"写了文档但代码已过时"的根本问题。
**Downsides:** 需要编写代码生成器（额外工具链）；注释质量决定生成质量——需要先审查 Binding 源码的注释完整性
**Confidence:** 85%
**Complexity:** Medium

### 4. Live2D macOS/Linux 支持 — 兑现"跨平台现代引擎"承诺
**Description:** Live2D 当前仅 Windows 可用 (README 标注 macOS/Linux "Deferred")。实现 Metal 渲染路径 (macOS) 和 OpenGL 渲染路径 (Linux)，使 Live2D 成为真正的跨平台一等公民。引擎已有 4 条渲染路径的接口定义 (ILive2DRenderPath)，MetalNativeRenderPath 和 OpenGLSharedRenderPath 文件已存在但可能未完成。
**Basis:** `direct:` README Known Limitations 表格标明 macOS/Linux Live2D 为 Deferred；`src/live2d/Live2D/MetalNativeRenderPath.cpp` 文件已存在；策略方案"现代能力一等公民内建"
**Rationale:** 策略核心承诺是跨平台现代能力——核心用户群"需要Live2D+3D"在macOS/Linux上目前无法获得Live2D，这直接违反"一等公民"声明。文件已存在说明工作已有开端，属于"完成而非开始"。
**Downsides:** Cubism SDK 在不同平台的编译链接配置可能复杂；Metal shader 和 OpenGL shader 的调试周期长
**Confidence:** 75%
**Complexity:** High

### 5. 抽象接口即插件系统 — 将 9 个接口变为社区可扩展的插件协议
**Description:** 已有的 IAudioBackend/IRenderDevice/IAnimationBackend 等 9 个接口本身就是完美的插件边界。为每个接口提供文档化的扩展规范 + 示例插件模板，让社区能编写自定义渲染后端、音频后端、动画系统而不需 fork 引擎。
**Basis:** `direct:` 9 个 Abstract Interfaces 已定义在 README Architecture 章节，均为纯虚接口，天然支持多实现
**Rationale:** 9 个接口 = 9 个插件入口——文档化后社区贡献的杠杆远超核心团队单独开发。这也是"开源自由引擎"承诺的关键组成部分——如果用户不能扩展引擎，就不是真正的自由。
**Downsides:** 接口稳定化需要成本（当前接口可能还在演变）；文档化接口是一笔持续维护的债务
**Confidence:** 80%
**Complexity:** Medium

### 6. Figma AI 模式 — "观察→建议"，而非"等待→提问"
**Description:** Figma AI 不是聊天机器人——它理解你的设计并直接建议下一步。Caesura AI 应从当前项目状态（场景结构、资源引用、脚本内容）出发，主动提议"你刚加了背景，要不要我生成显示文本的代码？"或"检测到未使用的音频资源，要清理吗？"，而非等用户输入 prompt。
**Basis:** `external:` Figma AI "Make Design" 的上下文感知辅助模式；Cursor Tab 的预测补全模式
**Rationale:** AI 最有效的形态不是"聊天"，而是"观察你正在做什么并主动提供下一步"。对目标用户（会写不会编程），这种形态消除了 prompt engineering 的门槛——创作者专注于故事，AI 关注技术实现。
**Downsides:** "主动建议"可能干扰创作流——需要精确的触发条件和可关闭性；建议质量取决于上下文理解的深度（依赖 #2）
**Confidence:** 70%
**Complexity:** High

### 7. 面向社区贡献的架构设计 — 按 100 个贡献者重构模块边界
**Description:** 如果项目明天迎来社区贡献热潮，当前架构哪里会崩？审计并重构：编辑器组件如何避免冲突？引擎模块边界是否足够清晰让新人独立开发？插件系统是否让贡献者能独立工作？按"100 人同时开发"设计模块边界、贡献指南和 review 流程。
**Basis:** `reasoned:` 策略目标是开源社区牵引力（指标：GitHub Stars + 活跃贡献者）；不准备好架构，社区来了也会因合并冲突、模块耦合而散
**Rationale:** 开源项目的最大杠杆是社区——但社区需要架构承载。Godot 和 Blender 的成功很大程度上归功于清晰的模块边界和贡献流程。Caesura 当前没有 AGENTS.md 也没有贡献指南——这应该在社区到来之前建立。
**Downsides:** 架构重构有破坏性风险；过早为"可能不会来的社区"优化可能浪费精力
**Confidence:** 65%
**Complexity:** High
