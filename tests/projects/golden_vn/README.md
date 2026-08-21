# Golden Project (tests/projects/golden_vn/)

> **长期回归夹具**——产品化总任务书（§14 / release-gate.md §7）要求每次 release 都完整跑一遍的
> 黄金项目。不是 showcase，是**自动化全 feature 面回归**。

## 目的

- 覆盖引擎全部主要功能面，作为 release 前的确定性冒烟
- 新 PR 若改到 KAG 命令 / 存档 / 渲染 / 音频 / i18n 等热路径，跑它即可发现回归
- 与 `demo/example_game/`（面向用户的示例游戏）互补：golden_vn 偏测试、example_game 偏展示

## 运行

```bash
# 直接跑（需真实 GPU 窗口）
lua tests/projects/golden_vn/entry.lua

# 完整门禁（静态契约 + headless 全跑 + 分支可达 + feature 覆盖）
bash scripts/verify_golden_vn.sh
```

## 覆盖的 feature 面

| Feature | 脚本位置（story.ks） |
|---|---|
| dialogue / text markup (color/b/i/s) | 开场+Section A |
| choices [select]/[sel] | *choice_moment |
| save/load（slot 9 自动存档） | Section A |
| rollback / history（阻塞式，headless 用非阻塞替代） | Section A 注释 |
| NVL 模式 | *common_mid |
| tween（非阻塞 wait=false，headless 兼容） | *common_mid |
| layout 容器（hbox） | *common_mid |
| i18n 热切换（en/ja/zh） | *i18n_check |
| audio（bgm/se/voice） | Section A/C |
| expression conditional [if] | *i18n_check |
| replay / mod（专项测试覆盖，主路径非阻塞） | Section D |
| 转场 [trans] | Section A/B |
| 结束 [end] + credits | Section D |

## 为什么某些命令在主路径外

`[history]`、`[replay mode=record]`、阻塞 `[tween]` 在无输入/无真实时钟的 headless 环境会死等
（v1.0.0 时 [history]/[gallery] 阻塞式菜单 headless 死等的教训）。golden_vn 的原则：**feature 面
必须被提到（source grep 锁定），但自动回归主路径保持非阻塞**；这些阻塞特性由
`tests/scripts/sample_game_headless.lua`（example_game）与 replay.lua / tween 单元测试分别覆盖。
