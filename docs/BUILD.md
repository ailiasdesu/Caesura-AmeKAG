# Caesura (AmeKAG) 构建与部署指南

> 三平台构建 + Electron 打包 + 测试

## 前置要求

| 工具 | 版本 | 说明 |
|------|------|------|
| CMake | 3.25+ | 构建系统 |
| Visual Studio 2022 | 17+ | Windows 编译 (MSVC) |
| GCC / Clang | 12+ | Linux/macOS 编译 |
| Node.js | 18+ | Electron 编辑器 |
| npm | 9+ | 包管理 |
| Python 3 | 3.10+ | bgfx shaderc 构建 (可选) |

## Windows 构建

```bash
# 标准构建 (无 Live2D, FFmpeg 自动检测)
cmake -B build_nol2d
cmake --build build_nol2d --config Release

# 带 Live2D (需先下载 Cubism SDK 到外部路径)
cmake -B build_l2d -DCAESURA_ENABLE_LIVE2D=ON -DCUBISM_SDK_PATH=C:/path/to/CubismSdkForNative-5-r.5
cmake --build build_l2d --config Release

# 带 FFmpeg (需安装 ffmpeg-dev 库)
cmake -B build_ffmpeg -DCAESURA_ENABLE_FFMPEG=ON
cmake --build build_ffmpeg --config Release
```

> SDL3 开发包应放在 `SDL3-3.4.10/` 目录下，CMake 自动检测。

## Linux 构建

```bash
# 安装依赖
sudo apt install libsdl3-dev libfreetype-dev

# FFmpeg (可选)
sudo apt install libavcodec-dev libavformat-dev libswscale-dev libavutil-dev

# 构建
cmake -B build
cmake --build build --config Release
```

## macOS 构建

```bash
# 安装依赖
brew install sdl3 freetype

# FFmpeg (可选)
brew install ffmpeg

# 构建
cmake -B build
cmake --build build --config Release
```

## 运行 Demo

```bash
# Windows
build_nol2d/Release/CaesuraAmeKAG.exe --demo

# Linux/macOS
./build/CaesuraAmeKAG --demo
```

## 运行测试

```bash
cmake --build build_nol2d --config Debug
build_nol2d/Debug/tests/CaesuraTests.exe
```

24 个测试文件覆盖 Core/Render/Audio/Scripting/System/CARC/Resource/MiniGame/Debug。

## 编辑器开发

```bash
cd web-editor
npm install
npm run dev        # 开发模式 (Vite + Electron)
npm run build      # 仅构建前端
```

## 编辑器打包

```bash
cd web-editor
npm run package:win    # Windows NSIS 安装包
npm run package:mac    # macOS DMG
npm run package:linux  # Linux AppImage
```

打包产物在 `web-editor/release/`。

## Shader 编译

```bash
# 需要先构建 bgfx shaderc 工具
# (参考 external/bgfx/bgfx/makefile)

cd shaders
compile_shaders.bat windows   # D3D11
compile_shaders.bat linux     # OpenGL
compile_shaders.bat macos     # Metal
```

编译后需更新 `src/Render/EmbeddedShaders_*.cpp`。

## CI/CD

GitHub Actions: `.github/workflows/ci.yml`
- Windows MSVC Debug + Release
- Linux GCC
- macOS Clang

跨平台 CI 测试启用需要非 GPU 测试桩（当前跳过）。
