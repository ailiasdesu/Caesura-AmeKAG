#include "doctest.h"
#include "Core/Engine.h"
#include "Core/BackendRegistry.h"
#include "Audio/NullAudioBackend.h"
#include "Render/NullRenderDevice.h"
#include <thread>

using namespace Caesura;

TEST_CASE("Engine::default construct destruct no-crash") {
    Engine::s_mainThreadId = std::this_thread::get_id();
    static NullRenderDevice nullRender;
    BackendRegistry::instance().setRenderDevice(nullRender);
    static NullAudioBackend nullAudio;
    BackendRegistry::instance().setAudioBackend(nullAudio);
    {
        Engine engine;
    }
}

TEST_CASE("Engine::double destruct via scope safe") {
    Engine::s_mainThreadId = std::this_thread::get_id();
    static NullRenderDevice nullRender;
    BackendRegistry::instance().setRenderDevice(nullRender);
    static NullAudioBackend nullAudio;
    BackendRegistry::instance().setAudioBackend(nullAudio);
    Engine* engine = new Engine();
    delete engine;
}
