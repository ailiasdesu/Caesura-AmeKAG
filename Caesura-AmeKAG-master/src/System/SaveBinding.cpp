// ===========================================================================
//  Caesura (AmeKAG) -- SaveBinding.cpp
//  Phase 6: C++ Lua binding for save/load.
//  Registers: KAG.save_game(jsonData, sceneName, tokenIndex, [thumbnail])
//             KAG.load_game(slot) → jsonData, metaTable
//             KAG.list_saves() → {{slot, timestamp, scene, ...}, ...}
//             KAG.delete_save(slot) → bool
// ===========================================================================

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "SaveBinding.h"
#include "SaveManager.h"
#include <cstdio>
#include <ctime>

namespace Caesura {

// -- lua_Save_game ----------------------------------------------------------
// KAG.save_game(jsonData, sceneName, tokenIndex, [thumbnailBase64]) → bool
static int lua_Save_game(lua_State* L) {
    // Determine slot: use KAG style -- caller passes slot as first arg or
    // we auto-pick next available. Let's use explicit slot parameter.
    // Signature: save_game(slot, jsonData, sceneName, tokenIndex, [thumbnail])
    int slot        = (int)luaL_checkinteger(L, 1);
    const char* jsonData  = luaL_checkstring(L, 2);
    const char* sceneName = luaL_checkstring(L, 3);
    int tokenIndex = (int)luaL_checkinteger(L, 4);
    const char* thumbnail = luaL_optstring(L, 5, "");

    bool ok = SaveManager::instance().save(slot, jsonData, sceneName,
                                           tokenIndex, thumbnail);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// -- lua_Load_game ----------------------------------------------------------
// KAG.load_game(slot) → jsonData, metaTable | nil, error
static int lua_Load_game(lua_State* L) {
    int slot = (int)luaL_checkinteger(L, 1);

    SaveMeta meta;
    std::string data = SaveManager::instance().load(slot, &meta);

    if (data.empty()) {
        lua_pushnil(L);
        lua_pushfstring(L, "Failed to load save slot %d", slot);
        return 2;
    }

    // Push JSON data string
    lua_pushstring(L, data.c_str());

    // Push metadata table = {slot, timestamp, scene, token_index, schema_version}
    lua_newtable(L);
    lua_pushinteger(L, meta.slot);
    lua_setfield(L, -2, "slot");
    lua_pushinteger(L, (lua_Integer)meta.timestamp);
    lua_setfield(L, -2, "timestamp");
    lua_pushstring(L, meta.sceneName.c_str());
    lua_setfield(L, -2, "scene");
    lua_pushinteger(L, meta.tokenIndex);
    lua_setfield(L, -2, "token_index");
    lua_pushinteger(L, meta.schemaVersion);
    lua_setfield(L, -2, "schema_version");

    return 2;  // jsonData, meta
}

// -- lua_List_saves ---------------------------------------------------------
// KAG.list_saves() → {{slot=N, timestamp=T, scene=S, token_index=I}, ...}
static int lua_List_saves(lua_State* L) {
    auto saves = SaveManager::instance().listSaves();

    lua_newtable(L);
    int idx = 1;
    for (const auto& meta : saves) {
        lua_newtable(L);
        lua_pushinteger(L, meta.slot);
        lua_setfield(L, -2, "slot");
        lua_pushinteger(L, (lua_Integer)meta.timestamp);
        lua_setfield(L, -2, "timestamp");
        lua_pushstring(L, meta.sceneName.c_str());
        lua_setfield(L, -2, "scene");
        lua_pushinteger(L, meta.tokenIndex);
        lua_setfield(L, -2, "token_index");
        lua_pushinteger(L, meta.schemaVersion);
        lua_setfield(L, -2, "schema_version");
        // raw_seti for array index
        lua_rawseti(L, -2, idx++);
    }

    return 1;
}

// -- lua_Delete_save ---------------------------------------------------------
// KAG.delete_save(slot) → bool
static int lua_Delete_save(lua_State* L) {
    int slot = (int)luaL_checkinteger(L, 1);
    bool ok = SaveManager::instance().deleteSlot(slot);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// -- lua_Save_exists ---------------------------------------------------------
// KAG.save_exists(slot) → bool
static int lua_Save_exists(lua_State* L) {
    int slot = (int)luaL_checkinteger(L, 1);
    bool ok = SaveManager::instance().slotExists(slot);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// -- lua_Save_get_dir --------------------------------------------------------
// KAG.get_save_dir() → string
static int lua_Get_save_dir(lua_State* L) {
    // Return a sensible default; engine sets this via SaveManager::init()
    lua_pushstring(L, "saves/");
    return 1;
}

// ============================================================================
//  Registration
// ============================================================================

void registerSaveBinding(lua_State* L) {
    // Get or create the global KAG table
    lua_getglobal(L, "KAG");
    bool hasKAG = lua_istable(L, -1);

    if (!hasKAG) {
        lua_pop(L, 1);
        lua_newtable(L);
    }

    // Register save functions on KAG table
    static const luaL_Reg saveFuncs[] = {
        { "save_game",   lua_Save_game   },
        { "load_game",   lua_Load_game   },
        { "list_saves",  lua_List_saves  },
        { "delete_save", lua_Delete_save },
        { "save_exists", lua_Save_exists },
        { "get_save_dir", lua_Get_save_dir },
        { nullptr, nullptr }
    };

    luaL_setfuncs(L, saveFuncs, 0);

    if (!hasKAG) {
        lua_setglobal(L, "KAG");
    } else {
        lua_pop(L, 1);  // pop KAG table
    }

    printf("[SaveBinding] KAG save/load functions registered (6 APIs).\n");
}

} // namespace Caesura
