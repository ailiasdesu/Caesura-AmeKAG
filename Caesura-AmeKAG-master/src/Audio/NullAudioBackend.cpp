#include "NullAudioBackend.h"
#include <cstdio>

namespace Caesura {

NullAudioBackend::NullAudioBackend() {
    printf("[BackendRegistry] Using NullAudioBackend.\n");
}

// -- Lifecycle ---------------------------------------------------------

bool NullAudioBackend::init() { return true; }
void NullAudioBackend::shutdown() {}
void NullAudioBackend::update(float /*deltaTime*/) {}

// -- BGM bus -----------------------------------------------------------

unsigned int NullAudioBackend::playBGM(const std::string& /*file*/, float /*fadeTime*/) { return 0; }
void NullAudioBackend::stopBGM(float /*fadeTime*/) {}

// -- VOICE bus ---------------------------------------------------------

unsigned int NullAudioBackend::playVoice(const std::string& /*file*/) { return 0; }
void NullAudioBackend::stopVoice() {}

// -- SE bus (2D + 3D) --------------------------------------------------

unsigned int NullAudioBackend::playSE(const std::string& /*file*/) { return 0; }
unsigned int NullAudioBackend::playSE3D(const std::string& /*file*/,
                                         float /*x*/, float /*y*/, float /*z*/) { return 0; }
void NullAudioBackend::stopSE() {}

// -- 3D Audio ----------------------------------------------------------

void NullAudioBackend::update3dListener(float /*posX*/, float /*posY*/, float /*posZ*/,
                                         float /*atX*/, float /*atY*/, float /*atZ*/,
                                         float /*upX*/, float /*upY*/, float /*upZ*/) {}

// -- Global volume -----------------------------------------------------

void NullAudioBackend::setGlobalVolume(float /*volume*/) {}
float NullAudioBackend::getGlobalVolume() const { return 1.0f; }

// -- Per-bus volume ----------------------------------------------------

void NullAudioBackend::setBusVolume(const char* /*bus*/, float /*volume*/) {}
float NullAudioBackend::getBusVolume(const char* /*bus*/) const { return 1.0f; }

// -- Wave cache ---------------------------------------------------------

void NullAudioBackend::flushWaveCache() {}

// -- State query -------------------------------------------------------

bool NullAudioBackend::isVoicePlaying() { return false; }
bool NullAudioBackend::isBGMPlaying()  { return false; }
int  NullAudioBackend::activeVoiceCount() { return 0; }

// -- Playback position / length / fade (Spec [3.2][3.3]) ---------------

float NullAudioBackend::getPosition(const char* /*bus*/) { return 0.0f; }
float NullAudioBackend::getLength(const char* /*bus*/)   { return 0.0f; }
void  NullAudioBackend::fadeVolume(const char* /*bus*/, float /*targetVolume*/,
                                    float /*fadeTime*/) {}

// -- Backend identification --------------------------------------------

const char* NullAudioBackend::getBackendName() const { return "NullAudio"; }


// -- [10.2.27] SE per-handle (no-op stubs) -----------------------------------
void NullAudioBackend::setSEVolume(unsigned int, float) {}
float NullAudioBackend::getSEVolume(unsigned int) { return 0.0f; }
void NullAudioBackend::stopSEHandle(unsigned int) {}
} // namespace Caesura