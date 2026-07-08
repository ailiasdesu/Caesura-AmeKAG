// ⚠ DEPRECATED — _CAESURA_BACKEND proxy is superseded by direct Render/KAG/DevCore bindings.
// This module is NO LONGER REGISTERED by LuaManager::registerModules().
// Retained for reference. backend.lua in Lua scripts handles the fallback path.
// Remove this file entirely after a deprecation period if no issues arise.
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
