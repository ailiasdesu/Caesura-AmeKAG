#pragma once

struct lua_State;

namespace Caesura {

// Register VFX Lua module (global table "VFX" with particle system functions).
// Also provides the _CAESURA_BACKEND-compatible particle command routing.
void registerVFXBinding(lua_State* L);

// Call before Lua shutdown to release particle system GPU resources.
void VFXBinding_Shutdown();

} // namespace Caesura