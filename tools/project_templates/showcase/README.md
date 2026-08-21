# Caesura (AmeKAG) — Showcase VN 模板

**功能展示模板。** 既是"先看看这引擎能干什么"的演示场景，也是一个**多场景**骨架，
覆盖引擎的一批招牌能力。它比 `tests/projects/golden_vn` 更偏"**展示**"而非"回归"——更短、
更好读、刻意突出每项特性的写法，方便你复制片段到自己的项目里。

> `tests/projects/golden_vn`（黄金项目）是**长期回归夹具**，每次发版都会端到端驱动；
> 本模板是给它做"精简 + 展示"的版本，**读完即拆**，别把它当正式剧本。

## 覆盖能力

| 场景 | 命令 | 能力 |
|------|------|------|
| 入口/分支 | `[select] [sel] [endselect]` | 玩家选择分支 |
| Tween & Layout | `[tween]` `[layout]` `[layout_slot]` `[layfade]` | 声明式补间 + 布局容器（hbox） |
| NVL & i18n | `[nvl]` `[nvl off]` `[i18n language=...]` | 整屏文本累积 + 语言热切换 |
| Trans / Save | `[trans]` `[save]` `[notify]` | 转场 + 存档提示 |
| 条件 | `[set] [if] [else] [endif]` | 类型化变量与条件分支 |
| 收尾 | `[flash]` `[scroll]` `[ending]` | 特效闪屏 + 滚动 credits + 结局解锁 |

## 结构

```
tools/project_templates/showcase/
├── README.md     # 本文档
├── entry.lua     # KAG runner 启动入口
├── story.ks      # 4 场景功能展示：branch / tween+layout / nvl+i18n / trans+save+ending
└── assets/       # 资产骨架占位
```

## 怎么用

```bash
lua tools/project_templates/showcase/entry.lua
lua scripts/ks_check.lua tools/project_templates/showcase/story.ks   # 静态契约校验
```

- 想给玩家一个"开局预览"，保留入口的 `[sel]` 分支；
- 想把它变成正式作品，删掉不需要的场景，直接往对应场景里填自己的故事；
- 想系统了解全部命令，看 `docs/api/command-contracts.md`（123 命令）。

## 相关文档

| 文档 | 内容 |
|------|------|
| `tests/projects/golden_vn/story.ks` | 全功能**回归**夹具（本模板的精简展示版） |
| `docs/api/command-contracts.md` | 全部 123 个命令契约（权威） |
| `docs/guides/kag-language-tour.md` | 语言速览 + 命令分类 |
| `docs/guides/asset-pipeline.md` | 资产格式与缺失降级 |
