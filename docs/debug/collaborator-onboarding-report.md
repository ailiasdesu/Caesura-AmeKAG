# Caesura (AmeKAG) 引擎 — 合作者入门报告

> 2026-06-28 | 版本 v1.0 | 作者：A（架构师）

---

## 1. 引擎当前状态

| 指标 | 数值 |
|------|------|
| 测试 | **412 passed**, 967 assertions, 0 failed |
| 耦合 | entry=15, di=12, script=10, render=3 (在预算内) |
| 代码 | 16模块, 30纯虚接口, 0循环依赖 |
| 远程 | origin/master @ ab447fbf |

## 2. 架构拓扑

```
main.cpp → EngineConfig → Engine::init() → BackendRegistry (总机)
                                               ↓ (21个 I* 接口)
  ┌──────────────┬──────────────┬──────────────┬──────────────┐
  │  Script      │  Render      │  Audio       │  Content     │
  │  Lua 5.4     │  bgfx GPU    │  SoLoud      │  Save/Archive │
  │  KAG Parser  │  Layers      │  BGM/Voice/SE│  AsyncLoader │
  │  Scheduler   │  Textures    │  3D Spatial  │  Live2D/Mini │
  └──────────────┴──────────────┴──────────────┴──────────────┘
  ┌──────────────┬──────────────┬──────────────┐
  │  Platform    │  Input       │  JobSystem   │
  │  SDL3        │  Router      │  ThreadPool  │
  └──────────────┴──────────────┴──────────────┘
```

完整架构图: https://www.figma.com/board/tTmkngNDvAv79y07s3ipZf

## 3. Galgame 核心就绪度

对引擎进行了76项galgame功能域的全面审计：

| 域 | 通过 | P2后续 | 通过率 |
|----|------|--------|--------|
| D1 启动初始化 | 5/5 | 0 | 100% |
| D2 背景图层 | 7/8 | 1 | 88% |
| D3 文字渲染 | 6/8 | 2 | 75% |
| D4 音频系统 | 7/8 | 1 | 88% |
| D5 KAG脚本 | 12/12 | 0 | 100% |
| D6 存档读档 | 10/10 | 0 | 100% |
| D7 转场特效 | 5/6 | 1 | 83% |
| D8 资源加载 | 6/6 | 0 | 100% |
| D9 输入交互 | 5/7 | 1 | 71% |
| D10 Live2D | 3/4 | 1 | 75% |
| D11 集成 | 2/2 | 0 | 100% |
| **总计** | **68/76** | **7** | **89%** |

## 4. 飞书共享追踪表

所有76项验证记录在飞书多维表格中，支持实时协作：

https://mcn95ia2oj1a.feishu.cn/base/SKH2bMea7aF0TlsykBkcRdM1nWH

- 14个字段：ID、状态、域名、描述、现象、根因、修复提交、负责人、严重程度、关联模块、耗时、备注、日期
- 3个视图：Grid(列表)、按负责人分组、Kanban看板

## 5. 三人分工

### A（架构师） — 核心层 (23项)
**模块**: entry/di/platform/input/job/debug
**项目**: D1(启动) + D9(输入交互) + D11(集成) + 全局把控

### B（渲染） — 视觉层 (32项)
**模块**: render/resource/live2d
**项目**: D2(背景图层) + D3(文字渲染) + D7(转场) + D8(资源加载) + D10(立绘)

### C（脚本+功能） — 逻辑层 (21项)
**模块**: script/audio/storage
**项目**: D4(音频) + D5(KAG脚本) + D6(存档读档) + KAG命令文档

## 6. 快速入门

### 构建
```bash
cd "D:\文件存放处\codex\Caesura(AmeKAG)"
cmake -B build -DCAESURA_LIVE2D=OFF -G "Visual Studio 17 2022"
cmake --build build --config Debug --parallel
```

### 测试
```bash
cd build\tests\Debug
./CaesuraTests.exe                    # 全量
./CaesuraTests.exe -tc="*Render*"    # 按模块筛选
```

### 开发规则 (铁律 — 必须遵守)

1. **所有模块通过 BackendRegistry 访问后端**，不能直接 `new` 或 `make_unique`
2. **include 只能是 `<module>/api/I*.h`**，不允许 include 具体实现头文件
3. **TDD 强制**：先写测试 → 红 → 绿 → 重构
4. **Commit 原子化**：每个 commit 一个逻辑单元，conventional commit 格式
5. **5维度审查**：正确性/安全性/可靠性/测试/可维护性，P0=0才能交付

## 7. VS Code 环境

`.vscode/` 已配置好：
- C++20 + MSVC + Windows SDK 路径
- cmake-tools 预设
- clang-format (WebKit based, 120col)
- Lua 5.4 语言支持
- F5 启动调试（自动先构建）
- Ctrl+Shift+B 构建

队友打开项目后 VS Code 会提示安装推荐插件。

## 8. 关键文件

| 文件 | 用途 |
|------|------|
| `AGENTS.md` | 模块边界、接口规范、耦合预算 |
| `CLAUDE.md` | 项目概览、构建命令、开发规则 |
| `docs/plans/2026-06-18-galgame-core-readiness-audit.md` | 就绪度排查方案 |
| `docs/debug/audit-tracker.md` | 共享问题追踪表 |
| `docs/debug/B-domain-audit-guide.md` | B域排查执行指南 |
| `docs/debug/C-domain-audit-guide.md` | C域排查执行指南 |
| `docs/solutions/architecture-patterns/galgame-engine-readiness-audit.md` | 经验文档 |
| `docs/api/cpp-interfaces.md` | 30个C++接口参考 |
| `docs/api/kag-commands.md` | 69个KAG Neo-Genesis 命令契约参考（docs/api/command-contracts.md） |
| `docs/design/engine-topology-mermaid.md` | Mermaid拓扑图源码 |

## 9. 下一步

1. B和C各自排查负责域 (参照各自执行指南)
2. 排查结果实时更新到飞书Base
3. A每天检查飞书Base，发现跨模块问题同步
4. 所有域通过后跑一次完整demo验证
5. 7个P2后续项根据优先级逐步完成

## 10. 联系方式

- GitHub: https://github.com/ailiasdesu/Caesura-AmeKAG
- Figma拓扑图: https://www.figma.com/board/tTmkngNDvAv79y07s3ipZf
- 飞书Base: https://mcn95ia2oj1a.feishu.cn/base/SKH2bMea7aF0TlsykBkcRdM1nWH
