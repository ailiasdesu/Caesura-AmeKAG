#include "doctest.h"
#include "script/vm/LuaManager.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Caesura;

TEST_CASE("LuaManager::init creates valid state") {
    LuaManager lm;
    CHECK(lm.init());
    CHECK(lm.state() != nullptr);
}

TEST_CASE("LuaManager::shutdown idempotent") {
    LuaManager lm;
    lm.init();
    lm.shutdown();
    lm.shutdown();
    CHECK(lm.state() == nullptr);
}

TEST_CASE("LuaManager::lockdownScriptEnv removes dangerous globals") {
    LuaManager lm;
    lm.init();
    lm.lockdownScriptEnv();
    lua_State* L = lm.state();
    REQUIRE(L != nullptr);

    // loadfile should be removed
    lua_getglobal(L, "loadfile");
    CHECK(lua_isnil(L, -1));
    lua_pop(L, 1);

    // dofile should be removed
    lua_getglobal(L, "dofile");
    CHECK(lua_isnil(L, -1));
    lua_pop(L, 1);

    // debug should be restricted (read-only subset)
    lua_getglobal(L, "debug");
    CHECK(lua_istable(L, -1));
    lua_pop(L, 1);

    // require should be overridden (safe wrapper)
    lua_getglobal(L, "require");
    CHECK(lua_isfunction(L, -1));
    lua_pop(L, 1);
}

TEST_CASE("LuaManager::loadScript fails on nonexistent file") {
    LuaManager lm;
    lm.init();
    CHECK_FALSE(lm.loadScript("nonexistent_script.lua"));
}

TEST_CASE("LuaManager::double init is safe") {
    LuaManager lm;
    CHECK(lm.init());
    CHECK(lm.init());  // second call
    CHECK(lm.state() != nullptr);
}

// =============================================================================
// Expanded: instruction budget, update, resumeKAG
// =============================================================================

TEST_CASE("LuaManager::instruction budget set/get round-trip") {
    LuaManager lm;
    lm.setInstructionBudget(200000);
    CHECK(lm.getInstructionBudget() == 200000);
    lm.resetInstructionBudget();
    CHECK_FALSE(lm.isInstructionBudgetExceeded());
}

TEST_CASE("LuaManager::instruction budget interrupts infinite loop") {
    LuaManager lm;

    // Set very low budget (100 instructions) and run an infinite loop
    // The instruction hook fires on LuaManager::instance(), not on locals.
    auto& mgr = LuaManager::instance();
    REQUIRE(mgr.init());
    lua_State* L = mgr.state();
    REQUIRE(L != nullptr);

    // Save budget to restore after test
    int oldBudget = mgr.getInstructionBudget();
    mgr.setInstructionBudget(100);
    // With budget=100 and hook firing every 1000 instrs, the loop will
    // be interrupted after multiple hook invocations. Verify it doesn't hang.
    int result = luaL_dostring(L, "while true do end");
    // The loop should be interrupted (not hang forever)
    CHECK(result != LUA_OK);
    mgr.resetInstructionBudget();
    mgr.setInstructionBudget(oldBudget);  // Restore for other tests
    CHECK_FALSE(mgr.isInstructionBudgetExceeded());
}

TEST_CASE("LuaManager::update does not crash") {
    LuaManager lm;
    lm.init();
    lm.update(0.016f);
    lm.update(0.0f);
}

TEST_CASE("LuaManager::resumeKAGCoroutine does not crash without coroutine") {
    LuaManager lm;
    lm.init();
    // No KAG coroutine running — should be a safe no-op
    lm.resumeKAGCoroutine();
}

TEST_CASE("Lua BackendFactory name commands report actual engine backend info") {
    LuaManager lm;
    REQUIRE(lm.init());
    lua_State* L = lm.state();
    REQUIRE(L != nullptr);

    const char* script =
        "package.path = 'scripts/?.lua;scripts/?/init.lua;' .. package.path\n"
        "package.loaded['backend_factory'] = nil\n"
        "KAG = {}\n"
        "Render = {}\n"
        "DevCore = {}\n"
        "Engine = {\n"
        "  select_platform_backend = function() return true end,\n"
        "  get_backend_info = function()\n"
        "    return { render = 'NullRender', audio = 'NullAudio', platform = 'NullPlatform' }\n"
        "  end,\n"
        "}\n"
        "local BackendFactory = require('backend_factory')\n"
        "local backend = BackendFactory.create({ render = 'bgfx', audio = 'soloud', platform = 'sdl3' })\n"
        "assert(backend.render('name') == 'NullRender', backend.render('name'))\n"
        "assert(backend.audio('name') == 'NullAudio', backend.audio('name'))\n"
        "assert(backend.platform('name') == 'NullPlatform', backend.platform('name'))\n";

    int luaStatus = luaL_loadstring(L, script);
    if (luaStatus == LUA_OK) {
        luaStatus = lua_pcall(L, 0, 0, 0);
    }
    if (luaStatus != LUA_OK) {
        CAPTURE(lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    CHECK(luaStatus == LUA_OK);
}

TEST_CASE("Lua config ready log reports actual engine backend info") {
    LuaManager lm;
    REQUIRE(lm.init());
    lua_State* L = lm.state();
    REQUIRE(L != nullptr);

    const char* script =
        "package.path = 'scripts/?.lua;scripts/?/init.lua;' .. package.path\n"
        "package.loaded['backend_factory'] = nil\n"
        "local captured = {}\n"
        "local oldPrint = print\n"
        "print = function(msg) captured[#captured + 1] = tostring(msg) end\n"
        "KAG = { set_bus_volume = function() return true end }\n"
        "Render = {}\n"
        "DevCore = {\n"
        "  set_resolution = function() return true end,\n"
        "  set_fullscreen = function() return true end,\n"
        "}\n"
        "Engine = {\n"
        "  select_platform_backend = function() return true end,\n"
        "  get_backend_info = function()\n"
        "    return { render = 'NullRender', audio = 'NullAudio', platform = 'NullPlatform' }\n"
        "  end,\n"
        "}\n"
        "local ok, err = pcall(dofile, 'scripts/config.lua')\n"
        "print = oldPrint\n"
        "assert(ok, err)\n"
        "local expected = '[config] Backends ready: render=NullRender audio=NullAudio platform=NullPlatform'\n"
        "for _, line in ipairs(captured) do\n"
        "  if line == expected then return end\n"
        "end\n"
        "error('missing ready log: ' .. table.concat(captured, '\\n'))\n";

    int luaStatus = luaL_loadstring(L, script);
    if (luaStatus == LUA_OK) {
        luaStatus = lua_pcall(L, 0, 0, 0);
    }
    if (luaStatus != LUA_OK) {
        CAPTURE(lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    CHECK(luaStatus == LUA_OK);
}

TEST_CASE("Lua backend audio helpers prefer unified backend proxy") {
    LuaManager lm;
    REQUIRE(lm.init());
    lua_State* L = lm.state();
    REQUIRE(L != nullptr);

    const char* script =
        "package.path = 'scripts/?.lua;scripts/?/init.lua;' .. package.path\n"
        "package.loaded['backend'] = nil\n"
        "local calls = {}\n"
        "_CAESURA_BACKEND = {\n"
        "  audio = function(cmd, ...)\n"
        "    calls[#calls + 1] = cmd\n"
        "    if cmd == 'is_voice_playing' then return true end\n"
        "    if cmd == 'is_bgm_playing' then return true end\n"
        "    if cmd == 'is_playing' then return true end\n"
        "    return 'proxy:' .. cmd\n"
        "  end,\n"
        "}\n"
        "KAG = {\n"
        "  play_bgm = function() error('fallback play_bgm used') end,\n"
        "  play_voice = function() error('fallback play_voice used') end,\n"
        "  play_se = function() error('fallback play_se used') end,\n"
        "  stop_bgm = function() error('fallback stop_bgm used') end,\n"
        "  stop_voice = function() error('fallback stop_voice used') end,\n"
        "  stop_se = function() error('fallback stop_se used') end,\n"
        "  is_voice_playing = function() error('fallback is_voice_playing used') end,\n"
        "  is_bgm_playing = function() error('fallback is_bgm_playing used') end,\n"
        "  is_se_playing = function() error('fallback is_se_playing used') end,\n"
        "}\n"
        "local backend = require('backend')\n"
        "assert(backend.audio_play('bgm', 'song.ogg') == 'proxy:play_bgm')\n"
        "assert(backend.audio_play('voice', 'voice.ogg') == 'proxy:play_voice')\n"
        "assert(backend.audio_play('se', 'click.wav') == 'proxy:play_se')\n"
        "assert(backend.audio_stop('bgm') == 'proxy:stop_bgm')\n"
        "assert(backend.audio_stop('voice') == 'proxy:stop_voice')\n"
        "assert(backend.audio_stop('se') == 'proxy:stop_se')\n"
        "assert(backend.audio_is_playing('voice') == true)\n"
        "assert(backend.audio_is_playing('bgm') == true)\n"
        "assert(backend.audio_is_playing('se') == true)\n"
        "assert(#calls == 9, 'calls=' .. tostring(#calls))\n";

    int luaStatus = luaL_loadstring(L, script);
    if (luaStatus == LUA_OK) {
        luaStatus = lua_pcall(L, 0, 0, 0);
    }
    if (luaStatus != LUA_OK) {
        CAPTURE(lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    CHECK(luaStatus == LUA_OK);
}

TEST_CASE("Lua backend platform helpers prefer unified backend proxy") {
    LuaManager lm;
    REQUIRE(lm.init());
    lua_State* L = lm.state();
    REQUIRE(L != nullptr);

    const char* script =
        "package.path = 'scripts/?.lua;scripts/?/init.lua;' .. package.path\n"
        "package.loaded['backend'] = nil\n"
        "local calls = {}\n"
        "_CAESURA_BACKEND = {\n"
        "  platform = function(cmd, ...)\n"
        "    calls[#calls + 1] = cmd\n"
        "    if cmd == 'get_resolution' then return 1280, 720 end\n"
        "    if cmd == 'get_input_focus' then return 'game' end\n"
        "    return 'proxy:' .. cmd\n"
        "  end,\n"
        "}\n"
        "DevCore = {\n"
        "  set_resolution = function() error('fallback set_resolution used') end,\n"
        "  get_resolution = function() error('fallback get_resolution used') end,\n"
        "  set_input_focus = function() error('fallback set_input_focus used') end,\n"
        "  get_input_focus = function() error('fallback get_input_focus used') end,\n"
        "  set_fullscreen = function() error('fallback set_fullscreen used') end,\n"
        "}\n"
        "local backend = require('backend')\n"
        "assert(backend.set_resolution(1920, 1080) == 'proxy:set_resolution')\n"
        "local w, h = backend.get_resolution()\n"
        "assert(w == 1280 and h == 720, tostring(w) .. 'x' .. tostring(h))\n"
        "assert(backend.set_input_focus('ui') == 'proxy:set_input_focus')\n"
        "assert(backend.get_input_focus() == 'game')\n"
        "assert(backend.set_fullscreen(true) == 'proxy:set_fullscreen')\n"
        "assert(#calls == 5, 'calls=' .. tostring(#calls))\n";

    int luaStatus = luaL_loadstring(L, script);
    if (luaStatus == LUA_OK) {
        luaStatus = lua_pcall(L, 0, 0, 0);
    }
    if (luaStatus != LUA_OK) {
        CAPTURE(lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    CHECK(luaStatus == LUA_OK);
}
