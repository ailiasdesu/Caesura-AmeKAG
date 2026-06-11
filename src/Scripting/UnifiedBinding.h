// @deprecated: BackendFactory (scripts/backend_factory.lua) now creates _CAESURA_BACKEND.
// UnifiedBinding is no longer registered. Keep file for reference only.
#pragma once

struct lua_State;

namespace Caesura {

// Register the unified _CAESURA_BACKEND Lua proxy table.
// Provides render(), audio(), platform() dispatch methods
// plus show_text, show_image, clear_screen, wait_click convenience methods.
// This is the primary dispatch channel for backend.lua.
void registerUnifiedBackendBinding(lua_State* L);

} // namespace Caesura
