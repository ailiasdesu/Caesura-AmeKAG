// ===========================================================================
//  Caesura (AmeKAG) -- GameState.h
//  Native reference to the session table owned by the Lua KAG runner.
//  This bridge neither creates a context nor copies session state.
// ===========================================================================

#pragma once

struct lua_State;

namespace Caesura {

class GameState {
public:
    // Reference the exact table at a positive/negative stack index, or clear
    // for nil. Returns false for a null VM, invalid/pseudo index, or other type.
    // Leaves the caller's stack unchanged; rejection preserves the old reference.
    static bool bind(lua_State* L, int index);

    // Push the referenced table and return true. An absent or malformed
    // reference returns false without leaving anything on the caller's stack.
    static bool push(lua_State* L);

    // Ask an already-loaded runner to stop while its backends still exist.
    // Does not load modules or construct state; preserves the caller's stack.
    static bool stopRunner(lua_State* L);

private:
    static constexpr const char* REGISTRY_KEY = "caesura_ctx";
};

} // namespace Caesura
