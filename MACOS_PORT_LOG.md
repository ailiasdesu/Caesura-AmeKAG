# Caesura (AmeKAG) macOS 跨平台适配修改日志

> 适配日期: 2026-06-07
> 上游 commit: f7ee618
> 测试环境: macOS 26 + Apple Silicon + Metal 3

---

## 修改总览

26 个文件修改 + 4 个新文件，按原因分组：

---

## 一、编译阻断修复（不改就编不过）

### 1. `src/Carc/CryptoEngine.cpp` — 全文件重写

**问题:** 原文件全量使用 Windows BCrypt API，无平台保护。

**修改:** 三段式条件编译 `#if defined(_WIN32)` / `#elif defined(CAESURA_CRYPTO_OPENSSL)` / `#else`

| 功能 | Windows | macOS/Linux |
|------|---------|-------------|
| AES-256-GCM | BCrypt | OpenSSL EVP |
| SHA-256 | BCrypt | OpenSSL EVP |
| 安全随机数 | BCryptGenRandom | OpenSSL RAND_bytes |
| 密钥文件 I/O | CreateFileA/ReadFile/WriteFile | fopen/fread/fwrite (C 标准) |
| Ed25519 | ed25519 (不变) | ed25519 (不变) |

新增 `#else` 分支：无加密后端时返回空/失败，允许引擎主体编译（CARC 加密功能不可用）。

### 2. `src/Carc/CryptoEngine.h` — 注释更新

```cpp
// 旧: Windows-only implementation. No external crypto dependencies.
// 新: Windows: BCrypt, macOS/Linux: OpenSSL (optional via CAESURA_CRYPTO_OPENSSL).
```

### 3. `src/Resource/DirAssetProvider.cpp` — 文件系统 API

| 行 | 原来 | 改为 | 原因 |
|----|------|------|------|
| include | `<windows.h>` | `<sys/stat.h>` | POSIX stat() |
| 路径拼接 | `+ "\\" +` | `+ "/" +` | 正斜杠 Windows 也支持 |
| exists() | `GetFileAttributesA` | `stat()` + `S_ISREG` | POSIX 等价检查 |

### 4. `src/Carc/CARCFormat.h` — 缺少头文件

```cpp
// 新增
#include <cstddef>  // size_t
```

### 5. `CMakeLists.txt` — 4 处构建修复

| 位置 | 修改 | 原因 |
|------|------|------|
| `target_link_libraries` | bcrypt 移入 `if(WIN32)` | macOS 无 bcrypt 库 |
| `add_custom_command` SDL3.dll | 包在 `if(WIN32)` 中 | macOS 使用 dylib 无需拷贝 |
| SoLoud 后端 | 新增 `CoreAudio` (macOS), `ALSA+PulseAudio` (Linux) | 非 Windows 无音频输出 |
| `target_compile_definitions` | `if(WIN32)` 中定义 `CAESURA_PLATFORM_WINDOWS` | 该宏之前从未定义 |

### 6. `tests/CMakeLists.txt` — 3 处修复

- Windows 专用宏 (`_CRT_SECURE_NO_WARNINGS`/`NOMINMAX`/`WIN32_LEAN_AND_MEAN`/`CAESURA_PLATFORM_WINDOWS`) 移入 `if(WIN32)`
- bcrypt 移入 `if(WIN32)`
- macOS 新增 Cocoa/IOKit/Metal/QuartzCore 链接（bgfx 依赖）
- doctest C++20 兼容修复 (`operator<<` 歧义)

### 7. `tools/carc_pack/CMakeLists.txt`

- bcrypt 链接移入 `if(WIN32)`
- ed25519 路径修正 (`external/ed25519` → `external/ed25519/src`)
- Windows 宏移入 `if(WIN32)`
- 新增 OpenSSL 链接

---

## 二、依赖配置

### 8-13. `external/` 构建文件（新建/修改）

| 文件 | 作用 |
|------|------|
| `external/lua/CMakeLists.txt` | Lua 5.4 静态库（原 tarball 不带 CMake） |
| `external/bgfx/bx/CMakeLists.txt` | bx 库（排除 mingw 兼容路径） |
| `external/bgfx/bimg/CMakeLists.txt` | bimg 库（排除 astcenc/img_encode） |
| `external/bgfx/bimg/bimg_stubs.cpp` | ASTC + 格式解析桩函数 |
| `external/bgfx/bgfx/CMakeLists.txt` | bgfx 库（仅 Metal 渲染器, BGFX_CONFIG 宏） |
| `external/soloud/contrib/CMakeLists.txt` | cmake_minimum_required 版本修正 |

---

## 三、渲染适配

### 14. `src/Render/BgfxRenderDevice.cpp` — 5 处修改

**a) 默认后端 (行 183)**
```cpp
#if defined(__APPLE__)
static bgfx::RendererType::Enum s_preferredBackend = bgfx::RendererType::Metal;
#elif defined(__linux__)
static bgfx::RendererType::Enum s_preferredBackend = bgfx::RendererType::Vulkan;
#else
static bgfx::RendererType::Enum s_preferredBackend = bgfx::RendererType::Direct3D11;
#endif
```

**b) bgfx v11 枚举值修正 — 核心 bug！**

原代码用硬编码数字匹配渲染器类型，但当前 bgfx 版本中枚举值已变化：

| 原代码数字 | 原以为 | 实际 bgfx v11 | 修正为 |
|-----------|--------|--------------|--------|
| `renderer == 2` | Vulkan | **Direct3D11** | `bgfx::RendererType::Vulkan` |
| `renderer == 3` | (不存在) | **Direct3D12** | `bgfx::RendererType::Metal` |
| `renderer == 4` | D3D11 | **Gnm** | `bgfx::RendererType::Direct3D11` |
| `renderer == 5` | D3D12 | **Metal** | `bgfx::RendererType::Direct3D12` |

**c) 新增 Metal 着色器分支**

在 `initEmbeddedShaders()` 中新增 `renderer == bgfx::RendererType::Metal` 分支，引用 `kEmbeddedMetal_*` 数组。

**d) 全屏效果着色器重构**

用数据驱动方式替代硬编码 DXBC 路径，按渲染器类型选择 SPIR-V/Metal/DXBC 字节码。

**e) BgfxDebugCallback 生命周期**

从文件级 `static` 改为 `unique_ptr<bgfx::CallbackI>` 成员，避免 main() 返回后静态析构顺序导致 segfault。

### 15. `src/Render/BgfxRenderDevice.h` — 新增成员

```cpp
std::unique_ptr<bgfx::CallbackI> m_debugCallback;
```

### 16. `src/Render/EmbeddedShaders.h` — 新增 Metal 声明

新增 20 个 extern 符号（10 个数组 + 10 个 size_t）。

### 17. `src/Render/EmbeddedShaders.cpp` — include 修复

- `#include "EmbeddedShaders_SPIRV.cpp"` 和 `#include "EmbeddedShaders_Metal.cpp"` 移入 `namespace Caesura {}` 内部，确保符号在正确命名空间。

### 18. `src/Render/EmbeddedShaders_Metal.cpp` — **新文件**

10 个 Metal 着色器的 MSL 源码嵌入（bgfx Metal 后端通过 `newLibraryWithSource` 运行时编译）。

### 19. `shaders/embed_metal.sh` — **新文件**

着色器嵌入自动化脚本：`.metal` 源码 → `xxd -i` → C 数组。

### 20. `shaders/metal/*.metal` — 全部 10 个文件

- 移除 UTF-8 BOM (`\xef\xbb\xbf`)
- 入口函数名统一改为 `xlatMtlMain`（bgfx Metal 后端硬编码要求）

---

## 四、引擎行为修正

### 21. `src/Core/Engine.cpp` — 2 处修改

**a) Esc/窗口关闭处理**
```cpp
if (event.type == SDL_EVENT_QUIT ||
    (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
    m_running = false;
    return;
}
```

**b) 渲染函数简化**

原 `bgfx::dbgTextPrintf` 在 Metal 上叠字，改为 stderr 帧日志。

### 22. `src/main.cpp`

- 新增 `#include <unistd.h>`
- `return 0` → `_exit(0)`（跳过 CRT 静态析构，避免退出时 segfault）

### 23. `src/Core/TextureBudget.cpp`

`#ifdef CAESURA_PLATFORM_WINDOWS` → `#ifdef _WIN32`（编译器内置宏，无需 CMake 定义）

---

## 五、文档

### 24. `README.md` — 新增平台适配链接

### 25. `MACOS_PORT_LOG.md` — 本文件

### 26. `MACOS_ISSUES.md` — macOS Metal 已知问题

### 27. `.gitignore` — 新增

```
shaders/compiled/
*.metallib
*.metal.bin
```

---

## 验证结果

| 项目 | 状态 |
|------|------|
| cmake 配置 | ✅ |
| cmake 构建 | ✅ |
| C++ 单元测试 | ✅ 110/110 passed (220 assertions) |
| carc_pack 工具 | ✅ |
| Metal 初始化 | ✅ |
| 嵌入式着色器 | ✅ 10/10 READY |
| 音频 CoreAudio | ✅ |
| Esc 退出 | ✅ 无 segfault |

---

## 未修改的 Windows 代码

所有 Windows 路径原样保留在 `#if defined(_WIN32)` 块中，未删除任何原有逻辑。
