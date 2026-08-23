#pragma once

#include <cstdint>  // uint32_t (GCC strict; MSVC used to get it transitively)
#include <string>
#include <utility>

// Minimal forward declarations to avoid transitive includes
namespace Caesura {

class IRenderDevice;
class IAudioBackend;
class IPlatformBackend;
class IMiniGameBackend;
class IAnimationBackend;
class ISteamBackend;
class IDisplayService;
class IVideoDecoder;
class LuaManager;
class InputRouter;
class IGpuMonitor;
class VideoPlayer;
class ILayerManager;
class ISandboxQuota;

// Move-only dependency bundle consumed by Engine.
// Every pointer transfers ownership when the configuration is moved into Engine.
// A nullptr requests the built-in default where that subsystem supports one.
struct EngineConfig {
    EngineConfig() = default;
    ~EngineConfig();
    EngineConfig(const EngineConfig&) = delete;
    EngineConfig& operator=(const EngineConfig&) = delete;
    EngineConfig& operator=(EngineConfig&&) = delete;

    EngineConfig(EngineConfig&& other) noexcept
        : render(std::exchange(other.render, nullptr))
        , audio(std::exchange(other.audio, nullptr))
        , platform(std::exchange(other.platform, nullptr))
        , lua(std::exchange(other.lua, nullptr))
        , inputRouter(std::exchange(other.inputRouter, nullptr))
        , gpuMonitor(std::exchange(other.gpuMonitor, nullptr))
        , videoPlayer(std::exchange(other.videoPlayer, nullptr))
        , layerManager(std::exchange(other.layerManager, nullptr))
        , sandboxQuota(std::exchange(other.sandboxQuota, nullptr))
        , miniGame(std::exchange(other.miniGame, nullptr))
        , animation(std::exchange(other.animation, nullptr))
        , steam(std::exchange(other.steam, nullptr))
        , displayService(std::exchange(other.displayService, nullptr))
        , title(other.title)
        , width(other.width)
        , height(other.height)
        , headless(other.headless)
        , editorMode(other.editorMode)
        , enableDebugger(other.enableDebugger)
        , renderBackend(other.renderBackend)
        , frameLimit(other.frameLimit)
        , exportReplayFile(std::move(other.exportReplayFile))
        , exportDir(std::move(other.exportDir)) {}

    // Required core subsystems in GPU mode (Engine owns via unique_ptr)
    IRenderDevice*    render          = nullptr;
    IAudioBackend*    audio           = nullptr;
    IPlatformBackend* platform        = nullptr;

    // Scripting / Input (Engine owns via unique_ptr)
    LuaManager*       lua             = nullptr;
    InputRouter*      inputRouter     = nullptr;

    // Render enhancements (Engine owns via unique_ptr)
    IGpuMonitor*      gpuMonitor      = nullptr;
    VideoPlayer*      videoPlayer     = nullptr;

    // Shared engine services (Engine owns via unique_ptr)
    ILayerManager*    layerManager    = nullptr;
    ISandboxQuota*    sandboxQuota    = nullptr;

    // Optional — default nullptr (Engine owns via unique_ptr)
    IMiniGameBackend* miniGame        = nullptr;
    IAnimationBackend* animation      = nullptr;
    ISteamBackend*    steam           = nullptr;

    // Display metrics service (Track P1). nullptr = Engine supplies a Null
    // default; desktop builds inject SDL3DisplayService from main.cpp.
    IDisplayService*  displayService  = nullptr;

    // Dimensions
    const char*       title           = "Caesura (AmeKAG)";
    int               width           = 1280;
    int               height          = 720;
    bool              headless        = false;
    bool              editorMode      = false;
    bool              enableDebugger  = false;

    // Optional GPU backend override for the render device, e.g. "opengl",
    // "vulkan", "dx11", "dx12", "metal", "webgpu". nullptr = driver default.
    const char*       renderBackend   = nullptr;

    // Maximum frames to render before Engine::run() exits (0 = unlimited).
    // Enables deterministic, CI-reproducible GPU smoke runs via --frames N.
    uint32_t          frameLimit      = 0;

    // Demo/video export (Neo-Genesis): when exportReplayFile is non-empty
    // the engine activates replay playback (scripts/replay.lua) before the
    // main loop and writes one PNG per rendered frame into exportDir
    // (frame_%05u.png). Requires a real GPU window; --frames N bounds the
    // export length. The recorded input drives the game itself, producing
    // a trailer/attract sequence deterministically.
    std::string       exportReplayFile;
    std::string       exportDir       = "export_out";
};

} // namespace Caesura
