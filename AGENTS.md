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
- **唯一例外：`entry/` 和 `main.cpp`** ——它们是组合根，可以 include 任何具体头文件来创建对象。
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

**`main.cpp` + `entry/Engine.cpp` 是唯一创建具体对象的地方。**

```
main.cpp:        new 具体后端 → 填入 EngineConfig → 传给 Engine
Engine::init():  接收 EngineConfig → init → 注册到 BackendRegistry
```

**禁止在其他模块中 `new` 或 `make_unique` 具体后端类型。**

## 5. 构建与测试（不可协商）

- **代码合并前必须通过全量构建：** `cmake --build . --config Debug` 零错误。
- **测试必须全绿：** `149/149 passed, 0 failed, 0 skipped`。
- **禁止**合并导致测试数量减少或新增失败的 PR。
- 测试从 `build/tests/Debug/` 目录执行（CWD 需匹配资源路径）。

## 6. 命名与风格

- **模块目录：全部小写**（`audio/`, `render/`, `script/`，不是 `Audio/`, `Render/`）。`n`n- **大小写必须与 git 索引一致。** 16 个模块目录已统一为全小写。新增模块必须使用小写目录名。Windows 文件系统不区分大小写但 git 区分——在 Windows 上创建 `src/NewModule/` 后，git 索引会记录为 `src/NewModule/`，必须在提交前修正：`n  ```powershell`n  git mv src/NewModule src/newmodule_tmp`n  git mv src/newmodule_tmp src/newmodule`n  ````n  Linux/macOS 构建会因大小写不匹配而失败。`n
- **接口文件名：** `I` 前缀 + PascalCase（`IRenderDevice.h`, `IAudioBackend.h`）。
- **实现文件名：** PascalCase（`BgfxRenderDevice.h`, `SoLoudAudioEngine.cpp`）。
- **命名空间：** 所有公共类型在 `Caesura::` 下。
- **include 路径：** 使用 `../<module>/` 相对路径，或从 `src/` 根的裸路径（CMake 配置决定）。

## 7. 禁止事项

1. **禁止循环依赖。** 如果 A 依赖 B，B 不能依赖 A。使用接口打破循环。
2. **禁止头文件级具体类型依赖。** `.h` 文件不能 include 其他模块的非 `api/` 头文件。
3. **禁止在接口中暴露实现细节。** 例如 `IRenderDevice` 不应有 `bgfx::TextureHandle`（可以用 `uint32_t` 或不透明句柄代替）——注：当前接口仍有 bgfx 类型，后续迭代应替换。
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

| 模块 | 目标（跨模块数） |
|---|---|
| `entry` | ≤14（组合根，允许较高） |
| `di` | ≤8（全是接口，已达上限） |
| 其他 | ≤4 |

任何非组合根模块超过 5 个跨模块依赖时，必须先解耦再添加新功能。

## 10. 修改接口的流程

1. 修改 `src/<module>/api/I*.h`
2. 更新所有实现类（添加 `override`）
3. 更新 `BackendRegistry`（如果需要新 getter/setter）
4. 更新 `Engine::init()`（如果需要新注册调用）
5. 全量构建 → 149 测试全绿 → 提交

## 11. 已文档化的解决方案

`docs/solutions/` — 按类别组织的过往问题解决方案（bug 诊断、架构模式、最佳实践），
使用 YAML frontmatter（`module`, `tags`, `problem_type`）可搜索。在已文档化
的领域实现或调试时参考。



## 11. 文档分类

引擎文档按用途分为 5 类，放在 `docs/` 下：

### api/ — API 参考文档
| 文件 | 内容 |
|------|------|
| `api/kag-commands.md` | 68 个 KAG 命令的完整参考（签名/参数/示例） |
| `api/lua-modules.md` | 6 个 Lua 模块的 API 参考 |

### design/ — 架构与设计文档
| 文件 | 内容 |
|------|------|
| `design/engine-architecture-topology.md` | 引擎架构拓扑说明（16 模块 + 数据流） |
| `design/engine-capability-matrix.md` | 79 项能力的完成状态矩阵 |
| `design/engine-safety-and-qa-mechanisms.md` | JobSystem 线程安全、Lua 沙箱、BackendRegistry 依赖说明 |
| `design/engine-topology-mermaid.md` | 4 张 Mermaid 拓扑图源码 |
| `design/backend-registry-dependency-guide.md` | BackendRegistry 依赖矩阵与使用规范 |

### guides/ — 用户与开发者指南
| 文件 | 内容 |
|------|------|
| `guides/getting-started.md` | 从克隆到 Demo 可跑的入门指南 |
| `guides/asset-pipeline.md` | 支持的资源格式与目录规范 |
| `guides/carc-packaging.md` | CARC 打包格式与工具使用 |
| `guides/live2d-setup.md` | Cubism SDK 集成步骤 |

### plans/ — 执行记录与当前计划
| 文件 | 内容 |
|------|------|
| `plans/2026-06-12-null-jobsystem-plan.md` | 当前活跃计划 |
| `plans/T1-T5-execution-summary.md` | T1-T5 执行记录 |
| `plans/R1-R5-execution-summary.md` | R1-R5 执行记录 |
| `plans/F1-F4-execution-summary.md` | F1-F4 执行记录 |
| `plans/null-jobsystem-execution-summary.md` | NullJobSystem 执行记录 |

### solutions/ — 经验与模式
| 文件 | 内容 |
|------|------|
| `solutions/architecture-patterns/engine-constructor-sigsegv-testing.md` | Engine 构造崩溃的 NullGpuMonitor 解决模式 |
| `solutions/architecture-patterns/header-only-to-instance-class.md` | 头文件内联类重构为实例类模式 |
| `solutions/build-errors/clean-build-include-path.md` | 全量构建 include 路径修复模式 |

### 规则
- **新 API 文档** → `docs/api/`
- **新架构/设计文档** → `docs/design/`
- **新使用指南** → `docs/guides/`
- **执行计划与记录** → `docs/plans/`（按日期命名：`YYYY-MM-DD-NNN-描述.md`）
- **可复用的经验/模式** → `docs/solutions/`
- **禁止**将一次性执行提示词（prompts）留在 docs/ 中——执行完成后删除，仅保留执行总结