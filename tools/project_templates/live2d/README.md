# Caesura (AmeKAG) — Live2D VN 模板

**Live2D 导向的骨架。** 引擎**没有**独立的 `[live2d]` 契约命令——Live2D 模型是
**通过前景图层（`[fg]`）加载**、并由动画后端驱动的。本模板演示这条正确接入路径。

> **重要**：使用**真** Live2D 需要 **Cubism SDK**（`CAESURA_LIVE2D=ON` 编译）。没有 SDK 时
> 同一段 `[fg]` 调用会**安全降级**——把模型当静态 PNG 立绘显示，游戏照常可跑。
> 完整集成步骤见 `docs/guides/live2d-setup.md`。

## 模型放哪里

`assets/live2d/<name>/`（本模板示例用 `character`）：

```
assets/live2d/character/
├── character.moc3                # 模型文件
├── character.model3.json         # 模型描述（story.ks 里 [fg] 引用的就是它）
├── textures/                     # 纹理
├── motions/                      # 动作（idle / tap_body …）
└── expressions/                  # 表情（happy …）
```

## story.ks 怎么用

`story.ks` 演示了完整接入三件套：

1. **加载模型**（已激活——`[fg]` 是契约命令，可通过静态检查）：
   ```kag
   [fg storage="assets/live2d/character/character.model3.json"]
   ```
2. **动作 `@motion`** 与 **表情 `@expression`**（**注释占位**——它们是 KAG3 风格指令、
   不是 Neo-Genesis 契约命令；启用 Live2D 构建后把注释解开即可）：
   ```kag
   ; @motion name="idle"        ; 循环待机动
   ; @expression name="happy"   ; 设置表情
   ; @motion name="tap_body"    ; 点击反应
   ```
3. **对白 `[ch]`** 文本里明确标注"Live2D 模型入口在此"，告诉你到哪里接。

> 为什么不直接用 `[motion]`/`[expression]`？它们不在命令契约里，直接写会被 `ks_check`
> 判为未知命令。所以模板用注释占位 + `[ch]` 说明，既保零错误又给出接入点。

## 启用真 Live2D

```bash
cmake -B build -DCAESURA_LIVE2D=ON -DCUBISM_SDK_ROOT="path/to/CubismSdkForNative-5-r.5"
cmake --build build --config Debug --parallel
```

- SDK 可用时构建自动定义 `CAESURA_HAS_LIVE2D` 并启用 `Live2DBackend`；
- 没有 SDK 时引擎回退 `NullAnimationBackend`（PNG 静态立绘）——教程在无 GPU / 无 SDK
  环境下依旧能写能跑；
- 运行期加载也可走编辑器 RPC `POST /api/live2d/load`。

完整说明见 `docs/guides/live2d-setup.md`。

## 结构

```
tools/project_templates/live2d/
├── README.md     # 本文档
├── entry.lua     # KAG runner 启动入口
├── story.ks      # Live2D 接入演示：[fg] 加载 + @motion/@expression 注释占位
└── assets/       # 资产骨架占位（assets/live2d/ 放模型）
```

## 相关文档

| 文档 | 内容 |
|------|------|
| `docs/guides/live2d-setup.md` | Cubism SDK 集成步骤、目录规范、降级行为 |
| `docs/api/cpp-interfaces.md` §8 | live2d（动画后端）接口 |
| `docs/api/command-contracts.md` | `[fg]` 等命令契约 |
| `docs/guides/asset-pipeline.md` | 资产格式与缺失降级 |
