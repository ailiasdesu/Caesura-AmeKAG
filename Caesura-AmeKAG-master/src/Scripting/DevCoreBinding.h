 #pragma once

struct lua_State;

namespace Caesura {

// Register the DevCore Lua module (global table "DevCore").
// Resolves InputRouter* from BackendRegistry / Lua registry.
void registerDevCoreBinding(lua_State* L);

} // namespace Caesura
