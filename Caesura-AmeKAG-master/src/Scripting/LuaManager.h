#pragma once
struct lua_State;

namespace Caesura {

//      Lua Manager                                                                                                                       
// Initializes the Lua VM and registers all engine binding modules.
// Acts as the central scripting hub for KAG, Render, and DevCore.

class LuaManager {
public:
    static LuaManager& instance();

    LuaManager(const LuaManager&) = delete;
    LuaManager& operator=(const LuaManager&) = delete;

    // Lifecycle
    bool init();
    void shutdown();
    void update(float deltaTime);

    // Load and execute a Lua script file
    bool loadScript(const char* path);

    // Resume the KAG coroutine (for input-driven progression)
    void resumeKAGCoroutine();

    // Access the raw Lua state for advanced usage
    lua_State* state() { return m_L; }

public:
    LuaManager() = default;
    ~LuaManager() = default;

    void registerModules();
    void lockdownScriptEnv();

    lua_State* m_L = nullptr;
    bool m_initialized = false;
};

} // namespace Caesura
