// SteamBinding — Lua bindings for Steam SDK
// Registers global 'steam' table with achievement/stats/cloud functions.
#pragma once
struct lua_State;
namespace Caesura { class ISteamBackend; }

namespace Caesura {

void registerSteamBinding(lua_State* L, ISteamBackend* backend);
void unregisterSteamBinding(ISteamBackend* backend);

} // namespace Caesura
