// EngineBinding.h — Lua "Engine" module (backend selection) + registry
// helpers. Round 21 (P1-5): moved OUT of di/BackendRegistry so the
// dependency-injection container stops binding to the Lua C API. The
// container stays a pure I* registry; script owns the Lua surface.
#pragma once

struct lua_State;

namespace Caesura {

namespace engine_binding {

// Register the global "Engine" table (select_render_backend /
// select_audio_backend / select_platform_backend / get_backend_info).
void registerEngineBindings(lua_State* L);

} // namespace engine_binding
} // namespace Caesura
