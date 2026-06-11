# Raw Candidates — 2026-06-11 Surprise-Me Ideation

## Frame 1: Pain & Friction

1. **AI Panel 接线计划 — 把模拟响应替换为真实 LLM 调用**
   - 摘要：AIPanel.jsx 当前用 setTimeout 返回硬编码文本。将已有的 provider 框架 (openai.js/codex.js/custom.js) 真正接线，实现 @generate (自然语言→KAG脚本) 和 @fix (调试分析) 的实际 LLM 调用。
   - basis: direct: `web-editor/src/components/AIPanel.jsx` 第22-28行使用 `setTimeout` 模拟响应；`web-editor/src/providers/openai.js` 已存在但未被引用
   - why_it_matters: 策略首要赛道"AI深入编辑器创作流程"的核心界面当前是假的——接线是让这个赛道成立的最低门槛动作

2. **AI 上下文感知 — 让 AI 看到当前脚本、场景列表、资源清单**
   - 摘要：AI 生成 KAG 代码时需要知道当前项目有什么资源（哪些图层、音频、角色），否则生成的代码引用不存在的资源名。将 EditorContext 的完整状态注入 AI prompt。
   - basis: direct: EditorContext.jsx 持有 openFiles/activeFileIdx/assets/scenes，AI Panel 当前无任何上下文传入
   - why_it_matters: 没有上下文的 AI 生成等于随机生成——上下文感知是从"AI玩具"到"AI工具"的分界线

3. **一键运行错误诊断 — AI 自动读取引擎 stderr 并定位 KAG 脚本错误行**
   - 摘要：LogPanel 已经实时转发引擎 stderr。当用户按 F5 后脚本报错，AI 自动读取日志中的错误行号，跳转到对应位置并给出修复建议，而非让用户手动对照日志和代码。
   - basis: direct: LogPanel.jsx 已有日志转发；AI Panel 的 @fix 命令当前只返回通用建议
   - why_it_matters: 新手最大的摩擦点是"报错了不知道什么意思"——AI 桥接日志和代码直接消除这个痛点

4. **Demo 场景扩展 — 从 3 个 Demo 到覆盖 84 个 KAG API 的示例库**
   - 摘要：当前仅 3 个 Demo 场景（教室渲染、MiniGame 3D、存档）。为 84 个 KAG API 每个提供至少一个可运行的示例脚本，并内置到编辑器的场景模板选择器中。
   - basis: direct: README 显示 "Demo covers 3 scenes"；策略赛道"创作者上手体验与文档"；指标"新人上手到 Demo 可跑时间"
   - why_it_matters: 84 个 API 只有 3 个有示例——创作者面对剩余 81 个时只能猜或读源码

5. **web-editor 版本追赶 — 将编辑器从 0.1.0-alpha 推向 1.0**
   - 摘要：引擎已是 1.0-rc，编辑器仍为 0.1.0-alpha。差距意味着编辑器缺乏稳定性和功能完整性。制定编辑器 1.0 的验收标准并排期。
   - basis: direct: engine v1.0-rc per CMakeLists.txt/README；editor v0.1.0-alpha per package.json
   - why_it_matters: 编辑器是创作者的主要接触面——0.1.0 到 1.0 的差距会让潜在用户望而却步

6. **Live2D macOS/Linux 支持 — 兑现"跨平台现代引擎"承诺**
   - 摘要：Live2D 当前仅 Windows 可用 (README 标注 macOS/Linux "Deferred")。实现 Metal (macOS) 和 OpenGL (Linux) 渲染路径，使 Live2D 成为真正的跨平台一等公民。
   - basis: direct: README Known Limitations 表格；策略方案"现代能力一等公民内建"
   - why_it_matters: 策略核心承诺是跨平台现代能力——核心用户群"需要 Live2D+3D"在 macOS/Linux 上目前无法获得 Live2D

7. **修复 3 个失败测试 — 达到 138/138 全绿**
   - 摘要：README 显示 135/138 测试通过。定位并修复剩余 3 个失败测试，将 CI 绿率推到 100%。
   - basis: direct: README "tests/ C++ unit tests (24 files, 135/138 pass)"
   - why_it_matters: 开源项目的信任基础——不完整的测试覆盖率在策略指标"跨平台 CI 绿率"上直接扣分

## Frame 2: Inversion, Removal, or Automation

8. **AI 即编辑器 — 用对话界面替代传统代码编辑**
   - 摘要：不是"编辑器里有一个 AI 面板"，而是"整个编辑器就是一个对话界面"。创作者用自然语言描述场景，AI 生成并实时预览，代码编辑器降级为"高级模式"的可选项。
   - basis: reasoned: Cursor/Windsurf 已证明对话式 IDE 可行；策略方案押注 AI 一等公民
   - why_it_matters: 目标用户"会写剧本但不会编程"——对话式创作直接消除了 KAG 语法的学习门槛

9. **KAG API 文档自动生成 — 从 Binding 源码提取并生成中文文档**
   - 摘要：84 个 KAG API 的文档应从 C++ Binding 源码 (KAGBinding.cpp 等) 中自动提取函数签名、参数说明和注释，生成结构化的中英文 API 参考文档，而非手写维护。
   - basis: direct: KAGBinding.cpp 包含 lua_KAG_* 函数实现，注释可作为文档源；docs/ 目录当前为空
   - why_it_matters: 84 个 API 手写文档不可能跟上变更——自动生成是文档可维护的前提

10. **测试自愈 — 失败的 3 个测试自动分析和建议修复**
    - 摘要：当 CI 检测到测试失败时，自动将失败日志和测试源码送入 AI，生成修复建议并作为 PR comment 提交。
    - basis: direct: 135/138 pass, 3 failures；reasoned: GitHub Actions + AI code review 模式在行业中已验证
    - why_it_matters: 将"发现失败→手动排查→修复"的循环从小时级缩短到分钟级

11. **一键打包到 Steam/itch.io — 移除分发摩擦**
    - 摘要：当前打包流程需手动配置 electron-builder。实现 web-editor 内一键导出：选择平台 (Win/Mac/Linux) → 自动构建引擎 → 打包编辑器 → 输出可分发的安装包或 itch.io zip。
    - basis: direct: package.json 已有 package:win/mac/linux 脚本但需手动调用
    - why_it_matters: 目标用户是小社团——每一步分发摩擦都在消耗他们做游戏的时间

12. **移除写 KAG 脚本的需求 — 可视化场景编辑器**
    - 摘要：实现拖拽式场景编辑——从资源面板拖入背景图、角色立绘、音频到时间轴，自动生成对应 KAG 脚本。KAG 代码降级为"高级用户"的底层表示。
    - basis: reasoned: 类比 Unity Timeline / Godot AnimationPlayer + 可视化脚本；目标用户"会写不会编程"
    - why_it_matters: 如果创作者不需要写代码就能做 VN，上手时间从"数天学语法"变为"数分钟拖拽"

## Frame 3: Assumption-Breaking & Reframing

13. **引擎即 WASM — 把整个引擎编译到 WebAssembly 在浏览器中运行**
    - 摘要：打破"引擎必须是原生二进制"的假设。将 C++ 引擎通过 Emscripten 编译为 WASM，在 web-editor 的浏览器端直接渲染，消除引擎和编辑器之间的进程边界。
    - basis: reasoned: bgfx 已有 WebGL 后端；SDL3 和 SoLoud 理论上可移植到 WASM；类比 Figma (C++ → WASM)
    - why_it_matters: 消除"下载引擎→安装→配置"的全部门槛——浏览器打开即用

14. **打破 Electron 假设 — 编辑器可以是 Tauri 或纯 Web**
    - 摘要：Electron 打包体积大（~200MB+）。考虑 Tauri (Rust, ~5MB) 或 PWA (纯浏览器) 作为编辑器宿主。PWA 模式可配合 WASM 引擎实现完全免安装。
    - basis: reasoned: Tauri 已在多个开源项目中替代 Electron；PWA + WASM 消除所有安装步骤
    - why_it_matters: 策略指标"新人上手时间"——下载 200MB vs 5MB vs 0MB 是数量级差异

15. **引擎内核与编辑器分离发布 — 引擎作为独立库被其他项目引用**
    - 摘要：目前引擎和编辑器高度耦合（编辑器通过子进程 spawn 引擎）。将引擎核心发布为独立 CMake 库，允许其他编辑器或工具直接链接，扩大生态。
    - basis: direct: CMakeLists.txt 构建单体可执行文件；reasoned: Godot 和 Unity 的编辑器-引擎分离是标配
    - why_it_matters: 策略方案是"开源跨平台自由引擎"——如果引擎不能作为库被第三方使用，"自由"只打了一半

16. **反向假设：如果 KAG 兼容性不重要呢？**
    - 摘要：84 个 KAG API 是历史包袱还是资产？如果目标用户从来没用过 KAG (Kirikiri)，为什么强迫他们学 KAG 语法？设计一套更现代的、面向 AI 友好的脚本 API。
    - basis: reasoned: 目标用户"需要 Live2D+3D 不想学多技术栈"——KAG 兼容性对他们可能不是卖点而是另一个要学的东西
    - why_it_matters: 如果 KAG 兼容性不带来用户增长却绑定了 API 设计，去掉它释放的设计空间可能更大

## Frame 4: Leverage & Compounding

17. **抽象接口即插件系统 — 将 9 个接口变为社区可扩展的插件协议**
    - 摘要：已有的 IAudioBackend/IRenderDevice/IAnimationBackend 等接口本身就是完美的插件边界。文档化接口规范，让社区能编写自定义渲染后端、自定义音频后端、自定义动画系统，而不需 fork 引擎。
    - basis: direct: 9 个 Abstract Interfaces 已定义在 README Architecture 中
    - why_it_matters: 9 个接口 = 9 个插件入口——文档化后社区贡献的杠杆远超核心团队单独开发

18. **JSON-RPC 可扩展协议 — 让每个新引擎功能自动暴露编辑器 API**
    - 摘要：当前 JSON-RPC 只有 8 个方法 (ping/run/stop/eval/getFrame/getState/assets/logs)。设计一套注册机制——引擎内任何新功能调用 `REGISTER_RPC("methodName", handler)` 后，编辑器自动发现并可调用。
    - basis: direct: README 列出 8 个 RPC 方法；RpcServer.cpp 在 Core 目录
    - why_it_matters: 每加一个引擎功能就手动加编辑器桥接是不可持续的。自动注册让功能扩展成本降到 O(1)

19. **Lua 脚本市场 — 让社区贡献和分享 KAG 脚本模块**
    - 摘要：搭建一个轻量级的脚本分享平台（可以是 GitHub repo + 编辑器内浏览），让创作者分享/复用场景模板、特效脚本、MiniGame 逻辑。编辑器内置"社区脚本"面板。
    - basis: reasoned: 类比 Unity Asset Store / Godot Asset Library；Lua 脚本天生适合分享（纯文本）
    - why_it_matters: 84 个 API 的示例 × 社区贡献 = 指数级增长的学习资源——解决"文档不足"的根本途径

20. **AI 记忆系统 — AI 记住项目上下文跨会话复用**
    - 摘要：让 AI 在同一个项目的多次会话间保持上下文——记住角色名、图层名、常用模式、用户偏好。将项目元数据（资源清单、脚本结构、历史对话摘要）持久化到 `.caesura/ai-memory.json`。
    - basis: reasoned: Copilot/Cursor 的上下文窗口限制导致跨会话遗忘；本地记忆文件是轻量级解决方案
    - why_it_matters: AI 如果每次打开都是"失忆"状态，创作者每次都要重新解释项目——记忆消除重复沟通

21. **从引擎源码到编辑器类型定义自动同步**
    - 摘要：KAG API 的参数类型、Live2D 的事件类型、MiniGame 的 API 签名——这些在 C++ 侧定义，在 JS 编辑器侧手动维护，必然不一致。写一个代码生成器从 C++ 头文件提取类型定义，生成 TypeScript 类型文件。
    - basis: direct: KAGBinding.cpp 定义了各 API 的参数；CodeEditor.jsx 的 KAG_COMMANDS 数组手动维护
    - why_it_matters: C++ 和 JS 两边手动维护的 API 签名必然漂移——自动同步消除一整类 bug

## Frame 5: Cross-Domain Analogy

22. **Figma AI 模式 — 设计→代码自动转换，而非聊天面板**
    - 摘要：Figma AI 不是聊天机器人——它理解你的设计并直接生成代码/组件。类比：Caesura AI 应理解你的场景结构（图层列表、资源引用、时间线）并直接生成 KAG 脚本，而非等待用户输入自然语言。
    - basis: external: Figma AI "Make Design" / "Make Prototype" 模式
    - why_it_matters: AI 最有效的形态不是"聊天"，而是"观察你在做什么并提供下一步"——消除 prompt engineering 门槛

23. **Cursor Tab 补全模式 — AI 预测用户下一步要写的 KAG 代码**
    - 摘要：Cursor 的 Tab 补全不是等用户输入完整命令，而是根据上下文预测并建议下一行代码。在 KAG 编辑器中实现类似的多行预测补全——"你刚加了背景图层，下一步大概率是显示文本"。
    - basis: external: Cursor Tab / GitHub Copilot inline completions
    - why_it_matters: 从"用户问 AI"变为"AI 主动建议"——将创作速度从打字速度提升到确认速度

24. **Godot 编辑器模式 — 引擎内嵌而非分离进程**
    - 摘要：Godot 编辑器和引擎是同一个进程——编辑器本质上是在运行游戏的同时叠加编辑 UI。Caesura 当前是编辑器 spawn 引擎子进程。考虑将编辑器嵌入引擎窗口内，消除进程边界。
    - basis: external: Godot Engine 的一体化编辑器设计
    - why_it_matters: 进程边界带来延迟（200ms 帧轮询）和复杂度（IPC 协议）——同进程可实时渲染到编辑器 Stage 面板

25. **Vercel v0 模式 — 提示词→完整场景→可运行**
    - 摘要：v0 从自然语言提示生成完整的、可运行的 UI 代码。类比：输入"教室场景，3个角色对话，背景音乐是钢琴曲"，AI 生成完整的 KAG 脚本 + 资源配置，一键运行。
    - basis: external: Vercel v0 prompt-to-app 模式
    - why_it_matters: 将创作从"手动编写每个命令"变为"描述意图→得到结果"——对不会编程的目标用户来说这是质变

26. **Notion AI 模式 — 内联辅助而非侧面板**
    - 摘要：Notion AI 通过 `/` 命令和选中文本弹出菜单提供 AI 辅助，而非独立聊天面板。在 KAG 编辑器中按 `/` 弹出 AI 命令菜单（`/generate /fix /explain /optimize`），AI 直接在光标处产生结果。
    - basis: external: Notion AI 的内联交互模式
    - why_it_matters: 侧面板 AI 需要用户切换注意力——内联 AI 让辅助发生在创作的自然流中

## Frame 6: Constraint-Flipping

27. **翻转：0 个贡献者 → 项目必须靠 AI 自我维护**
    - 摘要：假设项目只有 1 个维护者 + AI。哪些工作必须自动化？测试自动生成、文档自动生成、issue triage 自动分类、PR review 自动检查、发布自动构建签名。设计一条完全的 AI-first 维护流水线。
    - basis: reasoned: 开源项目长期面临维护者倦怠；AI 辅助维护是防御性投入
    - why_it_matters: 如果 AI 是策略的一等公民，那么维护工作也应该由 AI 承担——这本身就是"AI深入流程"的标杆案例

28. **翻转：100 个贡献者 → 架构必须支持大规模并行开发**
    - 摘要：如果明天有 100 个人贡献代码，当前架构哪里会崩？编辑器组件如何避免冲突？引擎模块边界是否足够清晰？插件系统是否让贡献者能独立开发？按 100 人团队设计模块边界和贡献指南。
    - basis: reasoned: 策略目标是"开源社区牵引力"——架构不准备好，社区来了也会散
    - why_it_matters: 开源项目的最大杠杆是社区——但社区需要架构来承载

29. **翻转：纯移动端 → 引擎在 iOS/Android 上运行**
    - 摘要：README 标注 Mobile "Planned, MobileAdapter interface complete"。如果移动端是主战场而非备用计划？实现 SDL3 移动后端 + bgfx Metal/Vulkan 移动渲染 + 触控适配的编辑器。
    - basis: direct: MobileAdapter.cpp 已存在于 src/Core/；README 标注 Mobile 为 Planned
    - why_it_matters: 视觉小说在移动端的消费场景天然匹配——在手机上玩 VN 的用户远多于在 PC 上

30. **翻转：纯云端 → 引擎在服务器上渲染，浏览器接收视频流**
    - 摘要：引擎部署在云端，通过 WebRTC 将渲染帧流式传输到浏览器。创作者不需要下载任何东西，浏览器即编辑器+播放器。项目文件存储在云端，多人可协作编辑同一场景。
    - basis: reasoned: 类比 Google Stadia / GeForce Now 的云游戏模式；消除本地安装和性能门槛
    - why_it_matters: 策略指标"新人上手时间"→云端版本 = 0 秒下载 + 0 配置——终极的上手体验

31. **翻转：无 AI → 如果 AI 突然不可用，编辑器还有什么价值？**
    - 摘要：策略大量押注 AI，但如果 API 涨价、模型停服、用户无网络？编辑器在没有 AI 的情况下必须有独立价值——CodeMirror 编辑体验、KAG 自动补全、实时预览、资源管理。审计并加固非 AI 路径。
    - basis: reasoned: AI API 依赖是单点故障；编辑器不应在没有 AI 时就是空壳
    - why_it_matters: 策略不能建立在单一外部依赖上——无 AI 模式是风险对冲

32. **翻转：收费模式 → 如果项目需要盈利来维持长期开发？**
    - 摘要：MIT 许可免费——但如果需要资金持续开发？探索不破坏开源承诺的盈利模式：托管服务（云端渲染）、Steam 发布集成付费、企业支持合同、AI token 按量付费（开源引擎免费 + AI 服务收费）。
    - basis: reasoned: 长期维护需要资金；Blender/Godot 等开源项目都有周边收入
    - why_it_matters: 策略假设"开源自由"是卖点——但如果项目因资金不足停更，所有承诺归零

## Cross-Cutting Combinations

33. **AI 接线 + 上下文感知 + Figma AI 模式 = 智能场景生成器**
    - 组合自 #1, #2, #22：接线 AI → 注入项目上下文 → 观察用户的场景结构自动建议下一步
    - 一个能"看懂"当前项目并主动建议完整场景的 AI，而非等待用户输入 prompt 的聊天机器人

34. **WASM 引擎 + PWA 编辑器 + 云端渲染 = 三层访问模式**
    - 组合自 #13, #14, #30：本地 WASM（最快）→ PWA（免安装）→ 云渲染（零门槛）
    - 用户从"打开浏览器试试"到"下载本地版"到"STEAM 发布"——无缝升级路径

35. **API 文档自动生成 + 社区脚本市场 + KAG 示例库 = 自生长的知识生态**
    - 组合自 #9, #19, #4：自动生成文档→社区贡献示例→编辑器内置浏览
    - 文档不是静态页面而是活的、社区驱动的知识网络

36. **抽象接口插件化 + JSON-RPC 自动注册 + 类型同步 = 可扩展引擎核心**
    - 组合自 #17, #18, #21：文档化接口→自动暴露 RPC→自动同步类型
    - 社区贡献者写一个新渲染后端，编辑器自动发现并提供类型安全的调用

37. **AI 记忆系统 + 内联 AI + 智能补全 = 持续增长的项目智能**
    - 组合自 #20, #26, #23：AI 记住项目→内联建议→预测下一步
    - AI 从"每次打开都失忆"变为"越来越懂这个项目"的助手

38. **移动端 + 云端 + 协作 = 任何设备都能创作**
    - 组合自 #29, #30：手机编辑→云端渲染→PC 继续→分享链接预览
    - 一个社团成员在手机上改台词，另一个在 PC 上调 Live2D——同一个项目
