# Live2D Cubism SDK 集成指南

## 前置条件

- **Live2D Cubism SDK for Native** 许可
  - 免费获取: [Live2D 官网下载页](https://www.live2d.com/download/cubism-sdk/)
  - 需要同意 Live2D 专有许可协议
- 支持的平台:
  - Windows (D3D11 渲染路径)
  - macOS (OpenGL 路径；Metal 路径尚未完成生产验证)
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

将 SDK 放在项目根目录，或通过 `CUBISM_SDK_ROOT` 指定其他位置：

```
Caesura(AmeKAG)/
└── CubismSdkForNative-5-r.5/
    ├── Core/
    │   ├── include/       # Cubism 核心头文件
    │   │   ├── Live2DCubismCore.h
    │   │   └── ...
    │   └── lib/           # 平台相关库文件
    │       ├── windows/x86_64/
    │       ├── macos/
    │       └── linux/x86_64/
    └── Framework/
        └── src/           # Live2D 框架源码
```

### 3. CMake 配置

```bash
# 启用 Live2D 支持
cmake -B build -DCAESURA_LIVE2D=ON \
  -DCUBISM_SDK_ROOT="path/to/CubismSdkForNative-5-r.5"

# 构建
cmake --build build --config Debug --parallel
```

CMake 会检测 `CUBISM_SDK_ROOT`（默认是项目根目录下的 `CubismSdkForNative-5-r.5`）并：
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
| `CAESURA_LIVE2D` | CMake 选项，用户手动设置 |

## 渲染路径

引擎根据平台自动选择渲染后端：

| 平台 | 首选 | 回退 |
|------|------|------|
| Windows | D3D11（bgfx 渲染器必须为 D3D11，否则初始化失败） | — |
| macOS | 取决于 bgfx 渲染器：OpenGL/GLES → OpenGLShared；Metal → MetalNative（stub，`init()` 恒失败） | OpenGL/GLES 分支：OpenGLReadback；Metal 分支：引擎层回退 NullAnimation |
| Linux | OpenGL | OpenGLReadback |

渲染路径实现在 `src/live2d/Live2D/` 下：
- `D3D11NativeRenderPath.cpp`
- `MetalNativeRenderPath.cpp`
- `OpenGLSharedRenderPath.cpp` / `OpenGLReadbackRenderPath.cpp`

## 当前实现状态（2026-07-31 审计）

> 状态更新（2026-08-01）：Cubism SDK for Native 5-r.5 已下载并在本地完成 **Windows D3D11 路径首次真实编译+运行验证**（详见下文「2026-08-01 验证记录」）。`CAESURA_LIVE2D` 默认仍 OFF，CI 不编译 Cubism 路径；D3D11 之外的 OpenGL 路径与 Metal stub 状态不变。

| 渲染路径 | 平台 | 状态 | 说明 |
|----------|------|------|------|
| `D3D11NativeRenderPath` | Windows | ✓ 已验证（2026-08-01） | 首次真实编译+运行：Haru.moc3 加载渲染成功、无设备丢失。要点：共享 bgfx D3D11 设备，`SetConstantSettings(1, device)`，Cubism 渲染进共享纹理（RTV），`bgfx::overrideInternal()` 交给 bgfx；模型纹理经 D3D11 SRV + `BindTexture`；shader 依赖 `FrameworkShaders/*.fx`（构建时复制到输出目录） |
| `OpenGLSharedRenderPath` | Windows/Linux/macOS | 代码就绪·未验证 | 从未编译。注意：CMake 目前只在 Apple/Linux 平台加入该源文件，Windows 仅编译 D3D11 路径 |
| `OpenGLReadbackRenderPath` | Windows/Linux/macOS | 代码就绪·未验证 | FBO + `glReadPixels` + `bgfx::updateTexture2D` 读回方案；OpenGL 分支 init 失败时的回退路径 |
| `MetalNativeRenderPath` | macOS/iOS | STUB 未实现 | `name()` 返回 `"MetalNative(STUB)"`，`init()` 恒返回 false；需要 macOS 开发者实现并验证 |
| `NullAnimationBackend`（PNG 静态降级） | 全部平台 | ✅ 已测试的默认降级 | 无 SDK 时的默认路径，由 `tests/cpp/test_live2d.cpp` 覆盖；仅支持静态 PNG 立绘 |

要点：

- **SDK 不在仓库内**：`CubismSdkForNative-5-r.5` 未随仓库提供（`.gitignore` 已忽略），需按上文「安装步骤」手动下载。
- **CI 不编译任何 Cubism 路径**：没有 SDK 就没有 `CAESURA_HAS_LIVE2D` 宏，`Live2DBackend` 及其全部渲染路径都不会进入构建。
- **D3D11 方案要点（已实现并验证）**：共享 bgfx 的 D3D11 设备与纹理（RTV+SRV）→ Cubism 渲染进共享纹理 → `bgfx::overrideInternal()` 挂给 bgfx。2026-08-01 首次真实编译并加载 SDK Haru 模型渲染成功。
- **Metal 需要 macOS 开发者实现**：当前 stub 的 `init()` 恒失败。注意 stub 日志声称「Falling back to OpenGL readback」，但实际代码路径是 `Live2DBackend::init()` 失败后由引擎层（`Engine::init()`，Engine.cpp 第 443-450 行）整体回退到 `NullAnimationBackend`。

### 验证路线图

有 SDK 访问权限的开发者应按以下顺序验证，并在完成后更新状态：

1. 下载 Cubism SDK for Native（R5），放到项目根目录（默认查找 `CubismSdkForNative-5-r.5`）或通过 `CUBISM_SDK_ROOT` 指定位置。
2. 执行 `cmake -B build -DCAESURA_LIVE2D=ON`，确认 `CAESURA_HAS_LIVE2D` 被定义、`Live2DBackend` 编译通过（✓ 2026-08-01 完成：修复 3 个编译硬错误后零错误构建）。
3. Windows 上以 D3D11 渲染器运行，加载 `.moc3` 模型并渲染，验证 D3D11Native 路径（✓ 2026-08-01 完成：HTTP RPC 加载 `Samples/Resources/Haru/Haru.model3.json`，渲染多帧无崩溃、无设备丢失）。
4. 逐路径验证其余平台（Linux/macOS 的 OpenGLShared → OpenGLReadback → MetalNative），修复发现的问题后，同步更新能力矩阵（`docs/design/engine-capability-matrix.md` 的 C1 行）与本文档的状态表。

### 2026-08-01 验证记录（Windows D3D11）

首次真实编译+运行验证（SDK for Native 5-r.5，VS2022 Debug）：

- **编译修复**：3 个硬错误（`Live2DModel::textures` 成员缺失、`ILive2DRenderPath::createTexture` 静态不存在、`blitTexture` 传 `bgfx::TextureHandle` 与 `uint32_t` 形参不匹配）+ 缺失 `IRenderDevice` include。
- **运行时修复**：`CubismRenderer_D3D11::SetConstantSettings(1, device)`（模型加载前必须）、`beginFrame` 绑定共享纹理 RTV + `StartFrame/DrawModel/EndFrame` + 恢复 bgfx 渲染目标、`endFrame` 直接 `bgfx::overrideInternal`（去掉 `CopyResource`）、`cubismModel->Update()` 顶点更新、模型纹理 D3D11 SRV + `BindTexture`、renderer double-free 修复、`CubismFramework::Option` 生命周期修复。
- **shader**：`FrameworkShaders/*.fx` 由 CMake 构建时复制到输出目录；`LoadFileFunction/ReleaseBytesFunction` 回调接入。
- **验证方式**：`--editor` 模式 HTTP `POST /api/live2d/load` 加载 `Samples/Resources/Haru/Haru.model3.json`（ASCII 路径副本），多帧渲染无崩溃、无设备丢失、无 shader 编译错误、无 `ContextNum` 警告。
- **套件**：Live2D 构建与无 Live2D 构建均 557/557 通过，ctest 9/9，耦合度 PASS。
- **遗留**：OpenGLShared/OpenGLReadback 路径未编译验证（无对应平台环境）；Metal 维持 stub；画面像素正确性未做视觉确认。


## 常见问题

**Q: 构建提示找不到 `Live2DCubismCore.h`**
A: 确认 `CUBISM_SDK_ROOT/Core/include/Live2DCubismCore.h` 存在。

**Q: 运行时崩溃 "Cubism Core not initialized"**
A: 确认 `.moc3` 和 `.model3.json` 文件路径正确。检查模型文件版本是否与 SDK 版本兼容。

**Q: macOS 上 Metal 渲染不工作**
A: 当前 Metal 路径仍是 stub，不应作为发布能力启用；请使用 OpenGL renderer，或保持默认 NullAnimation 降级。

**Q: 能否在 Release 构建中去掉 Live2D？**
A: 可以。不设置 `-DCAESURA_LIVE2D=ON`，引擎将使用 `NullAnimationBackend`（仅支持静态 PNG 立绘）。
