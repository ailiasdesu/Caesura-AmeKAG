# Project Template Quickstart（新项目模板使用指南）

> 从零起步做一部视觉小说：用 `demo/template/` 作为你的项目骨架。
> 它是最小可运行的「两场景 + 一次选择」脚本，配合资产占位、入口启动、
> 契约校验、headless 冒烟与一键打包全链路。

---

## 1. 模板在哪

`demo/template/`，与完整示例 `demo/example_game/` 并列：

```
demo/template/
├── README.md     # 五步启动指南（面向内容作者）
├── entry.lua     # KAG runner 启动入口
├── story.ks      # 最小剧本：两场景 + 一次选择 + 片尾
└── assets/       # 资产骨架占位（bg/fg/bgm/se/voice + README）
```

配套验证脚本：`scripts/verify_template.sh`。

## 2. 五步从模板到你的游戏

### 2.1 拷贝为新项目

```bash
cp -r demo/template my_game
```

> 想发布成 **GitHub 模板仓库**（别人一键 "Use this template" 起步）？见下文 §4。

### 2.2 写场景

编辑 `my_game/story.ks`。最小骨架已含：

```ks
*start                 ; 开场（Scene 1）
[bg storage="assets/bg/classroom.png"]
[ch name="Aina" text="Hello!"]
[p]
[select]               ; 一次玩家选择
[sel target=*forest text="Forest"]
[sel target=*city  text="City"]
[endselect]

*forest                ; Scene 2a
[jump *credits]
*city                  ; Scene 2b
[jump *credits]
*credits               ; 片尾 -> [end]
[end]
```

命令全集见 `docs/api/command-contracts.md`（118 契约命令）；语言速览见
`docs/guides/kag-language-tour.md`。

### 2.3 跑起来 & 校验

```bash
lua my_game/entry.lua                          # 打开窗口跑
lua scripts/ks_check.lua my_game/story.ks      # 契约校验（目标：零警告）
bash scripts/verify_template.sh                # 全链路：零警告 + headless DONE
```

> 想校验自己改的剧本，告诉脚本用哪个 story：
> `TEMPLATE_STORY=my_game/story.ks bash scripts/verify_template.sh`

### 2.4 打包

```bash
bash scripts/package_game.sh --out dist/my_game --assets my_assets my_game
```

- 不传 `--assets` 时默认带仓库共享 `assets/` 池。
- 独立项目请把资产放你自己树里并 `--assets <你的资产根>` 指定。

### 2.5 发布

- **网页**：dist 目录推到 GitHub Pages / itch.io / Netlify / S3。
- **桌面**：`bash scripts/package_game.sh --release my_game`（CPack 交接，
  见 `docs/guides/release-process.md`）。

## 3. 资产占位与缺失降级

模板 `assets/` 带子目录骨架（bg/fg/bgm/se/voice）与每个目录的 README。
引擎**不因缺资产而崩溃**：缺图显示占位纹理（开发紫/发布灰），缺音频静音且
相关等待命令不死等。所以你可以**先写剧本、后补资产**。详见
`docs/guides/asset-pipeline.md` 与 `demo/template/assets/README.md`。

> 模板 `story.ks` 默认引用仓库共享资产池（克隆即可跑）。独立项目把资产拷进
> 自己的树，并把 `storage=".../..."` 改成相对你自己项目的路径即可。

## 4. 启用 GitHub Template repository（模板仓库）

把本项目（或你的新项目）设为 **template repository**，社区就可以一键
"Use this template" 从它建自己的仓库起步：

1. 登录 GitHub，打开目标仓库（如 `<you>/my_game`）。
2. 进入 **Settings** → 左侧 **General**。
3. 滚动到 **Danger Zone**（危险区）→ **Template repository** → 点
   **Turn this repository into a template**。
4. 确认后，仓库页右上角的 "Use this template" 绿色按钮即刻可用。

完成后每个访问者都能点击 **Use this template → Create a new repository**，
得到一个带了全部骨架（`story.ks`/`entry.lua`/`assets/` 占位）的空项目实例。

### 模板仓库的最佳实践

- 把示例标题/占位台词写成模板占位符（如 `YOUR GAME TITLE`），让作者一眼看出要替换。
- 随模板附 `README.md` 的「五步」与资产占位说明（已内置）。
- 模板只放通用骨架与引擎用法示例，**不放**你的具体剧情资产（避免把别人的作品卷进来）。

## 5. 反馈与延伸

| 想做什么 | 看哪 |
|---------|------|
| 多结局、i18n、SMA、画廊 | `demo/example_game/` + `docs/guides/sample-game-verification.md` |
| 资产格式与目录规范 | `docs/guides/asset-pipeline.md` |
| 完整引擎构建 | `docs/guides/getting-started.md` |
| 打包与发布 | `docs/guides/release-process.md`、`docs/guides/packaging-ux.md` |
