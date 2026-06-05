 #pragma once

struct lua_State;

namespace Caesura {

// Register the KAG Lua module (global table "KAG" with all C functions).
// All functions resolve their backend pointers from the Lua registry
// via BackendRegistry, not from global C++ pointers.
void registerKAGBinding(lua_State* L);

} // namespace Caesura
