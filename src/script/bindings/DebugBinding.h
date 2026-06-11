#pragma once
struct lua_State;

namespace Caesura {

// Register the "Debug" Lua module (10 diagnostic APIs).
// Called by LuaManager during module registration.
void registerDebugBinding(lua_State* L);

} // namespace Caesura
