# Project Templates (tools/project_templates/)

> Caesura 项目模板库——由 Caesura Studio 的 Project Manager（POST /api/project/create）
> 使用。也支持命令行手工复制使用。

## 模板列表

| id | 名称 | 用途 | 初始剧本 |
|---|---|---|---|
| `blank` | Blank VN | 最小空模板，从零开始 | 36 行，单场景+占位对白 |
| `basic` | Basic VN | 两场景一选择 + 存档 + 转场 + credits | 91 行 |
| `live2d` | Live2D VN | Live2D 接入演示（需 Cubism SDK） | 53 行 |
| `kag3` | KAG3 Migration | KAG3 兼容风格（%var%/裸位置参数/别名） | 80 行 |
| `showcase` | Advanced/Showcase | 功能展示（tween/layout/i18n/NVL/postfx） | 111 行 |

## 结构

每个模板一致：

```text
<template>/
├── story.ks    # KAG Neo-Genesis 剧本
├── entry.lua   # kag_runner 启动入口（指向本模板 story.ks）
├── assets/     # 可引用共享资产池（复制进项目使用）
└── README.md   # 模板使用说明
```

## 使用

### 通过 Studio Project Manager（推荐）

GET /api/project/templates → 列出模板 → POST /api/project/create {template, name} →
在项目根 ./projects/<name>/ 创建。

### 命令行手工

```bash
cp -r tools/project_templates/basic ./mygame
cd mygame && lua entry.lua    # 从仓库根或项目目录运行
```

## 验证

每个模板必须通过：

- `lua scripts/ks_check.lua tools/project_templates/<t>/story.ks`（零错误）
- `SAMPLE_STORY="tools/project_templates/<t>/story.ks" lua tests/scripts/sample_game_headless.lua`（跑到 [end]）

`verify_golden_vn.sh` 已覆盖 golden_vn；模板本身由 headless_http_smoke 的 create 流程隐式验证。

参见 docs/compatibility.md（KAG3 兼容范围）与 docs/guides/getting-started.md。