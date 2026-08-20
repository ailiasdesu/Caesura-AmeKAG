# Sample Game Verification（示例游戏双端验证）

> 目标：为样例游戏 `demo/example_game/story.ks` 提供端到端验证——`跑通 DONE + 零错误`，不写死剧情内容断言。

## 验证脚本

| 脚本 | 作用 |
|------|------|
| `scripts/verify_sample_game.sh` | 一键验证入口（bash，正斜杠路径），输出 PASS/FAIL |
| `tests/scripts/sample_game_headless.lua` | headless 驱动：mock C++ 绑定 + 自动点击，把 .ks 跑到 `[end]` |

## 验证流程

从仓库根目录运行：

```bash
bash scripts/verify_sample_game.sh
```

脚本依次执行：

1. **静态契约校验** `lua scripts/ks_check.lua demo/example_game/story.ks`
   - 目标：零警告（lint 警告不算 CI 门禁，仅提示）。
2. **headless 全流程** `sample_game_headless.lua`
   - mock 掉 C++ 绑定（`KAG.*`/`Render.*`/`DevCore.*`/`engine.*`），`kag_runner` 逐 token 推进，自动点击 `[p]` 等待与 `[select]` 首选项。
   - 断言：脚本跑到 `[end]`（返回 `RESULT DONE`），全程零错误；帧上限保护（默认 200000 帧）。
3. **三结局可达性**（`SAMPLE_ENDING` 环境变量）
   - 对 `ending_zero` / `ending_companion` / `ending_promise` 各跑一次：启动时 `stage_label_jump` 跳到对应 `*ending_*` 标签，跑到 `[end]`。
   - 每个结局分支尾随 `[stopbgm] -> [jump *credits] -> [end]`，能到 `[end]` 即证明该分支可读。
4. **Web 冒烟**（信息性，不执行）——见下文手动指引。

单步运行（便于调试）：

```bash
# 只跑静态校验
external/lua/lua.exe scripts/ks_check.lua demo/example_game/story.ks

# 只跑 headless 全流程
external/lua/lua.exe tests/scripts/sample_game_headless.lua

# 只验证某个结局
SAMPLE_ENDING=ending_promise external/lua/lua.exe tests/scripts/sample_game_headless.lua
```

## headless 驱动说明

- **Mock 关键点**：`KAG.is_voice_playing` / `is_bgm_playing` 必须返回 `false`，否则 `[playvoice]`/`[playbgm]` 的等待循环会无限挂起（它们轮询 mock 判断音频是否播完）。
- **自动点击策略**：每帧 `kag_runner.update(0.016)`；当 `ctx.waiting_input` 或 `_choiceMode` 为真时触发点击：
  - 普通文本：`kag_runner.on_click()` 完成打字机显示 + 越过 `[p]`。
  - `[select]`/`[endbutton]`：把鼠标移到第一个可见选项并调用 `_G._KAG_onClick()` 选中（兜底直接强制 `_selectedChoice = 第一个`）。
- **帧上限**：超过 `SAMPLE_FRAMES`（默认 200000）未到 `[end]` 判失败——防止死循环拖住 CI。

## 三结局可达性方案

草稿期场景 4/5 与结局分支仍是 stub（合法 `[ch]` 文本），结局标签 `*ending_zero` / `*ending_companion` / `*ending_promise` 尚未被场景 5 的最终选择接线。因此当前方案：

- **主路径**：自然顺序走下去会经 场景 5 stub → 落入 `*ending_zero` → `*credits` → `[end]`，因此主路径已覆盖一个结局。
- **其余两个结局**：启动时用 `SAMPLE_ENDING` 让驱动 `stage_label_jump` 跳到对应标签，从该分支头部跑到 `[end]`，证明该结局分支可读。
- **后续完整化**：当场景 5 加上定时选择并分别 `[jump]` 到三结局后，可把 `[select]` 的三个 `sel` 通过 `SAMPLE_CHOICE_INDEX` 驱动（当前驱动默认选第一个），即可不用标签跳转、纯选择路径跑全三结局。

## Web 端手动冒烟指引

Web 播放器跑的是 `ks_bake --dir demo --web cache/story` 生成的打包场景（对 `demo/**/*.ks`），**不含** `story.ks`（扩展名非 `.ks`）。要手动验证草稿的 Web 端表现：

1. 生成 bundle：`external/lua/lua.exe scripts/ks_bake.lua --dir demo --web cache/story`
   - 仅覆盖发货用的 `demo/example_game/story.ks`。
2. 若需在浏览器里跑 `story.ks`：将其临时并入 bundle 场景集（或单独 bake），再跑 `web/story.bundle.sweep.test.js`，确认 `runFromBundle` 驱动该场景到 `DONE` 且零 error 事件。
3. 不跑完整 vitest 时，可直接在 `web/main.mjs` 打开自动播放（autoClick）从场景首帧看到 `[end]` 结束即可。

> 说明：`story.ks` 已定稿（round 102 由 `story.ks.new` 改名），
> verify_sample_game.sh 默认路径即 `demo/example_game/story.ks`（round 110
> 已修复默认值），可直接裸跑 `bash scripts/verify_sample_game.sh`。
