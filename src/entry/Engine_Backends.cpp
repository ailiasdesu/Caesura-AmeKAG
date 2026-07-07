#include "../audio/NullAudioBackend.h"
#include "../live2d/NullAnimationBackend.h"
#include "../minigame/NullMiniGameBackend.h"
#include "../steam/NullSteamBackend.h"

#ifdef CAESURA_HAS_LIVE2D
#include "../live2d/Live2D/Live2DBackend.h"
#endif

#ifdef CAESURA_HAS_STEAM
#include "../steam/SteamBackend.h"
#endif

#include <memory>

namespace Caesura {

class IRenderDevice;

std::unique_ptr<ISteamBackend> createDefaultSteamIntegration(ISteamBackend* configured) {
    if (configured) {
        return std::unique_ptr<ISteamBackend>(configured);
    }

#ifdef CAESURA_HAS_STEAM
    return std::make_unique<SteamBackend>();
#else
    return std::make_unique<NullSteamBackend>();
#endif
}

std::unique_ptr<IAudioBackend> createHeadlessAudioBackend() {
    return std::make_unique<NullAudioBackend>();
}

std::unique_ptr<IMiniGameBackend> createFallbackMiniGameBackend() {
    return std::make_unique<NullMiniGameBackend>();
}

std::unique_ptr<IAnimationBackend> createDefaultAnimationBackend() {
#ifdef CAESURA_HAS_LIVE2D
    return std::make_unique<Live2DBackend>();
#else
    return std::make_unique<NullAnimationBackend>();
#endif
}

std::unique_ptr<IAnimationBackend> createFallbackAnimationBackend() {
    return std::make_unique<NullAnimationBackend>();
}

void attachRenderDeviceToAnimationBackend(IAnimationBackend* animationBackend,
                                          IRenderDevice* renderDevice) {
#ifdef CAESURA_HAS_LIVE2D
    if (auto* live2d = dynamic_cast<Live2DBackend*>(animationBackend)) {
        live2d->setRenderDevice(renderDevice);
    }
#else
    (void)animationBackend;
    (void)renderDevice;
#endif
}

} // namespace Caesura
