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
