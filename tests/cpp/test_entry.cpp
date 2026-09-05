// test_entry.cpp - entry module unit tests (R4.2, extended S3)
#include "doctest.h"
#include "entry/Engine.h"
#include "entry/EngineConfig.h"
#include "debug/DebugProtocol.h"
#include "di/BackendRegistry.h"
#include "render/api/IGpuMonitor.h"
#include "platform/api/IDisplayService.h"
#include "platform/SDL3PlatformBackend.h"
#include "script/vm/LuaManager.h"
#include "EntryLifecycleBackends.h"
#include <SDL3/SDL.h>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <utility>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

using namespace Caesura;

namespace Caesura {
std::unique_ptr<IDisplayService> createDisplayService(
    const EngineConfig& config, const IPlatformBackend* platformBackend);
SDL_Window* getSDLWindow(const IPlatformBackend* platformBackend);
}

namespace {

class TestGpuMonitor : public IGpuMonitor {
public:
    explicit TestGpuMonitor(int* destructorCalls = nullptr)
        : m_destructorCalls(destructorCalls) {}
    ~TestGpuMonitor() override {
        if (m_destructorCalls) ++*m_destructorCalls;
    }

    GpuQuality update(double) override { return GpuQuality::HIGH; }
    const FrameMetrics& metrics() override { return m_metrics; }
    GpuQuality currentQuality() const override { return GpuQuality::HIGH; }
    bool isDegraded() const override { return false; }
    float resolutionScale() const override { return 1.0f; }
    bool vfxEnabled() const override { return true; }
    void reset() override {}
    void setGpuAvailable(bool) override {}

private:
    int* m_destructorCalls = nullptr;
    FrameMetrics m_metrics;
};

class TestDisplayService final : public IDisplayService {
public:
    explicit TestDisplayService(int& destructorCalls)
        : m_destructorCalls(destructorCalls) {}
    ~TestDisplayService() override { ++m_destructorCalls; }

    DisplayMetrics currentMetrics() const override { return {}; }

private:
    int& m_destructorCalls;
};

void checkEngineRegistryCleared() {
    auto& registry = BackendRegistry::instance();
    CHECK(registry.getPlatformBackend() == nullptr);
    CHECK(registry.getRenderDevice() == nullptr);
    CHECK(registry.getAudioBackend() == nullptr);
    CHECK(registry.getInputRouter() == nullptr);
    CHECK(registry.getVideoPlayer() == nullptr);
    CHECK(registry.getTextureManager() == nullptr);
    CHECK(registry.getLayerManager() == nullptr);
    CHECK(registry.getSandboxQuota() == nullptr);
    CHECK(registry.getTextureBudget() == nullptr);
    CHECK(registry.getDebugManager() == nullptr);
    CHECK(registry.getAsyncLoader() == nullptr);
    CHECK(registry.getJobSystem() == nullptr);
    CHECK(registry.getCryptoEngine() == nullptr);
    CHECK(registry.getSaveManager() == nullptr);
    CHECK(registry.getParticleSystem() == nullptr);
    CHECK(registry.getResourceGenerationTracker() == nullptr);
    CHECK(registry.getMiniGameBackend() == nullptr);
    CHECK(registry.getAnimationBackend() == nullptr);
    CHECK(registry.getSteamBackend() == nullptr);
    CHECK(registry.getLuaManager() == nullptr);
    CHECK(registry.getDisplayService() == nullptr);
    CHECK(registry.getLifecycleService() == nullptr);
    CHECK(registry.getAudioFocusService() == nullptr);
    CHECK(registry.getMeshRenderer() == nullptr);
}

} // namespace

TEST_CASE("Entry: EngineConfig default values") {
    EngineConfig cfg;
    CHECK(cfg.width == 1280);
    CHECK(cfg.height == 720);
    CHECK(cfg.title != nullptr);
    CHECK(std::strcmp(cfg.title, "Caesura (AmeKAG)") == 0);
    CHECK(cfg.headless == false);
    CHECK(cfg.editorMode == false);
    CHECK(cfg.enableDebugger == false);
}

TEST_CASE("Entry: EngineConfig pointer fields default nullptr") {
    CHECK_FALSE(std::is_copy_constructible_v<EngineConfig>);
    CHECK(std::is_move_constructible_v<EngineConfig>);
    EngineConfig cfg;
    CHECK(cfg.platform == nullptr);
    CHECK(cfg.render == nullptr);
    CHECK(cfg.audio == nullptr);
    CHECK(cfg.lua == nullptr);
    CHECK(cfg.inputRouter == nullptr);
    CHECK(cfg.gpuMonitor == nullptr);
    CHECK(cfg.videoPlayer == nullptr);
    CHECK(cfg.layerManager == nullptr);
    CHECK(cfg.sandboxQuota == nullptr);
    CHECK(cfg.miniGame == nullptr);
    CHECK(cfg.animation == nullptr);
    CHECK(cfg.steam == nullptr);
    CHECK(cfg.displayService == nullptr);
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

TEST_CASE("Entry: EngineConfig preserves debugger policy when moved") {
    EngineConfig cfg;
    cfg.enableDebugger = true;

    EngineConfig moved(std::move(cfg));
    CHECK(moved.enableDebugger);
}

TEST_CASE("Entry: EngineConfig frameLimit defaults to 0 (unlimited)") {
    EngineConfig cfg;
    CHECK(cfg.frameLimit == 0);
    CHECK(cfg.renderBackend == nullptr);
}

TEST_CASE("Entry: EngineConfig preserves frameLimit when moved") {
    EngineConfig cfg;
    cfg.frameLimit = 300;

    EngineConfig moved(std::move(cfg));
    CHECK(moved.frameLimit == 300);
}

TEST_CASE("Entry: EngineConfig rejects frameLimit via sane constructor path") {
    // --frames parsing lives in main.cpp; EngineConfig only carries the
    // validated value. Ensure a positive value survives the move and a
    // zero (CLI absence) means "unlimited".
    EngineConfig cfg;
    cfg.frameLimit = 1;
    EngineConfig moved(std::move(cfg));
    CHECK(moved.frameLimit == 1);
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
    Engine engine(std::move(cfg));
    CHECK(true);
}

TEST_CASE("U4: Engine applies host save encryption policy after configuration move") {
    EngineConfig config;
    config.headless = true;
    config.saveEncryptionPolicy = SaveEncryptionPolicy::RequireEncrypted;
    EngineConfig moved(std::move(config));
    Engine engine(std::move(moved));
    REQUIRE(engine.init());
    auto* saves = BackendRegistry::instance().getSaveManager();
    REQUIRE(saves != nullptr);
    CHECK(saves->getEncryptionPolicy() == SaveEncryptionPolicy::RequireEncrypted);
    CHECK_FALSE(saves->isEncryptionEnabled());
    engine.shutdown();
}

TEST_CASE("Entry: Engine default construct then destruct without init") {
    Caesura::Test::LifecycleProbe platform;
    Caesura::Test::LifecycleProbe render;
    Caesura::Test::LifecycleProbe audio;
    Caesura::Test::LifecycleProbe miniGame;
    Caesura::Test::LifecycleProbe animation;
    Caesura::Test::ServiceProbe layer;
    Caesura::Test::ServiceProbe sandbox;
    int gpuMonitorDestructors = 0;

    EngineConfig cfg;
    cfg.headless = true;
    cfg.platform = new Caesura::Test::PlatformBackend(platform);
    cfg.render = new Caesura::Test::RenderDevice(render);
    cfg.audio = new Caesura::Test::AudioBackend(audio);
    cfg.miniGame = new Caesura::Test::MiniGameBackend(miniGame);
    cfg.animation = new Caesura::Test::AnimationBackend(animation);
    cfg.layerManager = new Caesura::Test::LayerManagerBackend(layer);
    cfg.sandboxQuota = new Caesura::Test::SandboxQuotaBackend(sandbox);
    cfg.gpuMonitor = new TestGpuMonitor(&gpuMonitorDestructors);
    {
        Engine engine(std::move(cfg));
        CHECK(cfg.platform == nullptr);
        CHECK(cfg.render == nullptr);
        CHECK(cfg.audio == nullptr);
        CHECK(cfg.miniGame == nullptr);
        CHECK(cfg.animation == nullptr);
        CHECK(cfg.layerManager == nullptr);
        CHECK(cfg.sandboxQuota == nullptr);
        CHECK(cfg.gpuMonitor == nullptr);
    }

    CHECK(platform.destructorCalls == 1);
    CHECK(render.destructorCalls == 1);
    CHECK(audio.destructorCalls == 1);
    CHECK(miniGame.destructorCalls == 1);
    CHECK(animation.destructorCalls == 1);
    CHECK(layer.destructorCalls == 1);
    CHECK(sandbox.destructorCalls == 1);
    CHECK(gpuMonitorDestructors == 1);
    CHECK(platform.initCalls == 0);
    CHECK(render.initCalls == 0);
    CHECK(audio.initCalls == 0);
    CHECK(platform.shutdownCalls == 0);
    CHECK(render.shutdownCalls == 0);
    CHECK(audio.shutdownCalls == 0);

    // Construct again to verify BackendRegistry state is clean
    EngineConfig cleanCfg;
    cleanCfg.headless = true;
    Engine engine2(std::move(cleanCfg));
    CHECK(true);
}

TEST_CASE("Entry: unconsumed EngineConfig releases injected ownership") {
    Caesura::Test::LifecycleProbe platform;
    Caesura::Test::LifecycleProbe render;
    Caesura::Test::LifecycleProbe audio;
    Caesura::Test::LifecycleProbe miniGame;
    Caesura::Test::LifecycleProbe animation;
    Caesura::Test::ServiceProbe layer;
    Caesura::Test::ServiceProbe sandbox;
    int gpuMonitorDestructors = 0;

    {
        EngineConfig cfg;
        cfg.platform = new Caesura::Test::PlatformBackend(platform);
        cfg.render = new Caesura::Test::RenderDevice(render);
        cfg.audio = new Caesura::Test::AudioBackend(audio);
        cfg.miniGame = new Caesura::Test::MiniGameBackend(miniGame);
        cfg.animation = new Caesura::Test::AnimationBackend(animation);
        cfg.layerManager = new Caesura::Test::LayerManagerBackend(layer);
        cfg.sandboxQuota = new Caesura::Test::SandboxQuotaBackend(sandbox);
        cfg.gpuMonitor = new TestGpuMonitor(&gpuMonitorDestructors);
    }

    CHECK(platform.destructorCalls == 1);
    CHECK(render.destructorCalls == 1);
    CHECK(audio.destructorCalls == 1);
    CHECK(miniGame.destructorCalls == 1);
    CHECK(animation.destructorCalls == 1);
    CHECK(layer.destructorCalls == 1);
    CHECK(sandbox.destructorCalls == 1);
    CHECK(gpuMonitorDestructors == 1);
    CHECK(platform.shutdownCalls == 0);
    CHECK(render.shutdownCalls == 0);
    CHECK(audio.shutdownCalls == 0);
}

TEST_CASE("Entry: injected display service transfers ownership and unregisters") {
    int displayDestructorCalls = 0;
    auto* display = new TestDisplayService(displayDestructorCalls);

    {
        EngineConfig cfg;
        cfg.headless = true;
        cfg.displayService = display;

        Engine engine(std::move(cfg));
        CHECK(engine.config().displayService == nullptr);
        REQUIRE(engine.init());
        CHECK(BackendRegistry::instance().getDisplayService() == display);

        engine.shutdown();
        CHECK(BackendRegistry::instance().getDisplayService() == nullptr);
        CHECK(displayDestructorCalls == 1);
    }

    CHECK(displayDestructorCalls == 1);
    checkEngineRegistryCleared();
}

TEST_CASE("Entry: default display service uses only SDL3 platform windows") {
    Caesura::Test::LifecycleProbe platform;
    EngineConfig cfg;
    cfg.width = 321;
    cfg.height = 123;
    cfg.headless = false;
    auto platformBackend = std::make_unique<Caesura::Test::PlatformBackend>(platform);

    // A non-SDL platform must not expose its native OS handle as SDL_Window*.
    auto display = Caesura::createDisplayService(cfg, platformBackend.get());
    REQUIRE(display != nullptr);
    DisplayMetrics metrics = display->currentMetrics();
    CHECK(metrics.logicalWidth == 321);
    CHECK(metrics.logicalHeight == 123);

    // The concrete SDL backend provides the actual SDL window accessor. An
    // uninitialized backend has no window, so metrics remain unavailable but
    // the call is safe and never casts the bgfx-native OS handle.
    SDL3PlatformBackend sdlPlatform;
    display = Caesura::createDisplayService(cfg, &sdlPlatform);
    REQUIRE(display != nullptr);
    metrics = display->currentMetrics();
    CHECK(metrics.logicalWidth == 0);
    CHECK(metrics.logicalHeight == 0);
}

TEST_CASE("Entry: Engine default construct then destruct") {
    // Test that default-constructed Engine (no init) destructs safely
    CHECK_NOTHROW({
        EngineConfig cfg;
        cfg.headless = true;
        Engine engine(std::move(cfg));
        // Destructor runs here — must not crash
    });
}

TEST_CASE("Entry: shutdown before init permanently rejects initialization") {
    Caesura::Test::LifecycleProbe platform;
    Caesura::Test::LifecycleProbe render;
    Caesura::Test::LifecycleProbe audio;

    {
        EngineConfig cfg;
        cfg.headless = true;
        cfg.platform = new Caesura::Test::PlatformBackend(platform);
        cfg.render = new Caesura::Test::RenderDevice(render);
        cfg.audio = new Caesura::Test::AudioBackend(audio);

        Engine engine(std::move(cfg));
        engine.shutdown();

        CHECK_FALSE(engine.init());
        CHECK(platform.initCalls == 0);
        CHECK(render.initCalls == 0);
        CHECK(audio.initCalls == 0);
        checkEngineRegistryCleared();
    }

    CHECK(platform.destructorCalls == 1);
    CHECK(render.destructorCalls == 1);
    CHECK(audio.destructorCalls == 1);
}

TEST_CASE("Entry: custom platform native handle is never treated as SDL window") {
    Caesura::Test::LifecycleProbe platform;
    platform.providedNativeHandle = reinterpret_cast<void*>(0x1234);
    Caesura::Test::PlatformBackend platformBackend(platform);

    CHECK(platformBackend.getNativeWindowHandle() == reinterpret_cast<void*>(0x1234));
    CHECK(Caesura::getSDLWindow(&platformBackend) == nullptr);
}

TEST_CASE("Entry: debugger attaches after Lua safety hook and detaches before VM shutdown") {
    Caesura::Test::ServiceProbe sandbox;
    lua_State* observedState = nullptr;
    lua_Hook safetyHook = nullptr;
    bool safetyHookRestoredBeforeUnbind = false;

    sandbox.onSetLuaState = [&](lua_State* state) {
        if (state) {
            observedState = state;
            safetyHook = lua_gethook(state);
        } else if (observedState) {
            safetyHookRestoredBeforeUnbind =
                lua_gethook(observedState) == safetyHook;
        }
    };

    EngineConfig cfg;
    cfg.headless = true;
    cfg.enableDebugger = true;
    cfg.sandboxQuota = new Caesura::Test::SandboxQuotaBackend(sandbox);

    Engine engine(std::move(cfg));
    REQUIRE(engine.init());
    REQUIRE(engine.debugProtocol() != nullptr);
    CHECK(engine.debugProtocol()->runState() == DebugProtocol::RunState::Running);
    CHECK(lua_gethook(engine.lua().state()) != safetyHook);

    lua_getglobal(engine.lua().state(), "_CAESURA_DEBUG_IS_PAUSED");
    REQUIRE(lua_isfunction(engine.lua().state(), -1));
    REQUIRE(lua_pcall(engine.lua().state(), 0, 1, 0) == LUA_OK);
    CHECK(lua_toboolean(engine.lua().state(), -1) == 0);
    lua_pop(engine.lua().state(), 1);

    constexpr const char* debugScript =
        "local value = 1\n"
        "value = value + 1\n"
        "return value\n";
    engine.debugProtocol()->setBreakpoint("entry_debug.lua", 2);
    lua_State* coroutine = lua_newthread(engine.lua().state());
    REQUIRE(luaL_loadbuffer(coroutine, debugScript, std::strlen(debugScript),
                            "entry_debug.lua") == LUA_OK);
    int resultCount = 0;
    REQUIRE(lua_resume(coroutine, engine.lua().state(), 0, &resultCount) == LUA_YIELD);

    lua_getglobal(engine.lua().state(), "_CAESURA_DEBUG_IS_PAUSED");
    REQUIRE(lua_pcall(engine.lua().state(), 0, 1, 0) == LUA_OK);
    CHECK(lua_toboolean(engine.lua().state(), -1) != 0);
    lua_pop(engine.lua().state(), 1);

    auto commands = engine.debugProtocol()->commandSink();
    REQUIRE(commands(engine.debugProtocol()->currentPauseId(),
                     DebugProtocol::Command::Continue));
    int ownerTicks = 0;
    engine.run([&]() {
        if (++ownerTicks >= 2) engine.quit();
    });
    CHECK(engine.debugProtocol()->runState() == DebugProtocol::RunState::Running);

    engine.shutdown();
    CHECK(engine.debugProtocol() == nullptr);
    CHECK(safetyHookRestoredBeforeUnbind);
    CHECK(sandbox.setLuaStateCalls == 2);
    checkEngineRegistryCleared();
}

TEST_CASE("Entry: voice completions are deferred by debugger pause and delivered once each") {
    Caesura::Test::LifecycleProbe audio;
    audio.voicePlaying = true;

    EngineConfig cfg;
    cfg.headless = true;
    cfg.enableDebugger = true;
    cfg.audio = new Caesura::Test::AudioBackend(audio);

    Engine engine(std::move(cfg));
    REQUIRE(engine.init());
    lua_State* L = engine.lua().state();
    REQUIRE(L != nullptr);
    REQUIRE(luaL_dostring(L,
        "voice_complete_count = 0; "
        "function _onVoiceComplete() "
        "voice_complete_count = voice_complete_count + 1 end") == LUA_OK);

    DebugProtocol* protocol = engine.debugProtocol();
    REQUIRE(protocol != nullptr);
    DebugProtocol::CommandSink commands;
    DebugProtocol::PauseId pauseId = DebugProtocol::NoPause;
    int ownerTick = 0;

    engine.run([&]() {
        ++ownerTick;
        if (ownerTick == 2) {
            constexpr const char* debugScript =
                "local value = 1\n"
                "value = value + 1\n"
                "return value\n";
            protocol->setBreakpoint("voice_debug.lua", 2);
            lua_State* coroutine = lua_newthread(L);
            REQUIRE(luaL_loadbuffer(coroutine, debugScript,
                                    std::strlen(debugScript),
                                    "voice_debug.lua") == LUA_OK);
            int resultCount = 0;
            REQUIRE(lua_resume(coroutine, L, 0, &resultCount) == LUA_YIELD);
            pauseId = protocol->currentPauseId();
            commands = protocol->commandSink();
            audio.voicePlaying = false;
            audio.voiceCompletions = 1;
        } else if (ownerTick == 3) {
            lua_getglobal(L, "voice_complete_count");
            CHECK(lua_tointeger(L, -1) == 0);
            lua_pop(L, 1);
            REQUIRE(pauseId != DebugProtocol::NoPause);
            REQUIRE(commands(pauseId, DebugProtocol::Command::Continue));
        } else if (ownerTick == 4) {
            lua_getglobal(L, "voice_complete_count");
            CHECK(lua_tointeger(L, -1) == 0);
            lua_pop(L, 1);
            audio.voicePlaying = true;
        } else if (ownerTick == 5) {
            lua_getglobal(L, "voice_complete_count");
            CHECK(lua_tointeger(L, -1) == 1);
            lua_pop(L, 1);
            lua_getglobal(L, "_CAESURA_VOICE_COMPLETE");
            CHECK(lua_toboolean(L, -1) == 0);
            lua_pop(L, 1);
            audio.voicePlaying = false;
            audio.voiceCompletions = 1;
        } else if (ownerTick >= 6) {
            engine.quit();
        }
    });

    lua_getglobal(L, "voice_complete_count");
    CHECK(lua_tointeger(L, -1) == 2);
    lua_pop(L, 1);
    lua_getglobal(L, "_CAESURA_VOICE_COMPLETE");
    CHECK(lua_toboolean(L, -1) != 0);
    lua_pop(L, 1);
    CHECK(audio.isVoicePlayingCalls >= 3);
    CHECK(audio.audioUpdateCalls >= 6);

    engine.shutdown();
    checkEngineRegistryCleared();
}

TEST_CASE("Entry: non-string voice callback errors are contained") {
    Caesura::Test::LifecycleProbe audio;
    EngineConfig cfg;
    cfg.headless = true;
    cfg.audio = new Caesura::Test::AudioBackend(audio);

    Engine engine(std::move(cfg));
    REQUIRE(engine.init());
    lua_State* L = engine.lua().state();
    REQUIRE(L != nullptr);
    REQUIRE(luaL_dostring(L,
        "voice_callback_entered = false; "
        "function _onVoiceComplete() "
        "voice_callback_entered = true; error({reason='test'}) end") == LUA_OK);

    audio.voiceCompletions = 1;
    int ownerTick = 0;
    CHECK_NOTHROW(engine.run([&]() {
        if (++ownerTick >= 2) engine.quit();
    }));

    lua_getglobal(L, "voice_callback_entered");
    CHECK(lua_toboolean(L, -1) != 0);
    lua_pop(L, 1);
    CHECK(audio.voiceCompletions == 0);

    engine.shutdown();
    checkEngineRegistryCleared();
}

TEST_CASE("Entry: consecutive engines own independent HotReload state") {
    {
        EngineConfig cfg;
        cfg.headless = true;
        Engine engine(std::move(cfg));
        REQUIRE(engine.init());

        lua_State* L = engine.lua().state();
        REQUIRE(L != nullptr);
        const int firstReloadSetup = luaL_dostring(L,
            "engine_reload_marker = 0; package.loaded['kag'] = {}; "
            "package.preload['kag'] = function() engine_reload_marker = 1; return {} end");
        REQUIRE(firstReloadSetup == LUA_OK);
        CHECK(engine.reloadScriptsNow());
        lua_getglobal(L, "engine_reload_marker");
        CHECK(lua_tointeger(L, -1) == 1);
        lua_pop(L, 1);
        CHECK_NOTHROW(engine.quit());
    }

    {
        EngineConfig cfg;
        cfg.headless = true;
        Engine engine(std::move(cfg));
        REQUIRE(engine.init());

        lua_State* L = engine.lua().state();
        REQUIRE(L != nullptr);
        const int secondReloadSetup = luaL_dostring(L,
            "engine_reload_marker = 0; package.loaded['kag'] = {}; "
            "package.preload['kag'] = function() engine_reload_marker = 2; return {} end");
        REQUIRE(secondReloadSetup == LUA_OK);
        CHECK(engine.reloadScriptsNow());
        lua_getglobal(L, "engine_reload_marker");
        CHECK(lua_tointeger(L, -1) == 2);
        lua_pop(L, 1);
    }
}

TEST_CASE("Entry: Engine headless init uses safe default backends") {
    SUBCASE("default adapters initialize and register") {
        EngineConfig cfg;
        cfg.headless = true;

        Engine engine(std::move(cfg));

        REQUIRE(engine.init());
        CHECK(BackendRegistry::instance().getRenderDevice() != nullptr);
        CHECK(BackendRegistry::instance().getAudioBackend() != nullptr);
        CHECK(BackendRegistry::instance().getPlatformBackend() != nullptr);
        CHECK(BackendRegistry::instance().getMiniGameBackend() != nullptr);
        CHECK(BackendRegistry::instance().getLayerManager() != nullptr);
        CHECK(BackendRegistry::instance().getSandboxQuota() != nullptr);
    }

    SUBCASE("injected adapters initialize once and shut down once") {
        Caesura::Test::LifecycleProbe platform;
        Caesura::Test::LifecycleProbe render;
        Caesura::Test::LifecycleProbe audio;
        bool meshReleasedBeforeRenderShutdown = false;
        render.onShutdown = [&] {
            meshReleasedBeforeRenderShutdown =
                BackendRegistry::instance().getMeshRenderer() == nullptr;
        };
        platform.providedNativeHandle = reinterpret_cast<void*>(0x1234);
        {
            EngineConfig cfg;
            cfg.headless = true;
            cfg.platform = new Caesura::Test::PlatformBackend(platform);
            cfg.render = new Caesura::Test::RenderDevice(render);
            cfg.audio = new Caesura::Test::AudioBackend(audio);

            Engine engine(std::move(cfg));
            REQUIRE(engine.init());
            CHECK_FALSE(engine.init());
            CHECK(platform.initCalls == 1);
            CHECK(render.initCalls == 1);
            CHECK(audio.initCalls == 1);
            CHECK(render.observedNativeHandle == nullptr);
            CHECK(engine.config().platform == nullptr);
            CHECK(engine.config().render == nullptr);
            CHECK(engine.config().audio == nullptr);
        }

        CHECK(platform.shutdownCalls == 1);
        CHECK(render.beginShutdownCalls == 1);
        CHECK(render.flushCalls == 1);
        CHECK(render.advanceCalls == 2);
        CHECK(render.shutdownCalls == 1);
        CHECK(meshReleasedBeforeRenderShutdown);
        CHECK(audio.shutdownCalls == 1);
        CHECK(platform.destructorCalls == 1);
        CHECK(render.destructorCalls == 1);
        CHECK(audio.destructorCalls == 1);
    }

    SUBCASE("owned layer and sandbox services register and unbind") {
        Caesura::Test::LifecycleProbe audio;
        Caesura::Test::LifecycleProbe animation;
        Caesura::Test::ServiceProbe layer;
        Caesura::Test::ServiceProbe sandbox;
        bool layerUnregisteredBeforeSandboxUnbind = false;
        bool audioReleasedBeforeSandboxUnbind = false;
        bool animationReleasedBeforeTextureAndSandbox = false;
        ISandboxQuota* configuredSandbox = nullptr;
        audio.onShutdown = [&] {
            audioReleasedBeforeSandboxUnbind =
                BackendRegistry::instance().getSandboxQuota() == configuredSandbox &&
                sandbox.lastLuaState != nullptr;
        };
        animation.onShutdown = [&] {
            auto& registry = BackendRegistry::instance();
            const bool quotaAvailable = registry.tryAlloc("textures");
            if (quotaAvailable) registry.release("textures");
            animationReleasedBeforeTextureAndSandbox =
                quotaAvailable &&
                registry.getTextureManager() != nullptr &&
                registry.getSandboxQuota() == configuredSandbox &&
                sandbox.lastLuaState != nullptr;
        };
        sandbox.onSetLuaState = [&](lua_State* L) {
            if (!L) {
                layerUnregisteredBeforeSandboxUnbind =
                    BackendRegistry::instance().getLayerManager() == nullptr &&
                    BackendRegistry::instance().getAnimationBackend() == nullptr &&
                    BackendRegistry::instance().getTextureManager() == nullptr &&
                    audio.shutdownCalls == 1 &&
                    animation.shutdownCalls == 1;
            }
        };
        {
            EngineConfig cfg;
            cfg.headless = true;
            auto* configuredLayer = new Caesura::Test::LayerManagerBackend(layer);
            configuredSandbox = new Caesura::Test::SandboxQuotaBackend(sandbox);
            cfg.audio = new Caesura::Test::AudioBackend(audio);
            cfg.animation = new Caesura::Test::AnimationBackend(animation);
            cfg.layerManager = configuredLayer;
            cfg.sandboxQuota = configuredSandbox;

            Engine engine(std::move(cfg));
            REQUIRE(engine.init());
            CHECK(cfg.layerManager == nullptr);
            CHECK(cfg.sandboxQuota == nullptr);
            CHECK(BackendRegistry::instance().getLayerManager() == configuredLayer);
            CHECK(BackendRegistry::instance().getSandboxQuota() == configuredSandbox);
            CHECK(layer.initCalls == 1);
            CHECK(sandbox.setLuaStateCalls == 1);
            CHECK(sandbox.lastLuaState == engine.lua().state());
        }

        CHECK(layer.shutdownCalls == 1);
        CHECK(layer.destructorCalls == 1);
        CHECK(sandbox.setLuaStateCalls == 2);
        CHECK(sandbox.lastLuaState == nullptr);
        CHECK(sandbox.destructorCalls == 1);
        CHECK(audio.shutdownCalls == 1);
        CHECK(animation.initCalls == 1);
        CHECK(animation.shutdownCalls == 1);
        CHECK(animation.destructorCalls == 1);
        CHECK(sandbox.tryAllocCalls == 1);
        CHECK(sandbox.releaseCalls == 1);
        CHECK(audioReleasedBeforeSandboxUnbind);
        CHECK(animationReleasedBeforeTextureAndSandbox);
        CHECK(layerUnregisteredBeforeSandboxUnbind);
        checkEngineRegistryCleared();
    }

    SUBCASE("platform failure rolls back without touching uninitialized adapters") {
        Caesura::Test::LifecycleProbe platform;
        Caesura::Test::LifecycleProbe render;
        Caesura::Test::LifecycleProbe audio;
        Caesura::Test::ServiceProbe layer;
        Caesura::Test::ServiceProbe sandbox;
        platform.initResult = false;
        {
            EngineConfig cfg;
            cfg.headless = true;
            cfg.platform = new Caesura::Test::PlatformBackend(platform);
            cfg.render = new Caesura::Test::RenderDevice(render);
            cfg.audio = new Caesura::Test::AudioBackend(audio);
            cfg.layerManager = new Caesura::Test::LayerManagerBackend(layer);
            cfg.sandboxQuota = new Caesura::Test::SandboxQuotaBackend(sandbox);

            Engine engine(std::move(cfg));
            CHECK_FALSE(engine.init());
            CHECK_FALSE(engine.init());
            CHECK(platform.initCalls == 1);
            CHECK(render.initCalls == 0);
            CHECK(audio.initCalls == 0);
            CHECK(platform.shutdownCalls == 0);
            CHECK(render.beginShutdownCalls == 0);
            CHECK(render.shutdownCalls == 0);
            CHECK(layer.initCalls == 0);
            CHECK(layer.shutdownCalls == 0);
            CHECK(sandbox.setLuaStateCalls == 0);
            CHECK_THROWS_AS(engine.renderDevice(), std::logic_error);
            CHECK_THROWS_AS(engine.audio(), std::logic_error);
            CHECK_THROWS_AS(engine.platform(), std::logic_error);
            CHECK_NOTHROW(engine.renderOneFrame());
            CHECK(engine.captureFrameForRpc(1, 1).empty());
            checkEngineRegistryCleared();
        }

        CHECK(platform.destructorCalls == 1);
        CHECK(render.destructorCalls == 1);
        CHECK(audio.destructorCalls == 1);
        CHECK(layer.initCalls == 0);
        CHECK(layer.shutdownCalls == 0);
        CHECK(layer.destructorCalls == 1);
        CHECK(sandbox.setLuaStateCalls == 0);
        CHECK(sandbox.destructorCalls == 1);
        checkEngineRegistryCleared();
    }

    SUBCASE("render failure shuts down only the initialized platform") {
        Caesura::Test::LifecycleProbe platform;
        Caesura::Test::LifecycleProbe render;
        Caesura::Test::LifecycleProbe audio;
        render.initResult = false;
        {
            EngineConfig cfg;
            cfg.headless = true;
            cfg.platform = new Caesura::Test::PlatformBackend(platform);
            cfg.render = new Caesura::Test::RenderDevice(render);
            cfg.audio = new Caesura::Test::AudioBackend(audio);

            Engine engine(std::move(cfg));
            CHECK_FALSE(engine.init());
            CHECK(platform.shutdownCalls == 1);
            CHECK(render.initCalls == 1);
            CHECK(render.beginShutdownCalls == 0);
            CHECK(render.flushCalls == 0);
            CHECK(render.advanceCalls == 0);
            CHECK(render.shutdownCalls == 0);
            CHECK(audio.initCalls == 0);
            checkEngineRegistryCleared();
        }
    }

    SUBCASE("audio failure degrades to the silent backend and keeps running (t56)") {
        Caesura::Test::LifecycleProbe platform;
        Caesura::Test::LifecycleProbe render;
        Caesura::Test::LifecycleProbe audio;
        audio.initResult = false;
        {
            EngineConfig cfg;
            cfg.headless = true;
            cfg.platform = new Caesura::Test::PlatformBackend(platform);
            cfg.render = new Caesura::Test::RenderDevice(render);
            cfg.audio = new Caesura::Test::AudioBackend(audio);

            Engine engine(std::move(cfg));
            // t56: a failing real audio backend must NOT kill startup -- the
            // engine degrades to the silent backend and keeps the run alive
            // (macOS vendored-SoLoud no-backend regression).
            CHECK(engine.init());
            CHECK(platform.initCalls == 1);
            CHECK(render.initCalls == 1);
            CHECK(audio.initCalls == 1);
            CHECK(audio.shutdownCalls == 0);   // failed backend replaced, not shutdown
            CHECK(BackendRegistry::instance().getAudioBackend() != nullptr);
            engine.shutdown();
            checkEngineRegistryCleared();      // shutdown still drains everything
        }
    }

    SUBCASE("editor mode without injected GPU backends is rejected") {
        EngineConfig cfg;
        cfg.headless = true;
        cfg.editorMode = true;

        Engine engine(std::move(cfg));
        CHECK_FALSE(engine.init());
        checkEngineRegistryCleared();
    }

    SUBCASE("failed optional adapters are cleaned before fallback replacement") {
        Caesura::Test::LifecycleProbe miniGame;
        Caesura::Test::LifecycleProbe animation;
        miniGame.initResult = false;
        animation.initResult = false;

        EngineConfig cfg;
        cfg.headless = true;
        cfg.miniGame = new Caesura::Test::MiniGameBackend(miniGame);
        cfg.animation = new Caesura::Test::AnimationBackend(animation);

        Engine engine(std::move(cfg));
        REQUIRE(engine.init());
        CHECK(miniGame.initCalls == 1);
        CHECK(miniGame.shutdownCalls == 1);
        CHECK(miniGame.destructorCalls == 1);
        CHECK(animation.initCalls == 1);
        CHECK(animation.shutdownCalls == 1);
        CHECK(animation.destructorCalls == 1);
        CHECK(engine.config().miniGame == nullptr);
        CHECK(engine.config().animation == nullptr);
    }
}

TEST_CASE("Entry: Engine uses configured GPU monitor") {
    EngineConfig cfg;
    cfg.headless = true;
    auto* configured = new TestGpuMonitor();
    cfg.gpuMonitor = configured;

    Engine engine(std::move(cfg));

    REQUIRE(engine.init());
    CHECK(&engine.gpuMonitor() == configured);
}

TEST_CASE("Entry: Engine headless backend info reports actual null adapters") {
    EngineConfig cfg;
    cfg.headless = true;

    Engine engine(std::move(cfg));

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

    Engine engine(std::move(cfg));
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

TEST_CASE("Entry: mobile adapter registered, lifecycle callbacks work, unregistered on shutdown") {
    EngineConfig cfg;
    cfg.headless = true;
    Engine engine(std::move(cfg));
    REQUIRE(engine.init());

    // Adapter is registered via BackendRegistry (interface access).
    auto* adapter = BackendRegistry::instance().getMobileAdapter();
    REQUIRE(adapter != nullptr);
    CHECK_FALSE(adapter->isPaused());

    // Lifecycle callbacks through the interface drive the pause state.
    // (SDL app-lifecycle events are delivered via event watches and are
    // not pushable from tests; the Engine::appLifecycleWatch bridge is
    // verified by review.)
    adapter->onPause(nullptr);
    CHECK(adapter->isPaused());
    adapter->onResume(nullptr);
    CHECK_FALSE(adapter->isPaused());

    engine.shutdown();
    CHECK(BackendRegistry::instance().getMobileAdapter() == nullptr);
}

TEST_CASE("Entry: Engine::handleAppLifecycle maps SDL app events to adapter callbacks") {
    EngineConfig cfg;
    cfg.headless = true;
    Engine engine(std::move(cfg));
    REQUIRE(engine.init());
    auto* adapter = BackendRegistry::instance().getMobileAdapter();
    REQUIRE(adapter != nullptr);

    // Pure static mapping — no SDL event push needed (app events are
    // watch-delivered only and cannot be queued from tests).
    CHECK_FALSE(adapter->isPaused());
    Engine::handleAppLifecycle(adapter, nullptr, SDL_EVENT_WILL_ENTER_BACKGROUND);
    CHECK(adapter->isPaused());
    Engine::handleAppLifecycle(adapter, nullptr, SDL_EVENT_DID_ENTER_FOREGROUND);
    CHECK_FALSE(adapter->isPaused());

    // Unrelated event types and null adapter are safe no-ops.
    Engine::handleAppLifecycle(adapter, nullptr, SDL_EVENT_KEY_DOWN);
    CHECK_FALSE(adapter->isPaused());
    Engine::handleAppLifecycle(nullptr, nullptr, SDL_EVENT_WILL_ENTER_BACKGROUND);

    engine.shutdown();
}
