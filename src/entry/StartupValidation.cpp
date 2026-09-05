extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include "../di/BackendRegistry.h"
#include "../render/api/ITextureManager.h"

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

} // namespace Caesura
