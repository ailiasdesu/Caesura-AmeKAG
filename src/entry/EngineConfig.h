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

// EngineConfig — aggregate struct for DI into Engine.
// All fields are abstract interface pointers (non-owning).
// Optional subsystems (Live2D, Steam, FFmpeg) default to nullptr.
// Use C++20 designated initializers at call site for readability.
struct EngineConfig {
    // Required core subsystems
    IRenderDevice*    render          = nullptr;  // R8: injected via main.cpp
    IAudioBackend*    audio           = nullptr;  // R9: injected via main.cpp
    IPlatformBackend* platform        = nullptr;  // SDL3, etc.

    // Scripting / Input
    LuaManager*       lua             = nullptr;
    InputRouter*      inputRouter     = nullptr;

    // Render enhancements
    IGpuMonitor*      gpuMonitor      = nullptr;
    VideoPlayer*      videoPlayer     = nullptr;

    // Optional — default nullptr, no conditional compilation in signature
    IMiniGameBackend* miniGame        = nullptr;  // optional: bgfx or null
    IAnimationBackend* animation      = nullptr;  // optional: Live2D or null
    ISteamBackend*    steam           = nullptr;  // optional: SteamWorks or null

    // Dimensions
    const char*       title           = "Caesura (AmeKAG)";
    int               width           = 1280;
    int               height          = 720;
    bool              headless        = false;
    bool              editorMode      = false;
};

} // namespace Caesura
