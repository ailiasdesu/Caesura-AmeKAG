# First-VN E2E Project (tests/projects/first_vn/)

> **完整用户创作流程验收夹具**——产品化总任务书（§6 First-VN 全流程验收）。
> 与 `golden_vn` 职责不同：**golden = Runtime 全 feature 面回归**，
> **first_vn = 一个新作者从模板到成品的完整创作流程验收**
> （template → project → script → assets → headless run → choices →
> save/load → package → packaged launch）。

## 故事结构

| 场景 | 内容 |
|---|---|
| `*start`（Scene 1） | 背景 + BGM + 对白 + 角色（Aina）+ SE + i18n 热切换（en/ja/zh）+ 自动存档（slot 7） |
| `*choice_moment` | 玩家二选一：`*branch_sun` / `*branch_rain` |
| `*branch_sun` / `*branch_rain` | 各自写入 `f.route` 变量、专属对白，汇合到 `*ending` |
| `*ending`（Scene 2） | 表达式条件文本（按分支）、空槽 `[load slot=8]` 优雅处理、收尾对白、`[end]` |

## 运行

```bash
# 直接跑（需真实 GPU 窗口；仓库根执行）
lua tests/projects/first_vn/entry.lua

# 完整验收门禁（13 项 PASS，见 scripts/verify_first_vn.sh）
bash scripts/verify_first_vn.sh

# 单项快速检查
external/lua/lua.exe scripts/ks_check.lua tests/projects/first_vn/story.ks
SAMPLE_STORY=tests/projects/first_vn/story.ks external/lua/lua.exe tests/scripts/sample_game_headless.lua
```

## assets/ —— 最小资源包（自包含）

故事引用共享池风格路径（`assets/bg|fg|bgm|se/...`），与 basic 模板 /
golden_vn 一致；本目录放置同名镜像文件，使项目拷出仓库后仍可独立运行：

| 文件 | 来源 | 说明 |
|---|---|---|
| `assets/bg/classroom.png` | 复制自 `assets/bg/classroom.png`（demo 同款教室图） | 唯一背景 |
| `assets/fg/girl_uniform.png` | 复制自 `assets/fg/girl_uniform.png` | 角色 Aina 立绘 |
| `assets/bgm/daily.wav` | 复制自 `tests/audio/silence.wav` | BGM 占位（8.8KB 静音 wav，保持文件名与共享池一致） |
| `assets/se/click.wav` | 复制自 `tests/audio/silence.wav` | SE 占位（同上） |

> 正式作品请用真实音频替换同名文件；静音占位让测试项目保持极小体积。

## save/load 的如实标注

- `[save slot=7]`：主路径真实存档，headless 下走 mock 绑定返回成功。
- `[load slot=8]`：**故意读空槽**。headless 无 C++ SaveManager，load 走
  “miss 后优雅继续”路径——验证的是 load 命令全链路不破坏流程；
  **真实的存档往返（round-trip restore）由 golden_vn 与 C++
  SaveManager 测试套件保证**（若在此做真往返，恢复的 token 游标会让
  自动化驱动无限回放该段，属于已知死等坑）。

## 打包

```bash
bash scripts/package_game.sh --out dist/first_vn tests/projects/first_vn/story.ks
```

默认随包发布共享池 `assets/`（故事引用的四个路径全部命中）；产物
`dist/first_vn/index.html` + `cache/story/story.lua` + `MANIFEST.txt`。
真实浏览器启动属 §9 Web 验证任务（manual）。
