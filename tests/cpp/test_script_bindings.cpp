// test_script_bindings.cpp - script bindings unit tests (R4.1)
#include "doctest.h"
#include "script/vm/LuaManager.h"
#include "script/bindings/KAGBinding.h"
#include "script/bindings/RenderBinding.h"
#include "script/bindings/VFXBinding.h"
#include "script/bindings/DebugBinding.h"
#include "script/bindings/DevCoreBinding.h"
#include "script/bindings/UnifiedBinding.h"
#include "script/bindings/SteamBinding.h"
#include "di/BackendRegistry.h"
#include "input/InputRouter.h"
#include "render/NullRenderDevice.h"
#include "platform/NullPlatformBackend.h"
#include "steam/NullSteamBackend.h"

#include <string>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Caesura;

namespace {

class UnlockingSteamBackend final : public NullSteamBackend {
public:
    bool unlockAchievement(const char* id) override {
        lastAchievement = id ? id : "";
        return true;
    }

    std::string lastAchievement;
};

class ScopedSteamRegistration {
public:
    explicit ScopedSteamRegistration(ISteamBackend* backend)
        : m_previous(BackendRegistry::instance().getSteamBackend()) {
        BackendRegistry::instance().setSteamBackend(backend);
    }

    ~ScopedSteamRegistration() {
        BackendRegistry::instance().setSteamBackend(m_previous);
    }

private:
    ISteamBackend* m_previous;
};

} // namespace

static LuaManager* initBindingLua() {
    auto* lm = new LuaManager();
    if (!lm->init()) { delete lm; return nullptr; }
    lua_State* L = lm->state();
    static NullRenderDevice render;
    static NullPlatformBackend platform;
    BackendRegistry::instance().setRenderDevice(&render);
    BackendRegistry::instance().setPlatformBackend(&platform);
    registerKAGBinding(L);
    registerRenderBinding(L);
    registerVFXBinding(L);
    registerDebugBinding(L);
    registerDevCoreBinding(L);
    registerUnifiedBackendBinding(L);
    return lm;
}

TEST_CASE("Bindings: DevCore module registered as global") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "DevCore");
    CHECK(lua_istable(L, -1));
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("Bindings: DevCore.log callable") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "DevCore");
    lua_getfield(L, -1, "log");
    CHECK(lua_isfunction(L, -1));
    lua_pushstring(L, "test message");
    int ret = lua_pcall(L, 1, 0, 0);
    CHECK(ret == LUA_OK);
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("Bindings: DevCore input focus roundtrip and validation") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    InputRouter router;
    lua_pushlightuserdata(L, &router);
    lua_setfield(L, LUA_REGISTRYINDEX, "Caesura.InputRouter");

    const int focusScriptResult = luaL_dostring(L,
        "local ok = DevCore.set_input_focus('game')\n"
        "assert(ok == true)\n"
        "assert(DevCore.get_input_focus() == 'GAME')\n"
        "local invalid, reason = DevCore.set_input_focus('UI')\n"
        "assert(invalid == false and type(reason) == 'string')\n"
        "assert(DevCore.get_input_focus() == 'GAME')\n"
        "local wrong_type, type_reason = DevCore.set_input_focus({})\n"
        "assert(wrong_type == false and type(type_reason) == 'string')\n"
        "assert(DevCore.get_input_focus() == 'GAME')\n"
        "assert(DevCore.set_input_focus('KAG') == true)\n"
        "assert(DevCore.get_input_focus() == 'KAG')");
    REQUIRE(focusScriptResult == LUA_OK);
    CHECK(router.getFocus() == InputFocus::KAG);

    delete lm;
}

TEST_CASE("Bindings: VFX module registered") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "VFX");
    CHECK(lua_istable(L, -1));
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("Bindings: VFX particles functions exist") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "VFX");
    REQUIRE(lua_istable(L, -1));
    lua_getfield(L, -1, "particles_create_emitter");
    CHECK(lua_isfunction(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, -1, "particles_alive_count");
    CHECK(lua_isfunction(L, -1));
    lua_pop(L, 1);
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("Bindings: Render module registered") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "Render");
    CHECK(lua_istable(L, -1));
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("Bindings: KAG.play_bgm returns boolean") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "KAG");
    lua_getfield(L, -1, "play_bgm");
    lua_pushstring(L, "nonexistent.wav");
    lua_pushnumber(L, 0.5);
    int ret = lua_pcall(L, 2, 1, 0);
    CHECK(ret == LUA_OK);
    CHECK(lua_isboolean(L, -1));
    CHECK_FALSE(lua_toboolean(L, -1)); // returns false (no audio backend)
    lua_pop(L, 2);
    delete lm;
}

TEST_CASE("Bindings: KAG.stop_bgm does not crash") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "KAG");
    lua_getfield(L, -1, "stop_bgm");
    int ret = lua_pcall(L, 0, 0, 0);
    CHECK(ret == LUA_OK);
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("Bindings: Debug module functions exist") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "Debug");
    REQUIRE(lua_istable(L, -1));
    lua_getfield(L, -1, "get_last_error");
    CHECK(lua_isfunction(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, -1, "log");
    CHECK(lua_isfunction(L, -1));
    lua_pop(L, 1);
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("Bindings: KAG global table must have expected APIs") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();
    lua_getglobal(L, "KAG");
    REQUIRE(lua_istable(L, -1));
    lua_getfield(L, -1, "play_bgm");
    CHECK(lua_isfunction(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, -1, "play_se");
    CHECK(lua_isfunction(L, -1));
    lua_pop(L, 1);
    lua_getfield(L, -1, "render_text");
    CHECK(lua_isfunction(L, -1));
    lua_pop(L, 1);
    lua_pop(L, 1);
    delete lm;
}

TEST_CASE("Bindings: unified render delegates exact arguments and results") {
    auto* lm = initBindingLua();
    REQUIRE(lm != nullptr);
    lua_State* L = lm->state();

    const int result = luaL_dostring(L,
        "local text_args\n"
        "KAG.render_text = function(...)\n"
        "  text_args = table.pack(...)\n"
        "  return true, 'text-forwarded'\n"
        "end\n"
        "KAG.clear_text = function(...)\n"
        "  return select('#', ...), 'clear-forwarded'\n"
        "end\n"
        "KAG.render_ruby = function(...)\n"
        "  return select('#', ...), 'ruby-forwarded'\n"
        "end\n"
        "KAG.line_height = function(...) assert(select('#', ...) == 0); return 31 end\n"
        "KAG.is_voice_playing = function(...)\n"
        "  return select('#', ...), 'voice-forwarded'\n"
        "end\n"
        "local ok, marker = _CAESURA_BACKEND.render(\n"
        "  'render_text', 'dialogue', 12.5, 34.25, 10, 20, 30, 40)\n"
        "assert(ok == true and marker == 'text-forwarded')\n"
        "assert(text_args.n == 7)\n"
        "assert(text_args[1] == 'dialogue')\n"
        "assert(text_args[2] == 12.5 and text_args[3] == 34.25)\n"
        "assert(text_args[4] == 10 and text_args[5] == 20)\n"
        "assert(text_args[6] == 30 and text_args[7] == 40)\n"
        "local argc, clear_marker = _CAESURA_BACKEND.render('clear_text')\n"
        "assert(argc == 0 and clear_marker == 'clear-forwarded')\n"
        "local ruby_argc, ruby_marker = _CAESURA_BACKEND.render(\n"
        "  'render_ruby', 'base', 'ruby', 1, 2, 3, 4, 5, 6)\n"
        "assert(ruby_argc == 8 and ruby_marker == 'ruby-forwarded')\n"
        "assert(_CAESURA_BACKEND.render('line_height') == 31)\n"
        "local voice_argc, voice_marker = _CAESURA_BACKEND.audio('is_playing')\n"
        "assert(voice_argc == 0 and voice_marker == 'voice-forwarded')");
    if (result != LUA_OK) {
        MESSAGE("Lua error: "
                << (lua_tostring(L, -1) ? lua_tostring(L, -1) : "unknown"));
    }
    CHECK(result == LUA_OK);

    delete lm;
}

TEST_CASE("Bindings: Steam module resolves backend through BackendRegistry") {
    LuaManager lua;
    REQUIRE(lua.init());

    UnlockingSteamBackend steam;
    ScopedSteamRegistration registration(&steam);
    registerSteamBinding(lua.state());

    lua_getglobal(lua.state(), "steam");
    REQUIRE(lua_istable(lua.state(), -1));
    lua_getfield(lua.state(), -1, "unlock_achievement");
    lua_pushstring(lua.state(), "FIRST_SCENE");
    REQUIRE(lua_pcall(lua.state(), 1, 1, 0) == LUA_OK);
    CHECK(lua_toboolean(lua.state(), -1));
    CHECK(steam.lastAchievement == "FIRST_SCENE");
    lua_pop(lua.state(), 2);
}
