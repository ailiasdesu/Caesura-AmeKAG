extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "../archive/CARCReader.h"
#include "../di/BackendRegistry.h"
#include "../render/api/ITextureManager.h"

#include <cstdio>

namespace Caesura {

void applyDevModeToTextureManager(lua_State* L) {
    if (!L) return;
    lua_getglobal(L, "config");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "dev_mode");
        const bool devMode = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        if (auto* textures = BackendRegistry::instance().getTextureManager()) {
            textures->setDevMode(devMode);
        }
    }
    lua_pop(L, 1);
}

void validateCarcOnStartup(lua_State* L) {
    if (!L) return;
    lua_getglobal(L, "config");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    lua_getfield(L, -1, "carc_verify_on_startup");
    const bool verifyCarc = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    if (!verifyCarc) {
        lua_pop(L, 1);
        return;
    }
    lua_pop(L, 1);

    printf("[main] CARC startup validation enabled.\n");
    const char* dataFiles[] = {"data.carc", "game.carc", "patch.carc"};
    for (const char* fname : dataFiles) {
        carc::CARCReader reader;
        if (reader.open(fname)) {
            const bool ok = reader.verifySignature();
            printf("[main] CARC %s: signature %s\n", fname, ok ? "OK" : "FAILED");
            reader.close();
        }
    }
}

} // namespace Caesura
