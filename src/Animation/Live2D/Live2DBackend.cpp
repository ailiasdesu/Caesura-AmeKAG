#ifdef CAESURA_HAS_LIVE2D

#include "Live2DBackend.h"

// Windows / D3D11
#include <d3d11.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

// bgfx
#include <bgfx/bgfx.h>

// Cubism Core
#include <Live2DCubismCore.h>

// Cubism Framework
#include <CubismFramework.hpp>
#include <Model/CubismMoc.hpp>
#include <Model/CubismModel.hpp>
#include <Model/CubismUserModel.hpp>
#include <ICubismModelSetting.hpp>
#include <CubismModelSettingJson.hpp>
#include <Motion/CubismMotion.hpp>
#include <Rendering/CubismRenderer.hpp>
#include <Rendering/D3D11/CubismRenderer_D3D11.hpp>
#include <Rendering/D3D11/CubismOffscreenSurface_D3D11.hpp>

// Engine
#include "../../Render/BgfxRenderDevice.h"

#include <fstream>
#include <vector>
#include <SDL3/SDL.h>

namespace Caesura {

using namespace Csm;
using namespace Csm::Rendering;

// ============================================================
// File helpers
// ============================================================
static std::vector<char> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};
    size_t size = file.tellg();
    file.seekg(0);
    std::vector<char> data(size);
    file.read(data.data(), size);
    return data;
}

static std::string baseDir(const std::string& fullPath) {
    auto pos = fullPath.find_last_of("/\\");
    if (pos == std::string::npos) return "";
    return fullPath.substr(0, pos + 1);
}

// ============================================================
// Cubism allocator
// ============================================================
namespace {
    class EngineAllocator : public ICubismAllocator {
    public:
        void* Allocate(const csmSizeType size) override { return SDL_malloc(size); }
        void  Deallocate(void* memory) override { SDL_free(memory); }
        void* AllocateAligned(const csmSizeType size, csmUint32 alignment) override { return SDL_aligned_alloc(alignment, size); }
        void  DeallocateAligned(void* alignedMemory) override { SDL_aligned_free(alignedMemory); }
    };

    static void cubismLog(const csmChar* message) {
        SDL_Log("[Live2D] %s", message);
    }
}

// ============================================================
// Live2DModel destructor
// ============================================================
Live2DBackend::Live2DModel::~Live2DModel() {
    if (renderer) {
        CubismRenderer::Delete(static_cast<CubismRenderer*>(renderer));
        renderer = nullptr;
    }
    userModel.reset();
    if (mocData) {
        csmFreeAlignedMoc(mocData);
        mocData = nullptr;
    }
    if (stagingTex) {
        static_cast<ID3D11Texture2D*>(stagingTex)->Release();
        stagingTex = nullptr;
    }
    if (bgfx::isValid(bgfxTex)) {
        bgfx::destroy(bgfxTex);
    }
}

// ============================================================
// init / shutdown
// ============================================================
bool Live2DBackend::init() {
    static EngineAllocator allocator;
    CubismFramework::Option option;
    option.LogFunction = cubismLog;
    option.LoggingLevel = CubismFramework::Option::LogLevel_Verbose;

    if (!CubismFramework::StartUp(&allocator, &option)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[Live2D] StartUp failed");
        return false;
    }
    CubismFramework::Initialize();
    m_initialized = true;
    SDL_Log("[Live2D] CubismFramework initialized");
    return true;
}

void Live2DBackend::shutdown() {
    m_models.clear();
    if (m_initialized) {
        CubismFramework::Dispose();
        m_initialized = false;
    }
    m_deviceShared = false;
}

void Live2DBackend::setRenderDevice(BgfxRenderDevice* device) {
    m_renderDevice = device;
    if (!device) return;

    // Get D3D11 device from bgfx
    auto* internalData = bgfx::getInternalData();
    if (!internalData || !internalData->context) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[Live2D] bgfx not using D3D11 backend");
        return;
    }

    m_d3dDevice = static_cast<ID3D11Device*>(internalData->context);
    m_d3dDevice->GetImmediateContext(&m_d3dContext);

    // Register D3D11 device with Cubism
    CubismRenderer_D3D11::SetConstantSettings(1, m_d3dDevice);
    m_deviceShared = true;
    SDL_Log("[Live2D] D3D11 device shared from bgfx");
}

// ============================================================
// Model loading
// ============================================================
bool Live2DBackend::loadModelInternal(Live2DModel& model) {
    std::string dir = baseDir(model.path);
    std::string baseName = model.name;

    // 1. Load .model3.json
    std::string jsonPath = model.path;
    auto jsonData = readFile(jsonPath);
    if (jsonData.empty()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[Live2D] Cannot read %s", jsonPath.c_str());
        return false;
    }

    auto* setting = CubismModelSettingJson::Create(
        reinterpret_cast<const csmByte*>(jsonData.data()),
        static_cast<csmSizeInt>(jsonData.size())
    );
    if (!setting) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[Live2D] Failed to parse model3.json");
        return false;
    }

    // 2. Load .moc3
    std::string mocPath = dir + setting->GetModelFileName();
    auto mocBytes = readFile(mocPath);
    if (mocBytes.empty()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[Live2D] Cannot read %s", mocPath.c_str());
        CubismModelSettingJson::Delete(setting);
        return false;
    }

    model.mocData = csmAllocateAligned(
        mocBytes.data(), static_cast<csmSizeInt>(mocBytes.size()), csmAlignofMoc
    );
    if (!model.mocData) {
        CubismModelSettingJson::Delete(setting);
        return false;
    }

    auto* cubismMoc = CubismMoc::Create(
        model.mocData, static_cast<csmSizeInt>(mocBytes.size())
    );
    if (!cubismMoc) {
        CubismModelSettingJson::Delete(setting);
        return false;
    }

    // 3. Create user model
    model.userModel = std::make_unique<CubismUserModel>();
    model.userModel->LoadModel(cubismMoc->CreateModel());

    // 4. Load textures via Cubism (D3D11 handles this internally)
    for (csmInt32 i = 0; i < setting->GetTextureCount(); ++i) {
        std::string texPath = dir + setting->GetTextureFileName(i);
        SDL_Log("[Live2D] Texture [%d]: %s", i, texPath.c_str());
        // Texture loading is handled by CubismRenderer_D3D11::BindTexture
    }

    // 5. Create renderer if device is available
    if (m_deviceShared) {
        createRendererForModel(model);
    }

    CubismModelSettingJson::Delete(setting);
    return true;
}

bool Live2DBackend::createRendererForModel(Live2DModel& model) {
    if (!m_d3dDevice || !model.userModel) return false;

    // Create CubismRenderer_D3D11 via the static factory
    auto* renderer = CubismRenderer::Create();
    if (!renderer) {
        SDL_LogError(SDL_LOG_LOGICAL_CATEGORY, "[Live2D] CubismRenderer::Create failed");
        return false;
    }

    auto* d3dRenderer = dynamic_cast<CubismRenderer_D3D11*>(renderer);
    if (!d3dRenderer) {
        CubismRenderer::Delete(renderer);
        return false;
    }

    d3dRenderer->Initialize(model.userModel->GetModel());

    // Create staging texture for CPU readback
    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width = model.renderWidth;
    stagingDesc.Height = model.renderHeight;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    HRESULT hr = m_d3dDevice->CreateTexture2D(&stagingDesc, nullptr, (ID3D11Texture2D**)&model.stagingTex);
    if (FAILED(hr)) {
        SDL_LogError(SDL_LOG_LOGICAL_CATEGORY, "[Live2D] CreateTexture2D staging failed: 0x%08X", hr);
    }

    // Create bgfx dynamic texture
    model.bgfxTex = bgfx::createTexture2D(
        model.renderWidth, model.renderHeight,
        false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE
    );

    model.renderer = renderer;
    SDL_Log("[Live2D] Renderer created for model (D3D11 ¡ú bgfx bridge)");
    return true;
}

// ============================================================
// Per-frame: Cubism render ¡ú staging ¡ú bgfx upload
// ============================================================
void Live2DBackend::uploadToBgfx(Live2DModel& model) {
    if (!model.stagingTex || !bgfx::isValid(model.bgfxTex)) return;

    // Map staging texture
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = m_d3dContext->Map(
        static_cast<ID3D11Texture2D*>(model.stagingTex),
        0, D3D11_MAP_READ, 0, &mapped
    );
    if (FAILED(hr)) return;

    // Upload to bgfx (RGBA8 ¡ú RGBA8, pitch is row pitch)
    bgfx::updateTexture2D(
        model.bgfxTex, 0, 0, 0, 0,
        model.renderWidth, model.renderHeight,
        bgfx::copy(mapped.pData, mapped.RowPitch * model.renderHeight)
    );

    m_d3dContext->Unmap(static_cast<ID3D11Texture2D*>(model.stagingTex), 0);
}

void Live2DBackend::render(float dt) {
    if (!m_deviceShared || !m_d3dContext) return;

    for (auto& [handle, model] : m_models) {
        if (!model->visible || !model->renderer || !model->userModel) continue;

        auto* d3dRenderer = static_cast<CubismRenderer_D3D11*>(model->renderer);
        auto* cubismModel = model->userModel->GetModel();
        if (!cubismModel) continue;

        // Update model
        model->userModel->Update();

        // Render with Cubism
        d3dRenderer->StartFrame(m_d3dContext);

        d3dRenderer->SetRenderState(
            m_d3dDevice, m_d3dContext, model->renderWidth, model->renderHeight
        );

        d3dRenderer->DrawModel();
        d3dRenderer->EndFrame();

        // Copy Cubism render target ¡ú staging via shader resource view
        auto& renderTargets = d3dRenderer->GetModelRenderTargets();
        if (!renderTargets.IsEmpty() && model->stagingTex) {
            ID3D11ShaderResourceView* srv = renderTargets[0].GetTextureView();
            if (srv) {
                ID3D11Resource* res = nullptr;
                srv->GetResource(&res);
                if (res) {
                    m_d3dContext->CopyResource(
                        static_cast<ID3D11Texture2D*>(model->stagingTex),
                        res
                    );
                    res->Release();
                }
            }
        }

        // Upload result to bgfx
        uploadToBgfx(*model);

        // Blit the texture using the render device
        if (m_renderDevice && bgfx::isValid(model->bgfxTex)) {
            m_renderDevice->blitTexture(
                0,  // view 0
                model->bgfxTex,
                model->x, model->y,
                static_cast<float>(model->renderWidth)  * model->scale,
                static_cast<float>(model->renderHeight) * model->scale,
                static_cast<uint8_t>(model->opacity * 255.0f)
            );
        }
    }
}

// ============================================================
// Public API
// ============================================================
int Live2DBackend::loadModel(const std::string& path, const std::string& name) {
    int handle = m_nextHandle++;
    auto model = std::make_unique<Live2DModel>();
    model->path = path;
    model->name = name;

    if (!loadModelInternal(*model)) {
        return -1;
    }

    m_models[handle] = std::move(model);
    return handle;
}

void Live2DBackend::unloadModel(int handle) { m_models.erase(handle); }
bool Live2DBackend::isLoaded(int handle) const { return m_models.count(handle) > 0; }

void Live2DBackend::showModel(int handle, float x, float y, float scale) {
    auto it = m_models.find(handle);
    if (it == m_models.end()) return;
    it->second->visible = true;
    it->second->x = x;
    it->second->y = y;
    it->second->scale = scale;
}

void Live2DBackend::hideModel(int handle) {
    auto it = m_models.find(handle);
    if (it != m_models.end()) it->second->visible = false;
}

void Live2DBackend::setOpacity(int handle, float opacity) {
    auto it = m_models.find(handle);
    if (it != m_models.end()) it->second->opacity = opacity;
}

bool Live2DBackend::playMotion(int handle, const std::string& name) {
    (void)handle; (void)name;
    return true;
}

void Live2DBackend::setExpression(int handle, const std::string& name) {
    (void)handle; (void)name;
}

void Live2DBackend::setParameter(int handle, const std::string& param, float value) {
    auto it = m_models.find(handle);
    if (it == m_models.end() || !it->second->userModel) return;
    auto* cubismModel = it->second->userModel->GetModel();
    if (!cubismModel) return;
    csmInt32 id = csmGetParameterId(cubismModel, param.c_str());
    if (id >= 0) csmSetParameterValue(cubismModel, id, value);
}

void registerLive2DBinding(void* luaState) { (void)luaState; }

} // namespace Caesura

#endif // CAESURA_HAS_LIVE2D
