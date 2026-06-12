---
name: Caesura Next-Phase Plan
created: 2026-06-12
status: active
origin: docs/plans/2026-06-12-001-module-completion-plan.md
---

# Caesura (AmeKAG) 下一阶段计划

## 当前基线

架构解耦完成。26 个 api/ 接口，193/193 测试全绿，0 循环依赖。

**但这仍然不是一个可用的 galgame 引擎。** 架构干净不等于产品可用。

## 目标问题

创作者（小型 galgame 社团）下载引擎后需要能够：
1. 写出 KAG 脚本 → 看到画面渲染 → 听到音频 → 保存/读取进度
2. 在 Windows/macOS/Linux 上都能构建和运行
3. 有文档知道怎么用
4. 有 Demo 可以照猫画虎

当前这些都不成立。

---

## 赛道

### T1: 端到端集成测试（最高优先）

> 当前 193 个测试全是单元测试（"构造不崩溃"级别）。需要验证 galgame 核心链路。

**目标：** 添加至少 10 个集成测试，覆盖从 KAG 脚本到屏幕输出的完整链路。

**实施：**

| IU | 描述 | 文件 |
|----|------|------|
| T1.1 | KAG 脚本解析→执行集成测试 | `tests/cpp/test_kag_execution.cpp` |
| T1.2 | 渲染管线集成测试（Layer→RTT→blit） | `tests/cpp/test_render_integration.cpp` |
| T1.3 | 音频播放集成测试（实际 .wav 播放） | `tests/cpp/test_audio_integration.cpp` |
| T1.4 | 存档→读档完整往返测试 | `tests/cpp/test_save_roundtrip.cpp` |
| T1.5 | 热重载集成测试（修改脚本→自动重载） | `tests/cpp/test_hotreload_integration.cpp` |

**验收：** 每个 IU 至少 3 个 TEST_CASE，全绿。

### T2: Engine.cpp 分阶段初始化

> Engine::init() 586 行，单点故障，不可测试。——这是 M8 的延期项。

**目标：** 将 init() 拆为独立可测试的阶段方法。

**实施：**

| IU | 描述 |
|----|------|
| T2.1 | 提取 `initPlatformPhase()` — SDL3/bgfx/FreeType |
| T2.2 | 提取 `initScriptingPhase()` — Lua VM + 模块注册 |
| T2.3 | 提取 `initAssetPhase()` — 资源/异步加载/CARC |
| T2.4 | 提取 `initOptionalPhase()` — Steam/Live2D/MiniGame |
| T2.5 | 每个阶段失败时清理已分配资源（不泄漏） |

**验收：** Engine.cpp <400 行，每个阶段方法可独立在测试中验证。

### T3: 跨平台 CI 真正运行

> `.github/workflows/ci.yml` 存在但只跑耦合计数。从未在 Linux/macOS 上实际构建。

**目标：** 三端 CI 自动构建+测试。

**实施：**

| IU | 描述 |
|----|------|
| T3.1 | 修复 CMakeLists.txt 跨平台兼容（SDL3 路径、bgfx 后端选择） |
| T3.2 | Linux CI job：ubuntu-latest，apt 安装依赖，构建+测试 |
| T3.3 | macOS CI job：macos-latest，brew 安装依赖，构建+测试 |
| T3.4 | Windows CI job：保持现有 MSVC 构建 |
| T3.5 | CI badge 加入 README.md |

**验收：** PR 自动触发三端构建，任一端失败则 CI 红。

### T4: KAG API 文档

> 84 个 KAG API 无文档，创作者只能读 Lua 绑定源码。

**目标：** 生成 KAG 命令参考文档。

**实施：**

| IU | 描述 |
|----|------|
| T4.1 | 从 `scripts/kag/commands/` 提取所有命令签名 |
| T4.2 | 生成 `docs/api/kag-commands.md`（每条命令：签名/参数/示例/注意事项） |
| T4.3 | 生成 `docs/api/lua-modules.md`（Debug/Render/VFX/Steam 模块文档） |
| T4.4 | 生成 `docs/guides/getting-started.md`（从下载到 Demo 可跑的完整指南） |

**验收：** 新贡献者按文档操作 10 分钟内能跑出 Demo。

### T5: Demo 场景修复与扩展

> `scripts/demo.lua` + `scripts/demo_story.ks` 存在但从未在 CI 中验证。

**目标：** 一个能展示核心功能的 Demo galgame。

**实施：**

| IU | 描述 |
|----|------|
| T5.1 | 修复 Demo 脚本（确保在当前引擎上能跑） |
| T5.2 | 添加 CI 中的 Demo 冒烟测试（headless 模式跑 30 秒不崩溃） |
| T5.3 | 扩展 Demo 覆盖：立绘淡入/淡出、BGM 切换、选项分支、存档 |

**验收：** Demo 在 headless CI 中稳定运行。

---

## 执行顺序

```
T1 (集成测试) ──→ T2 (Engine 拆分) ──→ T3 (CI) ──→ T4 (文档) + T5 (Demo)
```

T1 和 T2 可并行。T3 依赖 T2 完成（Engine 拆分后 CI 构建更稳定）。T4/T5 可并行。

## 非目标（本次不做）

- Live2D/MiniGame/Steam 实际实现（依赖外部 SDK 和 license，属于 R2）
- web-editor 功能完善（属于独立项目）
- AI 辅助创作集成（R3）
- 性能优化/GPU 分析（等有实际使用数据）