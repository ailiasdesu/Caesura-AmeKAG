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
