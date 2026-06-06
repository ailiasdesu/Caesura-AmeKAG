---
title: "Caesura G6: Alpha 完成冲刺 — KAG 标签补全 + 关键修复"
type: feat
status: active
date: 2026-06-06
origin: docs/Caesura_功能实现规格说明书_整合版.md
---

## Summary

补齐 Caesura 引擎从当前 85% 到 Alpha 可发布级别的剩余缺口：3 个阻塞性 KAG 标签（`[position]`、`[current]`、`[history]`）、FFmpeg 视频解码、dev_mode 配置链路、存档版本管理，以及 6 个重要但非阻塞的 KAG 标签。完成后引擎应能运行完整的商业 AVG 剧本。

## Problem Frame

G0-G5 审计暴露了当前引擎的核心缺口：

- **无法精确放置立绘** — 缺 `[position]`，立绘只能居中，无法适配标准 AVG 布局（左/中/右站位）
- **存档恢复文本位置丢失** — 缺 `[current]`，读档后文字排版不确定
- **玩家无法回溯历史** — Backlog 存储层已完成，但缺 `[history]` 显示界面
- **视频格式单一** — 仅 MPEG1，无法播放现代 AVG 使用的 H.264/VP9
- **沙箱状态不确定** — `dev_mode` 配置未接入 Sandbox 判定链路
- **未来存档崩溃风险** — 无 `engine_version` 字段

## Requirements

### R1: 阻塞性 KAG 标签（Alpha 必须）

- **R1.1 `[position]`** — 支持 `layer`、`x`、`y`、`scale` 参数，将立绘/图层放置到屏幕任意位置。x/y 支持像素值和 NDC 归一化坐标
- **R1.2 `[current]`** — 在 save 时捕获文本渲染位置（当前行、字符偏移、ruby 状态），load 时恢复
- **R1.3 `[history]`** — 显示 Backlog 界面：滚动列表、语音回放按钮、跳转至对应对话点

### R2: 关键基础设施修复

- **R2.1 FFmpeg 集成** — 替换 pl_mpeg，支持 H.264/VP9/VP8。保持现有 VideoPlayer API 不变，内部切换解码器
- **R2.2 dev_mode 配置链路** — config.lua 增加 `dev_mode` 字段，Engine 启动时写入 `_CAESURA_CONFIG.dev_mode`，Sandbox.is_strict() 据此判���
- **R2.3 存档版本管理** — save JSON 增加 `engine_version`、添加 `SaveManager::migrateSave()` 版本迁移入口

### R3: 重要 KAG 标签（增强表现力）

- **R3.1 `[ruby]`** — 振假名/注音，支持 `text` + `ruby` 双参数
- **R3.2 `[font]`** — 运行时字体切换，参数 `face`、`size`、`color`
- **R3.3 `[layopt]`** — 图层选项：`opacity`、`blend`、`visible`
- **R3.4 `[skip]`** — 跳过模式，快速推进已读文本
- **R3.5 `[reset]`** — 重置文本层状态（字体/颜色/缩进）
- **R3.6 `[pt]`** — 逐字显示速度控制（打字机效果间隔）

### R4: 端到端验证

- **R4.1** 编写完整测试剧本（标题→选项分支→多场景跳转→存档→读档→结束）
- **R4.2** 在构建的引擎上运行该剧本，所有 KAG 标签无崩溃

## Key Technical Decisions

- **KTD1: `[position]` 使用 NDC 归一化坐标** — x/y 范围 [0,1] 映射到屏幕宽高，与 bgfx 正交投影一致。像素坐标通过 `unit="px"` 参数支持。选择 NDC 因为与底层渲染系统对齐，跨分辨率无额外计算
- **KTD2: `[current]` 存储为 ctx 子表** — `ctx.text_state = {line, char_offset, ruby, font, color}` 而非在 save JSON 中内联。选择子表因为文本状态变更频繁，独立管理避免序列化膨胀
- **KTD3: `[history]` 复用现有 Backlog 存储** — system.lua 已有 `push_backlog` + `capture_snapshot`，只需加 Lua 渲染界面（纯 Lua UI 层，通过 backend 绘制半透明覆盖层 + 文本列表）。不引入 C++ UI 框架
- **KTD4: FFmpeg 使用 libavcodec 子集** — 仅链接 avcodec + avformat + swscale，通过 -DCAESURA_VIDEO_FFMPEG=ON 编译开关。保持 pl_mpeg 作为 fallback（-DCAESURA_VIDEO_FFMPEG=OFF）
- **KTD5: 存档迁移使用递增版本号 + 迁移函数链** — version=1 到 version=2 调用 migrate_v1_to_v2()，链式执行到当前版本。避免大版本跳跃时的手动修复

## Implementation Units

### U1: `[position]` + `[layopt]` — 图层精确定位 (@p0)

**Files:** `scripts/kag/commands/layer.lua`, `scripts/layers.lua`

Layer 命令模块增加 `[position]` 和 `[layopt]`：
- `position` handler 解析 `x`、`y`、`layer`、`scale`、`unit` 参数
- NDC 默认坐标系，`unit="px"` 时切换像素
- 调用 `layers.set_position(layer_name, x, y, scale)`
- `layopt` handler 设置 `opacity`、`visible`、`blend`

### U2: `[current]` — 文本位置追踪 (@p0)

**Files:** `scripts/kag/commands/text.lua`, `scripts/system.lua`

- 每次 `[ch]`/`[text]` 渲染后更新 `ctx.text_state`
- `System.save()` 序列化 text_state 到 save JSON
- `System.load()` 恢复 text_state 并调用 `layers.restore_text_state()`

### U3: `[history]` — Backlog 显示界面 (@p0)

**Files:** `scripts/kag/commands/system.lua`, `scripts/history_ui.lua` (新建)

- Lua 侧渲染：半透明背景 + 滚动列表 + 选中高亮
- 条目显示：角色名 + 文本预览（截断到 60 字）+ 语音回放按钮
- 键盘/鼠标导航：上下滚动、回车跳转、Esc 退出
- 跳转逻辑：使用 `[jump]` 机制回到选定对话点的标签

### U4: FFmpeg 视频解码 (@p1)

**Files:** `src/Render/VideoPlayer.cpp`, `CMakeLists.txt`

- 在 VideoPlayer 内部增加 `#ifdef CAESURA_VIDEO_FFMPEG` 路径
- `open()` 检测文件扩展名：.mpg/.mpeg → pl_mpeg，其余 → FFmpeg
- 链接 avcodec/avformat/swscale，解码到 RGBA8 texture
- PTS 同步维持现有逻辑

### U5: dev_mode 配置链路 (@p1)

**Files:** `scripts/config.lua`, `src/main.cpp`

- config.lua 增加 `dev_mode = true`（默认 Debug 构建为 true）
- main.cpp 在 `lockdownScriptEnv()` 前写入 `_CAESURA_CONFIG.dev_mode`
- Sandbox.is_strict() 读取链路自动生效

### U6: 存档版本管理 (@p1)

**Files:** `src/System/SaveManager.cpp`, `scripts/system.lua`

- save JSON 根增加 `"engine_version": "1.0.0"`
- SaveManager 加载时检查版本，不匹配则尝试迁移
- 当前版本仅记录，迁移框架预留（未来版本间迁移在此扩展）

### U7: `[ruby]` + `[font]` + `[skip]` + `[reset]` + `[pt]` (@p2)

**Files:** `scripts/kag/commands/text.lua`, `scripts/kag/commands/system.lua`

- `[ruby]` — text="漢字" ruby="かんじ"，TextRenderer 上层渲染小字注音
- `[font]` — face="Noto Serif" size=28 color="#333"，切换 FreeType face
- `[skip]` — 设置 `ctx.skip_mode`，scheduler 快速推进，不等待点击
- `[reset]` — 重置 font/color/indent 到默认值
- `[pt]` — speed 参数（ms/字），text 命令内部分批 coroutine.yield

### U8: 端到端集成验证 (@p2)

**Files:** `tests/scripts/full_story.ks` (新建)

- 完整 AVG 测试剧本：标题→选择支→3个场景→存档→读档→CG收集→结束
- 覆盖所有 P0+P1 KAG 标签
- `external/lua/lua.exe tests/test_integration.lua` 扩展验证

## Execution Order

```
U1 [position] ──→ U2 [current] ──→ U3 [history]
                                        │
U4 FFmpeg ──────────────────────────────┤
                                        │
U5 dev_mode ──→ U6 save-version ────────┤
                                        │
                              U7 remaining tags
                                        │
                              U8 end-to-end test
```

优先完成 U1-U3（阻塞性标签），然后 U4-U6（基础设施），最后 U7-U8（增强+验证）。
