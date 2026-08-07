// ===========================================================================
//  Caesura (AmeKAG) -- SaveBinding.cpp
//  Phase 6: C++ Lua binding for save/load.
//  Uses nlohmann/json for structured data interchange between Lua and C++.
//  Registers: KAG.save_game(slot, dataTable, sceneName, tokenIndex, [thumbnail])
//             KAG.load_game(slot) -> dataTable, metaTable
//             KAG.list_saves() -> {{slot, timestamp, scene, ...}, ...}
//             KAG.delete_save(slot) -> bool
// ===========================================================================

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "SaveBinding.h"
#include "../../di/BackendRegistry.h"
#include "../../storage/api/ISaveManager.h"
#include <cstdio>
#include <ctime>

namespace Caesura {

static ISaveManager* getSaveManager() {
    return BackendRegistry::instance().getSaveManager();
}
// -- Lua table -> nlohmann::json (recursive) -------------------------------
// Detect if a Lua table is a dense array (all keys are 1..N positive integers)
static bool isLuaArray(lua_State* L, int absIdx) {
    int maxKey = 0, count = 0;
    lua_pushnil(L);
    while (lua_next(L, absIdx) != 0) { count++; if (lua_type(L, -2) == LUA_TNUMBER) { int k = (int)lua_tointeger(L, -2); if (k > maxKey) maxKey = k; } lua_pop(L, 1); }
    return (count > 0 && maxKey == count);
}

// Recursion depth cap: malicious self-referential Lua tables or deeply nested
// corrupt save files must not blow the C stack (the save path is not wrapped
// in lua_pcall, so LUAI_MAXCCALLS does not apply).
static constexpr int kMaxTableDepth = 64;

static json luaTableToJson(lua_State* L, int index, int depth = 0) {
    if (depth > kMaxTableDepth) return nullptr;
    json result;

    // Normalize negative indices
        int absIdx = (index < 0) ? lua_gettop(L) + index + 1 : index;
    // Check if this table is a dense array
    if (isLuaArray(L, absIdx)) {
        json arr = json::array();
        lua_pushnil(L);
        while (lua_next(L, absIdx) != 0) {
            int vt = lua_type(L, -1);
            switch (vt) {
                case LUA_TBOOLEAN: arr.push_back((bool)lua_toboolean(L, -1)); break;
                case LUA_TNUMBER:  arr.push_back(lua_isinteger(L, -1) ? (json)(int64_t)lua_tointeger(L, -1) : (json)(double)lua_tonumber(L, -1)); break;
                case LUA_TSTRING:  arr.push_back(std::string(lua_tostring(L, -1))); break;
                case LUA_TTABLE:   arr.push_back(luaTableToJson(L, -1, depth + 1)); break;
                default:           arr.push_back(nullptr); break;
            }
            lua_pop(L, 1);
        }
        return arr;
    }


    lua_pushnil(L);  // first key
    while (lua_next(L, absIdx) != 0) {
        // Stack: ... table key value
        // Duplicate key for lua_next
        std::string key;
        if (lua_type(L, -2) == LUA_TSTRING) {
            key = lua_tostring(L, -2);
        } else if (lua_type(L, -2) == LUA_TNUMBER) {
            key = std::to_string((int)lua_tointeger(L, -2));
        } else {
            lua_pop(L, 1);
            continue;
        }

        int valueType = lua_type(L, -1);
        switch (valueType) {
            case LUA_TBOOLEAN:
                result[key] = (bool)lua_toboolean(L, -1);
                break;
            case LUA_TNUMBER:
                if (lua_isinteger(L, -1))
                    result[key] = (int64_t)lua_tointeger(L, -1);
                else
                    result[key] = (double)lua_tonumber(L, -1);
                break;
            case LUA_TSTRING:
                result[key] = std::string(lua_tostring(L, -1));
                break;
            case LUA_TTABLE:
                result[key] = luaTableToJson(L, -1, depth + 1);
                break;
            case LUA_TNIL:
                result[key] = nullptr;
                break;
            default:
                // Unsupported type -- skip
                break;
        }
        lua_pop(L, 1);  // pop value, keep key for next iteration
    }

    return result;
}

// -- nlohmann::json -> Lua table (recursive) -------------------------------
static void jsonToLuaTable(lua_State* L, const json& j, int depth = 0) {
    if (depth > kMaxTableDepth) return;  // corrupt save: truncate instead of crashing
    lua_newtable(L);

    if (j.is_object()) {
        for (auto it = j.begin(); it != j.end(); ++it) {
            const std::string& key = it.key();
            const json& value = it.value();

            lua_pushstring(L, key.c_str());

            switch (value.type()) {
                case json::value_t::null:
                    lua_pushnil(L);
                    break;
                case json::value_t::boolean:
                    lua_pushboolean(L, value.get<bool>() ? 1 : 0);
                    break;
                case json::value_t::number_integer:
                case json::value_t::number_unsigned:
                    lua_pushinteger(L, (lua_Integer)value.get<int64_t>());
                    break;
                case json::value_t::number_float:
                    lua_pushnumber(L, value.get<double>());
                    break;
                case json::value_t::string:
                    lua_pushstring(L, value.get<std::string>().c_str());
                    break;
                case json::value_t::array:
                    jsonToLuaTable(L, value, depth + 1);  // array -> indexed table
                    break;
                case json::value_t::object:
                    jsonToLuaTable(L, value, depth + 1);
                    break;
                default:
                    lua_pushnil(L);
                    break;
            }

            lua_settable(L, -3);
        }
    } else if (j.is_array()) {
        int idx = 1;
        for (const auto& elem : j) {
            switch (elem.type()) {
                case json::value_t::null:
                    lua_pushnil(L);
                    break;
                case json::value_t::boolean:
                    lua_pushboolean(L, elem.get<bool>() ? 1 : 0);
                    break;
                case json::value_t::number_integer:
                case json::value_t::number_unsigned:
                    lua_pushinteger(L, (lua_Integer)elem.get<int64_t>());
                    break;
                case json::value_t::number_float:
                    lua_pushnumber(L, elem.get<double>());
                    break;
                case json::value_t::string:
                    lua_pushstring(L, elem.get<std::string>().c_str());
                    break;
                case json::value_t::array:
                case json::value_t::object:
                    jsonToLuaTable(L, elem, depth + 1);
                    break;
                default:
                    lua_pushnil(L);
                    break;
            }
            lua_rawseti(L, -2, idx++);
        }
    }
}

// -- lua_Save_game ----------------------------------------------------------
// KAG.save_game(slot, dataTable, sceneName, tokenIndex, [thumbnail]) -> bool
static int lua_Save_game(lua_State* L) {
    int slot              = (int)luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    const char* sceneName = luaL_checkstring(L, 3);
    int tokenIndex        = (int)luaL_checkinteger(L, 4);
    const char* thumbnail = luaL_optstring(L, 5, "");

    // Convert Lua table to structured JSON
    json gameData = luaTableToJson(L, 2);

    auto* manager = getSaveManager();
    bool ok = manager && manager->save(slot, gameData, sceneName,
                                       tokenIndex, thumbnail);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// -- lua_Load_game ----------------------------------------------------------
// KAG.load_game(slot) -> dataTable, metaTable | nil, error
static int lua_Load_game(lua_State* L) {
    int slot = (int)luaL_checkinteger(L, 1);

    auto* manager = getSaveManager();
    if (!manager) {
        lua_pushnil(L);
        lua_pushstring(L, "Save manager is not available");
        return 2;
    }

    SaveMeta meta;
    json data = manager->load(slot, &meta);

    // Only a missing/corrupt save (load() returns json()) is a failure. An
    // empty table {} or [] saved by the game is a valid save and must load.
    if (data.is_null()) {
        lua_pushnil(L);
        lua_pushfstring(L, "Failed to load save slot %d", slot);
        return 2;
    }

    // Push game data as Lua table
    jsonToLuaTable(L, data);

    // Push metadata table
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

    return 2;  // dataTable, metaTable
}

// -- lua_List_saves ---------------------------------------------------------
// KAG.list_saves() -> {{slot=N, timestamp=T, scene=S, token_index=I}, ...}
static int lua_List_saves(lua_State* L) {
    auto* manager = getSaveManager();
    const auto saves = manager ? manager->listSaves() : std::vector<SaveMeta>{};

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
        lua_rawseti(L, -2, idx++);
    }

    return 1;
}

// -- lua_Delete_save ---------------------------------------------------------
// KAG.delete_save(slot) -> bool
static int lua_Delete_save(lua_State* L) {
    int slot = (int)luaL_checkinteger(L, 1);
    auto* manager = getSaveManager();
    bool ok = manager && manager->deleteSlot(slot);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// -- lua_Save_exists ---------------------------------------------------------
// KAG.save_exists(slot) -> bool
static int lua_Save_exists(lua_State* L) {
    int slot = (int)luaL_checkinteger(L, 1);
    auto* manager = getSaveManager();
    bool ok = manager && manager->slotExists(slot);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// -- lua_Save_get_dir --------------------------------------------------------

// -- lua_SetEncryptionKey ---------------------------------------------------
static int lua_SetEncryptionKey(lua_State* L) {
    size_t len; const char* keyStr = luaL_checklstring(L, 1, &len);
    if (len != 32) { lua_pushboolean(L, 0); lua_pushstring(L, "Key must be 32 bytes"); return 2; }
    auto* manager = getSaveManager();
    if (!manager) { lua_pushboolean(L, 0); return 1; }
    manager->setEncryptionKey(reinterpret_cast<const uint8_t*>(keyStr));
    lua_pushboolean(L, 1); return 1;
}

// -- lua_ClearEncryptionKey -------------------------------------------------
static int lua_ClearEncryptionKey(lua_State* L) {
    if (auto* manager = getSaveManager()) manager->clearEncryptionKey();
    return 0;
}

// -- lua_CaptureThumbnail ---------------------------------------------------
static int lua_CaptureThumbnail(lua_State* L) {
    auto* manager = getSaveManager();
    if (!manager) { lua_pushnil(L); return 1; }
    std::string b64 = manager->captureThumbnailPNG(320, 180);
    if (b64.empty()) { lua_pushnil(L); return 1; }
    lua_pushlstring(L, b64.c_str(), b64.size()); return 1;
}

// KAG.get_save_dir() -> string
static int lua_Get_save_dir(lua_State* L) {
    lua_pushstring(L, "saves/");
    return 1;
}

// ============================================================================
//  Registration
// ============================================================================

// -- Cloud sync (C7): HTTP cloud-save endpoint + slot push/pull ------------
// KAG.save.configure_cloud(endpoint) -- "" disables (back to local-only)
// KAG.save.cloud_push(slot) / cloud_pull(slot) -- bool, offline-safe
static int lua_ConfigureCloud(lua_State* L) {
    auto* manager = getSaveManager();
    if (!manager) { lua_pushboolean(L, 0); return 1; }
    const char* endpoint = luaL_optstring(L, 1, "");
    lua_pushboolean(L, manager->configureCloudSync(endpoint) ? 1 : 0);
    return 1;
}
static int lua_CloudPush(lua_State* L) {
    auto* manager = getSaveManager();
    if (!manager) { lua_pushboolean(L, 0); return 1; }
    const int slot = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, manager->pushSlotToCloud(slot) ? 1 : 0);
    return 1;
}
static int lua_CloudPull(lua_State* L) {
    auto* manager = getSaveManager();
    if (!manager) { lua_pushboolean(L, 0); return 1; }
    const int slot = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, manager->pullSlotFromCloud(slot) ? 1 : 0);
    return 1;
}

void registerSaveBinding(lua_State* L) {
    // [R12-FIX] Registration order note:
    // SaveBinding functions are appended to the existing "KAG" global table.
    // This requires that KAGBinding already registered the KAG table first.
    // LuaManager::registerModules() ensures this order (KAGBinding at line ~102,
    // SaveBinding at line ~108). If reordering, maintain this dependency.

    // Get or create the global KAG table
    lua_getglobal(L, "KAG");
    bool hasKAG = lua_istable(L, -1);

    if (!hasKAG) {
        lua_pop(L, 1);
        lua_newtable(L);
    }

static const luaL_Reg saveFuncs[] = {
        { "save_game",    lua_Save_game    },
        { "load_game",    lua_Load_game    },
        { "list_saves",   lua_List_saves   },
        { "delete_save",  lua_Delete_save  },
        { "save_exists",  lua_Save_exists  },
        { "get_save_dir", lua_Get_save_dir },
        { "set_encryption_key",  lua_SetEncryptionKey  },
        { "clear_encryption_key", lua_ClearEncryptionKey },
        { "capture_thumbnail",    lua_CaptureThumbnail },
        { "configure_cloud",     lua_ConfigureCloud },
        { "cloud_push",          lua_CloudPush },
        { "cloud_pull",          lua_CloudPull },
        { nullptr, nullptr }
    };

    luaL_setfuncs(L, saveFuncs, 0);

    if (!hasKAG) {
        lua_setglobal(L, "KAG");
    } else {
        lua_pop(L, 1);
    }

    printf("[SaveBinding] KAG save/load functions registered (9 APIs).\n");
}

} // namespace Caesura
