# iOS Build, Architecture & Validation Guide (Track I)

本文档定义 Caesura (AmeKAG) 引擎在 Apple iOS 平台（Track I: I0-I3）上的架构设计、构建流程、着色器管线与 CI 探针规范。

---

## 1. Track I 路线图与阶段划分

| 阶段 | 代号 | 核心目标 | 状态 |
|---|---|---|---|
| **I0** | Toolchain & Generator | CMake iOS 工具链、Xcode 工程生成与静态模块编译 | ✅ 探针在列 |
| **I1** | Metal / bgfx Bridge | Metal 图元管线、Metal 着色器编译、RenderDevice 适配 | 🟡 架构设计就绪 |
| **I2** | SDL3 iOS Lifecycle | UIApplication 生命周期、UIKit 窗口桥接、触控手势路由 | 🟡 代码适配 |
| **I3** | Packaging & Signing | Info.plist、Assets.car、证书签名、ipa 导出与真机测试 | ⏳ 待真机验证 |

---

## 2. 构建前置条件

- **操作系统**：macOS 14+ (Sonoma 或更高版本)
- **开发工具**：Xcode 15+（含 iOS 17+ SDK 与 Command Line Tools）
- **构建工具**：CMake 3.25+、Ninja

---

## 3. 本地构建流程（macOS）

### 3.1 生成 Xcode 工程

```bash
mkdir -p build-ios
cd build-ios

# 真机构建 (iphoneos)
cmake .. -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT=iphoneos \
    -DCMAKE_OSX_ARCHITECTURES="arm64" \
    -DCMAKE_BUILD_TYPE=Release

# 模拟器构建 (iphonesimulator)
cmake .. -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT=iphonesimulator \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
    -DCMAKE_BUILD_TYPE=Debug
```

### 3.2 命令行构建

```bash
xcodebuild -project CaesuraAmeKAG.xcodeproj \
    -scheme CaesuraAmeKAG \
    -configuration Release \
    -destination 'generic/platform=iOS' \
    CODE_SIGNING_ALLOWED=NO \
    build
```

---

## 4. 渲染与 Metal 着色器管线 (I1)

1. **后端选择**：iOS 平台使用 bgfx 的 Metal 渲染后端（`BGFX_RENDERER_TYPE_METAL`）。
2. **着色器编译**：使用 `shaderc` 生成 Metal 格式（`--platform ios -p metal`）的二进制着色器包（.bin）。
3. **纹理与字体**：RGBA8 字体图集与纹理管线与 Metal 完全兼容，无需通道重排。

---

## 5. CI 工作流与探针验证

GitHub Actions 在 `.github/workflows/ci.yml` 中配置了 `ios-compile` 探针任务：

- 运行于 `macos-latest` 运行器；
- 使用 CMake 生成 iOS 编译目标；
- 设置 `continue-on-error: true`，作为前瞻性构建探针，监控跨平台 C++20 模块在 Apple Clang 下的兼容性；
- 探针保持全绿无编译错误。

---

## 6. 注意事项与限制

1. **iOS 沙盒与资源定位**：iOS 应用的资源包路径位于 `CFBundleBundlePath`，保存数据位于 `NSDocumentDirectory`，需通过 `IStorageBackend` 适配路径映射。
2. **音频会话**：SoLoud 在 iOS 上需通过 CoreAudio / AVAudioSession 配置后台混音与静音开关行为。
3. **模拟器与真机差异**：Metal API 在某些老旧模拟器上存在功能子集限制，建议优先使用 Apple Silicon Mac 或真实 iPhone 调试。
