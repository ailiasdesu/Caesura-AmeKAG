# Build Guide — Caesura (AmeKAG)

## Prerequisites

- CMake 3.25+
- C++20 compiler
- Git (for submodules)

## Windows (MSVC + D3D11)

```powershell
git clone --recursive https://github.com/ailiasdesu/Caesura-AmeKAG.git
cd Caesura-AmeKAG

# Default build (no Live2D, no FFmpeg)
cmake -B build
cmake --build build --config Debug

# With Live2D (requires Cubism 5 Native SDK)
cmake -B build_l2d -DCAESURA_LIVE2D:BOOL=ON -DCUBISM_SDK_ROOT:PATH="C:/path/to/CubismSdkForNative-5-r.5"
cmake --build build_l2d --config Debug

# Without Live2D (clean build)
cmake -B build_nol2d
cmake --build build_nol2d --config Debug
```

## Linux (GCC + OpenGL)

```bash
git clone --recursive https://github.com/ailiasdesu/Caesura-AmeKAG.git
cd Caesura-AmeKAG

cmake -B build
cmake --build build
```

> **注意**: Linux CI 当前构建失败 (CryptoEngine 使用 BCrypt，需替换为 OpenSSL)。

## macOS (Clang + Metal)

```bash
git clone --recursive https://github.com/ailiasdesu/Caesura-AmeKAG.git
cd Caesura-AmeKAG

cmake -B build
cmake --build build
```

> **注意**: macOS CI 当前构建失败 (同 BCrypt 问题)。

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `CAESURA_LIVE2D` | OFF | Enable Live2D Cubism 5 support |
| `CUBISM_SDK_ROOT` | — | Path to Cubism Native SDK |
| `CAESURA_VIDEO_FFMPEG` | OFF | Enable FFmpeg video backend |

## Directory Layout After Build

```
build/Debug/
├── CaesuraAmeKAG.exe
├── SDL3.dll
└── scripts/          (copied from repo)
```

## Running Tests

```powershell
ctest --test-dir build --config Debug
```

## CI Pipeline

- `.github/workflows/ci.yml`: Windows MSVC (Debug+Release), Linux GCC, macOS Clang
- Windows: ✅ 通过
- Linux/macOS: ❌ 待修复 (全局约束最后)