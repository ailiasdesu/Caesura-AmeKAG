# Caesura (AmeKAG) — Blank VN 模板（从零开始）

**最小空模板。** 只有一个场景、一句占位对白、一个 `[end]`——**不含任何示例剧情文本、
分支或存档**。它的全部意义就是给你一个"能跑起来的最小内核"，然后你把自己真正想写的
故事填进去。

> 想一步一步来，先看更完整的 **basic** 模板（两场景 + 一次选择 + 存档 + 转场 + credits）；
> 本模板是刻意留空、不带任何示范的裸骨架。

## 从这个模板开始

### 1. 跑起来

从仓库根目录：

```bash
lua tools/project_templates/blank/entry.lua
lua scripts/ks_check.lua tools/project_templates/blank/story.ks   # 静态契约校验（目标：零警告）
```

游戏窗口弹出、显示一句占位对白并结束，说明引擎从头到尾都通——这正是"白板"的用途。

### 2. 写你的故事

编辑 `story.ks`。它只有一个 `*start` 场景：

- 把两句占位 `[ch]` 换成**你自己的开场对白**；
- 需要背景/立绘/音乐/音效时，按需加 `[bg]` / `[fg]` / `[playbgm]` / `[playse]` / `[playvoice]`；
- 需要分支时加 `[select]…[endselect]`；需要存档时加 `[save slot=N]`。

完整命令参考：`docs/api/command-contracts.md`（123 个契约命令，自动生成、权威）。

### 3. 资产

`assets/` 是骨架占位目录（`bg|fg|bgm|se|voice`），仓库内引用**共享资产池**（`assets/`）
所以克隆后立刻能跑；独立发布时把资产放进自己的 `<项目>/assets/` 树。缺资产时引擎按
**安全降级**显示占位（开发紫 / 发布灰），不会炸——见 `docs/guides/asset-pipeline.md`。

---

## 结构

```
tools/project_templates/blank/
├── README.md     # 本文档
├── entry.lua     # KAG runner 启动入口（kag_runner.start(".../blank/story.ks")）
├── story.ks      # 单场景空骨架：一句开场 + 一个占位 [ch] + [end]
└── assets/       # 资产骨架占位（bg | fg | bgm | se | voice）
```

## 相关文档

| 文档 | 内容 |
|------|------|
| `docs/api/command-contracts.md` | 全部 123 个命令契约（权威） |
| `docs/guides/kag-language-tour.md` | KAG Neo-Genesis 语言速览 + 五段模板 |
| `docs/guides/asset-pipeline.md` | 资产格式与目录规范、缺失降级 |
| `docs/guides/getting-started.md` | 引擎从零构建与运行 |
