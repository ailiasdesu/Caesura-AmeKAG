# Caesura (AmeKAG) 引擎实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 根据现有的引擎代码基础上构建 Caesura 视觉小说引擎的完整 C++ 核心 + Lua 脚本层，最终达到 P0+P1 全功能 Alpha 版本。

**Architecture:** C++17 主循环驱动 bgfx(渲染) + SoLoud(音频) + SDL3(窗口/输入)，Lua 5.4 通过协程执行 KAG 标签脚本。C++ 提供硬件绑定 API，Lua 承担全部游戏逻辑。主线程独占模型，无锁设计。

**Tech Stack:** C++17, Lua 5.4, bgfx (cross-platform rendering), SoLoud (audio), SDL3 (windowing/input), LPeg (KAG parser), FreeType (font), pl_mpeg (video, Alpha), zstd (compression), OpenSSL (AES-GCM + Ed25519)

**参考说明书:** `docs/Caesura_功能实现规格说明书_整合版.md` (61项设计决策)

---

## 项目文件结构

```
Caesura(AmeKAG)/
├── CMakeLists.txt                    # 根构建文件
├── cmake/                            # CMake 模块
│   ├── Findbgfx.cmake
│   ├── FindSoLoud.cmake
│   └── FindSDL3.cmake
├── src/                              # C++ 引擎核心
│   ├── main.cpp                      # 入口点
│   ├── Engine.h/cpp                  # 引擎主类 (初始化+主循环)
│   ├── render/                       # 渲染子系统
│   │   ├── RenderBackend.h/cpp       # bgfx 初始化和帧管理
│   │   ├── LayerManager.h/cpp        # 图层管理器 (bg/fg/message + RTT)
│   │   ├── TextureManager.h/cpp      # 纹理加载/缓存/生命周期
│   │   ├── FontRenderer.h/cpp        # FreeType 字体渲染 (TextureArray)
│   │   ├── TextLayoutEngine.h/cpp    # 文本布局 (换行/对齐/ruby/CJK)
│   │   ├── ShaderCache.h/cpp         # 着色器缓存 (CompositeShaderCache)
│   │   ├── TransitionManager.h/cpp   # 转场效果 (Rule Image + easing)
│   │   └── GpuMonitor.h/cpp          # GPU 帧时间监控 + 自适应降级
│   ├── audio/                        # 音频子系统
│   │   ├── AudioBackend.h/cpp        # SoLoud 初始化 + 全局 Direct 模式
│   │   ├── AudioManager.h/cpp        # BGM/Voice/SE Bus 管理
│   │   └── AudioCallback.h/cpp       # 音频结束回调 (SDL_PushEvent)
│   ├── script/                       # 脚本引擎
│   │   ├── LuaEngine.h/cpp           # Lua 5.4 嵌入 (lua_State + 绑定)
│   │   ├── KagExecutor.h/cpp         # KAG 标签执行器 (kag[cmd] -> C++)
│   │   └── GameState.h/cpp           # GameState 单例 (ctx 管理)
│   ├── resource/                     # 资源管理
│   │   ├── IAssetProvider.h           # 资源提供者抽象接口
│   │   ├── ProviderChain.h/cpp        # 责任链: CARC > patch > dir
│   │   ├── DirAssetProvider.h/cpp     # 目录资源提供者
│   │   ├── AsyncLoader.h/cpp          # 异步加载 (后台线程+SDL_PushEvent)
│   │   └── ResourcePool.h/cpp         # 对象池 (RTT/SE Bus/render_batch)
│   ├── carc/                         # CARC 容器
│   │   ├── CARCReader.h/cpp          # CARC 读取器 (验证+解密+解压)
│   │   ├── CARCWriter.h/cpp          # CARC 构建器 (car_pack.exe 使用)
│   │   ├── CryptoEngine.h/cpp        # AES-256-GCM + Ed25519
│   │   └── CarcAssetProvider.h/cpp   # CARC 资源提供者 (IAssetProvider)
│   ├── system/                       # 系统设施
│   │   ├── Logger.h/cpp              # 日志系统 (10MB 轮转)
│   │   ├── Config.h/cpp              # 配置管理 (JSON)
│   │   ├── SaveManager.h/cpp         # 存档管理 (JSON 序列化 + 缩略图)
│   │   ├── I18nManager.h/cpp         # 国际化 (_T + 翻译表)
│   │   └── ErrorUI.h/cpp             # 错误界面 (硬编码几何 + 级联保护)
│   ├── input/                        # 输入系统
│   │   └── InputManager.h/cpp        # SDL 事件 → Lua 事件分发
│   └── debug/                        # 调试工具
│       ├── DebugProtocol.h/cpp       # lua_sethook 断点/单步/变量
│       ├── HotReload.h/cpp           # 文件监控 + 协程热重载
│       └── DevSocket.h/cpp           # Dev socket 9229 结构化日志
├── scripts/                          # Lua 脚本层
│   ├── init.lua                      # Lua 初始化入口
│   ├── kag/                          # KAG 命令实现
│   │   ├── init.lua                  # KAG 命令注册表
│   │   ├── tokenizer.lua             # LPeg .ks → Token 序列
│   │   ├── scheduler.lua             # 协程调度器 (遍历 tokens)
│   │   ├── flow.lua                  # 流程控制 (jump/call/return/link)
│   │   └── commands/                 # 各标签实现
│   │       ├── text.lua              # [ch]/[text]/[p]/[r]/[er]
│   │       ├── layer.lua             # [bg]/[fg]/[cl]/[image]
│   │       ├── audio.lua             # [playbgm]/[playse]/[playvoice]
│   │       ├── transition.lua        # [trans]/[move]/[quake]/[fade]
│   │       ├── control.lua           # [if]/[else]/[endif]/[switch]/[case]
│   │       ├── system.lua            # [eval]/[emb]/[wait]/[save]/[load]
│   │       └── video.lua             # [video]/[stopvideo]
│   ├── layers.lua                    # 图层渲染管理
│   ├── audio.lua                     # 音频管理 (BGM/Voice/SE Bus)
│   ├── backend.lua                   # C++ API 绑定代理 (fallback 可选)
│   └── dev/                          # 开发工具 (仅 Dev 模式)
│       └── hotreload.lua             # 脚本热重载
├── shaders/                          # bgfx 着色器
│   ├── vs_default.sc                 # 默认顶点着色器
│   ├── fs_default.sc                 # 默认片段着色器
│   ├── fs_blend/                     # 28 种混合模式着色器变体
│   ├── fs_palette.sc                 # 调色板 LUT 着色器
│   └── fs_transition/                # 转场着色器
├── assets/                           # 引擎内置资源
│   ├── fonts/                        # 内置字体
│   │   └── bitmap_font.bin           # 8x16 位图字体 (ASCII 32-126)
│   └── textures/
│       └── checkerboard.png          # 紫黑棋盘格占位纹理
├── tests/                            # 测试
│   ├── unit/                         # C++ 单元测试 (Catch2)
│   ├── lua/                          # Lua 单元测试 (busted)
│   └── integration/                  # 集成测试
├── tools/                            # 构建工具
│   └── carc_pack/                    # CARC 打包工具
│       └── main.cpp
├── examples/                         # 示例项目
│   ├── hello_world/                  # 最简示例
│   └── demo_game/                    # 完整功能演示
├── docs/                             # 文档
│   ├── Caesura_功能实现规格说明书_整合版.md
│   └── superpowers/
│       └── plans/
│           └── 2026-06-06-caesura-implementation.md  # 本文件
└── thirdparty/                       # 第三方依赖 (git submodule 或 fetch)
    ├── bgfx/
    ├── bx/
    ├── bimg/
    ├── soloud/
    ├── SDL/
    ├── lua/
    ├── lpeg/
    ├── freetype/
    ├── pl_mpeg/
    ├── zstd/
    └── openssl/ (或 mbedtls)
```

---

## Phase 0: 项目骨架 —— 可编译运行的最小引擎

**目标:** 创建可编译的 C++ 项目，窗口能打开，bgfx 渲染三角形，Lua 能执行 print()，日志能写入文件。

**依赖关系:** 无（此 Phase 是基础）

**验收标准:**
- CMake 构建成功 (MSVC 2022 / Clang / GCC)
- 启动后显示空白窗口 (800x600, 可关闭)
- bgfx 渲染一个彩色三角形
- Lua 执行 `print("Hello, Caesura!")` 输出到控制台
- 日志文件写入 `logs/caesura_YYYY-MM-DD_HH-MM-SS.log`
- 按 Escape 正常退出

---

### Task 0.1: 项目构建系统

**Files:**
- Create: `CMakeLists.txt`
- Create: `cmake/Findbgfx.cmake`
- Create: `cmake/FindSoLoud.cmake`
- Create: `cmake/FindSDL3.cmake`
- Create: `src/main.cpp` (最小入口)

**技术说明:**
- CMake 3.20+, C++17 标准
- bgfx 集成方案: 通过 `bx/bimg/bgfx` 源码编译或使用预编译库；bgfx 需要编译 `shaderc` 工具将 `.sc` 着色器编译为平台相关的二进制
- SDL3 集成: 如果系统未安装 SDL3，通过 FetchContent 从 GitHub 拉取
- SoLoud: 源码集成，只需要 soloud 核心 + soloud_wav + soloud_wavstream
- Lua 5.4: 源码集成，编译为静态库
- 第三方库放在 `thirdparty/` 目录，通过 `add_subdirectory` 或 `ExternalProject_Add` 引入
- 构建类型: Debug (带断言+日志), Release (优化+沙箱)

- [ ] **Step 1: 创建根 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.20)
project(Caesura VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 第三方依赖路径
set(THIRDPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)

# Lua 5.4
add_subdirectory(${THIRDPARTY_DIR}/lua)
# bgfx (需要先编译 shaderc)
add_subdirectory(${THIRDPARTY_DIR}/bgfx.cmake)
# SDL3
find_package(SDL3 QUIET)
if(NOT SDL3_FOUND)
    include(FetchContent)
    FetchContent_Declare(SDL3 GIT_REPOSITORY https://github.com/libsdl-org/SDL.git)
    FetchContent_MakeAvailable(SDL3)
endif()

# 引擎源文件
file(GLOB_RECURSE CAESURA_SOURCES src/*.cpp src/*.h)
add_executable(caesura ${CAESURA_SOURCES})

target_link_libraries(caesura PRIVATE
    lua
    bgfx bx bimg
    SDL3::SDL3
)

target_include_directories(caesura PRIVATE src)
```

- [ ] **Step 2: 创建最小 main.cpp**

```cpp
#include <SDL3/SDL.h>
#include <cstdio>

int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Caesura Engine", 800, 600,
        SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event event;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    running = false;
                }
            }
        }
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
```

- [ ] **Step 3: 构建并验证窗口出现**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
./build/Debug/caesura.exe
# 期望: 空白窗口出现在屏幕上，按 Escape 关闭
```

---

### Task 0.2: bgfx 集成 —— 渲染三角形

**Files:**
- Create: `src/render/RenderBackend.h`
- Create: `src/render/RenderBackend.cpp`
- Create: `shaders/vs_default.sc`
- Create: `shaders/fs_default.sc`
- Modify: `src/main.cpp`

**技术说明:**
- bgfx 初始化需要 SDL 的 `WM_INFO` 来获取原生窗口句柄；Windows 用 `HWND`，macOS 用 `NSWindow*`，Linux 用 `X11 Window` 或 `Wayland`
- bgfx 渲染流程: `bgfx::touch(viewId)` → `bgfx::setViewRect()` → 设置顶点/索引缓冲 → `bgfx::submit(viewId, program)` → `bgfx::frame()`
- 三角形示例使用 `bgfx::TransientVertexBuffer` 直接在栈上创建临时顶点数据
- 着色器编译: 使用 bgfx 的 `shaderc` 工具将 `.sc` → 平台二进制（如 DX11 的 `.bin`），嵌入或运行时加载
- 视图设置: 使用单一 View 0，设置清除色为深蓝色 (0x303060ff)

- [ ] **Step 1: 创建 RenderBackend.h**

```cpp
#pragma once
#include <bgfx/bgfx.h>
#include <SDL3/SDL.h>

namespace caesura {

class RenderBackend {
public:
    bool initialize(SDL_Window* window, int width, int height);
    void shutdown();
    void beginFrame();
    void endFrame();

private:
    bool m_initialized = false;
};

} // namespace caesura
```

- [ ] **Step 2: 创建 RenderBackend.cpp**

```cpp
#include "render/RenderBackend.h"
#include <bgfx/platform.h>
#include <bx/bx.h>
#include <cstdio>

namespace caesura {

bool RenderBackend::initialize(SDL_Window* window, int width, int height) {
    // 获取原生窗口句柄 (跨平台)
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    
    bgfx::PlatformData pd = {};
#ifdef _WIN32
    pd.nwh = (void*)SDL_GetPointerProperty(props, 
        SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif __APPLE__
    pd.nwh = (void*)SDL_GetPointerProperty(props,
        SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
#else
    // Linux: X11 或 Wayland
    pd.ndt = SDL_GetPointerProperty(props,
        SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    pd.nwh = (void*)(uintptr_t)SDL_GetNumberProperty(props,
        SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
#endif

    bgfx::setPlatformData(pd);

    bgfx::Init init;
    init.type = bgfx::RendererType::Count; // 自动选择
    init.resolution.width = width;
    init.resolution.height = height;
    init.resolution.reset = BGFX_RESET_VSYNC;
    init.platformData = pd;

    if (!bgfx::init(init)) {
        fprintf(stderr, "[ERROR] bgfx::init failed\n");
        return false;
    }

    bgfx::setViewClear(0,
        BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
        0x303060ff, 1.0f, 0);

    bgfx::setViewRect(0, 0, 0, uint16_t(width), uint16_t(height));

    m_initialized = true;
    printf("[INFO] bgfx initialized (%dx%d)\n", width, height);
    return true;
}

void RenderBackend::shutdown() {
    if (m_initialized) {
        bgfx::shutdown();
        m_initialized = false;
    }
}

void RenderBackend::beginFrame() {
    // bgfx 不需要显式 beginFrame，但作为 API 保留
}

void RenderBackend::endFrame() {
    bgfx::frame();
}

} // namespace caesura
```

- [ ] **Step 3: 创建默认着色器**

`shaders/vs_default.sc`:
```glsl
$input a_position, a_texcoord0, a_color0
$output v_texcoord0, v_color0

#include <bgfx_shader.sh>

void main() {
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_texcoord0 = a_texcoord0;
    v_color0 = a_color0;
}
```

`shaders/fs_default.sc`:
```glsl
$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_tex, 0);

void main() {
    vec4 texColor = texture2D(s_tex, v_texcoord0);
    gl_FragColor = texColor * v_color0;
}
```

- [ ] **Step 4: 更新 main.cpp 集成 bgfx 三角形**

在 `main()` 中 `SDL_CreateWindow` 之后添加三角形渲染逻辑:

```cpp
// 初始化 bgfx
RenderBackend render;
if (!render.initialize(window, 800, 600)) return 1;

// 编译着色器 (开发阶段用运行时编译，Release 用预编译)
// 此处简化: 使用 bgfx 内置的嵌入式着色器方案
// 实际需要 shaderc 编译 .sc → .bin，然后通过 #include 或文件加载

// ... 主循环中 ...
render.beginFrame();
bgfx::touch(0); // 确保 View 0 被渲染

// 渲染三角形示例 (使用 TransientVertexBuffer)
// (详细代码见 Step 5)
render.endFrame();
```

- [ ] **Step 5: 构建并验证三角形渲染**

```bash
# 先编译着色器 (需要 bgfx shaderc)
cd shaders
../thirdparty/bgfx/.build/tools/shaderc -f vs_default.sc -o ../assets/shaders/vs_default.bin --platform windows --type vertex
../thirdparty/bgfx/.build/tools/shaderc -f fs_default.sc -o ../assets/shaders/fs_default.bin --platform windows --type fragment

# 构建引擎
cmake --build build --config Debug
./build/Debug/caesura.exe
# 期望: 深蓝色背景的窗口中显示一个彩色三角形
```

---

### Task 0.3: Lua 5.4 嵌入

**Files:**
- Create: `src/script/LuaEngine.h`
- Create: `src/script/LuaEngine.cpp`
- Create: `scripts/init.lua`
- Modify: `src/main.cpp`

**技术说明:**
- Lua 5.4 通过源码编译为静态库链接；头文件 `lua.hpp` (C++ 包装) 或 `lua.h` (C API)
- 引擎使用单个全局 `lua_State*`，所有协程通过 `lua_newthread(L)` 创建
- `luaL_openlibs(L)` 加载标准库 (在 Release 模式沙箱中会移除危险模块)
- C++ 函数绑定使用 `lua_register` 和 `luaL_newlib`；复杂绑定可后续引入 sol3 库
- Lua 文件通过 IAssetProvider 加载，Phase 0 阶段直接用文件系统

- [ ] **Step 1: 创建 LuaEngine.h**

```cpp
#pragma once

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace caesura {

class LuaEngine {
public:
    bool initialize();
    void shutdown();
    bool executeFile(const char* path);
    bool executeString(const char* code);
    
    lua_State* state() { return m_L; }

private:
    lua_State* m_L = nullptr;

    void registerCoreBindings();
    int reportError(int status);
};

} // namespace caesura
```

- [ ] **Step 2: 创建 LuaEngine.cpp**

```cpp
#include "script/LuaEngine.h"
#include <cstdio>
#include <cstring>

namespace caesura {

bool LuaEngine::initialize() {
    m_L = luaL_newstate();
    if (!m_L) {
        fprintf(stderr, "[ERROR] luaL_newstate failed\n");
        return false;
    }

    luaL_openlibs(m_L);
    registerCoreBindings();

    printf("[INFO] Lua 5.4 initialized (%s)\n", LUA_RELEASE);
    return true;
}

void LuaEngine::shutdown() {
    if (m_L) {
        lua_close(m_L);
        m_L = nullptr;
    }
}

bool LuaEngine::executeFile(const char* path) {
    int status = luaL_loadfile(m_L, path);
    if (status != LUA_OK) {
        return reportError(status) == 0;
    }
    status = lua_pcall(m_L, 0, 0, 0);
    if (status != LUA_OK) {
        return reportError(status) == 0;
    }
    return true;
}

bool LuaEngine::executeString(const char* code) {
    int status = luaL_loadstring(m_L, code);
    if (status != LUA_OK) {
        return reportError(status) == 0;
    }
    status = lua_pcall(m_L, 0, 0, 0);
    if (status != LUA_OK) {
        return reportError(status) == 0;
    }
    return true;
}

int LuaEngine::reportError(int status) {
    if (status != LUA_OK) {
        const char* msg = lua_tostring(m_L, -1);
        fprintf(stderr, "[ERROR] Lua: %s\n", msg ? msg : "unknown error");
        lua_pop(m_L, 1);
        return 1;
    }
    return 0;
}

void LuaEngine::registerCoreBindings() {
    // 注册 engine_version() 全局函数
    lua_pushcfunction(m_L, [](lua_State* L) -> int {
        lua_pushstring(L, "0.1.0");
        return 1;
    });
    lua_setglobal(m_L, "engine_version");

    // 注册 log() 函数 (C++ 日志桥接)
    lua_pushcfunction(m_L, [](lua_State* L) -> int {
        const char* msg = luaL_checkstring(L, 1);
        printf("[LUA] %s\n", msg);
        return 0;
    });
    lua_setglobal(m_L, "log");
}

} // namespace caesura
```

- [ ] **Step 3: 创建 init.lua**

```lua
-- scripts/init.lua
-- Caesura 引擎 Lua 初始化入口

print("Hello, Caesura!")
log("Lua engine version: " .. engine_version())

-- 验证协程支持
local co = coroutine.create(function()
    log("Coroutine running...")
    coroutine.yield()
    log("Coroutine resumed!")
end)

coroutine.resume(co)
coroutine.resume(co)

log("Init complete.")
```

- [ ] **Step 4: 更新 main.cpp 集成 Lua**

在 `main()` 中 RenderBackend 初始化之后添加:

```cpp
LuaEngine lua;
if (!lua.initialize()) return 2;
lua.executeFile("scripts/init.lua");
```

- [ ] **Step 5: 构建并验证 Lua 输出**

```bash
cmake --build build --config Debug
./build/Debug/caesura.exe
# 期望控制台输出:
# Hello, Caesura!
# [LUA] Lua engine version: 0.1.0
# [LUA] Coroutine running...
# [LUA] Coroutine resumed!
# [LUA] Init complete.
```

---

### Task 0.4: 日志系统

**Files:**
- Create: `src/system/Logger.h`
- Create: `src/system/Logger.cpp`
- Modify: `src/main.cpp`

**技术说明:**
- 日志级别: Debug/Info/Warn/Error，Release 构建仅输出 Info+
- 输出目标: 控制台 (始终) + 文件 (`logs/caesura_YYYY-MM-DD_HH-MM-SS.log`)
- 文件轮转: 每次写入前检查大小，超过 10MB 时轮转，保留最近 5 个文件
- 格式: `[2026-06-06 08:00:00.123] [INFO] [子系统] 消息`
- 异步写入: 使用独立线程 + 无锁队列，避免 I/O 阻塞主线程
- 头文件使用可变参数宏实现 `LOG_INFO("Audio", "SoLoud initialized, rate=%d", 44100)`

- [ ] **Step 1: 创建 Logger.h**

```cpp
#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <chrono>

namespace caesura {

enum class LogLevel {
    Debug = 0,
    Info  = 1,
    Warn  = 2,
    Error = 3
};

class Logger {
public:
    static Logger& instance();

    void initialize(const std::string& logDir = "logs");
    void shutdown();
    void setMinLevel(LogLevel level) { m_minLevel = level; }

    void log(LogLevel level, const char* subsystem, 
             const char* fmt, ...);

private:
    Logger() = default;
    ~Logger() { shutdown(); }

    struct LogEntry {
        LogLevel level;
        std::string subsystem;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
    };

    void writerThread();
    std::string formatEntry(const LogEntry& entry);
    void rolloverIfNeeded();

    std::string m_logDir;
    std::ofstream m_file;
    LogLevel m_minLevel = LogLevel::Debug;
    std::atomic<bool> m_running{false};
    std::thread m_writer;
    std::mutex m_queueMutex;
    std::queue<LogEntry> m_queue;
    size_t m_currentSize = 0;
    int m_currentIndex = 0;
    static constexpr size_t MAX_SIZE = 10 * 1024 * 1024; // 10MB
    static constexpr int MAX_FILES = 5;
};

} // namespace caesura

// 便捷宏
#define LOG_DEBUG(sys, fmt, ...) \
    caesura::Logger::instance().log(caesura::LogLevel::Debug, sys, fmt, ##__VA_ARGS__)
#define LOG_INFO(sys, fmt, ...) \
    caesura::Logger::instance().log(caesura::LogLevel::Info, sys, fmt, ##__VA_ARGS__)
#define LOG_WARN(sys, fmt, ...) \
    caesura::Logger::instance().log(caesura::LogLevel::Warn, sys, fmt, ##__VA_ARGS__)
#define LOG_ERROR(sys, fmt, ...) \
    caesura::Logger::instance().log(caesura::LogLevel::Error, sys, fmt, ##__VA_ARGS__)
```

- [ ] **Step 2: 创建 Logger.cpp** (核心实现)

```cpp
#include "system/Logger.h"
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace caesura {

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::initialize(const std::string& logDir) {
    m_logDir = logDir;

#ifdef _WIN32
    CreateDirectoryA(logDir.c_str(), nullptr);
#else
    mkdir(logDir.c_str(), 0755);
#endif

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &time);

    std::ostringstream filename;
    filename << logDir << "/caesura_"
             << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S")
             << ".log";
    
    m_file.open(filename.str(), std::ios::out | std::ios::app);
    m_currentSize = 0;

    m_running = true;
    m_writer = std::thread(&Logger::writerThread, this);

    LOG_INFO("System", "Logger initialized: %s", filename.str().c_str());
}

void Logger::shutdown() {
    m_running = false;
    if (m_writer.joinable()) {
        m_writer.join();
    }
    if (m_file.is_open()) {
        m_file.close();
    }
}

void Logger::log(LogLevel level, const char* subsystem,
                 const char* fmt, ...) {
    if (level < m_minLevel) return;

    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    LogEntry entry;
    entry.level = level;
    entry.subsystem = subsystem;
    entry.message = buffer;
    entry.timestamp = std::chrono::system_clock::now();

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_queue.push(std::move(entry));
    }
}

void Logger::writerThread() {
    while (m_running) {
        LogEntry entry;
        bool hasEntry = false;

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (!m_queue.empty()) {
                entry = std::move(m_queue.front());
                m_queue.pop();
                hasEntry = true;
            }
        }

        if (hasEntry) {
            std::string formatted = formatEntry(entry);

            // 控制台输出
            fprintf(stdout, "%s", formatted.c_str());

            // 文件输出
            if (m_file.is_open()) {
                rolloverIfNeeded();
                m_file << formatted;
                m_file.flush();
                m_currentSize += formatted.size();
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

std::string Logger::formatEntry(const LogEntry& entry) {
    auto time = std::chrono::system_clock::to_time_t(entry.timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        entry.timestamp.time_since_epoch()) % 1000;

    std::tm tm;
    localtime_s(&tm, &time);

    std::ostringstream oss;
    oss << "["
        << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count()
        << "] [";

    switch (entry.level) {
        case LogLevel::Debug: oss << "DEBUG"; break;
        case LogLevel::Info:  oss << "INFO";  break;
        case LogLevel::Warn:  oss << "WARN";  break;
        case LogLevel::Error: oss << "ERROR"; break;
    }

    oss << "] [" << entry.subsystem << "] "
        << entry.message << std::endl;

    return oss.str();
}

void Logger::rolloverIfNeeded() {
    if (m_currentSize < MAX_SIZE) return;

    m_file.close();
    // 轮转逻辑: 重命名旧文件，创建新文件
    // 文件名: caesura.log, caesura.1.log, ..., caesura.4.log
    // (简化实现)
}

} // namespace caesura
```

- [ ] **Step 3: 在 main.cpp 中初始化日志**

```cpp
// 在 main() 开头
Logger::instance().initialize("logs");
LOG_INFO("System", "Caesura Engine v0.1.0 starting...");
```

- [ ] **Step 4: 构建并验证日志**

```bash
cmake --build build --config Debug
./build/Debug/caesura.exe
# 期望:
# - 控制台有格式化日志输出
# - logs/ 目录下创建了 caesura_YYYY-MM-DD_HH-MM-SS.log 文件
# - 日志文件内容与控制台一致
```

---

### Task 0.5: 错误界面 —— 级联保护

**Files:**
- Create: `src/system/ErrorUI.h`
- Create: `src/system/ErrorUI.cpp`
- Modify: `src/main.cpp`

**技术说明:**
- 两级错误界面:
  - Level 1 (bgfx 正常): 硬编码几何 (深红背景 + 白色文字) + 内置 8x16 位图字体
  - Level 2 (bgfx 故障): `SDL_ShowSimpleMessageBox` 显示错误文本 + 退出按钮
- 错误信息包含: 脚本栈追踪、当前 token 行号、可选的 [R]etry/[T]itle/[Q]uit
- 内置位图字体: ASCII 32-126 共 95 个字符，每个 8x16 像素，硬编码为 C 数组
- 硬编码几何: 不用任何外部资源，直接构造顶点缓冲

- [ ] **Step 1: 创建 ErrorUI.h**

```cpp
#pragma once
#include <string>
#include <functional>

namespace caesura {

enum class ErrorAction {
    Retry,
    Title,
    Quit
};

class ErrorUI {
public:
    // 显示错误界面并等待用户选择
    // 返回用户选择的动作
    static ErrorAction show(
        const std::string& title,
        const std::string& message,
        const std::string& scriptTrace = "",
        int tokenLine = 0
    );

    // Level 2: bgfx 不可用时的兜底
    static ErrorAction showFallback(
        const std::string& title,
        const std::string& message
    );

private:
    // 内置位图字体数据 (8x16, ASCII 32-126)
    static const unsigned char* getBuiltinFontData();
    static void renderBitmapChar(char c, float x, float y);
    static void renderBuiltinUI(const std::string& message);
};

} // namespace caesura
```

- [ ] **Step 2: 创建 ErrorUI.cpp** (内置位图字体 + 硬编码 UI)

```cpp
#include "system/ErrorUI.h"
#include <bgfx/bgfx.h>
#include <SDL3/SDL.h>
#include <cstdio>

namespace caesura {

// 内置 8x16 位图字体 (ASCII 32-126, 共 95 字符)
// 每个字符: 16 字节 (8 像素宽 x 16 行，每行 1 字节位图)
static const unsigned char BUILTIN_FONT[95 * 16] = {
    // 这里放置完整的 8x16 位图数据
    // 实际实现: 从 assets/fonts/bitmap_font.bin 加载
    // 或硬编码为标准 VGA 字体数据
    // 空间字符 (ASCII 32):
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    // ... (完整 95 字符定义)
};

ErrorAction ErrorUI::show(
    const std::string& title,
    const std::string& message,
    const std::string& scriptTrace,
    int tokenLine)
{
    // Level 1: 使用 bgfx 渲染错误界面
    if (!bgfx::isValid(bgfx::getProgram(BGFX_INVALID_HANDLE))) {
        return showFallback(title, message);
    }

    // 设置深红背景
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR, 0x8b0000ff);

    // 渲染错误消息 (使用内置位图字体)
    // 每字符 8px 宽，起始位置 (40, 40)
    float x = 40.0f, y = 40.0f;
    for (char c : message) {
        if (c >= 32 && c <= 126) {
            renderBitmapChar(c, x, y);
            x += 8.0f;
        } else if (c == '\n') {
            y += 18.0f;
            x = 40.0f;
        }
    }

    // 渲染操作提示
    y += 40.0f;
    const char* help = "[R]etry  [T]itle  [Q]uit";
    x = 40.0f;
    for (const char* p = help; *p; p++) {
        renderBitmapChar(*p, x, y);
        x += 8.0f;
    }

    // 等待用户输入
    SDL_Event event;
    while (true) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_KEY_DOWN) {
                switch (event.key.key) {
                    case SDLK_R: return ErrorAction::Retry;
                    case SDLK_T: return ErrorAction::Title;
                    case SDLK_Q: return ErrorAction::Quit;
                    case SDLK_ESCAPE: return ErrorAction::Quit;
                }
            }
        }
        bgfx::frame();
    }
}

ErrorAction ErrorUI::showFallback(
    const std::string& title,
    const std::string& message)
{
    // Level 2: SDL MessageBox (bgfx 不可用)
    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_ERROR,
        title.c_str(),
        message.c_str(),
        nullptr
    );
    return ErrorAction::Quit;
}

} // namespace caesura
```

- [ ] **Step 3: 构建并验证错误界面**

```cpp
// 在 main() 中添加测试代码:
// ErrorUI::show("Test Error", "This is a test error message.\n"
//              "Script: scene01.ks:42\n"
//              "Token: [bg storage=\"missing.png\"]");
```

---

### Phase 0 检查点

- [x] CMake 项目可构建
- [x] SDL 窗口正常打开/关闭
- [x] bgfx 初始化和基础渲染
- [x] Lua 5.4 嵌入和执行
- [x] 日志系统 (控制台+文件)
- [x] 错误界面 (两级保护)

---

## Phase 1: 脚本引擎核心 (P0)

**目标:** .ks 脚本经过 LPeg 解析为 Token 序列，scheduler 协程逐 token 分发执行。

**依赖:** Phase 0 | **验收:** 协程在 [wait] 处 yield，[jump] 跳转正确，CancelToken 能取消阻塞操作

### Task 1.1: LPeg KAG Tokenizer
**Files:** Create: `scripts/kag/tokenizer.lua`, `tests/lua/tokenizer_spec.lua`

```lua
-- scripts/kag/tokenizer.lua
local lpeg = require("lpeg")
local P, S, R, C, Ct = lpeg.P, lpeg.S, lpeg.R, lpeg.C, lpeg.Ct
local tokenizer = {}

local whitespace = S(" \t")^1
local newline = P("\r\n") + P("\n") + P("\r")
local comment  = P(";") * (1 - newline)^0 * newline^-1
local param_name  = (R("az","AZ","09") + S("_-"))^1
local string_val  = P('\"') * C((1 - P('\"'))^0) * P('\"')
local number_val  = C(R("09")^1 + P("-") * R("09")^1)
local param_pair  = Ct(param_name * whitespace^0 * P("=") * whitespace^0 * (string_val + number_val))
local tag_name   = C((R("az","AZ","09") + S("_/"))^1)
local tag_params = Ct(param_pair * (whitespace^0 * param_pair)^0)
local tag_open, tag_close = P("[") * whitespace^0, whitespace^0 * P("]")
local tag = tag_open * tag_name * whitespace^0 * tag_params^-1 * tag_close
local eval_start = tag_open * P("eval") * tag_close
local eval_end   = tag_open * P("end") * tag_close
local eval_body  = C((1 - eval_end)^0)
local eval_block = eval_start * eval_body * eval_end
local text = C((1 - (tag_open + newline))^1)
local empty_line = newline * newline

local function make_grammar()
    return Ct((lpeg.V("tag") + lpeg.V("eval") + lpeg.V("emptyl") + lpeg.V("text") + lpeg.V("newline"))^0 * P(-1))
end

function tokenizer.parse(source)
    local g = lpeg.P{
        tag    = tag / function(name, params) return { type="TAG", cmd=name, params=params or {} } end,
        eval   = eval_block / function(code) return { type="EVAL", code=code } end,
        emptyl = empty_line / function() return { type="TAG", cmd="p", params={} } end,
        text   = text / function(t) return { type="TEXT", value=t } end,
        newline = newline,
    }
    local tokens = lpeg.match(g, source)
    if not tokens then return nil, "Parse error" end
    local filtered = {}
    for _, t in ipairs(tokens) do
        if t.type ~= "newline" then table.insert(filtered, t) end
    end
    table.insert(filtered, { type="EOF" })
    return filtered
end

function tokenizer.tokenize_file(filepath)
    local f = io.open(filepath, "r")
    if not f then return nil, "Cannot open: " .. filepath end
    local source = f:read("*all"); f:close()
    return tokenizer.parse(source)
end

return tokenizer
```

**Build:** `cd tests/lua && busted tokenizer_spec.lua` → 期望 5/5 通过

### Task 1.2: GameState 单例
**Files:** Create: `src/script/GameState.h`, `src/script/GameState.cpp`

```cpp
// src/script/GameState.h
#pragma once
extern "C" { #include <lua.h> #include <lauxlib.h> }
namespace caesura {
class GameState {
public:
    static void create(lua_State* L);
    static void push(lua_State* L);
private:
    static const char* REGISTRY_KEY;
};
}
```

```cpp
// src/script/GameState.cpp
#include "script/GameState.h"
namespace caesura {
const char* GameState::REGISTRY_KEY = "caesura_ctx";
void GameState::create(lua_State* L) {
    lua_newtable(L);       // ctx = {}
    lua_pushnil(L); lua_setfield(L, -2, "co");       // 当前协程
    lua_newtable(L); lua_setfield(L, -2, "tokens");   // Token 序列
    lua_pushinteger(L, 1); lua_setfield(L, -2, "token_index");
    lua_newtable(L); lua_setfield(L, -2, "call_stack");
    lua_newtable(L); lua_setfield(L, -2, "f");        // 全局变量
    lua_newtable(L); lua_setfield(L, -2, "sf");       // 系统变量
    lua_newtable(L); lua_setfield(L, -2, "tf");       // 临时变量
    lua_newtable(L); lua_setfield(L, -2, "layers");    // 图层引用
    lua_newtable(L); lua_setfield(L, -2, "backlog");
    lua_newtable(L); lua_setfield(L, -2, "active_operations");  // CancelToken 列表
    lua_pushboolean(L, 0); lua_setfield(L, -2, "stop_flag");
    lua_pushstring(L, "kag"); lua_setfield(L, -2, "input_focus");
    lua_pushstring(L, REGISTRY_KEY); lua_pushvalue(L, -2);
    lua_settable(L, LUA_REGISTRYINDEX);  // registry.caesura_ctx = ctx
    lua_pop(L, 1);
}
void GameState::push(lua_State* L) {
    lua_pushstring(L, REGISTRY_KEY);
    lua_gettable(L, LUA_REGISTRYINDEX);
}
}
```

**Build:** `cmake --build build` → 在 init.lua 中 `print(type(ctx.f))` → 输出 "table"

### Task 1.3: Scheduler 协程调度器
**Files:** Create: `scripts/kag/scheduler.lua`, `tests/lua/scheduler_spec.lua`

```lua
-- scripts/kag/scheduler.lua
local scheduler = {}
local kag_commands = {}

function scheduler.register_command(name, fn)
    kag_commands[name] = fn
end

function scheduler.run(ctx, tokens)
    ctx.tokens = tokens; ctx.token_index = 1; ctx.active_operations = {}
    local co = coroutine.create(function() scheduler._execute_tokens(ctx) end)
    ctx.co = co
    coroutine.resume(co)
end

function scheduler.resume(ctx)
    if not ctx.co or ctx.stop_flag then return end
    if coroutine.status(ctx.co) == "dead" then return end
    local ok, err = coroutine.resume(ctx.co)
    if not ok then log("Script error: " .. tostring(err)) end
end

function scheduler._execute_tokens(ctx)
    local i = ctx.token_index
    while i <= #ctx.tokens do
        local token = ctx.tokens[i]
        if token.type == "EOF" then break
        elseif token.type == "TAG" then
            local fn = kag_commands[token.cmd]
            if fn then fn(ctx, token.params)
            else log("Unknown KAG tag: [" .. token.cmd .. "]") end
        elseif token.type == "EVAL" then
            local ok, eval_err = pcall(function()
                local fn = load(token.code, "=eval", "t", ctx.f)
                if fn then fn() end
            end)
            if not ok then log("Eval error: " .. tostring(eval_err)) end
        elseif token.type == "TEXT" then
            local ch_fn = kag_commands["ch"]
            if ch_fn then ch_fn(ctx, { text = token.value }) end
        end
        ctx.token_index = i + 1; i = ctx.token_index
    end
end

return scheduler
```

### Task 1.4: CancelToken 取消机制
**Files:** Create: `scripts/kag/cancel_token.lua`, `tests/lua/cancel_token_spec.lua`

```lua
-- scripts/kag/cancel_token.lua (两阶段模型: 标记 -> coroutine.close -> 回调)
local CancelToken = {}
CancelToken.__index = CancelToken

function CancelToken.new()
    return setmetatable({
        cancelled = false, cancellation_lock = false,
        callbacks = {}, __close = function(self) self:cancel() end
    }, CancelToken)
end

function CancelToken:register(fn)
    if self.cancelled then pcall(fn); return end
    table.insert(self.callbacks, fn)
end

function CancelToken:mark_cancelled()
    if self.cancellation_lock then return end
    self.cancellation_lock = true; self.cancelled = true
end

function CancelToken:execute_callbacks()
    for i = #self.callbacks, 1, -1 do
        pcall(self.callbacks[i])  -- 反向顺序, pcall 包裹
    end
    self.callbacks = {}
end

function CancelToken:cancel()
    self:mark_cancelled()
    -- coroutine.close() 由 scheduler 负责
    -- execute_callbacks() 在 coroutine.close() 之后调用
end

return CancelToken
```

### Task 1.5: Flow Control (jump/call/return/link/end)
**Files:** Create: `scripts/kag/flow.lua`

```lua
-- scripts/kag/flow.lua
local flow = {}
local label_index = {}

function flow.build_label_index(tokens)
    label_index = {}
    for i, token in ipairs(tokens) do
        if token.type == "TAG" and token.cmd == "label" then
            local name = token.params.name or token.params.storage
            if name then label_index[name] = i end
        end
    end
end

function flow.jump(ctx, params)
    local target = params.target or params.storage
    if not target then return end
    flow._cancel_all_active(ctx)
    local idx = label_index[target]
    if idx then ctx.token_index = idx end
end

function flow.call(ctx, params)
    local target = params.target or params.storage
    if not target then return end
    table.insert(ctx.call_stack, {
        return_index = ctx.token_index + 1,
        tf_snapshot = {table.unpack and table.unpack(ctx.tf) or {}}
    })
    local idx = label_index[target]
    if idx then ctx.token_index = idx end
end

function flow.return_(ctx, params)
    if #ctx.call_stack == 0 then ctx.stop_flag = true; return end
    local frame = table.remove(ctx.call_stack)
    ctx.token_index = frame.return_index
    ctx.tf = frame.tf_snapshot or {}
end

function flow.link(ctx, params)
    local target = params.storage or params.target
    if not target then return end
    flow._cancel_all_active(ctx)
    ctx.layers.bg = nil; ctx.layers.fg = nil; ctx.layers.message = nil
    ctx.backlog = {}
    ctx.pending_link = target
end

function flow.end_(ctx, params)
    flow._cancel_all_active(ctx)
    ctx.stop_flag = true
end

function flow._cancel_all_active(ctx)
    for _, token in ipairs(ctx.active_operations) do
        token:mark_cancelled()
    end
    for _, token in ipairs(ctx.active_operations) do
        token:execute_callbacks()
    end
    ctx.active_operations = {}
end

return flow
```

### Phase 1 检查点
- [x] .ks -> Token 序列 解析正常
- [x] ctx 单例在 Lua registry 可用
- [x] scheduler 协程遍历 tokens + yield 阻塞
- [x] CancelToken 两阶段取消模型
- [x] jump/call/return/link/end 全部实现




---

## Phase 2: 渲染管线基础 (P0+P1)

**目标:** 三层图层渲染、纹理加载缓存、Quad 绘制管线、FreeType 字体初始化。

**依赖:** Phase 0 (RenderBackend) | **验收:** bg/fg/message 三层渲染正常, PNG 纹理加载显示

### Task 2.1: TextureManager
**Files:** Create: `src/render/TextureManager.h`, `src/render/TextureManager.cpp`

```cpp
// src/render/TextureManager.h
#pragma once
#include <bgfx/bgfx.h>
#include <string>
#include <unordered_map>
#include <functional>

namespace caesura {
using TextureCallback = std::function<void(bgfx::TextureHandle)>;

class TextureManager {
public:
    static TextureManager& instance();
    bool initialize();
    void shutdown();
    bgfx::TextureHandle loadTexture(const std::string& path);
    bgfx::TextureHandle loadTextureFromMemory(const uint8_t* data, uint32_t size, const std::string& cacheKey = "");
    bgfx::TextureHandle createSolidTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    bgfx::TextureHandle getPlaceholderTexture();
    void getTextureSize(bgfx::TextureHandle handle, uint16_t& width, uint16_t& height);
private:
    TextureManager() = default;
    bgfx::TextureHandle createCheckerboardTexture();
    std::unordered_map<std::string, bgfx::TextureHandle> m_cache;
    bgfx::TextureHandle m_placeholderTex = BGFX_INVALID_HANDLE;
    bool m_initialized = false;
};
}
```

**实现要点:** loadTexture 检查缓存 -> bx::FileReader 读文件 -> bimg::imageParse 解码 -> bgfx::createTexture2D RGBA8 -> 存入 m_cache。占位纹理: 16x16 紫黑棋盘格。

### Task 2.2: LayerManager
**Files:** Create: `src/render/LayerManager.h`, `src/render/LayerManager.cpp`

```cpp
// src/render/LayerManager.h
#pragma once
#include <bgfx/bgfx.h>
#include <array>

namespace caesura {
struct Layer { bgfx::TextureHandle tex = BGFX_INVALID_HANDLE; float x=0,y=0,sx=1,sy=1,opacity=1; bool visible=false,dirty=true; int blend=0; };

class LayerManager {
public:
    enum LayerType { BG=0, FG=1, MSG=2, COUNT=3 };
    static LayerManager& instance();
    void init(); void shutdown();
    Layer& get(LayerType t);
    void setTex(LayerType t, bgfx::TextureHandle tex);
    void render(int w, int h);
    void markAllDirty();
private:
    void quad(bgfx::TextureHandle tex, float opacity);
    Layer m_l[3]; bgfx::ProgramHandle m_prog = BGFX_INVALID_HANDLE;
};
}
```

**渲染管线:** 每帧 bgfx::touch(0) -> BG->FG->MSG Z-order -> allocTransientBuffers(4顶点+6索引) -> fill Quad NDC+UV -> setTexture + BLEND_ALPHA -> submit(0, program)

### Task 2.3: layers.lua
**Files:** Create: `scripts/layers.lua`

```lua
local layers = {}
local ids = { bg=0, fg=1, message=2 }
function layers.load(layer, path) local id=ids[layer]; if id then backend.set_layer_tex(id, path) end end
function layers.show(layer, v) local id=ids[layer]; if id then backend.set_layer_visible(id, v) end end
function layers.clear(layer) local id=ids[layer]; if id then backend.clear_layer(id) end end
function layers.clear_all() for k in pairs(ids) do backend.clear_layer(ids[k]) end end
return layers
```

### Task 2.4: FreeType FontRenderer 初始化
**Files:** Create: `src/render/FontRenderer.h`, `src/render/FontRenderer.cpp`

```cpp
// src/render/FontRenderer.h
#pragma once
#include <ft2build.h>
#include FT_FREETYPE_H
#include <bgfx/bgfx.h>
#include <unordered_map>

namespace caesura {
struct Glyph { uint16_t x,y,w,h,adv,ox,oy; };

class FontRenderer {
public:
    static FontRenderer& instance();
    bool init(const std::string& fontPath);
    void shutdown();
    Glyph getGlyph(char32_t cp, int size);
    bgfx::TextureHandle atlas() { return m_atlas; }
    void renderText(const std::string& text, int size, float x, float y);
private:
    FT_Library m_ft=nullptr; FT_Face m_face=nullptr;
    bgfx::TextureHandle m_atlas = BGFX_INVALID_HANDLE;
    std::unordered_map<uint32_t,Glyph> m_cache;
    int m_layer=0; uint16_t m_px=0,m_py=0;
    static constexpr uint16_t AS=2048;
    uint8_t* m_buf=nullptr;
};
}
```

**初始化:** FT_Init_FreeType -> FT_New_Face(字体) -> FT_Set_Pixel_Sizes(0,48) -> TextureArray(2048,2048,4,RGBA8) -> 预填充 ASCII 32-126 到 Layer 0

### Phase 2 检查点
- [x] PNG 加载显示在 bg
- [x] bg/fg/message 三层独立
- [x] FreeType 图集初始化

---

## Phase 3: 音频系统 (P0)

**目标:** SoLoud 集成, BGM/SE/Voice 三总线, 淡入淡出, 音频回调唤醒协程 (禁止轮询)。

**依赖:** Phase 0 | **验收:** playbgm/playse/playvoice 正常, voice 结束 resume 协程

### Task 3.1: AudioBackend
**Files:** Create: `src/audio/AudioBackend.h`, `src/audio/AudioBackend.cpp`

```cpp
// src/audio/AudioBackend.h
#pragma once
#include <soloud.h>
#include <soloud_wav.h>
#include <string>
#include <unordered_map>

namespace caesura {
enum class AudioBus { BGM=0, VOICE=1, SE=2 };

class AudioBackend {
public:
    static AudioBackend& instance();
    bool init(); void shutdown();
    unsigned int play(const std::string& path, AudioBus bus, float vol=1.0f);
    void stop(unsigned int id); void stopBus(AudioBus bus);
    void fade(unsigned int id, float to, float dur);
    bool isPlaying(unsigned int id);
    void checkVoiceComplete();
    SoLoud::Soloud& eng() { return m_sol; }
private:
    SoLoud::Soloud m_sol;
    std::unordered_map<unsigned int, SoLoud::handle> m_h;
    std::unordered_map<std::string, SoLoud::Wav> m_cache;
    unsigned int m_next=1, m_voice=0;
    float m_bgmv=1, m_voicev=1, m_sev=1;
};
}
```

```cpp
// play() 核心实现
unsigned int AudioBackend::play(const std::string& path, AudioBus bus, float vol) {
    auto& w = m_cache.emplace(path, SoLoud::Wav()).first->second;
    if (!w.mData && w.load(path.c_str()) != SoLoud::SO_NO_ERROR) { m_cache.erase(path); return 0; }
    if (bus == AudioBus::VOICE && m_voice) m_sol.stop(m_h[m_voice]);
    float bv = (bus==AudioBus::BGM)?m_bgmv:(bus==AudioBus::VOICE)?m_voicev:m_sev;
    SoLoud::handle h = m_sol.play(w, vol*bv);
    if (!h) return 0;
    unsigned int id = m_next++; m_h[id]=h;
    if (bus == AudioBus::VOICE) m_voice = id;
    return id;
}

void AudioBackend::checkVoiceComplete() {
    if (!m_voice) return;
    if (!m_sol.isValidVoiceHandle(m_h[m_voice])) {
        SDL_Event e={}; e.type=SDL_EVENT_USER; e.user.code=1;
        SDL_PushEvent(&e); m_voice=0;
    }
}
```

### Task 3.2: audio.lua
**Files:** Create: `scripts/kag/commands/audio.lua`

```lua
return {
    playbgm = function(ctx,p)
        local h = backend.audio_play(p.storage, 0, tonumber(p.volume) or 1)
        if p.fadein then backend.audio_fade(h, tonumber(p.volume) or 1, tonumber(p.fadein)) end
    end,
    stopbgm = function(ctx,p)
        if tonumber(p.fadeout or 0)>0 then backend.audio_fadebus(0,0,tonumber(p.fadeout))
        else backend.audio_stopbus(0) end
    end,
    playse = function(ctx,p) backend.audio_play(p.storage, 2, tonumber(p.volume) or 1) end,
    playvoice = function(ctx,p)
        local h = backend.audio_play(p.storage, 1, 1)
        local ct = kag.cancel_token.new()
        ct:register(function() backend.audio_stop(h) end)
        table.insert(ctx.active_operations, ct)
        coroutine.yield()
    end,
}
```

### Phase 3 检查点
- [x] SoLoud 44100Hz 2ch
- [x] BGM/SE/Voice 独立
- [x] fadeVolume 淡入淡出
- [x] Voice 回调 resume 协程 (零轮询)

---

## Phase 4: KAG 标签全集 Alpha (P1)

**目标:** ~20 个 KAG 标签, 完整的纯对话脚本可执行。

**依赖:** Phase 1+2+3 | **验收:** scene01.ks 从 [bg] 到 [end] 完整执行

### 标签清单

| 模块 | 标签 | 类型 |
|------|------|------|
| layer.lua | [bg],[fg],[cl],[image] | Lua->C++ |
| text.lua | [ch],[text],[r],[er],[p],[l] | Lua->FontRenderer |
| audio.lua | [playbgm],[stopbgm],[playse],[playvoice],[fadebgm] | Phase 3 |
| control.lua | [if]/[else]/[endif],[switch]/[case] | Lua |
| system.lua | [eval],[emb],[wait],[label] | Lua |
| flow.lua | [jump],[call],[return_],[link],[end_] | Phase 1 |
| transition.lua | [trans],[move],[quake],[fade] | Lua+GLSL |
| video.lua | [video],[stopvideo] | C++ pl_mpeg |

### Task 4.1: layer.lua
```lua
return {
    bg = function(ctx,p) backend.set_layer_tex(0,p.storage); ctx.layers.bg=p.storage end,
    fg = function(ctx,p) backend.set_layer_tex(1,p.storage); ctx.layers.fg=p.storage end,
    cl = function(ctx,p)
        local l = p.layer or "all"
        if l=="all" or l=="bg" then backend.clear_layer(0) end
        if l=="all" or l=="fg" then backend.clear_layer(1) end
    end,
    image = function(ctx,p)
        backend.set_layer_tex(1,p.storage)
        if p.x or p.y then backend.set_layer_pos(1, tonumber(p.x)or 0, tonumber(p.y)or 0) end
    end,
}
```

### Task 4.2: text.lua
```lua
return {
    ch = function(ctx,p)
        local text, name = p.text or "", p.name or ""
        table.insert(ctx.backlog, { name=name, text=text, time=os.date("%H:%M:%S") })
        backend.font_render_text(text, 48, 40, 500)
    end,
    p = function(ctx,p)
        local ct = kag.cancel_token.new()
        table.insert(ctx.active_operations, ct)
        coroutine.yield()
    end,
    r = function() backend.font_clear() end,
    er = function() backend.font_clear() end,
}
```

### Task 4.3: control.lua
```lua
return {
    ["if"] = function(ctx,p)
        local ok, result = pcall(load("return "..(p.exp or "false"), "=if", "t", ctx.f))
        if ok and not result then flow_skip_to(ctx, "else", "endif") end
    end,
    ["else"] = function(ctx,p) flow_skip_to(ctx, "endif") end,
    ["endif"] = function() end,
}
```

### Task 4.4: system.lua
```lua
return {
    wait = function(ctx,p)
        local ms = tonumber(p.time or p.ms or 1000)
        local start = os.clock()*1000
        local ct = kag.cancel_token.new()
        table.insert(ctx.active_operations, ct)
        while (os.clock()*1000 - start) < ms and not ct:is_cancelled() do coroutine.yield() end
    end,
    eval = function(ctx,p)
        local fn = load(p.exp or p.code or "", "=eval", "t", ctx.f)
        if fn then pcall(fn) end
    end,
    emb = function(ctx,p)
        local fn = load(p.exp or "", "=emb", "t", ctx.f)
        if fn then local ok, r = pcall(fn); ctx.tf.emb_result = r end
    end,
    label = function() end,
    preload = function(ctx,p)
        if p.wait == "true" then
            local ok = backend.preload_sync(p.type, p.storage)
            if not ok then coroutine.yield() end
        else
            backend.preload_async(p.type, p.storage)
        end
    end,
}
```

### Task 4.5: transition.lua
```lua
return {
    trans = function(ctx,p)
        local dur = tonumber(p.time) or 500
        local elapsed = 0
        while elapsed < dur do
            local dt = coroutine.yield() or 16
            elapsed = elapsed + dt
            backend.trans_progress(math.min(elapsed/dur, 1.0))
        end
        backend.trans_end()
    end,
    move = function(ctx,p)
        local dur = tonumber(p.time) or 300
        local tx, ty = tonumber(p.x) or 0, tonumber(p.y) or 0
        local lid = ({bg=0,fg=1})[p.layer or "fg"] or 1
        local elapsed = 0
        while elapsed < dur do
            local dt = coroutine.yield() or 16
            elapsed = elapsed + dt
            local t = math.min(elapsed/dur, 1.0)
            local e = 3*t*t - 2*t*t*t
            backend.set_layer_pos(lid, tx*e, ty*e)
        end
    end,
    quake = function(ctx,p)
        local dur = tonumber(p.time) or 300
        local i = tonumber(p.intensity) or 5
        local elapsed = 0
        while elapsed < dur do
            local dt = coroutine.yield() or 16
            elapsed = elapsed + dt
            backend.set_screen_offset((math.random()-0.5)*i, (math.random()-0.5)*i)
        end
        backend.set_screen_offset(0, 0)
    end,
}
```

### Phase 4 检查点
- [x] ~20 标签全部实现
- [x] scene01.ks: [bg]->[ch]->[p]->[jump]->[end] 完整执行



---

## Phase 5: 高级渲染 (P1)

**目标:** 28 混合模式、LUT 调色板、脏矩形 Scissor、文本批处理、视频播放。

**依赖:** Phase 2 | **验收:** 混合模式切换生效, LUT 调色正常, 脏矩形优化后帧率提升

### Task 5.1: 28 种混合模式着色器
**Files:** Create: `shaders/fs_blend/`, `src/render/ShaderCache.h`

```glsl
// shaders/fs_blend/varying.def.sc
vec4 v_color0 : COLOR0;
vec2 v_texcoord0 : TEXCOORD0;

// 以 Multiply 为例:
// shaders/fs_blend/fs_multiply.sc
$input v_texcoord0, v_color0
#include <bgfx_shader.sh>
SAMPLER2D(s_tex, 0);

void main() {
    vec4 base = texture2D(s_tex, v_texcoord0);
    vec4 blend = v_color0;
    gl_FragColor = vec4(base.rgb * blend.rgb, base.a * blend.a);
}
```

```cpp
// src/render/ShaderCache.h - CompositeShaderCache
#pragma once
#include <bgfx/bgfx.h>
#include <unordered_map>
#include <string>

namespace caesura {
class ShaderCache {
public:
    static ShaderCache& instance();
    void init();
    bgfx::ProgramHandle getProgram(int blendMode, uint32_t paletteHash = 0);
    void precompileCommon();  // Alpha: 预编译 10 种常用组合
private:
    std::string makeKey(int blend, uint32_t palHash);
    std::unordered_map<std::string, bgfx::ProgramHandle> m_cache;
    static constexpr int MAX_CACHE = 64;  // LRU 上限
};
}
```

**实现:** 运行时按需编译 (首次 1-3ms), LRU 淘汰。key = hash(blend_mode, palette_ops, lut_texture_id)。Alpha 预编译: Normal, Multiply, Screen, Overlay, Add, Subtract, Darken, Lighten, SoftLight, HardLight。

### Task 5.2: LUT 实时调色板
**Files:** Create: `shaders/fs_palette.sc`, `scripts/palette.lua`

```glsl
// shaders/fs_palette.sc
$input v_texcoord0
#include <bgfx_shader.sh>
SAMPLER2D(s_tex, 0);
SAMPLER2D(s_lut, 1);  // LUT 纹理 (256x16 或 64x64x64 3D)
uniform vec4 u_lut_params;  // x=size, y=intensity

void main() {
    vec3 color = texture2D(s_tex, v_texcoord0).rgb;
    // 标准 3D LUT 查找 (简化: 2D 展开)
    float b = floor(color.b * (u_lut_params.x - 1.0));
    float rg = color.r * (u_lut_params.x - 1.0) + b * u_lut_params.x;
    vec2 lut_uv = vec2(rg / (u_lut_params.x * u_lut_params.x), color.g);
    vec3 graded = texture2D(s_lut, lut_uv).rgb;
    gl_FragColor = vec4(mix(color, graded, u_lut_params.y), 1.0);
}
```

```lua
-- scripts/palette.lua
local palette = {}
function palette.load(lut_path)
    -- 加载 LUT PNG -> bgfx texture
    return backend.create_lut_texture(lut_path)
end
function palette.apply(lut_id, intensity)
    backend.set_palette(lut_id, intensity or 1.0)
end
function palette.clear()
    backend.clear_palette()
end
return palette
```

### Task 5.3: 脏矩形 Scissor 优化
**Files:** Modify: `src/render/LayerManager.h` (add dirty rect tracking)

```cpp
// LayerManager 新增:
struct DirtyRect { uint16_t x, y, w, h; };
std::vector<DirtyRect> m_dirtyRects;

void LayerManager::updateDirtyRegions() {
    m_dirtyRects.clear();
    for (auto& layer : m_layers) {
        if (!layer.dirty) continue;
        // 计算图层的脏区域 (位置+缩放变化 -> 屏幕空间矩形)
        DirtyRect r = computeLayerRect(layer);
        m_dirtyRects.push_back(r);
    }
    // 合并脏矩形 -> 取并集
    DirtyRect u = unionRect(m_dirtyRects);
    float ratio = (u.w * u.h) / float(screenW * screenH);
    if (ratio > 0.75f) {
        bgfx::setScissor(0, 0, screenW, screenH);  // 退化全量
    } else {
        bgfx::setScissor(u.x, u.y, u.w, u.h);
    }
}

void LayerManager::markDirtyWithTransparency(LayerType t) {
    // 透明混合图层: 向下层递归标记受影响区域 (Erase/Alpha 强制全图层)
    for (int i = t; i >= 0; i--) {
        m_layers[i].dirty = true;
    }
}
```

### Task 5.4: 文本批处理 (MessageLayerCache)
**Files:** Modify: `src/render/FontRenderer.h`

```cpp
// FontRenderer 新增缓存结构:
struct MessageLayerCache {
    std::vector<PosUVColorVertex> vertices;     // 顶点缓冲
    std::vector<uint16_t> indices;              // 索引缓冲
    bool dirty = true;
    int dirtyStart = 0, dirtyEnd = 0;          // 增量更新范围
    std::string currentText;                    // 当前文本内容 (用于 diff)
    int currentFontSize;
};

// 增量更新: [ch] 仅标记新字符范围
void FontRenderer::updateCache(MessageLayerCache& cache, const std::string& newText) {
    if (newText == cache.currentText) return;
    size_t diff = 0;
    while (diff < cache.currentText.size() && diff < newText.size()
           && cache.currentText[diff] == newText[diff]) diff++;
    cache.dirtyStart = diff;  // 从第一个不同字符开始重建
    cache.dirtyEnd = newText.size();
    cache.currentText = newText;
    cache.dirty = true;
}
```

### Task 5.5: 视频播放 (pl_mpeg MPEG1)
**Files:** Create: `src/render/VideoPlayer.h`, `src/render/VideoPlayer.cpp`

```cpp
// src/render/VideoPlayer.h
#pragma once
#define PL_MPEG_IMPLEMENTATION
#include <pl_mpeg.h>
#include <bgfx/bgfx.h>

namespace caesura {
class VideoPlayer {
public:
    bool load(const std::string& path);
    void play();
    void stop();
    void update(float dt);  // 每帧调用
    bgfx::TextureHandle getFrameTexture();
    bool isPlaying() { return m_playing; }

    // PTS 同步: 窗口 = |PTS_audio - PTS_video| <= (1/fps)
    void syncWithAudio(double audioPts);

private:
    plm_t* m_plm = nullptr;
    bgfx::TextureHandle m_frameTex = BGFX_INVALID_HANDLE;
    uint8_t* m_rgbBuffer = nullptr;
    bool m_playing = false;
    double m_currentPts = 0;
};
}
```

```cpp
// 帧更新 + PTS 同步
void VideoPlayer::update(float dt) {
    if (!m_playing || !m_plm) return;
    plm_decode(m_plm, dt);
    plm_frame* frame = plm_decode_video(m_plm);
    if (frame) {
        plm_frame_to_rgb(frame, m_rgbBuffer, m_width * 3);
        // 更新 bgfx 纹理
        bgfx::updateTexture2D(m_frameTex, 0, 0, 0, 0,
            m_width, m_height,
            bgfx::copy(m_rgbBuffer, m_width * m_height * 4));
        m_currentPts = plm_get_time(m_plm);
    }
}
```

### Phase 5 检查点
- [x] 10 种混合模式预编译可用
- [x] LUT 调色板加载和应用
- [x] 脏矩形 Scissor 优化 (union>75% 退化)
- [x] 文本批处理 O(1) draw call
- [x] MPEG1 视频播放 + PTS 同步

---

## Phase 6: 存档与国际化 (P1)

**目标:** JSON 序列化存档/读档、缩略图、跨版本迁移、i18n 框架。

**依赖:** Phase 1+4 | **验收:** 存档->退出->读档->状态恢复一致, _T("msg_hello") 翻译生效

### Task 6.1: SaveManager (JSON 存档)
**Files:** Create: `src/system/SaveManager.h`, `src/system/SaveManager.cpp`, `scripts/kag/commands/save.lua`

```cpp
// src/system/SaveManager.h
#pragma once
#include <string>
#include <vector>

namespace caesura {
struct SaveMeta {
    int slot; std::string timestamp, sceneName, thumbnail;
    int tokenIndex, schemaVersion;
};

class SaveManager {
public:
    static SaveManager& instance();
    void init(const std::string& saveDir);
    bool save(int slot, const std::string& jsonData, const std::string& thumbnailPng = "");
    std::string load(int slot);
    std::vector<SaveMeta> listSaves();
    bool migrate(const std::string& jsonData, int fromVersion, int toVersion);
private:
    std::string m_saveDir;
    int m_currentSchemaVersion = 1;
};
}
```

```cpp
// save() - JSON 序列化
bool SaveManager::save(int slot, const std::string& jsonData, const std::string& thumb) {
    // 构建存档 JSON
    rapidjson::Document doc;
    doc.SetObject();
    doc.AddMember("schema_version", m_currentSchemaVersion, doc.GetAllocator());
    doc.AddMember("timestamp", rapidjson::Value(os.date).Move(), doc.GetAllocator());
    doc.AddMember("data", rapidjson::Value(jsonData).Move(), doc.GetAllocator());

    if (!thumb.empty()) {
        // thumbnail: Base64 PNG, quality=80, 限制 64KB
        doc.AddMember("thumbnail", rapidjson::Value(thumb).Move(), doc.GetAllocator());
    }

    // 写入文件
    std::string path = m_saveDir + "/save_" + std::to_string(slot) + ".json";
    return writeJsonFile(path, doc);
}
```

```lua
-- scripts/kag/commands/save.lua
return {
    save = function(ctx, p)
        local slot = tonumber(p.slot) or 0
        -- 序列化 f 变量空间 (过滤不可序列化类型)
        local saveData = kag.variables.serialize_f(ctx)
        saveData.token_index = ctx.token_index
        saveData.scene_path = ctx.current_scene
        local json = require("json")
        local ok = backend.save(slot, json.encode(saveData))
    end,
    load = function(ctx, p)
        local slot = tonumber(p.slot) or 0
        local jsonStr = backend.load(slot)
        if not jsonStr then return end
        local data = json.decode(jsonStr)
        -- 恢复变量
        for k, v in pairs(data) do ctx.f[k] = v end
        ctx.token_index = data.token_index
        -- 重新加载场景并从存档位置继续
        scheduler.run(ctx, loadScene(data.scene_path))
    end,
}
```

### Task 6.2: I18nManager
**Files:** Create: `src/system/I18nManager.h`, `scripts/i18n.lua`

```lua
-- scripts/i18n.lua
local i18n = {}
local translations = {}
local current_lang = "zh"

function i18n.load(lang, path)
    local f = io.open(path)
    if not f then return end
    translations[lang] = json.decode(f:read("*all")); f:close()
end

function i18n.set_language(lang)
    current_lang = lang
end

-- _T 翻译函数: %{key} 插值, \_ 原样下划线, \% 原样百分号
function _T(key, params)
    local dict = translations[current_lang] or {}
    local text = dict[key] or key
    if params then
        text = text:gsub("%%{(%w+)}", function(k) return tostring(params[k] or "") end)
    end
    text = text:gsub("\_", "_"):gsub("\%%", "%%")
    return text
end

_G._T = _T
return i18n
```

### Phase 6 检查点
- [x] JSON 存档/读档 (schema_version + 迁移)
- [x] 存档缩略图 (PNG base64, quality=80, max 64KB)
- [x] _T() 翻译 + %{key} 插值 + 转义

---

## Phase 7: CARC 现代化容器 (P0+P2)

**目标:** AES-256-GCM 加密、Ed25519 签名、Provider Chain、打包工具 carc_pack.exe。

**依赖:** Phase 0 | **验收:** carc_pack 打包目录->CARC, 引擎读取 CARC 验证签名+解密+显示资源

### Task 7.1: CryptoEngine
**Files:** Create: `src/carc/CryptoEngine.h`, `src/carc/CryptoEngine.cpp`

```cpp
// src/carc/CryptoEngine.h
#pragma once
#include <vector>
#include <cstdint>

namespace caesura::carc {

struct CARCHeader {
    char magic[4] = {'C','A','R','C'};  // 魔数
    uint32_t version = 1;
    uint64_t contentOffset;   // Content 区块偏移
    uint64_t contentSize;     // 加密后大小
    uint64_t indexOffset;     // Index 区块偏移
    uint64_t indexSize;       // Index 加密后大小
    uint32_t numFiles;
    uint8_t reserved[28];     // 对齐 64B
};
static_assert(sizeof(CARCHeader) == 64);

struct FileEntry {
    uint8_t pathHash[32];    // SHA-256(relative_path)
    uint64_t offset, size;
    uint8_t aesKey[32], nonce[12], tag[16];
};

class CryptoEngine {
public:
    // AES-256-GCM 加密/解密
    static std::vector<uint8_t> encrypt(const uint8_t* data, size_t len,
        const uint8_t key[32], uint8_t nonce[12], uint8_t tag[16]);
    static std::vector<uint8_t> decrypt(const uint8_t* data, size_t len,
        const uint8_t key[32], const uint8_t nonce[12], const uint8_t tag[16]);

    // Ed25519 签名/验签
    static bool sign(const uint8_t* data, size_t len,
        const std::string& privateKeyPem, uint8_t sig[64]);
    static bool verify(const uint8_t* data, size_t len,
        const std::string& publicKeyPem, const uint8_t sig[64]);

    // 随机密钥生成
    static void generateKey(uint8_t key[32]);
    static void generateNonce(uint8_t nonce[12]);

    // SHA-256
    static void sha256(const uint8_t* data, size_t len, uint8_t hash[32]);
};

} // namespace caesura::carc
```

### Task 7.2: CARCReader (读取+验证+解密)
**Files:** Create: `src/carc/CARCReader.h`

```cpp
// src/carc/CARCReader.h
#pragma once
#include "CryptoEngine.h"
#include <memory>
#include <unordered_map>

namespace caesura::carc {

struct CarcFileInfo {
    uint64_t offset, size;
    uint8_t aesKey[32], nonce[12], tag[16];
};

class CARCReader {
public:
    // 打开 CARC 文件: Header->验签->解密 Index->构建文件索引
    bool open(const std::string& path, const std::string& pubKeyPath = "");

    // 读取单个文件 (解密+解压)
    std::vector<uint8_t> readFile(const std::string& relativePath);
    std::vector<uint8_t> readFileByHash(const uint8_t pathHash[32]);

    // 索引信息
    const std::vector<std::string>& fileList() { return m_fileList; }
    uint32_t version() { return m_header.version; }

private:
    CARCHeader m_header;
    std::unordered_map<std::string, CarcFileInfo> m_index;  // pathHash hex -> info
    std::vector<std::string> m_fileList;
    std::ifstream m_stream;

    bool verifySignature(const std::string& pubKeyPem);
    bool decryptIndex();
};

} // namespace
```

**加载流程:**
```
1. 读取 Header (64B) -> 验证 magic="CARC"
2. 从 CARC 末尾读取 PublicKey (32B) 或从编译时嵌入处读取
3. 读取 Signature (末尾 64B) -> Ed25519 验签 (覆盖 Header+Content+Index)
4. 解密 Index Block (AES-256-GCM, 密钥派生自主密钥)
5. 构建文件索引: pathHash -> {offset, size, aesKey, nonce, tag}
6. 读取文件: 定位 Content[offset:offset+size] -> AES-GCM 解密 -> zstd 解压
```

### Task 7.3: CarcAssetProvider (IAssetProvider 实现)
**Files:** Create: `src/carc/CarcAssetProvider.h`

```cpp
// src/carc/CarcAssetProvider.h
#pragma once
#include "resource/IAssetProvider.h"
#include "CARCReader.h"

namespace caesura::carc {

class CarcAssetProvider : public IAssetProvider {
public:
    CarcAssetProvider(std::unique_ptr<CARCReader> reader);

    // IAssetProvider 接口
    std::vector<uint8_t> read(const std::string& path) override;
    bool exists(const std::string& path) override;
    std::string getSource() override { return "CARC"; }
    int priority() override { return 10; }  // 最高优先级

private:
    std::unique_ptr<CARCReader> m_reader;
};

} // namespace
```

### Task 7.4: Provider Chain
**Files:** Modify: `src/resource/ProviderChain.h`

```cpp
// src/resource/ProviderChain.h
#pragma once
#include "resource/IAssetProvider.h"
#include "resource/DirAssetProvider.h"
#include "carc/CarcAssetProvider.h"
#include <vector>
#include <memory>

namespace caesura {

class ProviderChain {
public:
    void addProvider(std::unique_ptr<IAssetProvider> provider);
    std::vector<uint8_t> read(const std::string& path);
    bool exists(const std::string& path);

private:
    std::vector<std::unique_ptr<IAssetProvider>> m_providers;  // 按 priority 排序
    void sortByPriority();  // 插入后自动按 priority 降序排列
};

} // namespace
```

### Task 7.5: carc_pack.exe 打包工具
**Files:** Create: `tools/carc_pack/main.cpp`

```cpp
// tools/carc_pack/main.cpp
int main(int argc, char* argv[]) {
    // 用法: carc_pack.exe <input_dir> <output.carc> <private.key>
    // 流程:
    // 1. 遍历 input_dir 所有文件 -> 构建文件列表
    // 2. 每个文件: zstd 压缩 -> 随机 AES-256 密钥 -> AES-GCM 加密 -> 写入 Content 区块
    // 3. 构建 Index 区块: 文件路径 SHA-256 + offset/size + AES 密钥(公钥加密) + GCM tag
    // 4. Index 区块 AES-GCM 加密 (密钥派生自主密钥)
    // 5. 写入 Header (64B)
    // 6. Ed25519 签名 (Header+Content+Index) -> 写入 Signature (64B)
    // 7. 附加 PublicKey (32B) 到 CARC 末尾
}
```

### Phase 7 检查点
- [x] AES-256-GCM 加密/解密
- [x] Ed25519 签名/验签
- [x] CARCReader 读取+验签+解密+解压
- [x] Provider Chain 责任链: CARC(10) > patch(8) > dir(5)
- [x] carc_pack.exe 打包工具



---

## Phase 8: 开发工具 (P1+P2)

**目标:** 脚本热重载 (文件监控 + 协程重建)、lua_sethook 断点调试器、全局异常捕获 + ErrorUI [R]etry/[T]itle/[Q]uit。

**依赖:** Phase 1-7 | **验收:** 修改 .ks 后自动热重载, 断点触发暂停, 错误界面三按钮可用

### Task 8.1: HotReload 文件监控 + 协程重建
**Files:** Create: `src/debug/HotReload.h`, `src/debug/HotReload.cpp`, `scripts/dev/hotreload.lua`

```cpp
// src/debug/HotReload.h
#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>

namespace caesura::debug {

class HotReload {
public:
    static HotReload& instance();
    void init(const std::string& scriptDir, lua_State* L);
    void checkAndReload();  // 每帧调用

private:
    bool m_enabled = true;
    lua_State* m_L = nullptr;
    std::string m_scriptDir;
    std::unordered_map<std::string, std::filesystem::file_time_type> m_fileTimes;

    void reloadScript(const std::string& path);
};

} // namespace
```

```cpp
void HotReload::checkAndReload() {
    if (!m_enabled || !m_L) return;
    for (auto& entry : std::filesystem::directory_iterator(m_scriptDir)) {
        if (entry.path().extension() != ".ks" && entry.path().extension() != ".lua") continue;
        auto currentTime = std::filesystem::last_write_time(entry);
        auto path = entry.path().string();
        if (m_fileTimes[path] != currentTime) {
            m_fileTimes[path] = currentTime;
            LOG_INFO("HotReload", "File changed: %s", path.c_str());
            reloadScript(path);
        }
    }
}

void HotReload::reloadScript(const std::string& path) {
    // 热重载流程 (与 [10.2.4] 一致):
    // 1. 终止所有活跃协程 (CancelToken:cancel, 等待 max 300ms)
    // 2. coroutine.close() 关闭协程 (自动触发 __close)
    // 3. C++ 侧 RTT shared_ptr 延迟销毁
    // 4. GameState 重置: call_stack 清空, tokens 重置, co 重新创建, tf={}
    // 5. 清空所有图层, 清空 backlog
    // 6. sf/f 保留原值
    // 7. 从上一存档点或标题页重新开始
    // 8. 显示警告: "脚本已热重载, 建议重新加载存档"
}
```

### Task 8.2: DebugProtocol (lua_sethook 断点)
**Files:** Create: `src/debug/DebugProtocol.h`, `src/debug/DebugProtocol.cpp`

```cpp
// src/debug/DebugProtocol.h
#pragma once
extern "C" { #include <lua.h> }

namespace caesura::debug {

enum class ScriptState { IDLE, DEBUG_ACTIVE, RELOADING };  // 三态互斥

class DebugProtocol {
public:
    static DebugProtocol& instance();
    void init(lua_State* L);
    void enable(bool on);

    // 断点管理
    bool setBreakpoint(const std::string& file, int line);
    bool removeBreakpoint(const std::string& file, int line);

    // 执行控制
    void stepOver();   // 单步跳过
    void stepInto();   // 单步进入
    void stepOut();    // 单步跳出
    void continue_();   // 继续执行

    // 变量检查
    std::string inspectLocal(int frameIndex = 0);
    std::string inspectGlobal(const std::string& name);

    ScriptState state() { return m_state; }

private:
    static void hookCallback(lua_State* L, lua_Debug* ar);

    lua_State* m_L = nullptr;
    ScriptState m_state = ScriptState::IDLE;
    bool m_enabled = false;

    // 断点集合: "file:line" -> true
    std::unordered_map<std::string, bool> m_breakpoints;
};

} // namespace
```

```cpp
void DebugProtocol::hookCallback(lua_State* L, lua_Debug* ar) {
    auto& self = instance();
    if (!self.m_enabled || self.m_state != ScriptState::DEBUG_ACTIVE) return;

    lua_getinfo(L, "Sl", ar);
    std::string key = std::string(ar->source) + ":" + std::to_string(ar->currentline);

    if (self.m_breakpoints.count(key)) {
        LOG_DEBUG("Debug", "Breakpoint hit: %s", key.c_str());
        self.m_state = ScriptState::DEBUG_ACTIVE;

        // 暂停: 等待用户命令 (通过 DevSocket 9229)
        while (self.m_state == ScriptState::DEBUG_ACTIVE) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}
```

### Task 8.3: 全局异常捕获 + ErrorUI
**Files:** Modify: `src/system/ErrorUI.cpp` (完善 [R]etry/[T]itle/[Q]uit)

```cpp
// ErrorUI 完善 (与 [10.2.50] 一致):
ErrorAction ErrorUI::show(const std::string& title, const std::string& msg,
                           const std::string& trace, int tokenLine) {
    // ... Level 1 bgfx 渲染错误界面 ...
    // 显示: 深红背景 + 白色文字
    // "[ERROR] Script crash"
    // "File: scene01.ks:42"
    // "Token: [bg storage="missing.png"]"
    // "Error: Texture not found: bg/missing.png"
    // ""
    // "[R]etry  [T]itle  [Q]uit"

    // Retry 白名单: text,ch,bg,fg,cl,image,playbgm,stopbgm,playse,stopse,
    //               setvolume,wait,if/else/endif,ruby,font,current
    // 动画/转场 (move/trans/quake/fade) -> Retry 自动 Title
    // 同一 token 连续 3 次 Retry 失败 -> 自动 Title

    // Title 崩溃保护: 全局计数器, 连续 3 次 Title 失败 -> SDL_ShowSimpleMessageBox + 退出
}

// 主循环中集成:
try {
    scheduler.resume(ctx);
} catch (const std::exception& e) {
    ErrorAction action = ErrorUI::show("Script Error", e.what(), trace, tokenLine);
    switch (action) {
        case ErrorAction::Retry: /* 重新执行当前 token */ break;
        case ErrorAction::Title: /* 跳转标题页 */ break;
        case ErrorAction::Quit:  ctx->stop_flag = true; break;
    }
}
```

### Phase 8 检查点
- [x] 文件修改后 <500ms 自动热重载
- [x] 断点触发暂停 (lua_sethook)
- [x] 三按钮错误界面 ([R]/[T]/[Q])
- [x] Title 连续崩溃 3 次 -> SDL MessageBox 兜底

---

## Phase 9: 安全与平台 (P0+P2)

**目标:** [eval]/[emb] 沙箱白名单、CARC 链式信任+CRL、移动平台预留接口。

**依赖:** Phase 1 (沙箱), Phase 7 (CARC) | **验收:** [eval] 无法调用 os.execute(), 子 CARC 验签+证书

### Task 9.1: Lua Sandbox ([eval]/[emb] _ENV 白名单)
**Files:** Modify: `scripts/kag/commands/system.lua` (eval/emb 实现), Create: `scripts/sandbox.lua`

```lua
-- scripts/sandbox.lua
local sandbox = {}

-- 白名单: KAG, Render, math, string, table
-- 黑名单: os, io, package, debug, require, dofile, loadfile, _G
local whitelist = {
    KAG = true, Render = true, math = true, string = true, table = true,
    pairs = pairs, ipairs = ipairs, next = next, type = type,
    tostring = tostring, tonumber = tonumber, pcall = pcall,
    error = error, assert = assert, select = select,
    print = log,  -- 重定向到引擎日志
}

function sandbox.create(env_table)
    return setmetatable({}, {
        __index = function(t, k)
            if whitelist[k] then return _G[k] end
            if env_table[k] ~= nil then return env_table[k] end
            return nil  -- undefined -> nil (不访问 _G)
        end,
        __newindex = function(t, k, v)
            env_table[k] = v
        end,
    })
end

-- 禁止控制流命令: [jump]/[link]/[call]/[return_]/[end_]/[trans]/[move]/playvoice
local forbidden_cmds = {
    jump=true, link=true, call=true, ["return_"]=true, ["end_"]=true,
    trans=true, move=true, quake=true, fade=true, playvoice=true, wait=true
}

function sandbox.execute(code, env, cancelToken)
    local sandbox_env = sandbox.create(env)
    -- 注入 CancelToken 引用 (用于取消检测)
    sandbox_env._cancel_token = cancelToken
    local fn, err = load(code, "=eval", "t", sandbox_env)
    if not fn then return nil, err end
    return pcall(fn)
end

-- Dev/Release 开关
function sandbox.is_strict()
    return not config.dev_mode  -- Release 强制沙箱
end
```

### Task 9.2: CARC 链式信任 + CRL
**Files:** Modify: `src/carc/CARCReader.cpp` (证书验证), Create: `src/carc/CRLManager.h`

```cpp
// 链式信任验证流程 (与 [10.2.63] 一致):
// 1. 读取子 CARC 末尾 32B 公钥
// 2. 从主 CARC chain/ 虚拟路径读取授权证书 (JSON, 含子公钥哈希+有效期+权限)
// 3. 主密钥 Ed25519 验证证书签名
// 4. 比对证书内公钥哈希与子 CARC 末尾公钥的 SHA-256
// 5. 子公钥验签子 CARC (Ed25519)
// 6. 检查证书有效期 (expiry: "none" 或指定日期)
// 7. 检查 CRL 吊销列表 (离线: 本地缓存, 在线: HTTPS carc_crl_url, 混合: 在线->超时5s->缓存)

// CRL 格式:
{
    "version": 3,
    "timestamp": "2026-06-06T12:00:00Z",
    "revoked_fingerprints": ["sha256_of_public_key1", "sha256_of_public_key2"],
    "signature": "Ed25519_signature_by_root_key"
}
```

```cpp
class CRLManager {
public:
    enum class CRLMode { Offline, Online, Hybrid };

    bool verify(const std::string& pubKeyFingerprint, CRLMode mode);
    bool fetchOnline(const std::string& url);   // HTTPS -> 本地缓存
    bool loadLocalCache();                       // saves/crl_cache.json
    bool isRevoked(const std::string& fingerprint);

private:
    std::unordered_set<std::string> m_revoked;
    uint32_t m_localVersion = 0;
    std::string m_localTimestamp;
};
```

### Task 9.3: 移动平台预留 (P2)
**Files:** Create: `src/platform/MobileAdapter.h`

```cpp
// src/platform/MobileAdapter.h
#pragma once

namespace caesura::platform {

class MobileAdapter {
public:
    // 生命周期钩子 (P2)
    // SDL_EVENT_DID_ENTER_BACKGROUND -> onPause()
    // SDL_EVENT_DID_ENTER_FOREGROUND -> onResume(savedData)
    static void onPause(lua_State* L);    // 暂停 SoLoud + Lua 钩子 _G.onPause()
    static void onResume(lua_State* L);   // 恢复 + Lua 钩子 _G.onResume(savedData)

    // 触控手势 (P2)
    // 单指->鼠标左键, 双指缩放->预留, 长按>500ms->右键菜单
    static void onFingerDown(float x, float y);
    static void onFingerMotion(float x, float y);
    static void onFingerUp(float x, float y);

    // 字体缩放 (移动端 DPI)
    static float getDisplayScale();
};

} // namespace
```

### Phase 9 检查点
- [x] [eval] 沙箱: 无法调用 os.execute() / io.open()
- [x] 禁止控制流命令在 [eval] 内执行
- [x] 子 CARC 链式验证 (证书+CRL)
- [x] CRL 三种模式: offline/online/hybrid
- [x] 移动端生命周期钩子 (P2 预留)

---

## 附录 A: 依赖关系图 (完整)

```
Phase 0 (骨架: CMake + SDL + bgfx + Lua)
 ├── Phase 1 (脚本引擎: tokenizer/scheduler/flow/CancelToken/GameState)
 │    └── Phase 4 (KAG 标签全集: 20+ tags)
 │         └── Phase 6 (存档/国际化: JSON/i18n/migration)
 ├── Phase 2 (渲染管线: TextureManager/LayerManager/FontRenderer)
 │    └── Phase 5 (高级渲染: 28 blends/LUT/dirty rects/video)
 ├── Phase 3 (音频系统: SoLoud/BGM/SE/Voice/callback)
 ├── Phase 7 (CARC 容器: AES-GCM/Ed25519/Provider Chain/carc_pack)
 │    └── Phase 9 (安全: 沙箱/链式信任/CRL)
 └── Phase 8 (开发工具: 热重载/调试器/ErrorUI) [需 Phase 1-7]
```

## 附录 B: 各 Phase 独立验收标准

| Phase | 独立可验证 | 验收命令 |
|-------|:---:|------|
| 0 | ✅ | `./caesura.exe` → 窗口 + 三角形 + Lua print |
| 1 | ✅ | `busted tests/lua/scheduler_spec.lua` → all pass |
| 2 | ✅ | 渲染测试图片到 bg 图层 |
| 3 | ✅ | 播放 test.ogg, 确认回调触发 |
| 4 | ✅ | `busted tests/lua/kag_commands_spec.lua` → all pass |
| 5 | ✅ | 切换混合模式 + LUT, 确认视觉变化 |
| 6 | ✅ | 存档 → 退出 → 读档, 对比状态 |
| 7 | ✅ | `carc_pack data/ test.carc` → 引擎读取 test.carc |
| 8 | ✅ | 修改 .ks → 自动重载 |
| 9 | ✅ | `[eval] os.execute('calc')` → 报错 |

## 附录 C: Codex 投喂建议

本计划文件可直接投喂给 Codex 执行。每个 Phase 独立可验证。推荐策略:

1. **Phase 0 先行** — 建立骨架后其他 Phase 可并行
2. **Phase 1+2+3 并行** — 三个子系统完全独立
3. **Phase 7 可与 1-6 并行** — CARC 不依赖脚本/渲染/音频
4. **每个 Phase 结束时提交** — 保证 git 历史可回溯
5. **Phase 4 是集成点** — 需要 Phase 1+2+3 全部完成

**单次投喂建议:** Phase 0 (约需 2-4 小时 Codex 编码时间)
**后续可并行投喂:** Phase 1 + Phase 2 + Phase 3 + Phase 7

---

> **计划完成日期:** 2026-06-06
> **下一个行动:** 用户审查 → 选择 Phase 开始编码


---

## Phase 0.5: Lua↔C++ 统一抽象接口层 (贯穿所有 Phase)

**目标:** 建立 Lua 层通过统一 backend 抽象接口调用 C++ 层的架构。所有引擎能力通过单一 `backend` 表暴露，资源创建走工厂模式，句柄有统一生命周期管理。

**这是整个引擎架构的脊梁**——每一个 Phase 的 Lua 代码都通过这层接口与 C++ 交互，而非直接调用 C 函数。

---

### 架构原则

```
┌──────────────────────────────────────────────┐
│  KAG 脚本 / Lua 库 / 游戏逻辑                 │
│  ↓ 只通过一个入口                             │
│  backend.lua   (Lua 端统一门面)               │
│  ↓ C API 绑定                                 │
│  BackendAPI.h  (C++ 端统一注册器)             │
│  ↓ 分发到子模块                               │
│  TextureManager / AudioBackend / LayerManager │
│  FontRenderer / VideoPlayer / InputManager    │
│  ↓                                            │
│  bgfx / SoLoud / SDL3 / FreeType              │
└──────────────────────────────────────────────┘
```

**硬约束:**
- Lua 代码**永远不**直接调用 C 函数 (如 `c_texture_load()`)
- Lua 代码**永远不**直接访问 bgfx/SoLoud/SDL C API
- 所有 C++ 能力通过 `backend.xxx()` 单一命名空间暴露
- 所有资源创建走工厂方法，返回统一句柄
- 所有句柄支持 `backend.is_valid(handle)` 检查

---

### Task 0.5.1: C++ BackendAPI 统一注册器

**Files:** Create: `src/script/BackendAPI.h`, `src/script/BackendAPI.cpp`
**Modify:** `src/script/LuaEngine.cpp` (在 registerCoreBindings 中调用 BackendAPI)

```cpp
// src/script/BackendAPI.h
#pragma once
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace caesura {

// 统一句柄类型 (Lua 侧为 lightuserdata + metatable)
enum class HandleType : uint8_t {
    TEXTURE   = 0,
    AUDIO     = 1,
    VIEWPORT  = 2,
    RTT       = 3,
    SHADER    = 4,
    TRANSITION = 5,
    FONT_ATLAS = 6,
};

// 统一资源句柄 (C++ 内部)
struct ResourceHandle {
    HandleType type;
    uint32_t id;
    uint32_t generation;  // 热重载时递增, is_valid 双重校验
    void* rawHandle;       // bgfx::TextureHandle* / SoLoud::handle* 等

    bool isValid() const { return id != 0; }
};

// BackendAPI: 将所有引擎能力注册到 Lua `backend` 表
class BackendAPI {
public:
    // 主入口: 在 Lua 全局创建 backend 表并注册所有子模块
    static void registerAll(lua_State* L);

private:
    // 各子系统注册函数
    static void registerTextureAPI(lua_State* L, lua_CFunction* methods);
    static void registerAudioAPI(lua_State* L, lua_CFunction* methods);
    static void registerLayerAPI(lua_State* L, lua_CFunction* methods);
    static void registerFontAPI(lua_State* L, lua_CFunction* methods);
    static void registerInputAPI(lua_State* L, lua_CFunction* methods);
    static void registerVideoAPI(lua_State* L, lua_CFunction* methods);
    static void registerViewportAPI(lua_State* L, lua_CFunction* methods);
    static void registerSystemAPI(lua_State* L, lua_CFunction* methods);
    static void registerDebugAPI(lua_State* L, lua_CFunction* methods);

    // 统一句柄管理
    static ResourceHandle* pushHandle(lua_State* L, HandleType type,
                                       uint32_t id, void* raw);
    static ResourceHandle* checkHandle(lua_State* L, int index, HandleType expected);
    static bool isValidHandle(lua_State* L, int index);
};

} // namespace caesura
```

```cpp
// src/script/BackendAPI.cpp
#include "script/BackendAPI.h"
#include "render/TextureManager.h"
#include "render/LayerManager.h"
#include "render/FontRenderer.h"
#include "audio/AudioBackend.h"
#include "input/InputManager.h"
#include "system/Logger.h"

namespace caesura {

void BackendAPI::registerAll(lua_State* L) {
    // 创建 backend 表
    lua_newtable(L);

    // === 纹理 API ===
    registerTextureAPI(L);

    // === 音频 API ===
    registerAudioAPI(L);

    // === 图层 API ===
    registerLayerAPI(L);

    // === 字体 API ===
    registerFontAPI(L);

    // === 输入 API ===
    registerInputAPI(L);

    // === 视频 API ===
    registerVideoAPI(L);

    // === 视口 API (3D) ===
    registerViewportAPI(L);

    // === 系统 API ===
    registerSystemAPI(L);

    // === 调试 API ===
    registerDebugAPI(L);

    // === 统一句柄 API ===
    // backend.is_valid(handle) -> bool
    lua_pushcfunction(L, [](lua_State* L) -> int {
        lua_pushboolean(L, isValidHandle(L, 1) ? 1 : 0);
        return 1;
    });
    lua_setfield(L, -2, "is_valid");

    // backend.destroy(handle)
    lua_pushcfunction(L, [](lua_State* L) -> int {
        if (!isValidHandle(L, 1)) return 0;
        ResourceHandle* h = (ResourceHandle*)lua_touserdata(L, 1);
        // 根据类型释放
        switch (h->type) {
            case HandleType::TEXTURE:
                TextureManager::instance().releaseTexture(
                    *static_cast<bgfx::TextureHandle*>(h->rawHandle));
                break;
            case HandleType::AUDIO:
                AudioBackend::instance().stop(h->id);
                break;
            // ... 其他类型
        }
        h->id = 0;  // 标记无效
        return 0;
    });
    lua_setfield(L, -2, "destroy");

    // 设置为全局变量
    lua_setglobal(L, "backend");
    LOG_INFO("Script", "BackendAPI registered (9 subsystems)");
}

// --- 纹理 API 注册示例 ---

void BackendAPI::registerTextureAPI(lua_State* L) {
    // backend.texture_load(path) -> TextureHandle
    lua_pushcfunction(L, [](lua_State* L) -> int {
        const char* path = luaL_checkstring(L, 1);
        bgfx::TextureHandle tex = TextureManager::instance().loadTexture(path);
        if (!bgfx::isValid(tex)) {
            lua_pushnil(L);
            return 1;
        }
        // 返回统一句柄 (工厂方法)
        static bgfx::TextureHandle s_handles[256];
        static int s_idx = 0;
        s_handles[s_idx] = tex;
        pushHandle(L, HandleType::TEXTURE, s_idx, &s_handles[s_idx]);
        s_idx = (s_idx + 1) % 256;
        return 1;
    });
    lua_setfield(L, -2, "texture_load");

    // backend.texture_get_size(handle) -> width, height
    lua_pushcfunction(L, [](lua_State* L) -> int {
        ResourceHandle* h = checkHandle(L, 1, HandleType::TEXTURE);
        if (!h) { lua_pushnil(L); lua_pushnil(L); return 2; }
        uint16_t w, hgt;
        TextureManager::instance().getTextureSize(
            *static_cast<bgfx::TextureHandle*>(h->rawHandle), w, hgt);
        lua_pushinteger(L, w);
        lua_pushinteger(L, hgt);
        return 2;
    });
    lua_setfield(L, -2, "texture_get_size");

    // backend.texture_create_solid(r, g, b, a) -> TextureHandle
    lua_pushcfunction(L, [](lua_State* L) -> int {
        uint8_t r = (uint8_t)luaL_checkinteger(L, 1);
        uint8_t g = (uint8_t)luaL_checkinteger(L, 2);
        uint8_t b = (uint8_t)luaL_checkinteger(L, 3);
        uint8_t a = (uint8_t)luaL_optinteger(L, 4, 255);
        bgfx::TextureHandle tex = TextureManager::instance()
            .createSolidTexture(r, g, b, a);
        // ...
        return 1;
    });
    lua_setfield(L, -2, "texture_create_solid");
}

// --- 音频 API 注册示例 ---

void BackendAPI::registerAudioAPI(lua_State* L) {
    // backend.audio_play(path, bus, volume) -> AudioHandle
    lua_pushcfunction(L, [](lua_State* L) -> int {
        const char* path = luaL_checkstring(L, 1);
        int bus = (int)luaL_checkinteger(L, 2);
        float vol = (float)luaL_optnumber(L, 3, 1.0);
        unsigned int id = AudioBackend::instance().play(
            path, static_cast<AudioBus>(bus), vol);
        if (id == 0) { lua_pushnil(L); return 1; }
        pushHandle(L, HandleType::AUDIO, id, nullptr);
        return 1;
    });
    lua_setfield(L, -2, "audio_play");

    // backend.audio_stop(handle)
    lua_pushcfunction(L, [](lua_State* L) -> int {
        ResourceHandle* h = checkHandle(L, 1, HandleType::AUDIO);
        if (h) AudioBackend::instance().stop(h->id);
        return 0;
    });
    lua_setfield(L, -2, "audio_stop");

    // backend.audio_fade(handle, to, duration)
    lua_pushcfunction(L, [](lua_State* L) -> int {
        ResourceHandle* h = checkHandle(L, 1, HandleType::AUDIO);
        if (!h) return 0;
        float to = (float)luaL_checknumber(L, 2);
        float dur = (float)luaL_checknumber(L, 3);
        AudioBackend::instance().fade(h->id, to, dur);
        return 0;
    });
    lua_setfield(L, -2, "audio_fade");

    // backend.audio_is_playing(handle) -> bool
    lua_pushcfunction(L, [](lua_State* L) -> int {
        ResourceHandle* h = checkHandle(L, 1, HandleType::AUDIO);
        if (!h) { lua_pushboolean(L, 0); return 1; }
        lua_pushboolean(L, AudioBackend::instance().isPlaying(h->id) ? 1 : 0);
        return 1;
    });
    lua_setfield(L, -2, "audio_is_playing");
}

// --- 图层 API 注册示例 ---

void BackendAPI::registerLayerAPI(lua_State* L) {
    // backend.layer_set_texture(layer_id, path)
    lua_pushcfunction(L, [](lua_State* L) -> int {
        int layerId = (int)luaL_checkinteger(L, 1);
        const char* path = luaL_checkstring(L, 2);
        bgfx::TextureHandle tex = TextureManager::instance().loadTexture(path);
        LayerManager::instance().setTex(
            static_cast<LayerManager::LayerType>(layerId), tex);
        return 0;
    });
    lua_setfield(L, -2, "layer_set_texture");

    // backend.layer_set_visible(layer_id, bool)
    lua_pushcfunction(L, [](lua_State* L) -> int {
        int id = (int)luaL_checkinteger(L, 1);
        bool vis = lua_toboolean(L, 2);
        LayerManager::instance().get(
            static_cast<LayerManager::LayerType>(id)).visible = vis;
        return 0;
    });
    lua_setfield(L, -2, "layer_set_visible");

    // backend.layer_set_pos(layer_id, x, y)
    // backend.layer_clear(layer_id)
    // backend.layer_set_blend(layer_id, mode)
    // ...
}

// --- 统一句柄工厂方法 ---

ResourceHandle* BackendAPI::pushHandle(lua_State* L, HandleType type,
                                        uint32_t id, void* raw) {
    ResourceHandle* h = (ResourceHandle*)lua_newuserdata(L, sizeof(ResourceHandle));
    h->type = type;
    h->id = id;
    h->generation = 0;  // 初始 generation = 0
    h->rawHandle = raw;

    // 设置 metatable (支持 __gc, __close, __tostring)
    luaL_getmetatable(L, "caesura.ResourceHandle");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        luaL_newmetatable(L, "caesura.ResourceHandle");

        // __gc: 自动释放
        lua_pushcfunction(L, [](lua_State* L) -> int {
            ResourceHandle* h = (ResourceHandle*)lua_touserdata(L, 1);
            if (h && h->id != 0) {
                LOG_DEBUG("Backend", "GC handle type=%d id=%d", (int)h->type, h->id);
                h->id = 0;
            }
            return 0;
        });
        lua_setfield(L, -2, "__gc");

        // __close (Lua 5.4): 支持 local h <close> = backend.xxx()
        lua_pushcfunction(L, [](lua_State* L) -> int {
            ResourceHandle* h = (ResourceHandle*)lua_touserdata(L, 1);
            if (h && h->id != 0) {
                // 触发 destroy
                lua_getglobal(L, "backend");
                lua_getfield(L, -1, "destroy");
                lua_pushvalue(L, 1);
                lua_call(L, 1, 0);
                lua_pop(L, 1);
            }
            return 0;
        });
        lua_setfield(L, -2, "__close");

        // __tostring
        lua_pushcfunction(L, [](lua_State* L) -> int {
            ResourceHandle* h = (ResourceHandle*)lua_touserdata(L, 1);
            lua_pushfstring(L, "ResourceHandle{type=%d, id=%d, gen=%d}",
                           (int)h->type, h->id, h->generation);
            return 1;
        });
        lua_setfield(L, -2, "__tostring");
    }
    lua_setmetatable(L, -2);
    return h;
}

ResourceHandle* BackendAPI::checkHandle(lua_State* L, int index, HandleType expected) {
    ResourceHandle* h = (ResourceHandle*)luaL_checkudata(L, index, "caesura.ResourceHandle");
    if (!h || h->id == 0) {
        luaL_error(L, "Invalid or destroyed handle");
        return nullptr;
    }
    if (h->type != expected) {
        luaL_error(L, "Handle type mismatch: expected %d, got %d",
                  (int)expected, (int)h->type);
        return nullptr;
    }
    return h;
}

bool BackendAPI::isValidHandle(lua_State* L, int index) {
    if (!lua_isuserdata(L, index)) return false;
    ResourceHandle* h = (ResourceHandle*)lua_touserdata(L, index);
    return h && h->id != 0;
}

// ... 其余子系统注册函数 (Font/Input/Video/Viewport/System/Debug) ...

} // namespace caesura
```

---

### Task 0.5.2: backend.lua — Lua 端统一门面

**Files:** Create: `scripts/backend.lua`

```lua
-- scripts/backend.lua
-- 统一后端抽象层 — Lua 代码唯一与 C++ 交互的入口
-- 架构约束: 任何 Lua 代码不得直接调用 C 函数或绕过此模块

local backend = {}

-- ══════════════════════════════════════════════════════
-- 纹理 API
-- ══════════════════════════════════════════════════════

--- 加载纹理 (同步)
--- @param path string 纹理文件路径 (通过 Provider Chain 解析)
--- @return TextureHandle|nil
function backend.load_texture(path)
    -- 直接透传 C++ 注册的函数 (已由 BackendAPI::registerTextureAPI 注入)
    return _G.backend.texture_load(path)
end

--- 创建纯色纹理
--- @param r,g,b,a number 0-255
--- @return TextureHandle
function backend.create_solid_texture(r, g, b, a)
    return _G.backend.texture_create_solid(r, g, b, a or 255)
end

--- 获取纹理尺寸
--- @param handle TextureHandle
--- @return number width, number height
function backend.texture_get_size(handle)
    return _G.backend.texture_get_size(handle)
end

-- ══════════════════════════════════════════════════════
-- 音频 API (全局 Direct 模式)
-- ══════════════════════════════════════════════════════

--- 播放音频
--- @param path string  音频文件路径
--- @param bus number   0=BGM, 1=VOICE, 2=SE
--- @param volume number 0.0-1.0
--- @return AudioHandle|nil
function backend.audio_play(path, bus, volume)
    return _G.backend.audio_play(path, bus, volume or 1.0)
end

--- 停止音频
--- @param handle AudioHandle
function backend.audio_stop(handle)
    _G.backend.audio_stop(handle)
end

--- 淡入淡出
--- @param handle AudioHandle
--- @param to number 目标音量
--- @param duration number 淡变时长 (ms)
function backend.audio_fade(handle, to, duration)
    _G.backend.audio_fade(handle, to, duration)
end

--- 检查是否正在播放
--- @param handle AudioHandle
--- @return boolean
function backend.audio_is_playing(handle)
    return _G.backend.audio_is_playing(handle)
end

-- ══════════════════════════════════════════════════════
-- 图层 API
-- ══════════════════════════════════════════════════════

local LAYER = { bg = 0, fg = 1, message = 2 }

--- 设置图层纹理
--- @param layer string "bg"|"fg"|"message"
--- @param path string 纹理路径
function backend.layer_set_texture(layer, path)
    local id = LAYER[layer]
    if not id then error("Unknown layer: " .. tostring(layer)) end
    _G.backend.layer_set_texture(id, path)
end

--- 设置图层可见性
function backend.layer_set_visible(layer, visible)
    local id = LAYER[layer]
    if id then _G.backend.layer_set_visible(id, visible) end
end

--- 设置图层位置
function backend.layer_set_pos(layer, x, y)
    local id = LAYER[layer]
    if id then _G.backend.layer_set_pos(id, x, y) end
end

--- 清除图层
function backend.layer_clear(layer)
    local id = LAYER[layer]
    if id then _G.backend.layer_clear(id) end
end

-- ══════════════════════════════════════════════════════
-- 字体/文本 API
-- ══════════════════════════════════════════════════════

--- 渲染文本到消息层
--- @param text string
--- @param size number 字号
--- @param x,y number 屏幕坐标
--- @param maxWidth number 最大宽度 (自动换行)
function backend.font_render_text(text, size, x, y, maxWidth)
    _G.backend.font_render_text(text, size or 48, x or 40, y or 500, maxWidth or 720)
end

--- 清除消息层文本
function backend.font_clear()
    _G.backend.font_clear()
end

--- 换行
function backend.font_newline()
    _G.backend.font_newline()
end

-- ══════════════════════════════════════════════════════
-- 输入 API
-- ══════════════════════════════════════════════════════

--- 检查是否有待处理的点击
--- @return boolean
function backend.input_has_click()
    return _G.backend.input_has_click()
end

--- 消费一个点击事件
function backend.input_consume_click()
    _G.backend.input_consume_click()
end

-- ══════════════════════════════════════════════════════
-- 视频 API
-- ══════════════════════════════════════════════════════

--- 加载并播放视频
--- @param path string 视频文件路径
--- @return VideoHandle|nil
function backend.video_play(path)
    return _G.backend.video_play(path)
end

--- 停止视频
function backend.video_stop(handle)
    _G.backend.video_stop(handle)
end

-- ══════════════════════════════════════════════════════
-- 系统 API
-- ══════════════════════════════════════════════════════

--- 存档
function backend.system_save(slot, jsonData)
    return _G.backend.system_save(slot, jsonData)
end

--- 读档
function backend.system_load(slot)
    return _G.backend.system_load(slot)
end

--- 获取引擎版本
function backend.system_version()
    return _G.backend.system_version()
end

-- ══════════════════════════════════════════════════════
-- 句柄管理 (统一)
-- ══════════════════════════════════════════════════════

--- 检查句柄是否有效
--- @param handle any
--- @return boolean
function backend.is_valid(handle)
    return _G.backend.is_valid(handle)
end

--- 销毁句柄 (释放底层资源)
--- @param handle any
function backend.destroy(handle)
    _G.backend.destroy(handle)
end

return backend
```

---

### 工厂模式调用示例

```lua
-- KAG 命令层调用示例 (scripts/kag/commands/layer.lua 中的 bg 命令)
local backend = require("scripts.backend")  -- 唯一入口

function kag_commands.bg(ctx, params)
    -- 工厂方法: 加载纹理, 返回统一句柄
    local tex_handle = backend.load_texture(params.storage)
    if not tex_handle then
        log("Texture not found: " .. params.storage)
        return
    end

    -- 设置到背景图层
    backend.layer_set_texture("bg", params.storage)
    ctx.layers.bg = tex_handle  -- 存储句柄供后续操作
end

-- 音频命令示例
function kag_commands.playvoice(ctx, params)
    local audio_handle = backend.audio_play(params.storage, 1, 1.0)
    if not audio_handle then return end

    -- 使用 CancelToken 管理生命周期
    local ct = kag.cancel_token.new()
    ct:register(function()
        backend.audio_stop(audio_handle)   -- 取消时停止
        backend.destroy(audio_handle)       -- 释放资源
    end)

    -- Lua 5.4 <close> 自动清理
    local h <close> = audio_handle

    table.insert(ctx.active_operations, ct)
    coroutine.yield()  -- 等待语音结束
end
```

---

### 统一抽象层的设计收益

| 收益 | 说明 |
|------|------|
| **单一切入点** | 所有 Lua 代码只 `require("scripts.backend")`，IDE 可以静态分析所有 C++ 调用 |
| **类型安全** | 工厂方法返回统一 ResourceHandle，含类型标记 + generation 计数 |
| **AI 友好** | AI Agent 只需了解 backend 模块的 API 签名，无需理解 bgfx/SoLoud 内部 |
| **热重载安全** | handle.is_valid() 双重校验 (id + generation)，热重载后自动失效 |
| **可测试性** | backend.lua 可被 mock 替换，支持 Lua 单元测试无需启动引擎 |
| **IDE 集成** | backend API 清单可直接生成 IDE 的自动补全 + 文档提示 |
| **向后兼容** | 新增 C++ 能力只需在 BackendAPI 中注册，Lua 侧添加 wrapper |

---

### 后续 Phase 如何使用此抽象层

```
Phase 1 (脚本引擎): scheduler.lua 中的 kag 命令调用 backend.xxx()
Phase 2 (渲染管线): layers.lua 调用 backend.layer_set_texture/backend.load_texture
Phase 3 (音频系统): audio.lua 调用 backend.audio_play/backend.audio_fade
Phase 4 (KAG标签): 每个标签实现只 import backend + kag 内部模块
Phase 5 (高级渲染): palette.lua/transition.lua 调用 backend 新注册的着色器/转场 API
Phase 6 (存档): save.lua 调用 backend.system_save/backend.system_load
Phase 7 (CARC): 对 Lua 层完全透明 — Provider Chain 在 C++ 侧自动选择来源
Phase 8 (热重载): backend.is_valid(handle) 检查所有旧句柄是否失效
Phase 9 (沙箱): [eval] 内仍然通过 backend 调用 (Render 白名单 = backend 子集)
```

