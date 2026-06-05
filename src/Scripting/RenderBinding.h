 #pragma once

struct lua_State;

namespace Caesura {

// Register the Render Lua module (global table "Render" with all C functions).
// Resolves IRenderDevice* from BackendRegistry / Lua registry.
void registerRenderBinding(lua_State* L);

// Release all cached GPU textures. Call before bgfx::shutdown().
void RenderBinding_Shutdown();

} // namespace Caesura
