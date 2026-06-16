# Caesura P1-P3 游戏逻辑执行计划

> 基于 `docs/brainstorms/2026-06-16-game-logic-requirements.md`
> 2026-06-16 · 分支：新建 `codex/game-logic-p1-p3`

---

## 概述

目标：在 16 模块引擎审查完成的基础上，逐步完善 galgame 引擎的玩家可感知功能。P1 场景系统为基础层，P2 附加功能为外部特性，P3 3D 小游戏为独立子系统。三个优先级可串行执行。

---

## P1：场景系统

### U1.1：`[unlock]` 命令 — 画廊/音乐室数据入口

- **Goal**：在 `scripts/kag/commands/system.lua` 添加 `SystemCommands.unlock`，向 `ctx.unlockedCG` 表添加条目，作为 `[unlock type="cg" id="scene01"]` 的 KAG 命令处理
- **Modify**：`scripts/kag/commands/system.lua`
- **Test**：`tests/cpp/test_kag_execution.cpp` — 新增 `KAG: unlock cg command` 测试用例
- **Patterns**：参考已有 `SystemCommands.history` 的实现模式
- **Execution note**：test-first — 先写测试验证 unlock 未实现时返回 nil，再实现后验证 ctx.unlockedCG 表被更新
- **Verification**：`.\CaesuraTests.exe -tc="*unlock*"` 通过

### U1.2：`[saveplace]` / `[loadplace]` — 场景内书签

- **Goal**：完善 `scripts/system.lua` 中的 `System.saveplace`/`System.loadplace`，保存/恢复 `ctx.token_index` + `ctx.text_state` + `ctx.labels`
- **Modify**：`scripts/system.lua`
- **Test**：`tests/cpp/test_kag_execution.cpp` — 新增 `KAG: saveplace and loadplace roundtrip` 测试
- **Verification**：`.\CaesuraTests.exe -tc="*saveplace*"` 通过

### U1.3：`[ch]` 立绘多角色管理

- **Goal**：`scripts/kag/commands/text.lua` 的 `TextCommands.ch` 支持 `pos` 参数（left/center/right），管理 `ctx.characters` 表存储多角色状态
- **Modify**：`scripts/kag/commands/text.lua`
- **Test**：`tests/cpp/test_kag_execution.cpp` — 新增 `KAG: ch command with position` 测试
- **Verification**：`.\CaesuraTests.exe -tc="*ch*command*"` 通过

### U1.4：语音重播回调

- **Goal**：`scripts/history_ui.lua` 的 `[V]` 按键触发 `backend.audio_play("voice", entry.voiceFile)` 实际播放存档的语音
- **Modify**：`scripts/history_ui.lua`
- **Test**：纯 Lua 测试 — 新增 `tests/scripts/test_history.lua`
- **Verification**：`external\lua\lua.exe tests/scripts/test_history.lua` 通过

### U1.5：macro 宏录制/展开

- **Goal**：scheduler 中 `[macro]` 和 `[endmacro]` 之间捕获 token 序列存入 `ctx.macros[name]`，`[erasemacro]` 清除
- **Modify**：`scripts/scheduler.lua`、`scripts/kag.lua`
- **Test**：`tests/cpp/test_kag_execution.cpp` — 新增 `KAG: macro record and erase` 测试
- **Verification**：`.\CaesuraTests.exe -tc="*macro*"` 通过

---

## P2：附加功能

### U2.1：gallery 解锁条件触发器

- **Goal**：`scripts/gallery.lua` 的 `Gallery.isUnlocked` 读取 `ctx.unlockedCG`（由 U1.1 写入），在画廊入口过滤未解锁 CG
- **Modify**：`scripts/gallery.lua`
- **Test**：纯 Lua 测试 — 新增 `tests/scripts/test_gallery.lua`
- **Verification**：`external\lua\lua.exe tests/scripts/test_gallery.lua` 通过

### U2.2：music_room 收藏持久化

- **Goal**：`scripts/music_room.lua` 的 favorites 在 engine 重启后从 `config/music_room.lua` 恢复
- **Modify**：`scripts/music_room.lua`
- **Test**：纯 Lua 测试 — 新增 `tests/scripts/test_music_room.lua`
- **Verification**：`external\lua\lua.exe tests/scripts/test_music_room.lua` 通过

### U2.3：palette 日/夜模式

- **Goal**：`scripts/palette.lua` 已有完整 LUT load/apply 框架。添加 `palette.set_day_mode()` / `palette.set_night_mode()` 切换两个预置 LUT
- **Modify**：`scripts/palette.lua`
- **Test**：纯 Lua 测试 — 新增 `tests/scripts/test_palette.lua`
- **Verification**：`external\lua\lua.exe tests/scripts/test_palette.lua` 通过

### U2.4：i18n 字符串表

- **Goal**：`scripts/i18n.lua` 已有 `{key}` 替换。补充 `assets/lang/zh.lua` 中文字符串表，验证 `_T("start")` 返回翻译
- **Modify**：`scripts/i18n.lua`、新建 `assets/lang/zh.lua`
- **Test**：纯 Lua 测试 — 新增 `tests/scripts/test_i18n.lua`
- **Verification**：`external\lua\lua.exe tests/scripts/test_i18n.lua` 通过

---

## P3：3D 小游戏

### U3.1：Basic 3D demo — 立方体渲染

- **Goal**：在 `BgfxMiniGameBackend` 中硬编码一个旋转立方体（无需 GLTF 加载器），验证 bgfx 渲染管线与新模块的交互
- **Modify**：`src/minigame/BgfxMiniGameBackend.cpp`、`src/minigame/MiniGeometry.cpp`
- **Test**：`tests/cpp/test_minigame.cpp` — 新增 `BgfxMiniGameBackend: renders cube geometry without GPU`（构造性测试）
- **Verification**：`.\CaesuraTests.exe -tc="*cube*"` 通过

### U3.2：碰撞检测集成测试

- **Goal**：`MiniCollision` 的 AABB-vs-AABB 检测与 `BgfxMiniGameBackend::checkCollision` 连接
- **Files**：`src/minigame/MiniCollision.cpp`、`tests/cpp/test_mini_game.cpp`
- **Test**：扩展现有 test_mini_game.cpp — 新增两个碰撞盒的检测测试
- **Verification**：`.\CaesuraTests.exe -tc="*collision*"` 通过

### U3.3：键盘输入映射

- **Goal**：Lua 侧通过 `scripts/demo_minigame.lua` 读取 WASD 键状态，调用 backend 移动 3D 摄像机
- **Modify**：`scripts/demo_minigame.lua`
- **Test**：纯 Lua 测试 — 新增 `tests/scripts/test_minigame_input.lua`
- **Verification**：`external\lua\lua.exe tests/scripts/test_minigame_input.lua` 通过

---

## 依赖关系

```
U1.1 [unlock]  ──→  U2.1 (gallery 读取 unlockedCG)
U1.2 [saveplace]   (独立)
U1.3 [ch 多角色]   (独立)
U1.4 [语音重播]    (独立)
U1.5 [macro]       (独立)
U2.2 [收藏持久化]  (独立)
U2.3 [palette]     (独立)
U2.4 [i18n]        (独立)
U3.1 [立方体]  ──→  U3.2 [碰撞]  ──→  U3.3 [输入]
```

---

## 测试目标

| 优先级 | C++ 测试 | 纯 Lua 测试 | 总计 |
|---|---|---|---|
| P1 | +4 | +1 | +5 |
| P2 | 0 | +4 | +4 |
| P3 | +2 | +1 | +3 |
| **合计** | **+6** | **+6** | **+12** |

---

## 风险

- **缩略图 `captureThumbnailPNG`** 依赖 GPU 上下文，暂不实装。gallery 缩略图用占位纹理替代（记录在 `docs/solutions/deferred-gpu-tests.md`）
- **3D 小游戏** 的 `render` 方法依赖 bgfx 初始化，C++ 测试仅做构造验证，实际渲染效果需手动检查
