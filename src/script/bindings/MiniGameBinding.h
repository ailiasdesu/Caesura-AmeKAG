// MiniGameBinding.h — Lua bindings for the 3D mini-game backend (C2)
#pragma once
struct lua_State;

namespace Caesura {
void registerMiniGameBinding(lua_State* L);
} // namespace Caesura
