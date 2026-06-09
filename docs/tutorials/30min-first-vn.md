# 30 分钟制作第一个视觉小说 — Caesura (AmeKAG) 教程

> 目标：30 分钟内通过可视化编辑器 + AI 辅助，完成一个包含立绘、BGM、对话、转场的场景。

## 步骤 1: 启动编辑器

```bash
cd web-editor
npm install
npm run dev
```

编辑器自动启动引擎（headless 模式），你会看到：
- **左侧**: 素材浏览器
- **中央**: 舞台画布 + 代码编辑器
- **右侧**: 属性面板 + AI 面板
- **底部**: 时间线

## 步骤 2: 准备素材

在 `assets/` 目录下准备素材：
```
assets/
├── bg/classroom_day.png    # 背景
├── char/heroine_uniform.png # 角色立绘
├── bgm/gentle_piano.ogg     # 背景音乐
└── fg/desk_front.png        # 前景
```

可以从素材浏览器直接拖入舞台。

## 步骤 3: 用 AI 生成第一个场景

打开右侧 AI 面板，输入：
```
@generate a classroom scene with a heroine. She says "Welcome back."
Use gentle piano BGM, fade-in transition.
```

AI 会生成类似代码：
```lua
function scene_start()
    KAG.show_image("bg", "bg/classroom_day", 0, 0)
    KAG.play_bgm("bgm/gentle_piano", 0.6, true)
    KAG.transition("fade_in", 1.5)
    KAG.show_image("char", "char/heroine_uniform", 800, 200)
    KAG.show_text("她", "欢迎回来。")
    KAG.wait_click()
end
```

点击 **Insert** 将代码插入编辑器。

## 步骤 4: 可视化编辑

在时间线上你可以：
- **拖拽事件块** 调整顺序
- **点击事件** 在属性面板修改参数
- 舞台会实时显示素材位置预览

属性修改会自动同步到代码。

## 步骤 5: 预览运行

点击 **Run** 按钮（或 Ctrl+R），引擎会渲染你的场景。
点击 **Stop** 停止。

## 步骤 6: 扩展场景

用 AI 继续扩展：
```
@generate add a second character entering from the right.
The first character reacts with surprise.
```

## 步骤 7: 添加 Live2D 动态角色（可选）

需要先编译 Live2D 支持：
```bash
cmake -B build_l2d -DCAESURA_ENABLE_LIVE2D=ON -DCUBISM_SDK_PATH=../CubismSdkForNative-5-r.5
cmake --build build_l2d --config Release
```

然后在 AI 面板输入：
```
@generate load live2d model haruka and play idle animation.
Then switch to smile expression on first dialogue.
```

## 步骤 8: 保存与导出

编辑器自动保存。要打包发布：
```bash
npm run package:win   # Windows
npm run package:mac   # macOS
npm run package:linux # Linux
```

## 常用 KAG 命令速查

| 命令 | 说明 |
|------|------|
| `KAG.show_image(layer, path, x, y)` | 显示图像 (layer: bg/fg/char) |
| `KAG.show_text(name, text)` | 显示对话 |
| `KAG.wait_click()` | 等待点击 |
| `KAG.play_bgm(path, volume, loop)` | 播放 BGM |
| `KAG.transition(type, time)` | 转场 (fade_in/fade_out/crossfade) |
| `KAG.clear_screen()` | 清除所有图层 |
| `KAG.wait(ms)` | 等待毫秒 |
| `KAG.jump(label)` | 跳转到标签 |
| `KAG.save(slot)` | 存档 |
| `KAG.load(slot)` | 读档 |

## 下一步

- 完整 KAG API: `docs/api/KAG-API.md`
- MiniGame 3D: `docs/api/MiniGame-API.md`
- 构建指南: `docs/BUILD.md`

## 延伸阅读

- docs/solutions/best-practices/alpha-to-beta-sprint.md — 了解引擎从 Alpha 到 Beta 的升级模式
- docs/api/KAG-API.md — KAG 61 命令完整参考