# Caesura (AmeKAG) 构建指南

## 前置条件

- CMake 3.25+
- C++20 编译器 (MSVC 2022 / GCC 13+ / Clang 16+)
- Node.js 20+ (仅编辑器)
- Python 3 (仅 CI 脚本)

## 引擎构建

### Windows (MSVC)
```bash
# 不含 Live2D
cmake -B build_nol2d -DCAESURA_ENABLE_LIVE2D=OFF
cmake --build build_nol2d --config Debug
cmake --build build_nol2d --config Release

# 含 Live2D（需 Cubism SDK 路径）
cmake -B build_l2d -DCAESURA_ENABLE_LIVE2D=ON -DCUBISM_SDK_PATH="C:/path/to/CubismSdkForNative"
cmake --build build_l2d --config Debug
```

### Linux
```bash
cmake -B build -DCAESURA_ENABLE_LIVE2D=OFF
cmake --build build
```

### macOS
```bash
cmake -B build -DCAESURA_ENABLE_LIVE2D=OFF
cmake --build build
```

## 编辑器开发

```bash
cd web-editor
npm install
npm run dev          # Vite dev server + Electron
npm run build        # Vite production build
npm run package      # 完整打包 (Win/Mac/Linux)
npm run package:win  # 仅 Windows
```

编辑器 dev 模式下：
- Vite 运行在 `http://localhost:5173`
- Electron 自动启动，加载 Vite dev server
- 引擎以 headless 模式自动 spawn

## 测试

```bash
ctest --test-dir build_nol2d -C Debug
```

## 已知问题

- 测试项目 BgfxMiniGameBackend 链接错误（预存，非功能性缺陷）
- macOS/Linux CI 测试被 continue-on-error 掩盖
- pl_mpeg 无硬件加速