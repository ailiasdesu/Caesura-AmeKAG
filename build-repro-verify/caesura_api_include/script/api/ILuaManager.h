// ILuaManager - pure virtual interface for Lua VM management
// Concrete: LuaManager. Pattern: module api/ directory.
#pragma once

struct lua_State;

namespace Caesura {

class ILuaManager {
public:
    virtual ~ILuaManager() = default;

    virtual bool init() = 0;
    virtual void shutdown() = 0;
    virtual void update(float deltaTime) = 0;
    virtual bool loadScript(const char* path) = 0;
    virtual void resumeKAGCoroutine() = 0;
    virtual void lockdownScriptEnv() = 0;
    virtual void registerModules() = 0;
    virtual lua_State* state() = 0;
    virtual void setInstructionBudget(int budget) = 0;
    virtual int  getInstructionBudget() const = 0;
    virtual bool isInstructionBudgetExceeded() const = 0;
    virtual void resetInstructionBudget() = 0;
};

} // namespace Caesura
