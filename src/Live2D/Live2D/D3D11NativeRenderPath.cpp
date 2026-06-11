#ifdef CAESURA_HAS_LIVE2D

#include "D3D11NativeRenderPath.h"

// bgfx internals
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

// Cubism
#include <Rendering/CubismRenderer.hpp>
#include <Rendering/D3D11/CubismRenderer_D3D11.hpp>

// Direct3D 11
#include <d3d11.h>

#include <SDL3/SDL.h>

namespace Caesura {

using namespace Csm;
using namespace Csm::Rendering;

// ============================================================
// Get bgfx's D3D11 device
// ============================================================
static ID3D11Device* getBgfxD3D11Device() {
    const bgfx::InternalData* internal = bgfx::getInternalData();
    if (!internal) return nullptr;
    return static_cast<ID3D11Device*>(internal->context);
}

static ID3D11DeviceContext* getBgfxD3D11Context() {
    ID3D11Device* device = getBgfxD3D11Device();
    if (!device) return nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    device->GetImmediateContext(&ctx);
    return ctx;
}

// ============================================================
// init / shutdown
// ============================================================
bool D3D11NativeRenderPath::init(int width, int height) {
    m_device = getBgfxD3D11Device();
    if (!m_device) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
            "[Live2D/D3D11] bgfx D3D11 device not available, falling back");
        return false;
    }
    m_device->GetImmediateContext(&m_context);  // shared with bgfx, no Release
    m_width = width;
    m_height = height;

    SDL_Log("[Live2D/D3D11] Render path ready — shared device (bgfx D3D11)");
    return true;
}

void D3D11NativeRenderPath::shutdown() {
    if (m_srv)  { m_srv->Release();  m_srv = nullptr;  }
    if (m_rtv)  { m_rtv->Release();  m_rtv = nullptr;  }
    if (m_sharedTex) { m_sharedTex->Release(); m_sharedTex = nullptr; }
    // m_context is shared with bgfx; do NOT release it
    m_context = nullptr;
    m_device = nullptr;
}

// ============================================================
// Shared texture (RTV for Cubism output → SRV for bgfx input)
// ============================================================
bool D3D11NativeRenderPath::createSharedTexture(int width, int height) {
    if (m_sharedTex) {
        if (m_width == width && m_height == height) return true;
        m_rtv->Release();  m_rtv = nullptr;
        m_srv->Release();  m_srv = nullptr;
        m_sharedTex->Release(); m_sharedTex = nullptr;
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width  = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &m_sharedTex);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[Live2D/D3D11] CreateTexture2D failed: 0x%08X", hr);
        return false;
    }

    hr = m_device->CreateRenderTargetView(m_sharedTex, nullptr, &m_rtv);
    if (FAILED(hr)) return false;

    hr = m_device->CreateShaderResourceView(m_sharedTex, nullptr, &m_srv);
    if (FAILED(hr)) return false;

    m_width = width;
    m_height = height;
    return true;
}

// ============================================================
// Renderer (delegated to CubismUserModel)
// ============================================================
CubismRenderer* D3D11NativeRenderPath::createRenderer() {
    return nullptr; // renderer created by CubismUserModel::CreateRenderer()
}

// ============================================================
// Per-frame: Cubism D3D11 → GPU copy → bgfx
// ============================================================
void D3D11NativeRenderPath::beginFrame(CubismRenderer* renderer) {
    // Cubism D3D11 renderer does its own setup in DrawModel()
    // We just ensure the render target texture exists
    if (!m_sharedTex && !createSharedTexture(m_width, m_height)) return;

    auto* d3dRenderer = static_cast<CubismRenderer_D3D11*>(renderer);
    if (d3dRenderer) {
        d3dRenderer->DrawModel();
    }
}

void D3D11NativeRenderPath::endFrame(CubismRenderer* renderer, bgfx::TextureHandle bgfxTex) {
    if (!m_sharedTex || !m_context) return;

    // Get Cubism's current render target from D3D11 pipeline state
    ID3D11RenderTargetView* cubismRTV = nullptr;
    ID3D11DepthStencilView* cubismDSV = nullptr;
    m_context->OMGetRenderTargets(1, &cubismRTV, &cubismDSV);

    if (cubismRTV) {
        // Get the texture backing Cubism's render target
        ID3D11Resource* cubismResource = nullptr;
        cubismRTV->GetResource(&cubismResource);

        // GPU-side copy: Cubism's RT → our shared texture
        m_context->CopyResource(m_sharedTex, cubismResource);

        cubismResource->Release();
        cubismRTV->Release();
    }
    if (cubismDSV) cubismDSV->Release();

    // Swap bgfx's internal texture with our shared SRV (zero-copy from here)
    if (bgfx::isValid(bgfxTex)) {
        bgfx::overrideInternal(bgfxTex, reinterpret_cast<uintptr_t>(m_sharedTex));
    }
}

void D3D11NativeRenderPath::resize(int width, int height) {
    createSharedTexture(width, height);
}

} // namespace Caesura

#endif