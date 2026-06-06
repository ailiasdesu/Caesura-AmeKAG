// ===========================================================================
//  Caesura (AmeKAG) — SaveBinding.h
//  Phase 6: C++ Lua binding for save/load system.
//  Registers KAG.save_game / KAG.load_game / KAG.list_saves / KAG.delete_save
//  All save I/O goes through SaveManager singleton.
// ===========================================================================

#pragma once

struct lua_State;

namespace Caesura {

// Register save/load functions on the global KAG table.
// Called during LuaManager::registerModules().
void registerSaveBinding(lua_State* L);

} // namespace Caesura
