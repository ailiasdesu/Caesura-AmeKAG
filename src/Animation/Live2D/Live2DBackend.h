#pragma once
#ifdef CAESURA_HAS_LIVE2D

#include "../IAnimationBackend.h"
#include <string>
#include <unordered_map>
#include <memory>

struct SDL_IOStream;

namespace Live2D { namespace Cubism { namespace Framework {
    class CubismUserModel;
} } }
namespace Csm = Live2D::Cubism::Framework;

namespace Caesura {

class Live2DBackend : public IAnimationBackend {
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

    const char* name() const override { return "Live2D"; }

private:
    struct Live2DModel {
        std::string path;
        std::string name;
        bool visible = false;
        float x = 0, y = 0, scale = 1.0f, opacity = 1.0f;

        // Cubism objects
        std::unique_ptr<Csm::CubismUserModel> userModel;
        void* mocData = nullptr;  // raw .moc3 bytes (aligned)
        void* renderer = nullptr; // CubismRenderer_D3D11 (opaque ptr to avoid header deps)
        int renderTargetWidth = 0;
        int renderTargetHeight = 0;

        ~Live2DModel();
    };

    std::unordered_map<int, std::unique_ptr<Live2DModel>> m_models;
    int m_nextHandle = 1;
    bool m_initialized = false;

    bool loadModelInternal(Live2DModel& model);
    void createRenderer(Live2DModel& model);
};

void registerLive2DBinding(void* luaState);

} // namespace Caesura

#endif // CAESURA_HAS_LIVE2D
