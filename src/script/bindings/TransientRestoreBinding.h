#pragma once
struct lua_State;

namespace Caesura {
void registerTransientRestoreBinding(lua_State* state);
}
