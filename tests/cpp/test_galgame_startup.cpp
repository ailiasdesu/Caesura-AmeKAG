#include "doctest.h"
#include "entry/Engine.h"
#include "entry/EngineConfig.h"
#include "script/vm/LuaManager.h"

#include <string>
#include <utility>
#include <cstdio>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Caesura;

namespace {
// [galgame-flake-H1] The init.lua require chain behaves like a single atomic
// loadScript from the caller's perspective: a failure anywhere (most likely a
// transient LUA_ERRFILE on Windows) just returns false with the VM error
// already popped. This wrapper turns that into a diagnosable assertion: on
// failure it captures which modules already made it into package.loaded (the
// failing require is the one right after the last present module) plus a
// filesystem existence probe, so a future flake can be pinned to a module.
bool loadOrCapture(LuaManager& lua, lua_State* L, const char* path) {
    if (lua.loadScript(path)) return true;

    CAPTURE(path);
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded");
    int count = 0;
    std::string loadedNames;
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            ++count;
            if (loadedNames.size() < 512) {
                if (!loadedNames.empty()) loadedNames += ",";
                const char* name = lua_tostring(L, -2);
                loadedNames += name ? name : "?";
            }
            lua_pop(L, 1);
        }
        if (loadedNames.size() >= 512) loadedNames += ",...";
    }
    lua_pop(L, 2); // package.loaded + package
    CAPTURE(count);
    if (!loadedNames.empty()) CAPTURE(loadedNames);

    std::FILE* f = std::fopen(path, "rb");
    const bool exists = (f != nullptr);
    if (f) std::fclose(f);
    CAPTURE(exists);
    return false;
}

void configurePackagePath(lua_State* L, const std::string& scriptDir) {
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    const char* current = lua_tostring(L, -1);
    std::string currentPath = current ? current : "";
    lua_pop(L, 1);

    const std::string newPath =
        scriptDir + "?.lua;" +
        scriptDir + "?/init.lua;" +
        scriptDir + "kag/?.lua;" +
        currentPath;

    lua_pushstring(L, newPath.c_str());
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);
}

std::string readEntryScript(lua_State* L) {
    std::string entryScript = "game_logic.lua";
    lua_getglobal(L, "config");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "entry_script");
        if (lua_isstring(L, -1)) {
            entryScript = lua_tostring(L, -1);
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return entryScript;
}

int runLua(lua_State* L, const char* script) {
    int status = luaL_loadstring(L, script);
    if (status == LUA_OK) {
        status = lua_pcall(L, 0, 0, 0);
    }
    return status;
}

} // namespace

TEST_CASE("Galgame startup smoke: headless loads config, KAG init, and configured entry script") {
    EngineConfig cfg;
    cfg.headless = true;

    Engine engine(std::move(cfg));
    REQUIRE(engine.init());

    lua_State* L = engine.lua().state();
    REQUIRE(L != nullptr);

    configurePackagePath(L, "scripts/");

    REQUIRE(loadOrCapture(engine.lua(), L, "scripts/config.lua"));
    REQUIRE(loadOrCapture(engine.lua(), L, "scripts/kag/init.lua"));

    const std::string entryScript = readEntryScript(L);
    CAPTURE(entryScript);
    REQUIRE(entryScript == "../demo/entry.lua");
    REQUIRE(loadOrCapture(engine.lua(), L, ("scripts/" + entryScript).c_str()));

    const char* apiProbe =
        "assert(type(Engine) == 'table', 'Engine table missing')\n"
        "assert(type(Engine.get_backend_info) == 'function', 'Engine.get_backend_info missing')\n"
        "local info = Engine.get_backend_info()\n"
        "assert(info.render == 'NullRender', 'render=' .. tostring(info.render))\n"
        "assert(info.audio == 'NullAudio', 'audio=' .. tostring(info.audio))\n"
        "assert(info.platform == 'NullPlatform', 'platform=' .. tostring(info.platform))\n"
        "assert(type(KAG) == 'table', 'KAG table missing')\n"
        "assert(type(KAG.render_text) == 'function', 'KAG.render_text missing')\n"
        "assert(type(_KAG_onClick) == 'function', '_KAG_onClick missing after entry script')\n"
        "assert(type(engine_update) == 'function', 'engine_update missing after entry script')\n"
        "assert(type(engine_render) == 'function', 'engine_render missing after entry script')\n";

    int status = runLua(L, apiProbe);
    if (status != LUA_OK) {
        CAPTURE(lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    CHECK(status == LUA_OK);
}
