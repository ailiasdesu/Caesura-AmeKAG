#pragma once
#if defined(CAESURA_HAS_LIVE2D) && defined(_WIN32)

#include "ILive2DRenderPath.h"

#include <vector>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11RenderTargetView;
struct ID3D11ShaderResourceView;

namespace Caesura {

// Windows optimal: Cubism D3D11 → shared texture → bgfx D3D11 (zero copy).
// Shares bgfx's D3D11 device, renders Cubism directly into a bgfx-visible texture.
class D3D11NativeRenderPath : public ILive2DRenderPath {
public:
    bool init(int width, int height) override;
    void shutdown() override;
    CsmRendering::CubismRenderer* createRenderer() override;

    ID3D11ShaderResourceView* createModelTexture(int width, int height,
                                                 const unsigned char* pixels);
    void beginFrame(CsmRendering::CubismRenderer* renderer) override;
    void endFrame(CsmRendering::CubismRenderer* renderer, bgfx::TextureHandle bgfxTex) override;
    void resize(int width, int height) override;
    const char* name() const override { return "D3D11Native"; }

private:
    bool createSharedTexture(int width, int height);

    ID3D11Device*        m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    ID3D11Texture2D*     m_sharedTex = nullptr;
    ID3D11RenderTargetView*   m_rtv = nullptr;
    ID3D11ShaderResourceView* m_srv = nullptr;
    std::vector<ID3D11ShaderResourceView*> m_modelSrvs;

    int m_width = 1280;
    int m_height = 720;
};

} // namespace Caesura

#endif
