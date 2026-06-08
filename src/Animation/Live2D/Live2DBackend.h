#pragma once
#ifdef CAESURA_HAS_LIVE2D

#include "../IAnimationBackend.h"
#include <string>
#include <unordered_map>
#include <memory>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace bgfx { struct TextureHandle; }

namespace Live2D { namespace Cubism { namespace Framework {
    class CubismUserModel;
} } }
namespace Csm = Live2D::Cubism::Framework;

namespace Caesura {

class BgfxRenderDevice;

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

    void setRenderDevice(BgfxRenderDevice* device);

private:
    struct Live2DModel {
        std::string dir;          // model directory (e.g. "assets/live2d/haru/")
        std::string name;
        bool visible = false;
        float x = 0, y = 0, scale = 1.0f, opacity = 1.0f;

        // Cubism
        std::unique_ptr<Csm::CubismUserModel> userModel;
        void* mocData = nullptr;
        void* renderer = nullptr;        // CubismRenderer_D3D11*
        void* stagingTex = nullptr;      // ID3D11Texture2D* staging
        int renderWidth = 1280;
        int renderHeight = 720;

        // bgfx
        bgfx::TextureHandle bgfxTex;
        bool bgfxTexValid = false;

        // Motion cache
        std::unordered_map<std::string, std::vector<char>> motionCache;
        std::unordered_map<std::string, std::vector<char>> expressionCache;

        ~Live2DModel();
    };

    BgfxRenderDevice* m_renderDevice = nullptr;
    ID3D11Device* m_d3dDevice = nullptr;
    ID3D11DeviceContext* m_d3dContext = nullptr;
    bool m_deviceShared = false;

    std::unordered_map<int, std::unique_ptr<Live2DModel>> m_models;
    int m_nextHandle = 1;
    bool m_initialized = false;

    bool loadModelInternal(Live2DModel& model);
    bool createRendererForModel(Live2DModel& model);
    void uploadToBgfx(Live2DModel& model);
};

void registerLive2DBinding(void* luaState);
void Live2DBackend_setGlobal(Live2DBackend* backend);

} // namespace Caesura

#endif // CAESURA_HAS_LIVE2D
