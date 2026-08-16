# Caesura (AmeKAG) — Packaging UX: 一键打包指南

> 内容作者视角：从「写好 .ks」到「可分发产物」只需要一条命令。
>
> `bash scripts/package_game.sh <你的游戏目录 或 .ks>`

打包产物是 **静态 Web 播放器站点**（无后端、无需额外运行时），可上传到任何静态托管：
**itch.io** / **GitHub Pages** / **Netlify** / **S3 / 任意对象存储**，或本地 `python -m http.server`。

---

## 1. 工作流总览

```
写 .ks --> bash scripts/package_game.sh <game> --> dist/<game>/ (可分发静态站)
                 |                                          |
                 +-- ks_check 契约校验（失败即停）          +-- cache/story/story.lua(编译好的剧本包)
                 +-- ks_bake --web 生成 story bundle        +-- index.html + web-assets(播放器壳)
                 +-- 复制 web 播放器 + 运行时 + 资产         +-- demo/<game>/*.ks(兜底源码)
                 +-- 输出 MANIFEST.txt（文件树+大小）        +-- assets/(游戏资产)
                                                             +-- scripts/(引擎运行时)
```

## 2. 最小路径

从仓库根目录（git bash）：

```bash
# 打包 demo/example_game（默认输入就是它）
bash scripts/package_game.sh

# 或显式指定
bash scripts/package_game.sh demo/example_game

# 打包目录里的全部 .ks
bash scripts/package_game.sh demo/tutorial

# 打包单个场景
bash scripts/package_game.sh my_game/scene.ks
```

产物在 `dist/<game>/`，例如 `dist/example_game/`。

## 3. 本地试玩

```bash
cd dist/example_game
python -m http.server 8080
# 浏览器打开 http://127.0.0.1:8080 ，右上角下拉框选你的场景 -> Run
```

## 4. 分发渠道

### itch.io
1. 登录 itch.io -> 上传新项目 -> 平台选 **HTML**。
2. 把 `dist/<game>/` 的**内容**压缩成 zip（zip 顶层就要是 index.html，不要多套一层文件夹）。
3. 上传 zip，填写页面信息。itch.io 会把它当作网页游戏托管。

### GitHub Pages
1. 把 `dist/<game>/` 内容推送到任意仓库的 gh-pages 分支，或 CI 上传为 Pages 站点。
2. 访问 `https://<user>.github.io/<repo>/`。

### Netlify / Vercel / S3
直接拖拽/上传 `dist/<game>/` 目录即可（纯静态，无需 SPA 回退规则）。

### 桌面 Release（可选，需完整引擎构建）
```bash
bash scripts/package_game.sh --release demo/example_game
```
脚本会在打包后打印 CPack 桌面分发命令模板；完整流程见 `docs/guides/release-process.md`：

```bash
cmake --build build --config Release --parallel
cd build && cpack -C Release -G ZIP && cd ..
git tag -a vX.Y.Z -m "Caesura (AmeKAG) vX.Y.Z" && git push origin vX.Y.Z
gh release create vX.Y.Z build/CaesuraAmeKAG-*-Windows-AMD64.zip --title "..." \
  --notes-file CHANGELOG.md --draft
```

## 5. 脚本选项

| 选项 | 默认 | 作用 |
|---|---|---|
| 位置参数 | `demo/example_game` | 游戏目录（收集其下全部 .ks）或单个 .ks 路径 |
| `--out <dir>` | `dist/<game-name>` | 产物目录 |
| `--assets <dir>` | `assets` | 随包分发的资产根（仓库共享资产池） |
| `--no-web-build` | 自动构建 | 复用已有 web/dist，跳过 vite 重建 |
| `--release` | 关 | 打包成功后打印桌面 CPack 交接命令 |
| `--entry <scene>` | 无 | 在 MANIFEST.txt 记录推荐入口场景 |

示例：

```bash
bash scripts/package_game.sh --assets my_game/assets --entry opening.ks demo/example_game
bash scripts/package_game.sh --no-web-build demo/tutorial
```

## 6. 打包做了什么（阶段）

| 阶段 | 命令 | 说明 |
|---|---|---|
| 1. 契约校验 | ks_check.lua | 每个 .ks 通过 KAG Neo-Genesis 命令契约；任一失败即停 |
| 2. 剧本预编译 | ks_bake.lua --web | 把场景编译成 cache/story/story.lua（播放器零解析零编译直接跑） |
| 3. 组装 | - | 复制 web/dist（index.html + web-assets）+ scripts/ + 资产 + 场景源码 |
| 4. 清单 | - | 生成 MANIFEST.txt：时间、场景数、逐文件大小、总大小 |
| 5. (可选) Release 交接 | - | --release 时打印 CPack 命令模板 |

`web/dist` 由 `web/` 的 `npm run build`（即 `vite build`）产出，插件会把运行时目录
（`scripts`/`demo`/`assets`/`cache/story`）一并复制进去；脚本随后用**游戏专属**的 story bundle
覆盖 `cache/story/story.lua`，确保产物只含你的游戏（而不是整个 demo 集）。

## 7. 资产约定

- 默认 `--assets assets` 把仓库共享资产池整包带走（安全省心，代价是体积较大）。
- 若你的游戏有独立资产目录，用 `--assets <你的目录>`，脚本会把它复制为 `assets/`。
- 产物大小看 `MANIFEST.txt` 末尾的 `total KB`，据此判断是否值得裁掉未引用的资产。

## 8. 常见问题

**Q：为什么产物里没有我新建的 scene？**
A：确认该 .ks 在传入输入下（`find <dir> -name '*.ks'`），且 ks_check 通过。脚本只打包输入范围内的场景。

**Q：播放器默认跑的是 galgame_demo.ks，不是我的游戏。**
A：这是播放器启动的默认场景名；在打包产物的下拉框里选你自己的场景再点 Run 即可。`--entry` 会在清单里标注入口场景。

**Q：web/dist 没有 / 构建失败？**
A：先 `cd web && npm install && npm run build` 一次。脚本检测到 node_modules 会自动重建，缺失时优雅回退到已有产物并提示命令。

**Q：打包后点 Run 没反应？**
A：务必用 HTTP 服务访问（`python -m http.server`）；直接双击 index.html（file://）时浏览器会拦截 fetch 与模块加载。

## 9. 相关文档

- `docs/guides/release-process.md` — 桌面 Release + CPack + gh release 全流程
- `docs/guides/sample-game-verification.md` — 示例游戏端到端验证
- `web/package.json` / `web/vite.config.js` — Web 播放器构建配置
- `scripts/ks_bake.lua` — story bundle 烘焙逻辑（--web 模式）
