extern "C" {
#include <lua.h>
}

#include <cstdio>
#include <string>

namespace Caesura {

std::string discoverStartupScriptDir() {
    if (FILE* f = fopen("scripts/kag/init.lua", "r")) {
        fclose(f);
        return "scripts/";
    }
    if (FILE* f = fopen("../../scripts/kag/init.lua", "r")) {
        fclose(f);
        return "../../scripts/";
    }
    if (FILE* f = fopen("../../../scripts/kag/init.lua", "r")) {
        fclose(f);
        return "../../../scripts/";
    }

    fprintf(stderr, "[main] Warning: Cannot find scripts directory.\n");
    return "scripts/";
}

void configureStartupLuaPath(lua_State* L, const std::string& scriptDir) {
    if (!L) {
        return;
    }

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

} // namespace Caesura
