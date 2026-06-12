# Live2D Cubism SDK 集成指南

## 前置条件

- **Live2D Cubism SDK for Native** 许可
  - 免费获取: [Live2D 官网下载页](https://www.live2d.com/download/cubism-sdk/)
  - 需要同意 Live2D 专有许可协议
- 支持的平台:
  - Windows (D3D11 渲染路径)
  - macOS (Metal 渲染路径，自动回退 OpenGL)
  - Linux (OpenGL 渲染路径)

## 无 SDK 环境

**Caesura 默认不包含 Live2D Cubism SDK。** 没有 SDK 时：

- 引擎使用 `NullAnimationBackend`，提供降级的静态 PNG 立绘支持
- `loadModel("character.png")` 会将 PNG 作为静态纹理加载
- 动态功能（形变、眨眼、口型同步）不可用

## 安装步骤

### 1. 下载 SDK

从 Live2D 官网下载 Cubism SDK for Native R5 或更高版本。解压后你会得到类似结构的目录：

```
CubismSdkForNative/
├── Core/
│   ├── include/
│   └── lib/
├── Framework/
│   └── src/
└── Samples/
```

### 2. 放置 SDK 文件

将 SDK 放置到项目的 `external/Live2D/` 目录下：

```
Caesura(AmeKAG)/
└── external/
    └── Live2D/
        ├── Core/
        │   ├── include/       # Cubism 核心头文件
        │   │   ├── Live2DCubismCore.h
        │   │   └── ...
        │   └── lib/           # 平台相关库文件
        │       ├── windows/x86_64/
        │       ├── macos/
        │       └── linux/x86_64/
        ├── CubismNativeFramework/
        │   └── src/           # Live2D 框架源码
        └── Framework/
            └── src/           # Live2D 渲染框架
```

### 3. CMake 配置

```bash
# 启用 Live2D 支持
cmake -B build -DCAESURA_ENABLE_LIVE2D=ON

# 构建
cmake --build build --config Debug --parallel
```

CMake 会自动检测 `external/Live2D/` 目录并：
- 定义 `CAESURA_HAS_LIVE2D` 预处理器宏
- 链接对应平台的 Cubism Core 库
- 编译 `Live2DBackend` 替代 `NullAnimationBackend`

### 4. 放置模型文件

将 Live2D 模型文件放入 `assets/live2d/` 目录：

```
assets/live2d/
└── character_name/
    ├── character_name.moc3    # 模型文件
    ├── character_name.model3.json  # 模型描述
    ├── textures/              # 纹理
    │   └── texture_00.png
    ├── motions/               # 动作
    │   ├── idle.motion3.json
    │   └── tap_body.motion3.json
    └── expressions/           # 表情
        └── happy.exp3.json
```

### 5. 在 KAG 脚本中使用

```kag
; 加载 Live2D 模型
@fg storage="carc://live2d/character_name/character_name.model3.json"

; 播放动作
@motion name="idle"

; 设置表情
@expression name="happy"
```

## 编译宏参考

| 宏 | 说明 |
|-----|------|
| `CAESURA_HAS_LIVE2D` | SDK 可用时自动定义，启用 Live2DBackend |
| `CAESURA_ENABLE_LIVE2D` | CMake 选项，用户手动设置 |

## 渲染路径

引擎根据平台自动选择渲染后端：

| 平台 | 首选 | 回退 |
|------|------|------|
| Windows | D3D11 | — |
| macOS | Metal | OpenGL |
| Linux | OpenGL | — |

渲染路径实现在 `src/live2d/Live2D/` 下：
- `Live2DD3D11RenderPath.cpp`
- `Live2DMetalRenderPath.cpp`
- `Live2DOpenGLRenderPath.cpp`

## 常见问题

**Q: 构建提示找不到 `Live2DCubismCore.h`**
A: 确认 SDK 文件在 `external/Live2D/Core/include/` 下的目录结构正确。

**Q: 运行时崩溃 "Cubism Core not initialized"**
A: 确认 `.moc3` 和 `.model3.json` 文件路径正确。检查模型文件版本是否与 SDK 版本兼容。

**Q: macOS 上 Metal 渲染不工作**
A: 自动回退到 OpenGL 渲染路径。确保系统安装了 Metal 框架（macOS 10.15+ 默认）。

**Q: 能否在 Release 构建中去掉 Live2D？**
A: 可以。不设置 `-DCAESURA_ENABLE_LIVE2D=ON`，引擎将使用 `NullAnimationBackend`（仅支持静态 PNG 立绘）。
