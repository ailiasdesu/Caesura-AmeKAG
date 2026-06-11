#pragma once

// Minimal forward declarations to avoid transitive includes
namespace Caesura {

class IRenderDevice;
class IAudioBackend;
class IPlatformBackend;
class IMiniGameBackend;
class IAnimationBackend;
class ISteamBackend;
class IVideoDecoder;
class LuaManager;
class InputRouter;
class IGpuMonitor;
class VideoPlayer;

// EngineConfig -- aggregate struct for DI into Engine.
// Fields are concrete pointers where Engine takes ownership (unique_ptr),
// and interface pointers where Engine only holds a non-owning reference.
// Use C++20 designated initializers at call site for readability.
struct EngineConfig {
    // Required core subsystems (Engine owns via unique_ptr — must be concrete)
    IRenderDevice*    render          = nullptr;
    IAudioBackend*    audio           = nullptr;
    IPlatformBackend* platform        = nullptr;

    // Scripting / Input (Engine owns via unique_ptr)
    LuaManager*       lua             = nullptr;
    InputRouter*      inputRouter     = nullptr;

    // Render enhancements (Engine owns via unique_ptr)
    IGpuMonitor*      gpuMonitor      = nullptr;
    VideoPlayer*      videoPlayer     = nullptr;

    // Optional — default nullptr (Engine owns via unique_ptr)
    IMiniGameBackend* miniGame        = nullptr;
    IAnimationBackend* animation      = nullptr;
    ISteamBackend*    steam           = nullptr;

    // Dimensions
    const char*       title           = "Caesura (AmeKAG)";
    int               width           = 1280;
    int               height          = 720;
    bool              headless        = false;
    bool              editorMode      = false;
};

} // namespace Caesura