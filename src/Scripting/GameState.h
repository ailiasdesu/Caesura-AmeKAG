// ===========================================================================
//  Caesura (AmeKAG) — GameState.h
//  Spec [10.2.31]: GameState singleton — creates/manages ctx table in Lua
//  registry.  All KAG script state lives in ctx: coroutine, tokens,
//  call stack, flag tables (f/sf/tf), layer references, backlog,
//  active CancelToken operations, and flow-control flags.
// ===========================================================================

#pragma once

struct lua_State;

namespace Caesura {

class GameState {
public:
    // Create the ctx table in the Lua registry and populate default fields.
    // Must be called exactly once before any KAG script is loaded.
    static void create(lua_State* L);

    // Push the ctx table from the Lua registry onto the stack.
    // Returns true on success, false if ctx was never created.
    static bool push(lua_State* L);

private:
    // Registry key used to store the ctx table.
    static constexpr const char* REGISTRY_KEY = "caesura_ctx";
};

} // namespace Caesura
