# Caesura P1-P3 游戏逻辑完善要求

> 2026-06-16 · 基于 codex/archive-expanded-tests 审查结果

## P1：场景系统

### game_logic.lua — 场景切换

当前状态：`scripts/game_logic.lua` 是一个自动测试框架，非游戏主逻辑。真正的场景系统分布在 scheduler、flow、kag 命令中。

待完善：
- 场景切换协议：`[jump target="scene2"]` 跨文件跳转时保存/恢复 `ctx` 状态
- 变量系统：`ctx.f`（局部标志）、`ctx.sf`（全局标志）、`ctx.tf`（临时标志）的跨场景持久化
- 立绘管理：`[ch]` 多角色并发显示、表情切换

### history_ui.lua — 回看 UI

已有：backlog 滚动显示、导航、跳转场景、语音重播骨架。
待完善：`[V]` 语音重播触发后端 `playVoice` 回调。

### flow.lua — 宏系统

已有：`find_label`、`skip_to`。macro/endmacro/erasemacro 在 scheduler 中内联处理（空操作）。
待完善：macro 录制/展开逻辑。

---

## P2：附加功能

### gallery.lua — CG 画廊

已有：CG 扫描缓存、解锁状态（`ctx.unlockedCG`）、全屏浏览、左右导航。
待完善：
- 解锁条件触发器（`scripts/kag/commands/system.lua` 中的 `[unlock]` 命令未实现）
- 缩略图生成（依赖 `captureThumbnailPNG` → 需要 GPU 上下文）

### music_room.lua — 音乐鉴赏

已有：BGM 曲目列表、预览播放、收藏状态。
待完善：
- 封面显示（需要图片资源）
- 收藏持久化路径确认（`config/music_room.lua`）

### palette.lua — 调色板

待完善：日/夜模式切换通过修改全局调色板 → layer 渲染时应用。

### i18n.lua — 多语言

已有：字符串表加载（`assets/lang/xx.lua`）、`{key}` token 替换、`_T` 快捷方式。
待完善：字符串表覆盖率（需翻译资源）。

---

## P3：3D 小游戏

### BgfxMiniGameBackend

已有：`enter→render→leave` 循环框架、场景加载/卸载。
待完善：3D 模型加载（GLTF/GLB）、碰撞检测对接 `MiniCollision`、输入映射（键盘→3D 摄像机）。

---

## 优先级建议

1. **先做 P1 场景系统**——引擎核心体验依赖场景切换和立绘管理
2. **再补 P2 的 `[unlock]` 命令**——画廊/音乐室的数据入口
3. **P2 i18n 字符串表**——翻译资源是独立工作，可并行
4. **P3 3D 小游戏**——依赖资源管线，放在最后

## 依赖关系

- 缩略图 `captureThumbnailPNG` 依赖 GPU 上下文（记录在 `docs/solutions/deferred-gpu-tests.md`）
- 3D 模型加载依赖 bgfx 初始化，无窗口环境测试受限
