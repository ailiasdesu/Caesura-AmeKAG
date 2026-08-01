#if defined(CAESURA_HAS_LIVE2D) && defined(_WIN32)

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

    // Required before any CubismRenderer_D3D11::CreateRenderer() (model load).
    CubismRenderer_D3D11::SetConstantSettings(1, m_device);

    SDL_Log("[Live2D/D3D11] Render path ready — shared device (bgfx D3D11)");
    return true;
}

void D3D11NativeRenderPath::shutdown() {
    for (ID3D11ShaderResourceView* srv : m_modelSrvs) {
        if (srv) srv->Release();
    }
    m_modelSrvs.clear();
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
// Model texture (D3D11 SRV for CubismRenderer_D3D11::BindTexture)
// ============================================================
ID3D11ShaderResourceView* D3D11NativeRenderPath::createModelTexture(
    int width, int height, const unsigned char* pixels) {
    if (!m_device || width <= 0 || height <= 0 || !pixels) return nullptr;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width  = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = pixels;
    initData.SysMemPitch = static_cast<UINT>(width) * 4;

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = m_device->CreateTexture2D(&desc, &initData, &tex);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "[Live2D/D3D11] CreateTexture2D (model) failed: 0x%08X", hr);
        return nullptr;
    }
    ID3D11ShaderResourceView* srv = nullptr;
    hr = m_device->CreateShaderResourceView(tex, nullptr, &srv);
    tex->Release();
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "[Live2D/D3D11] CreateShaderResourceView (model) failed: 0x%08X", hr);
        return nullptr;
    }
    m_modelSrvs.push_back(srv);
    return srv;
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
    // Cubism renders into our shared texture (RTV); bgfx consumes it via
    // overrideInternal in endFrame(). bgfx runs single-threaded
    // (BGFX_CONFIG_MULTITHREADED=0), so driving the shared D3D11 context here
    // is serialized with bgfx's own frame.
    if (!m_sharedTex && !createSharedTexture(m_width, m_height)) return;

    auto* d3dRenderer = static_cast<CubismRenderer_D3D11*>(renderer);
    if (!d3dRenderer) return;

    ID3D11RenderTargetView* prevRTV = nullptr;
    ID3D11DepthStencilView* prevDSV = nullptr;
    m_context->OMGetRenderTargets(1, &prevRTV, &prevDSV);

    m_context->OMSetRenderTargets(1, &m_rtv, nullptr);
    const D3D11_VIEWPORT viewport = { 0.0f, 0.0f,
        static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
    m_context->RSSetViewports(1, &viewport);

    d3dRenderer->StartFrame(m_context);
    d3dRenderer->DrawModel();
    d3dRenderer->EndFrame();

    // Restore bgfx's render target for the rest of this frame.
    m_context->OMSetRenderTargets(1, &prevRTV, prevDSV);
    if (prevRTV) prevRTV->Release();
    if (prevDSV) prevDSV->Release();
}

void D3D11NativeRenderPath::endFrame(CubismRenderer* renderer, bgfx::TextureHandle bgfxTex) {
    (void)renderer;
    if (!m_sharedTex) return;

    // Cubism already drew into m_sharedTex (bound in beginFrame); hand it to bgfx.
    if (bgfx::isValid(bgfxTex)) {
        bgfx::overrideInternal(bgfxTex, reinterpret_cast<uintptr_t>(m_sharedTex));
    }
}

void D3D11NativeRenderPath::resize(int width, int height) {
    createSharedTexture(width, height);
}

} // namespace Caesura

#endif
