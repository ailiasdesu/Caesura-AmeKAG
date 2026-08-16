# AGENTS.md — Caesura (AmeKAG) 引擎核心约束

> 本文档是参与本项目的所有 AI Agent 必须遵守的宪章。
> 违反这些规则的 PR 不应被合并。

---

## 1. 模块边界（铁律）

```
src/
├── archive/     # 加密归档（CARC 格式）
├── audio/       # 音频后端（SoLoud）
├── debug/       # 日志/性能分析
├── di/          # 依赖注入（BackendRegistry + 配额/预算）
├── entry/       # 引擎组合根（Engine + EngineConfig）
├── input/       # 输入路由（SDL 事件分发）
├── job/         # 任务系统（多线程）
├── live2d/      # Live2D 动画
├── minigame/    # 3D 小游戏
├── platform/    # 平台抽象（SDL3）
├── render/      # 渲染（bgfx）
├── resource/    # 资源管理（异步加载 + 资产管线）
├── rpc/         # HTTP RPC（编辑器服务器）
├── script/      # Lua 脚本（VM + 绑定 + 游戏状态）
├── steam/       # Steamworks 集成
└── storage/     # 存档/读档
```

**规则：**

- **每个模块只能通过 `api/` 子目录对外暴露符号。** 例如 `render/api/IRenderDevice.h`。
- **禁止模块间直接 include 具体实现头文件。** 只允许 include 接口头文件 (`I*.h`)。
- **唯一例外：`src/entry/` 和 `src/main.cpp`** ——它们共同构成组合根，可以 include 具体头文件来创建对象。
- **`di/BackendRegistry.h` 只 include `I*.h` 接口头文件。** 绝不 include 具体实现。

## 2. 接口规范

每个子系统的接口文件遵循命名约定：

```
src/<module>/api/I<ModuleName>.h
```

**接口必须是纯虚类（`= 0` 方法），不包含数据成员。**

类型定义（枚举、结构体）如果被接口方法使用（按值返回或按引用传参），必须放在接口头文件中。

## 3. BackendRegistry —— 唯一访问点

**所有后端访问必须通过 `BackendRegistry`：**

```cpp
// ✅ 正确
auto* renderer = BackendRegistry::instance().getRenderDevice();
auto* lua = BackendRegistry::instance().getLuaState();
BackendRegistry::instance().tryAlloc("textures");

// ❌ 错误
auto& tm = TextureManager::instance();  // 绕过注册表
auto* L = LuaManager::instance().state(); // 绕过注册表
```

**规则：**
- `BackendRegistry` 存储非拥有指针（`I*`），Engine 持有 `unique_ptr` 所有权。
- 子系统通过 `set*()` 注册，通过 `get*()` 访问。
- 添加新后端：创建 `I*` 接口 → 实现 → 在 `BackendRegistry` 添加 `set/get` → 在 `Engine::init()` 中注册。

## 4. 组合根（Composition Root）

**`src/main.cpp` + `src/entry/` 是唯一创建具体后端对象的地方。**

```
src/main.cpp:      new 具体后端 → 填入 EngineConfig → 传给 Engine
src/entry/:        接收 EngineConfig → 补齐默认后端 → init → 注册到 BackendRegistry
```

**禁止在其他模块中 `new` 或 `make_unique` 具体后端类型。**

## 5. 构建与测试（不可协商）

- **代码合并前必须通过全量构建：** `cmake --build . --config Debug` 零错误。
- **测试必须全绿：** `CaesuraTests` 发现的全部用例通过，`0 failed, 0 skipped`。
- **禁止**合并导致测试数量减少或新增失败的 PR。
- 测试从 `build/tests/Debug/` 目录执行（CWD 需匹配资源路径）。

## 6. 命名与风格

- **模块目录：全部小写**（`audio/`, `render/`, `script/`，不是 `Audio/`, `Render/`）。
- **大小写必须与 git 索引一致。** 16 个模块目录已统一为全小写。新增模块必须使用小写目录名。Windows 文件系统不区分大小写但 git 区分。在 Windows 上创建 `src/NewModule/` 后，git 索引会记录为 `src/NewModule/`，必须在提交前修正：

  ```powershell
  git mv src/NewModule src/newmodule_tmp
  git mv src/newmodule_tmp src/newmodule
  ```

  Linux/macOS 构建会因大小写不匹配而失败。
- **接口文件名：** `I` 前缀 + PascalCase（`IRenderDevice.h`, `IAudioBackend.h`）。
- **实现文件名：** PascalCase（`BgfxRenderDevice.h`, `SoLoudAudioEngine.cpp`）。
- **命名空间：** 所有公共类型在 `Caesura::` 下。
- **include 路径：** 使用 `../<module>/` 相对路径，或从 `src/` 根的裸路径（CMake 配置决定）。

## 7. 禁止事项

1. **禁止循环依赖。** 如果 A 依赖 B，B 不能依赖 A。使用接口打破循环。
2. **禁止头文件级具体类型依赖。** `.h` 文件不能 include 其他模块的非 `api/` 头文件。
3. **禁止在接口中暴露实现细节。** 渲染接口使用 `RenderTextureHandle` 等引擎自有不透明句柄；不得重新暴露 `bgfx::TextureHandle` 等第三方具体类型。
4. **禁止绕过 BackendRegistry 访问后端单例。** 宏（`DEBUG_*`）可以调用 `DebugManager::instance()` 直接访问——这是唯一的例外，用于零开销日志。
5. **禁止在非组合根位置创建具体后端对象。**
6. **禁止提交包含 `../../../` 或绝对路径的 include。**

## 8. 测试规范

- 测试文件：`tests/cpp/test_<module>.cpp`
- 使用 doctest 框架。
- 每个新模块必须至少有一个测试用例（构造不崩溃 + 核心功能）。
- 渲染测试不应在无窗口环境下创建真实 GPU 资源（使用默认构造+访问器测试）。

## 9. 耦合度目标

耦合计数脚本：`python scripts/count_coupling.py`

| 模块 | 目标（跨模块数） | 理由 |
|---|---|---|
| `entry` | ≤14 | 组合根，持有并构造所有后端 |
| `di` | ≤14 | DI 容器，天然需要知道所有接口类型 |
| `script` | ≤14 | 绑定层，需要触达所有被绑定的模块 |
| 其他 | ≤4 | 业务模块，通过接口隔离保持低耦合 |

任何非组合根/DI/绑定层模块超过 5 个跨模块依赖时，必须先解耦再添加新功能。

## 10. 修改接口的流程

1. 修改 `src/<module>/api/I*.h`
2. 更新所有实现类（添加 `override`）
3. 更新 `BackendRegistry`（如果需要新 getter/setter）
4. 更新 `Engine::init()`（如果需要新注册调用）
5. 全量构建 → `CaesuraTests` 与 CTest 全绿 → 提交

## 11. 已文档化的解决方案

`docs/solutions/` — 按类别组织的过往问题解决方案（bug 诊断、架构模式、最佳实践），
使用 YAML frontmatter（`module`, `tags`, `problem_type`）可搜索。在已文档化
的领域实现或调试时参考。



## 12. 文档分类

引擎文档按用途分为 5 类，放在 `docs/` 下：

### api/ — API 参考文档
| 文件 | 内容 |
|------|------|
| `api/command-contracts.md` | 118 个 KAG Neo-Genesis 命令的声明式契约参考（自动生成，权威） |
| `api/lua-modules.md` | Lua 模块 API 参考 |
| `api/cpp-interfaces.md` | 全部 C++ 接口定义（31 个） |
| `api/editor-api-reference.md` | 编辑器 RPC 端点参考 |
| `api/api-stats.md` | 实时 API 普查（自动生成） |
| `api/kag-commands.md` | 已弃用的 KAG3 兼容参考（被 command-contracts.md 取代） |
| `api/kag-expression-language.md` | `[if]`/`[eval]`/`${}` 表达式语法参考 |

### design/ — 架构与设计文档
| 文件 | 内容 |
|------|------|
| `design/engine-architecture-topology.md` | 引擎架构拓扑说明（16 模块 + 数据流） |
| `design/engine-capability-matrix.md` | 79 项能力的完成状态矩阵 |
| `design/engine-safety-and-qa-mechanisms.md` | JobSystem 线程安全、Lua 沙箱、BackendRegistry 依赖说明 |
| `design/engine-topology-mermaid.md` | 1 张 Mermaid 架构拓扑图源码 |
| `design/backend-registry-dependency-guide.md` | BackendRegistry 依赖矩阵与使用规范 |
| `design/nextgen-kag-standard.md` | KAG Neo-Genesis 标准定义 |
| `design/engine-market-comparison.md` | 2026-08-03 市场对比（历史快照） |
| `design/engine-market-analysis-2026-08-06.md` | 2026-08-06 市场分析（数据更新版） |

### guides/ — 用户与开发者指南
| 文件 | 内容 |
|------|------|
| `guides/getting-started.md` | 从克隆到 Demo 可跑的入门指南 |
| `guides/asset-pipeline.md` | 支持的资源格式与目录规范 |
| `guides/carc-packaging.md` | CARC 打包格式与工具使用 |
| `guides/live2d-setup.md` | Cubism SDK 集成步骤 |

### plans/ — 执行记录与当前计划
按日期命名（`YYYY-MM-DD-NNN-描述.md`），最新交接文档为权威现状：
| 文件 | 内容 |
|------|------|
| `plans/2026-08-16-021-delivery-handoff.md` | **最新状态**交接文档（021，round 100 起点 / round 99 完成；轮次记录权威在 plans/audit/ROADMAP-100.md） |
| `plans/2026-08-14-019-delivery-handoff.md` | 交接文档（019，round 89 状态） |
| `plans/2026-08-12-008-delivery-handoff.md` | 交接文档（008，内联标记视觉化） |
| `plans/2026-08-12-007-delivery-handoff.md` | 交接文档（007，SMA 游戏循环接驳） |
| `plans/2026-08-12-006-delivery-handoff.md` | 交接文档（006，停滞修复 + 表达力扩展） |
| `plans/2026-08-12-005-delivery-handoff.md` | 交接文档（005，[until]/[button cond]/aidev） |
| `plans/2026-08-12-004-generation-gap-roadmap.md` | 代差路线图（五大战役，权威规划） |
| `plans/2026-08-04-006-perf-baseline-update.md` | 性能基线更新 |
| `plans/2026-06-17-001-feat-engine-stability-hardening-plan.md` | 引擎稳定性加固计划 |
| `plans/2026-06-18-galgame-core-readiness-audit.md` | Galgame 核心就绪度排查方案 |
| `plans/2026-07-02-architecture-hardening-summary.md` | 架构硬化执行总结 |
| `plans/2026-07-03-continued-hardening-summary.md` | 后续架构硬化总结 |
| `plans/2026-07-16-001-modular-static-library-migration-summary.md` | 模块静态库架构迁移总结 |

### solutions/ — 经验与模式
| 文件 | 内容 |
|------|------|
| `solutions/architecture-patterns/engine-constructor-sigsegv-testing.md` | Engine 构造崩溃的 NullGpuMonitor 解决模式 |
| `solutions/architecture-patterns/header-only-to-instance-class.md` | 头文件内联类重构为实例类模式 |
| `solutions/build-errors/clean-build-include-path.md` | 全量构建 include 路径修复模式 |
| `solutions/runtime-crashes/bgfx-predefined-uniform-name-conflict.md` | bgfx 预定义 uniform 命名冲突 |
| `solutions/deferred-gpu-tests.md` | 无 GPU 环境下无法覆盖的测试项清单 |

### 规则
- **新 API 文档** → `docs/api/`
- **新架构/设计文档** → `docs/design/`
- **新使用指南** → `docs/guides/`
- **执行计划与记录** → `docs/plans/`（按日期命名：`YYYY-MM-DD-NNN-描述.md`）
- **可复用的经验/模式** → `docs/solutions/`
- **历史需求文档** → `docs/brainstorms/`（仅保留被 plans/ 引用的 origin 需求，无引用后删除）
- **禁止**将一次性执行提示词（prompts）留在 docs/ 中——执行完成后删除，仅保留执行总结

<!-- >>> aimax:reasonix >>> -->
# AI MAX Reasonix 集成
本项目的 AI MAX 工作流已适配 Reasonix。Reasonix 是模型无关的宿主，模型由 `reasonix.toml` 或 `--model` 选择；不要在项目文件中写入 API key。

## 使用边界
- Reasonix 会自动读取本文件；需要专门工作流时，从 `.agents/skills/aimax-*` 中选择对应技能。
- 不得输出或调用 Claude Code 的 `/aimax:*` 斜杠命令；应直接使用 `aimax-command-*` 技能。
- 使用 `aimax-command-auto` 时，选中目标命令后必须读取对应的 `.agents/skills/aimax-command-<命令名>/SKILL.md`，并在当前轮执行完整流程，不得只报告路由结果。
- 执行任何 Git 命令前必须先确认当前项目或其父目录存在 `.git`；如果不存在，跳过所有 Git 命令并继续非 Git 工作流，不得将其视为失败。
- AI MAX 的 agent 和 command 已转换为 Reasonix 技能，原始副本保存在 `.aimax/reasonix` 供审阅。
- 下方只内嵌宿主无关的通用规则；Claude 专属的 agent 和 hook 配置不会注入 Reasonix。

### AI MAX 规则: coding-style.md

# 编码风格

## 不可变性（关键）

始终创建新对象，绝不修改原对象：

```javascript
// 错误: 可变操作
function updateUser(user, name) {
  user.name = name  // 可变操作！
  return user
}

// 正确: 不可变操作
function updateUser(user, name) {
  return {
    ...user,
    name
  }
}
```

## 文件组织

多个小文件 > 少量大文件：
- 高内聚，低耦合
- 通常 200-400 行，最多 800 行
- 从大型组件中提取工具函数
- 按功能/领域组织，而非按类型组织

## 错误处理

始终全面处理错误：

```typescript
try {
  const result = await riskyOperation()
  return result
} catch (error) {
  console.error('Operation failed:', error)
  throw new Error('Detailed user-friendly message')
}
```

## 输入验证

始终验证用户输入：

```typescript
import { z } from 'zod'

const schema = z.object({
  email: z.string().email(),
  age: z.number().int().min(0).max(150)
})

const validated = schema.parse(input)
```

## 代码质量检查清单

在标记工作完成前：
- [ ] 代码可读性好且命名规范
- [ ] 函数简短（<50 行）
- [ ] 文件聚焦（<800 行）
- [ ] 无深层嵌套（>4 层）
- [ ] 正确的错误处理
- [ ] 无 console.log 语句
- [ ] 无硬编码值
- [ ] 无可变操作（使用不可变模式）


### AI MAX 规则: git-workflow.md

# Git 工作流

## 提交信息格式

```
<type>: <description>

<optional body>
```

类型: feat, fix, refactor, docs, test, chore, perf, ci

## Pull Request 工作流

创建 PR 时：
1. 分析完整的提交历史（不仅仅是最新的提交）
2. 使用 `git diff [base-branch]...HEAD` 查看所有变更
3. 撰写全面的 PR 摘要
4. 包含带 TODO 的测试计划
5. 如果是新分支，推送时使用 `-u` 标志

## 功能实现工作流

1. **先规划**
   - 使用 **planner** agent 创建实现计划
   - 识别依赖和风险
   - 分解为多个阶段

2. **TDD 方法**
   - 使用 **tdd-guide** agent
   - 先编写测试（红灯）
   - 实现代码使测试通过（绿灯）
   - 重构（改进）
   - 验证 80%+ 覆盖率

3. **代码审查**
   - 编写代码后立即使用 **code-reviewer** agent
   - 解决关键和高优先级问题
   - 尽可能修复中等优先级问题

4. **提交和推送**
   - 详细的提交信息
   - 遵循约定式提交格式

## 输出规则
- 只输出提交信息本身，不要添加任何签名、标记或元信息
- 不要包含 "Generated with Claude Code"、"Co-Authored-By" 等署名内容
- 不要使用 emoji 表情符号
- 保持简洁专业的风格


### AI MAX 规则: patterns.md

# 常用模式

## API 响应格式

```typescript
interface ApiResponse<T> {
  success: boolean
  data?: T
  error?: string
  meta?: {
    total: number
    page: number
    limit: number
  }
}
```

## 自定义 Hook 模式

```typescript
export function useDebounce<T>(value: T, delay: number): T {
  const [debouncedValue, setDebouncedValue] = useState<T>(value)

  useEffect(() => {
    const handler = setTimeout(() => setDebouncedValue(value), delay)
    return () => clearTimeout(handler)
  }, [value, delay])

  return debouncedValue
}
```

## 仓储模式（Repository Pattern）

```typescript
interface Repository<T> {
  findAll(filters?: Filters): Promise<T[]>
  findById(id: string): Promise<T | null>
  create(data: CreateDto): Promise<T>
  update(id: string, data: UpdateDto): Promise<T>
  delete(id: string): Promise<void>
}
```

## 骨架项目

实现新功能时：
1. 搜索经过实战检验的骨架项目
2. 使用并行 agent 评估选项：
   - 安全评估
   - 可扩展性分析
   - 相关性评分
   - 实现规划
3. 克隆最佳匹配作为基础
4. 在经过验证的结构中迭代


### AI MAX 规则: performance.md

# 性能优化

## 模型选择策略

**Haiku 4.5**（Sonnet 90% 能力，节省 3 倍成本）：
- 频繁调用的轻量级 agent
- 结对编程和代码生成
- 多 agent 系统中的工作 agent

**Sonnet 4.5**（最佳编码模型）：
- 主要开发工作
- 编排多 agent 工作流
- 复杂编码任务

**Opus 4.5**（最深度推理）：
- 复杂架构决策
- 最高推理需求
- 研究和分析任务

## 上下文窗口管理

在上下文窗口的最后 20% 避免：
- 大规模重构
- 跨多文件的功能实现
- 调试复杂交互

对上下文敏感度较低的任务：
- 单文件编辑
- 独立工具函数创建
- 文档更新
- 简单 Bug 修复

## Ultrathink + Plan 模式

对于需要深度推理的复杂任务：
1. 使用 `ultrathink` 增强思考
2. 启用 **Plan 模式** 进行结构化方法
3. 通过多轮批评"预热引擎"
4. 使用分角色子 agent 进行多样化分析

## 构建故障排除

如果构建失败：
1. 使用 **build-error-resolver** agent
2. 分析错误信息
3. 增量修复
4. 每次修复后验证


### AI MAX 规则: security.md

# 安全指南

## 强制安全检查

每次提交前：
- [ ] 无硬编码密钥（API 密钥、密码、令牌）
- [ ] 所有用户输入已验证
- [ ] SQL 注入防护（参数化查询）
- [ ] XSS 防护（HTML 净化）
- [ ] CSRF 保护已启用
- [ ] 身份验证/授权已验证
- [ ] 所有端点已启用速率限制
- [ ] 错误信息不泄露敏感数据

## 密钥管理

```typescript
// 绝不: 硬编码密钥
const apiKey = "sk-proj-xxxxx"

// 始终: 使用环境变量
const apiKey = process.env.OPENAI_API_KEY

if (!apiKey) {
  throw new Error('OPENAI_API_KEY not configured')
}
```

## 安全响应协议

如果发现安全问题：
1. 立即停止
2. 使用 **security-reviewer** agent
3. 继续之前修复关键问题
4. 轮换任何泄露的密钥
5. 审查整个代码库是否存在类似问题


### AI MAX 规则: testing.md

# 测试要求

## 最低测试覆盖率：80%

测试类型（全部必需）：
1. **单元测试** - 单个函数、工具函数、组件
2. **集成测试** - API 端点、数据库操作
3. **E2E 测试** - 关键用户流程（Playwright）

## 测试驱动开发

强制工作流：
1. 先编写测试（红灯）
2. 运行测试 - 应该失败
3. 编写最小实现（绿灯）
4. 运行测试 - 应该通过
5. 重构（改进）
6. 验证覆盖率（80%+）

## 测试失败故障排除

1. 使用 **tdd-guide** agent
2. 检查测试隔离性
3. 验证 mock 是否正确
4. 修复实现，而非测试（除非测试有误）

## Agent 支持

- **tdd-guide** - 主动用于新功能，强制先写测试
- **e2e-runner** - Playwright E2E 测试专家


## Reasonix 模型配置
本机可使用 `reasonix --model deepseek/deepseek-v4-flash` 或在 Reasonix 全局配置中设置 `default_model`。模型接入和凭据由 Reasonix 管理，AI MAX 不复制或修改凭据。
<!-- <<< aimax:reasonix <<< -->
