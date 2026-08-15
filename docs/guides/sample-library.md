# Caesura 示例库（Sample Library）

> 示例库 = 可直接运行/参考的剧本 + 资产 + 教程。每个示例都经过
> 引擎与 Web 播放器双重验证（门禁：Lua 套件 + web flow 集成测试）。

## 已收录示例

| 示例 | 文件 | 覆盖命令 | 验证 |
|---|---|---|---|
| Command Showcase | demo/showcase.ks | **25 个命令**：bg/fg/ch/cl/font/pt/wait/playbgm/playse/playvoice/p/position/flash/trans/vib/sprite_move/sprite_fade/set/if/else/endif/jump/scroll/stopbgm/ending/end | 引擎 tokenize/compile（56 tokens/6 labels）+ Web 播放器 DONE:53 + backlog 9 页 + ending 解锁 |
| Galgame Demo | demo/galgame_demo.ks | 核心 VN 流程（bg/ch/playbgm/voice/sprite/ending） | Web flow 集成测试（park/点击/DONE/hana bg/立绘） |
| Full Pipeline | demo/full_pipeline_demo.ks | 全管线流程 | ks_bake bundle |
| SMA Demo | demo/sma_demo.ks | SMA 骨骼动画命令 | ks_bake bundle |

## 如何运行示例

### 引擎（桌面）

```bash
cmake --build build --config Debug
./build/Debug/CaesuraAmeKAG.exe
# 或在编辑器里打开 demo/showcase.ks
```

### Web 播放器

```bash
cd web
npm install
npx vite            # http://127.0.0.1:5174
# 场景下拉框选择 showcase.ks，▶ Run，点击推进 / ⏩ Auto
```

Web 播放器优先加载 ks_bake 预编译 bundle（cache/story/story.lua，零解析启动），
bundle 由 `lua scripts/ks_bake.lua --dir demo --web cache/story` 生成（需重新生成
以包含新示例）。

## 如何贡献示例

1. 在 demo/ 下写 `my_sample.ks`（KAG 命令见 docs/api/command-contracts.md）
2. 引擎侧验证：`external/lua/lua.exe scripts/ks_bake.lua demo/my_sample.ks`（tokenize/compile 通过）
3. 更新 web/flow.integration.test.js 加用例（可复用 showcase 测试骨架）
4. 重新生成 bundle：`lua scripts/ks_bake.lua --dir demo --web cache/story`
5. 全量门禁：Lua 套件 + web vitest + C++ 套件

## 覆盖矩阵（showcase.ks 命令）

| 类别 | 命令 |
|---|---|
| 文本 | font / pt / ch / p / scroll |
| 图层 | bg / fg / cl / position / sprite_move / sprite_fade |
| 音频 | playbgm / playse / playvoice / stopbgm |
| 特效 | flash / trans / vib |
| 流程 | wait / if / else / endif / jump / set / ending / end |

---
*示例库 round 41-42 建设（G6 起点）*
