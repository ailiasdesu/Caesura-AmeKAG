# Caesura-AmeKAG 后续产品化推进总任务书

> 用途：将本文档直接交给 AI coding agent（Codex / Claude Code / Cursor Agent 等），作为 Caesura-AmeKAG 后续开发的总目标、约束、优先级和验收标准。
>
> 仓库：`https://github.com/ailiasdesu/Caesura-AmeKAG`
>
> 当前定位：**现代 KAG / Lua 驱动的跨平台视觉小说 Runtime，核心能力已较完整，下一阶段重点从“继续堆 Runtime 功能”转向“产品化、创作体验、发行、第三方开发者验证和生态”。**

---

# 1. 项目当前状态与战略判断

## 1.1 当前状态

根据当前仓库截至 2026-08-16～2026-08-21 的路线图、能力矩阵和发布记录，项目已经完成：

- C++20 核心 Runtime
- SDL3 + bgfx + SoLoud + Lua 5.4
- KAG Neo-Genesis
- KAG3 兼容方向
- 123 个 KAG command contracts
- 16 个模块
- 31 个主要纯虚接口
- 跨平台构建体系
- Windows D3D11 / OpenGL 真实 GPU 验证
- Metal 引擎侧路径
- Live2D
- 3D mini-game framework
- SMA skeletal animation
- i18n
- save / load / migration
- CARC archive
- Steamworks abstraction
- cloud save abstraction
- replay
- debugger
- LSP
- editor RPC
- headless
- AI dialogue
- post-processing
- tween
- layout
- 示例项目与 16 步教程
- CPack / Web 打包
- v1.0.0 / v1.0.1 发布流程
- 大量安全和稳定性审计

仓库自己的 readiness audit 已将：

- Modular architecture migration：约 98%
- Core visual-novel usability：约 74%
- Cross-platform product release：约 34%

作为阶段性判断。

因此，项目现在不应再被视为“需要继续证明核心 Runtime 能不能工作”的早期原型。

## 1.2 核心战略转向

后续开发必须从：

> “继续增加 Runtime feature”

转变为：

> **“把已经完成的 Runtime 包装成别人能学会、能开发、能调试、能打包、能发布、能长期维护的产品。”**

最重要的衡量标准也从：

> API 数量、command 数量、模块数量

转变为：

> **第三方开发者是否可以在不了解 Caesura 内部实现的情况下完成一个可发布的 VN。**

---

# 2. 项目最终定位

Caesura 不应被定位成“另一个 Ren'Py”。

建议定位：

> **现代化 KAG Runtime / 面向程序员和独立团队的跨平台视觉小说引擎。**

核心差异化：

```text
KAG3 compatibility
        +
KAG Neo-Genesis
        +
Lua 5.4
        +
C++20
        +
bgfx / SDL3
        +
Live2D
        +
2D / 3D
        +
Steam / Web / Desktop
        +
Debugger / LSP / RPC
        +
AI-assisted authoring
```

目标用户优先级：

1. 熟悉 KAG/Kirikiri 的开发者
2. 程序员型独立 VN 开发者
3. 小型 VN 工作室
4. Live2D VN 开发者
5. 需要跨平台/Steam/Web 的独立开发者
6. 有技术能力、但不想直接使用 Unity/Unreal 的互动叙事开发者

不以“零代码拖拽用户”为第一目标，但应逐步降低非程序员的使用门槛。

---

# 3. 第一原则：不要继续无止境堆功能

## 3.1 暂时停止低价值 Runtime 扩张

除非出现明确用户需求或 release blocker，禁止优先级高于产品化工作的以下行为：

- 新增大量 KAG command
- 为了“功能数量”增加新 API
- 再增加新的实验性渲染功能
- 再增加新的高级动画类型
- 在已有能力没有形成完整工作流前继续扩展 Runtime

已有 123 个 command 已经足够丰富。

新 Runtime 功能必须满足至少一个条件：

- 对实际 VN 创作流程存在明确需求
- 修复真实作品中的 blocker
- 解决跨平台 release blocker
- 提升稳定性/性能
- 对既有 API 有明确兼容性价值

---

# 4. 后续总体路线

将项目划分为以下几个阶段。

```text
Phase 0  1.x 稳定化
      ↓
Phase 1  Creator / Studio
      ↓
Phase 2  Release / Distribution
      ↓
Phase 3  第三方开发者验证
      ↓
Phase 4  官方旗舰作品
      ↓
Phase 5  Plugin / Community Ecosystem
```

不要跳跃开发。

---

# 5. Phase 0：1.x 稳定化

## 目标

让 Runtime 从“功能丰富”转变为：

> **API 可预测、脚本兼容、存档兼容、升级安全。**

## 5.1 API 稳定策略

建立明确的 1.x 稳定规则：

### 必须尽可能保持稳定

- KAG command syntax
- command 参数 schema
- Lua API
- Save schema
- Project layout
- 资产路径语义
- 配置文件格式

### 可以暂不保证稳定

- Plugin ABI
- Editor internal RPC
- Experimental AI APIs
- Experimental 3D APIs
- Developer-only interfaces

## 5.2 建立 Compatibility Policy

新增：

`docs/compatibility.md`

必须说明：

- KAG3 compatibility 范围
- Neo-Genesis syntax stability
- Save compatibility
- Project compatibility
- Lua compatibility
- version migration strategy
- breaking change policy

## 5.3 建立版本迁移框架

确保未来：

```text
1.0 project
    ↓
1.1
    ↓
1.2
    ↓
2.0 migration
```

不会破坏用户作品。

---

# 6. Phase 1：Caesura Studio

这是当前最高优先级。

## 6.1 总目标

用户打开 Caesura 后，可以：

```text
New Project
    ↓
选择模板
    ↓
自动创建项目
    ↓
编辑脚本
    ↓
浏览资源
    ↓
预览场景
    ↓
调试
    ↓
Build
```

整个流程不需要用户理解：

- CMake
- bgfx
- SDL
- Lua binding internals
- C++ Runtime
- 手动 DLL 拷贝
- 内部目录结构

## 6.2 第一版 Studio 不需要做到 Unity 级别

MVP 只需要：

```text
Project Manager
Script Editor
Asset Browser
Scene Preview
Inspector
Console
Debugger
Build
```

建议布局：

```text
┌──────────────────────────────────────────────┐
│ Caesura Studio                               │
├─────────────┬──────────────────┬─────────────┤
│ Project     │ Scene Preview     │ Inspector   │
│ / Assets    │                  │             │
│             │  Background      │ Position    │
│ bg/         │  Character       │ Scale       │
│ char/       │  Dialogue        │ Opacity     │
│ audio/      │                  │ Layer       │
│ scripts/    │                  │             │
├─────────────┴──────────────────┴─────────────┤
│ Script / Console / Debugger                   │
└──────────────────────────────────────────────┘
```

## 6.3 Project Manager

支持：

- New Project
- Open Project
- Recent Projects
- Duplicate Project
- Import Project
- Project Settings

模板至少提供：

1. Blank VN
2. Basic VN
3. Live2D VN
4. KAG3 Migration
5. Advanced / Showcase

## 6.4 Asset Browser

必须支持：

- 图片预览
- 音频试听
- 目录树
- 搜索
- 类型过滤
- 文件大小
- 路径
- 引用关系（后续）
- 拖入场景/脚本（第一版可以后做）

## 6.5 Script Editor

利用现有 LSP / RPC / debugger 能力。

必须支持：

- KAG syntax highlighting
- Lua syntax highlighting
- label navigation
- label references
- diagnostics
- command parameter completion
- go to definition
- breakpoints
- step over / continue
- variable inspection

## 6.6 Scene Preview

第一阶段不用做完整可视化所见即所得编辑。

只要能：

- 运行当前 scene
- 重载 scene
- 热重载脚本
- 查看当前状态
- 调整基础 inspector 参数
- 快速定位脚本来源

## 6.7 Build Manager

开发者应该能点击：

```text
Run
Build
Package
```

而不是自己运行一串 shell 命令。

---

# 7. Phase 2：Release / Distribution

当前真正影响商业化成熟度的重点。

## 7.1 Windows

必须实现：

- portable package
- CPack package
- game-only package
- developer package
- Steam package

最终用户不需要安装开发环境。

## 7.2 Linux

目标优先级：

1. AppImage
2. Steam Linux build
3. Steam Deck 验证

必须建立真实硬件测试矩阵。

## 7.3 macOS

必须验证：

- Apple Silicon
- Intel（如继续支持）
- Metal
- .app bundle
- DMG
- signing
- notarization

## 7.4 Web

保持：

```text
Build → Web
```

输出完整可部署 bundle。

需要测试：

- Chrome
- Edge
- Firefox（如可行）
- 大型资源
- 音频
- save
- Web storage
- input
- Live2D（如果受限必须明确记录）
- 性能

---

# 8. Steam Release Pipeline

必须把 Steam 从“API 存在”变成“开发者能发布”。

目标：

```text
Project
   ↓
Build
   ↓
Steam Package
   ↓
SteamPipe
```

至少准备：

- Steam AppID configuration
- Depot configuration
- platform builds
- branch/beta build
- achievements
- stats
- cloud save
- launch options
- overlay smoke test

优先做 Steam Windows + Linux。

---

# 9. Phase 3：第三方开发者验证

这是正式产品化的核心实验。

## 9.1 不允许由作者自己完成验证

必须找完全不了解 Caesura 内部实现的人。

目标：

5～10 名第三方开发者。

## 9.2 核心指标：Time to First VN

从：

```text
下载 Caesura
```

开始计时。

目标：

### 30 分钟内完成

```text
背景
+
角色
+
文字
+
选择
+
BGM
+
存档
```

并成功运行。

## 9.3 记录所有阻塞点

必须记录：

- 第一次报错位置
- 第一次文档搜索
- 第一次卡死
- 第一次不知道下一步怎么办
- 最常查看的文档
- 最难理解的 API
- 最常见的操作错误
- 最常见的 packaging 问题

不要凭作者直觉优化。

---

# 10. Phase 4：官方旗舰作品

必须制作一个真正的 Showcase，而不只是技术 Demo。

建议规模：

- 30～90 分钟
- 2～4 个主要角色
- 多结局
- 全语音或部分语音
- Live2D
- 立绘
- BGM / SE
- Save / Load
- Gallery / 回想
- 多语言
- Steam achievements
- Cloud Save
- Web Demo

目标：

> 玩家认为它是一款游戏，而不是“Caesura 技术演示”。

这个作品同时承担：

- QA
- 性能测试
- API validation
- UI validation
- Steam validation
- 文档验证
- 第三方示例

---

# 11. 核心架构方向：Core / Official Extension / Experimental

未来不要把所有新能力全部塞进 Runtime 核心。

## Core

必须保持稳定：

```text
KAG
Lua
Render
Audio
Input
Scene
Text
Save
Resource
Project
```

## Official Extensions

适合：

```text
Steam
Live2D
AI
Cloud Save
3D MiniGame
SMA
```

## Experimental

适合：

```text
Experimental AI
new render effects
future editor protocols
prototype APIs
```

目标：

> 降低 Core 的长期维护成本。

---

# 12. Plugin SDK

在核心稳定以后，再设计正式 Plugin SDK。

插件应该可以注册：

- commands
- Lua bindings
- asset providers
- render passes
- audio providers
- platform services
- editor extensions

目标：

```text
Caesura Core
      │
      ├── Steam Plugin
      ├── Live2D Plugin
      ├── AI Plugin
      ├── Cloud Plugin
      └── Community Plugins
```

插件机制必须：

- 有生命周期
- 有权限边界
- 有版本兼容策略
- 可禁用
- 不破坏主 Runtime

---

# 13. Documentation Strategy

文档不要以源码模块为中心。

必须以用户任务为中心。

建议结构：

```text
docs/
├── getting-started/
├── tutorials/
├── concepts/
├── scripting/
├── assets/
├── animation/
├── audio/
├── localization/
├── save/
├── plugins/
├── deployment/
├── steam/
├── web/
├── migration/
└── internals/
```

优先级：

1. “5 分钟运行第一个 VN”
2. “30 分钟完成小游戏”
3. “如何做选择分支”
4. “如何做 Live2D”
5. “如何做多语言”
6. “如何发布 Steam”
7. “如何从 KAG3 迁移”

---

# 14. QA / Testing

必须开始从“代码测试”走向“产品测试”。

## 自动化

持续维持：

- C++ unit tests
- Lua tests
- API census
- command contract tests
- headless tests
- replay tests
- packaging tests
- Web build tests

## 新增

### Golden Project

建立一个长期维护的：

`tests/projects/golden_vn/`

包含：

- dialogue
- choices
- save
- rollback
- backlog
- NVL
- i18n
- audio
- Live2D
- tween
- layout
- replay
- mod
- Steam hooks

每次 release 都跑完整项目。

### Golden Save

保存若干不同版本生成的存档。

确保新版本能够迁移。

### Real Hardware Matrix

至少：

Windows:
- NVIDIA
- AMD / Intel GPU
- D3D11
- OpenGL

macOS:
- Apple Silicon
- Metal

Linux:
- NVIDIA
- Mesa AMD/Intel

Web:
- Chrome
- Edge

---

# 15. Crash / Diagnostics

商业化必须把错误反馈做好。

玩家出错时，不应该得到：

```text
Segmentation fault
```

而应该得到：

```text
Caesura encountered an error.

Project:
Version:
Platform:
Scene:
Line:
Command:

[Copy diagnostic information]
[Open log folder]
```

开发者模式需要：

- structured log
- stack trace
- runtime state
- scene
- script line
- command
- asset path
- platform
- GPU backend

---

# 16. Performance Benchmark

建立长期 benchmark：

```text
startup
scene load
texture load
text rendering
large dialogue
many sprites
Live2D
SMA
particles
postfx
Lua execution
GC
save/load
Web
```

必须记录 baseline。

每次 PR 如果影响 hot path：

> 必须有 benchmark 或性能解释。

---

# 17. Security / Trust

继续维持当前已有的安全审计路线。

特别关注：

- save parsing
- archive parsing
- path traversal
- mods
- RPC
- stdio input
- HTTP
- cloud save
- TLS
- Lua sandbox
- resource quotas
- malicious project
- malicious assets

开发编辑器的 RPC 不能默认暴露公网。

---

# 18. Agent 工作纪律

AI coding agent 执行任务时，必须遵循以下规则。

## 18.1 先研究再改

每个任务开始前：

1. 读取相关架构文档
2. 找到真实调用链
3. 找已有接口
4. 检查测试
5. 检查既有兼容约束
6. 再修改代码

禁止：

> 为了实现功能直接创造平行架构。

## 18.2 优先复用现有基础设施

优先使用：

- existing interfaces
- existing RPC
- existing Lua binding
- existing schema
- existing build system
- existing project template
- existing packaging

不要重新实现一套。

## 18.3 每个功能必须带验证

新 feature：

```text
implementation
+
unit test
+
headless test
+
integration test（适用时）
+
documentation
```

## 18.4 禁止只改 README 假装完成

如果 README 宣称：

> supported

必须存在实际：

- code
- test
- validation

否则明确标记为：

- experimental
- engine-side only
- unverified
- planned

---

# 19. Git / Commit 纪律

建议：

```text
feat(editor):
feat(build):
feat(packaging):
fix(runtime):
fix(kag):
fix(platform):
test:
docs:
refactor:
```

每个 commit 尽量单一目标。

不要一个 commit 同时：

```text
改 Editor
+ 改 Runtime
+ 改文档
+ 改测试
+ 改 CI
```

除非这些修改必须原子提交。

---

# 20. Agent 每次执行任务后的固定报告

每个任务结束时必须输出：

```text
## Completed
- ...

## Changed Files
- ...

## Tests
- ...

## Build
- ...

## Known Limitations
- ...

## Risks
- ...

## Follow-up
- ...
```

禁止声称：

> “跨平台支持完成”

除非真实平台已经验证。

---

# 21. Release Gate

任何正式版本必须满足：

## Runtime

- 所有关键 test pass
- command contract pass
- save migration pass
- no known high severity bug

## Platform

- Windows smoke
- Linux smoke
- macOS smoke
- Web smoke（若声明支持）

## Packaging

- package build
- clean extraction
- clean machine startup
- game launch
- save/load

## Editor

- New Project
- Open Project
- Run
- Debug
- Build

## Sample

- Official sample game complete run
- all endings reachable
- saves work
- localization works

## Docs

- getting started verified
- tutorial verified
- packaging guide verified

---

# 22. 最终 1.0+ 成功标准

不要用：

> “功能足够多”

作为完成标准。

最终成功定义为：

```text
陌生用户
    ↓
下载 Caesura
    ↓
Create Project
    ↓
30 分钟完成第一个 VN
    ↓
继续开发
    ↓
调试
    ↓
Build
    ↓
Steam / Web / Desktop
    ↓
玩家运行
```

整个流程中：

> **不需要阅读源码、不需要安装开发工具、不需要向作者询问隐藏步骤。**

达到这一点，才视为真正完成产品化。

---

# 23. 推荐执行顺序

## Sprint 1

目标：

**1.x stability**

任务：

- compatibility policy
- save compatibility
- API stability policy
- golden project
- release regression

---

## Sprint 2

目标：

**Project Manager + Template**

任务：

- New Project
- Open Project
- templates
- project metadata
- project validation

---

## Sprint 3

目标：

**Asset Browser + Script Editor**

任务：

- asset tree
- preview
- syntax highlighting
- LSP
- diagnostics

---

## Sprint 4

目标：

**Scene Preview + Debugger**

任务：

- run current scene
- hot reload
- debugger
- inspect
- breakpoints

---

## Sprint 5

目标：

**Build Manager**

任务：

- Run
- Build
- Package
- clean build
- error reporting

---

## Sprint 6

目标：

**Cross-platform release**

任务：

- Windows
- Linux
- macOS
- Web
- hardware validation

---

## Sprint 7

目标：

**Steam**

任务：

- Steam build
- achievements
- stats
- cloud
- beta branch
- clean machine validation

---

## Sprint 8

目标：

**Third-party creator test**

任务：

- recruit users
- observe
- collect failures
- remove friction

---

## Sprint 9+

目标：

**Flagship VN + ecosystem**

任务：

- flagship game
- Plugin SDK
- community docs
- templates
- examples
- ecosystem

---

# 24. Agent 的最高优先级判断规则

当多个任务冲突时，按以下优先级决策：

```text
P0  影响已有游戏运行
P0  安全 / 数据丢失 / 存档损坏
P0  发布 blocker

P1  第三方开发者无法完成核心流程
P1  Editor / Build / Packaging 阻塞
P1  跨平台真实验证缺失

P2  文档和开发体验
P2  性能
P2  API polish

P3  新 feature

P4  实验性 feature
```

如果一个新 feature 与 Studio / Packaging / Stability 冲突：

> **优先 Studio / Packaging / Stability。**

---

# 25. 禁止的战略误区

## 误区 1

“我们还缺功能，所以继续加功能。”

错误。

目前主要缺的是产品层。

## 误区 2

“Editor 可以最后再做。”

错误。

没有 Creator Experience，Runtime 很难形成生态。

## 误区 3

“作者自己可以使用，所以用户应该也可以。”

错误。

必须测试陌生用户。

## 误区 4

“CI 绿了，所以跨平台完成。”

错误。

必须有真实硬件验证。

## 误区 5

“支持 Steamworks，所以能 Steam 发布。”

错误。

必须完成实际 Build / Upload / Run / Save / Achievement 验证。

## 误区 6

“README 写了，所以功能完成。”

错误。

代码 + test + validation 才算完成。

---

# 26. 最终战略

Caesura 后续不应该成为一个越来越庞大的“技术展示项目”。

应该变成：

> **一个稳定的开源现代 KAG Runtime + 一个真正可用的创作工具 + 一个可靠的跨平台发行系统 + 一个可持续发展的开发者生态。**

优先顺序必须是：

```text
Runtime Stability
      ↓
Creator Experience
      ↓
Packaging / Distribution
      ↓
Third-party Validation
      ↓
Flagship Game
      ↓
Plugin / Community Ecosystem
```

而不是：

```text
Runtime
↓
Runtime
↓
Runtime
↓
更多 Runtime
```

---

# 27. 给 AI Agent 的最终执行指令

你现在负责继续推进 `ailiasdesu/Caesura-AmeKAG`。

你的核心目标不是无限增加引擎能力，而是把当前已经完成的 Runtime 转变为真正可交付、可使用、可发布的产品。

优先级：

1. 稳定 1.x API 与兼容性
2. 构建 Caesura Studio
3. 降低新用户创建项目和第一次运行的门槛
4. 完善 Build / Package / Distribution
5. 完成真实 Windows / Linux / macOS / Web 验证
6. 完成 Steam 发布链
7. 建立 Golden Project / Golden Save / Release Gate
8. 用第三方开发者验证产品，而不是仅由项目作者验证
9. 制作一个真正的旗舰 VN 作为 Showcase
10. 在核心稳定后再推进 Plugin SDK 与社区生态

任何开发决策都必须回答：

> **这个改动是否让“陌生开发者从下载 Caesura 到发布第一款 VN”的路径更短、更稳定、更可靠？**

如果答案是否定的，则不得自动赋予高优先级。

如果发现现有代码存在真实 bug、数据安全问题、兼容性问题或发布 blocker，可以优先修复。

如果发现一个新功能很有趣，但并非上述流程的 blocker，应将它放入 backlog，而不是打断产品化主线。

---

# 28. 当前最重要的单一目标

> **把 Caesura 从“一个已经很强的 VN Runtime”推进到“别人真的敢用来做作品的 VN Engine”。**

第一里程碑不是：

> 200 个 command。

而是：

> **一个完全不了解 Caesura 内部架构的人，可以在 30 分钟内创建并运行一个包含背景、角色、对白、选择、BGM 和存档的 VN。**

第二里程碑：

> **这个人可以一键把它打包到 Windows / Linux / macOS / Web。**

第三里程碑：

> **这个人可以把它发布到 Steam。**

第四里程碑：

> **这个人愿意继续使用 Caesura 做第二个作品。**

这四个指标，比继续增加任何单个 Runtime feature 都更能决定 Caesura 能否真正形成市场。
