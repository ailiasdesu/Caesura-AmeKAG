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
    // -- CPU budget for AI-generated scripts (Track 3) --
    // Maximum Lua VM instructions per frame before forced yield.
    // 0 = no limit (backward compat). Default: 500000 instructions/frame.
    void setInstructionBudget(int budget) { m_instructionBudget = budget; }
    int  getInstructionBudget() const    { return m_instructionBudget; }
    bool isInstructionBudgetExceeded() const { return m_budgetExceeded; }
    void resetInstructionBudget() { m_instructionCount = 0; m_budgetExceeded = false; }

private:
    static void instructionHook(lua_State* L, lua_Debug* ar);

    int  m_instructionCount = 0;
    int  m_instructionBudget = 500000;  // 500k instructions/frame
    bool m_budgetExceeded = false;

    ~LuaManager() = default;

    void registerModules();
    void lockdownScriptEnv();

    lua_State* m_L = nullptr;
    bool m_initialized = false;
};

} // namespace Caesura
