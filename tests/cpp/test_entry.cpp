// test_entry.cpp - entry module unit tests (R4.2, extended S3)
#include "doctest.h"
#include "entry/Engine.h"
#include "entry/EngineConfig.h"
#include "di/BackendRegistry.h"
#include "render/api/IGpuMonitor.h"
#include "script/vm/LuaManager.h"
#include <cstring>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Caesura;

namespace {

class TestGpuMonitor : public IGpuMonitor {
public:
    GpuQuality update(double) override { return GpuQuality::HIGH; }
    const FrameMetrics& metrics() override { return m_metrics; }
    GpuQuality currentQuality() const override { return GpuQuality::HIGH; }
    bool isDegraded() const override { return false; }
    float resolutionScale() const override { return 1.0f; }
    bool vfxEnabled() const override { return true; }
    void reset() override {}

private:
    FrameMetrics m_metrics;
};

} // namespace

TEST_CASE("Entry: EngineConfig default values") {
    EngineConfig cfg;
    CHECK(cfg.width == 1280);
    CHECK(cfg.height == 720);
    CHECK(cfg.title != nullptr);
    CHECK(std::strcmp(cfg.title, "Caesura (AmeKAG)") == 0);
    CHECK(cfg.headless == false);
    CHECK(cfg.editorMode == false);
}

TEST_CASE("Entry: EngineConfig pointer fields default nullptr") {
    EngineConfig cfg;
    CHECK(cfg.platform == nullptr);
    CHECK(cfg.render == nullptr);
    CHECK(cfg.audio == nullptr);
    CHECK(cfg.lua == nullptr);
    CHECK(cfg.inputRouter == nullptr);
    CHECK(cfg.gpuMonitor == nullptr);
    CHECK(cfg.videoPlayer == nullptr);
    CHECK(cfg.miniGame == nullptr);
    CHECK(cfg.animation == nullptr);
    CHECK(cfg.steam == nullptr);
}

TEST_CASE("Entry: EngineConfig headless mode flag") {
    EngineConfig cfg;
    cfg.headless = true;
    CHECK(cfg.headless == true);
    CHECK(cfg.editorMode == false);
}

TEST_CASE("Entry: EngineConfig editor mode flag") {
    EngineConfig cfg;
    cfg.editorMode = true;
    CHECK(cfg.editorMode == true);
    CHECK(cfg.headless == false);
}

TEST_CASE("Entry: EngineConfig custom title") {
    EngineConfig cfg;
    CHECK(std::strlen(cfg.title) > 0);
}

TEST_CASE("Entry: EngineConfig width/height range") {
    EngineConfig cfg;
    cfg.width = 640;
    cfg.height = 480;
    CHECK(cfg.width == 640);
    CHECK(cfg.height == 480);

    cfg.width = 1920;
    cfg.height = 1080;
    CHECK(cfg.width == 1920);
    CHECK(cfg.height == 1080);
}

// S3 — Engine construction and lifecycle tests

TEST_CASE("Entry: Engine constructs in headless mode without crash") {
    EngineConfig cfg;
    cfg.headless = true;
    Engine engine(cfg);
    CHECK(true);
}

TEST_CASE("Entry: Engine default construct then destruct without init") {
    EngineConfig cfg;
    cfg.headless = true;
    {
        Engine engine(cfg);
    }
    // Construct again to verify BackendRegistry state is clean
    Engine engine2(cfg);
    CHECK(true);
}

TEST_CASE("Entry: Engine default construct then destruct") {
    // Test that default-constructed Engine (no init) destructs safely
    CHECK_NOTHROW({
        EngineConfig cfg;
        cfg.headless = true;
        Engine engine(cfg);
        // Destructor runs here — must not crash
    });
}

TEST_CASE("Entry: Engine headless init uses safe default backends") {
    EngineConfig cfg;
    cfg.headless = true;

    Engine engine(cfg);

    REQUIRE(engine.init());
    CHECK(BackendRegistry::instance().getRenderDevice() != nullptr);
    CHECK(BackendRegistry::instance().getAudioBackend() != nullptr);
    CHECK(BackendRegistry::instance().getPlatformBackend() != nullptr);
    CHECK(BackendRegistry::instance().getMiniGameBackend() != nullptr);
}

TEST_CASE("Entry: Engine uses configured GPU monitor") {
    EngineConfig cfg;
    cfg.headless = true;
    auto* configured = new TestGpuMonitor();
    cfg.gpuMonitor = configured;

    Engine engine(cfg);

    REQUIRE(engine.init());
    CHECK(&engine.gpuMonitor() == configured);
}

TEST_CASE("Entry: Engine headless backend info reports actual null adapters") {
    EngineConfig cfg;
    cfg.headless = true;

    Engine engine(cfg);

    REQUIRE(engine.init());

    lua_State* L = engine.lua().state();
    REQUIRE(L != nullptr);

    const char* script =
        "local info = Engine.get_backend_info()\n"
        "assert(info.render == 'NullRender', 'render=' .. tostring(info.render))\n"
        "assert(info.audio == 'NullAudio', 'audio=' .. tostring(info.audio))\n"
        "assert(info.platform == 'NullPlatform', 'platform=' .. tostring(info.platform))\n";

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

TEST_CASE("Entry: Engine headless Lua registry services back KAG bindings") {
    EngineConfig cfg;
    cfg.headless = true;

    Engine engine(cfg);
    REQUIRE(engine.init());

    lua_State* L = engine.lua().state();
    REQUIRE(L != nullptr);

    const char* registryKeys[] = {
        "Caesura.RenderDevice",
        "Caesura.AudioBackend",
        "Caesura.PlatformBackend",
        "Caesura.InputRouter",
        "Caesura.VideoPlayer",
        "Caesura.TextureManager",
        "Caesura.AsyncLoader",
        "Caesura.DebugManager",
        "Caesura.MiniGameBackend",
    };

    for (const char* key : registryKeys) {
        lua_getfield(L, LUA_REGISTRYINDEX, key);
        CAPTURE(key);
        CHECK(lua_islightuserdata(L, -1));
        lua_pop(L, 1);
    }

    auto runLuaBoolean = [&](const char* script) {
        INFO(script);
        int luaStatus = luaL_loadstring(L, script);
        if (luaStatus == LUA_OK) {
            luaStatus = lua_pcall(L, 0, 1, 0);
        }
        if (luaStatus != LUA_OK) {
            CAPTURE(lua_tostring(L, -1));
            lua_pop(L, 1);
            return false;
        }
        const bool ok = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        return ok;
    };

    CHECK(runLuaBoolean("return KAG.set_bus_volume('bgm', 0.5)"));
    CHECK(runLuaBoolean("return type(KAG.get_bus_volume('bgm')) == 'number'"));
    CHECK(runLuaBoolean("return KAG.render_text('registry probe', 1, 2, 255, 255, 255, 255)"));
    CHECK(runLuaBoolean("return type(KAG.save_game) == 'function'"));
    CHECK(runLuaBoolean("return type(KAG.load_game) == 'function'"));
    CHECK(runLuaBoolean("return type(KAG.list_saves) == 'function'"));
}
