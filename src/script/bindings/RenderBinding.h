 #pragma once

struct lua_State;

namespace Caesura {

// Register the Render Lua module (global table "Render" with all C functions).
// Resolves IRenderDevice* from BackendRegistry / Lua registry.
void registerRenderBinding(lua_State* L);

// Cancel native requests and release their Lua closures at a session boundary.
void cancelRenderAsyncLoads(lua_State* L);

// Release all cached GPU textures. Call before bgfx::shutdown().
// (P2) RenderBinding_Shutdown removed: declaration was dangling (no def/call).

} // namespace Caesura
