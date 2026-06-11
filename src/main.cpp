extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include "render/BgfxRenderDevice.h"
#include "Audio/SoLoudAudioEngine.h"
#include "Audio/NullAudioBackend.h"
#include "platform/SDL3PlatformBackend.h"
#include "MiniGame/BgfxMiniGameBackend.h"
#include "MiniGame/NullMiniGameBackend.h"
#include "../Live2D/NullAnimationBackend.h"
#include "../Steam/NullSteamBackend.h"
#include "../Steam/SteamBackend.h"
#include "di/BackendRegistry.h"
#include "entry/Engine.h"
#include "Render/TextureManager.h"
#include "script/vm/LuaManager.h"
#include <cstdio>
#include <string>
#include "archive/CARCReader.h"
#include "rpc/RpcServer.h"
#include <thread>
#include <atomic>

int main(int argc, char* argv[]) {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    fprintf(stderr, "[main] Starting Caesura (AmeKAG)...\n");

    // -- Parse CLI flags -------------------------------------------------
    bool headless = false;
    bool editorMode = false;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--headless") {
            headless = true;
        } else if (arg == "--editor") {
            editorMode = true;
        }
    }

    printf("============================================\n");
    printf("  Caesura (AmeKAG) v1.0.0\n");
    printf("  Cross-platform Visual Novel Engine\n");
    printf("  SDL3 + bgfx + SoLoud + Lua\n");
    if (headless) printf("  [HEADLESS MODE]\n");
    printf("============================================\n\n");

    Caesura::EngineConfig config;
    config.title      = "Caesura (AmeKAG)";
    config.width      = 1280;
    config.height     = 720;
    config.headless   = headless;
    config.editorMode = editorMode;

    // Create all concrete implementations here (composition root)
    config.platform  = new SDL3PlatformBackend();
    config.render    = new BgfxRenderDevice();
    config.audio     = new SoLoudAudioEngine();
    config.miniGame  = new BgfxMiniGameBackend();
    config.animation = new NullAnimationBackend();
    config.steam     = new NullSteamBackend();

    Caesura::Engine engine(config);

    if (!engine.init()) {
        fprintf(stderr, "Failed to initialize engine.\n");
        return 1;
    }

    // -- Editor mode: hidden window + GPU + JSON-RPC ------------------------
    if (editorMode) {
        fprintf(stderr, "[main] Editor mode: JSON-RPC on stdin/stdout (GPU enabled)\n");

        std::string scriptDir = "scripts/";
        if (FILE* f = fopen("scripts/kag/init.lua", "r")) {
            fclose(f);
        } else {
            scriptDir = "../../scripts/";
            if (FILE* f = fopen("../../scripts/kag/init.lua", "r")) { fclose(f); }
            else { scriptDir = "../../../scripts/"; }
        }
        // Set Lua package.path BEFORE loading any scripts
        {
            lua_State* L = engine.lua().state();
            if (L) {
                lua_getglobal(L, "package");
                lua_getfield(L, -1, "path");
                std::string cp = lua_tostring(L, -1);
                lua_pop(L, 1);
                std::string np = scriptDir + "?.lua;" + scriptDir + "?/init.lua;" + scriptDir + "kag/?.lua;" + cp;
                lua_pushstring(L, np.c_str());
                lua_setfield(L, -2, "path");
                lua_pop(L, 1);
            }
        }
        engine.lua().loadScript("scripts/config.lua");
        engine.lua().loadScript((scriptDir + "kag/init.lua").c_str());
        engine.lua().lockdownScriptEnv();

        // Wire up frame capture for getFrame RPC
        Caesura::RpcServer::instance().setFrameCaptureCallback(
            [&engine](int w, int h) -> std::string {
                return engine.captureFrameForRpc(w, h);
            });

        engine.runRpc();
        printf("Caesura (AmeKAG) shut down cleanly.\n");
        return 0;
    }

    // -- Headless mode: stdin/stdout JSON-RPC -----------------------------
    if (headless) {
        fprintf(stderr, "[main] Headless mode: JSON-RPC on stdin/stdout\n");

        // Load minimal config for Lua VM
        std::string scriptDir = "scripts/";
        if (FILE* f = fopen("scripts/kag/init.lua", "r")) {
            fclose(f);
        } else {
            scriptDir = "../../scripts/";
            if (FILE* f = fopen("../../scripts/kag/init.lua", "r")) { fclose(f); }
            else { scriptDir = "../../../scripts/"; }
        }
        // Set Lua package.path BEFORE loading any scripts
        {
            lua_State* L = engine.lua().state();
            if (L) {
                lua_getglobal(L, "package");
                lua_getfield(L, -1, "path");
                std::string cp = lua_tostring(L, -1);
                lua_pop(L, 1);
                std::string np = scriptDir + "?.lua;" + scriptDir + "?/init.lua;" + scriptDir + "kag/?.lua;" + cp;
                lua_pushstring(L, np.c_str());
                lua_setfield(L, -2, "path");
                lua_pop(L, 1);
            }
        }
        engine.lua().loadScript("scripts/config.lua");
        engine.lua().loadScript((scriptDir + "kag/init.lua").c_str());
        engine.lua().lockdownScriptEnv();

        // Enter RPC loop (blocks until stdin EOF or quit command)
        engine.runRpc();
        printf("Caesura (AmeKAG) shut down cleanly.\n");
        return 0;
    }

    // Script directory discovery (3-level fallback)
    std::string scriptDir = "scripts/";

    if (FILE* f = fopen("scripts/kag/init.lua", "r")) {
        fclose(f);
    } else {
        scriptDir = "../../scripts/";
        if (FILE* f = fopen("../../scripts/kag/init.lua", "r")) {
            fclose(f);
        } else {
            scriptDir = "../../../scripts/";
            if (FILE* f = fopen("../../../scripts/kag/init.lua", "r")) {
                fclose(f);
            } else {
                fprintf(stderr, "[main] Warning: Cannot find scripts directory.\n");
                scriptDir = "scripts/";
            }
        }
    }

    // Add scripts dir to Lua package.path
    lua_State* L = engine.lua().state();
    if (L) {
        lua_getglobal(L, "package");
        lua_getfield(L, -1, "path");
        std::string currentPath = lua_tostring(L, -1);
        lua_pop(L, 1);
        std::string newPath = scriptDir + "?.lua;" + scriptDir + "?/init.lua;" + currentPath;
        lua_pushstring(L, newPath.c_str());
        lua_setfield(L, -2, "path");
        lua_pop(L, 1);
    }

    // Load config first (backend selection happens here)
    engine.lua().loadScript((scriptDir + "config.lua").c_str());

    // [10.2.57] Apply dev mode to placeholder texture
    if (L) {
        lua_getglobal(L, "config");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "dev_mode");
            bool devMode = lua_toboolean(L, -1) != 0;
            lua_pop(L, 1);
            Caesura::TextureManager::instance().setDevMode(devMode);
        }
        lua_pop(L, 1);
    }

    // Load KAG init (loads all Lua libraries)
    if (!engine.lua().loadScript((scriptDir + "kag/init.lua").c_str())) {
        fprintf(stderr, "Warning: Failed to load KAG init.\n");
    }

    // [10.2.30] CARC startup validation
    if (L) {
        lua_getglobal(L, "config");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "carc_verify_on_startup");
            bool verifyCarc = lua_toboolean(L, -1) != 0;
            lua_pop(L, 1);
            if (verifyCarc) {
                printf("[main] CARC startup validation enabled.\n");
                // Scan for .carc archives in current directory and verify
                namespace carc_ns = Caesura::carc;
                const char* dataFiles[] = {"data.carc", "game.carc", "patch.carc"};
                for (const char* fname : dataFiles) {
                    carc_ns::CARCReader reader;
                    if (reader.open(fname)) {
                        bool ok = reader.verifySignature();
                        printf("[main] CARC %s: signature %s\n", fname, ok ? "OK" : "FAILED");
                        reader.close();
                    }
                }
            }
        }
        lua_pop(L, 1);
    }

    // Load main game logic (entry point from config) [10.2.30]
    std::string entryScript = "game_logic.lua";
    if (L) {
        lua_getglobal(L, "config");
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "entry_script");
            if (lua_isstring(L, -1)) {
                entryScript = lua_tostring(L, -1);
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    printf("[main] Entry script: %s\n", entryScript.c_str());

if (!engine.lua().loadScript((scriptDir + entryScript).c_str())) {
        fprintf(stderr, "Warning: Failed to load game_logic.lua.\n");
        return 1;
    }

    // Push _CAESURA_CONFIG global for sandbox to read
    lua_getglobal(L, "config");  // config table loaded by config.lua
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "dev_mode");
        bool devMode = lua_toboolean(L, -1);
        lua_pop(L, 1);
        
        lua_newtable(L);
        lua_pushboolean(L, devMode ? 1 : 0);
        lua_setfield(L, -2, "dev_mode");
        lua_setglobal(L, "_CAESURA_CONFIG");
        printf("[main] _CAESURA_CONFIG.dev_mode = %s\n", devMode ? "true" : "false");
    }
    lua_pop(L, 1);


    // C3+W8: lockdown script env after ALL scripts are preloaded
    engine.lua().lockdownScriptEnv();

    engine.run();

    printf("Caesura (AmeKAG) shut down cleanly.\n");
    return 0;
}


