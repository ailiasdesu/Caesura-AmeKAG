# Caesura (AmeKAG) — New Project Template

给你的下一部视觉小说一个**零摩擦起点**。这个模板是一个最小可运行骨架：

- **两个场景**（开场 + 分支结局）与**一次玩家选择**（`[select]` / `[sel]`）
- 资产目录占位（`assets/bg|fg|bgm|se|voice` + README）
- 启动入口（`entry.lua`）、契约校验、headless 冒烟、一键打包全链路

> 模板在仓库内引用**共享资产池**（`assets/`），克隆后立刻能跑；独立发布时把
> 资产放进自己的 `<项目>/assets/` 树（见 `assets/README.md` 的占位策略）。

---

## 从模板开始：五步

### 1. 拷贝模板为新项目

```bash
# 在仓库内起步（或拷贝到新目录）
cp -r demo/template my_game
```

> 想发布成 GitHub 模板仓库？见 [docs/guides/template-quickstart.md](../../docs/guides/template-quickstart.md)
> ——repo settings → Templates → `Template repository` 一键让你 "Use this template"。

### 2. 写你的场景

`story.ks` 就是剧本本体。先跑通：

```bash
# 从仓库根目录
lua demo/template/entry.lua          # 打开游戏窗口跑起来
lua scripts/ks_check.lua demo/template/story.ks   # 静态契约校验（目标：零警告）
```

改剧本就编辑 `story.ks`：加场景加标签，分支用 `[select]…[endselect]`（本模板已有示例）。
完整的 KAG 命令参考见 `docs/api/command-contracts.md`（118 个契约命令）与
`docs/guides/kag-language-tour.md`。

### 3. 跑起来验证

```bash
# 全链路验证：ks_check 零警告 + headless DONE
bash scripts/verify_template.sh

# 只跑 headless 冒烟（跑到 [end] 即成功）
SAMPLE_STORY="demo/template/story.ks" \\
  external/lua/lua.exe tests/scripts/sample_game_headless.lua
```

> 缺资产不炸引擎：没有真实图片/音频时按**安全降级**显示占位（开发紫 / 发布灰），
> 详见 `assets/README.md` 与 `docs/guides/asset-pipeline.md`。

### 4. 打包成可发布站点

```bash
# 一键打包到 dist/my_game/（web player + 剧本 bundle + 资产 + manifest）
bash scripts/package_game.sh --out dist/my_game demo/template

# 本地预览
cd dist/my_game && python -m http.server 8080
# 浏览器打开 http://localhost:8080
```

打包产物是自包含静态站：直接传 GitHub Pages / itch.io / Netlify / S3。
> 打包默认带仓库共享 `assets/` 池；独立项目请用 `--assets <你的资产根>` 指定自己的资产树。

### 5. 发布

- **网页**：dist 目录拖到任意静态托管。
- **桌面**：`bash scripts/package_game.sh --release demo/template`（打印 CPack 交接，
  详见 `docs/guides/release-process.md`）。
- 发布前把模板标题/占位台词换成你的作品名（`story.ks` 的 `*credits` 段）。

---

## 结构

```
demo/template/
├── README.md     # 本文档：五步启动指南
├── entry.lua     # KAG runner 启动入口（仿 demo/example_game/entry.lua）
├── story.ks      # 最小剧本：两场景 + 一次选择 + 片尾
└── assets/       # 资产骨架占位
    ├── bg/       #   背景（背景图占位说明）
    ├── fg/       #   立绘（角色立绘占位说明）
    ├── bgm/      #   BGM（背景音乐占位说明）
    ├── se/       #   音效（点击等占位说明）
    └── voice/    #   语音（角色语音占位说明）
```

## 相关文档

| 文档 | 内容 |
|------|------|
| `docs/guides/template-quickstart.md` | 模板使用完整指南（含 GitHub template repo 启用） |
| `docs/guides/getting-started.md` | 引擎从零构建与运行 |
| `docs/guides/asset-pipeline.md` | 资产格式与目录规范、缺失降级 |
| `docs/guides/kag-language-tour.md` | KAG Neo-Genesis 语言速览 |
| `docs/guides/sample-game-verification.md` | 示例游戏双端验证方法 |
| `demo/example_game/` | 完整示例（三结局、i18n、SMA、画廊） |
