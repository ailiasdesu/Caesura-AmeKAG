#pragma once
#include "api/IAnimationBackend.h"
#include <unordered_map>

namespace Caesura {

// NullAnimationBackend — no-op animation backend with PNG fallback (R2.1)
// When Cubism SDK is unavailable, loads PNG/JPG/BMP as static textures.
class NullAnimationBackend : public IAnimationBackend {
public:
    bool init() override;
    void shutdown() override;

    int  loadModel(const std::string& path, const std::string& name) override;
    void unloadModel(int handle) override;
    bool isLoaded(int handle) const override;

    void showModel(int handle, float x, float y, float scale) override;
    void hideModel(int handle) override;
    void setOpacity(int handle, float opacity) override;
    void render(float dt) override;

    bool playMotion(int handle, const std::string& name) override;
    void setExpression(int handle, const std::string& name) override;
    void setParameter(int handle, const std::string& param, float value) override;

    const char* name() const override { return "NullAnimation+PNG"; }

private:
    struct StaticSprite {
        uint32_t textureId = 0;
        float x = 0, y = 0;
        float scale = 1.0f;
        float opacity = 1.0f;
        bool visible = false;
    };

    std::unordered_map<int, StaticSprite> m_sprites;
    int m_nextHandle = 1;
    bool m_initialized = false;

    static bool isImagePath(const std::string& path);
};

} // namespace Caesura
