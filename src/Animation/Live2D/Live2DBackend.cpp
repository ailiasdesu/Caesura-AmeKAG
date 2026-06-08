#ifdef CAESURA_HAS_LIVE2D

#include "Live2DBackend.h"

// D3D11
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
#include <Motion/CubismMotionManager.hpp>
#include <Motion/CubismExpressionMotion.hpp>
#include <Motion/CubismExpressionMotionManager.hpp>
#include <Rendering/CubismRenderer.hpp>
#include <Rendering/D3D11/CubismRenderer_D3D11.hpp>

// Engine
#include "../../Render/BgfxRenderDevice.h"

// Lua
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <fstream>
#include <vector>
#include <SDL3/SDL.h>

namespace Caesura {

using namespace Csm;
using namespace Csm::Rendering;

// ============================================================
// File / path utilities
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

static std::string dirName(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return "";
    return path.substr(0, pos + 1);
}

static std::string joinPath(const std::string& dir, const std::string& name) {
    if (dir.empty()) return name;
    if (dir.back() == '/' || dir.back() == '\\') return dir + name;
    return dir + "/" + name;
}

// ============================================================
// Cubism allocator & logging
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
    if (bgfxTexValid && bgfx::isValid(bgfxTex)) {
        bgfx::destroy(bgfxTex);
        bgfxTexValid = false;
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
    SDL_Log("[Live2D] CubismFramework 5 initialized");
    return true;
}

void Live2DBackend::shutdown() {
    m_models.clear();
    if (m_initialized) {
        CubismFramework::Dispose();
        m_initialized = false;
    }
    m_deviceShared = false;
    if (m_d3dContext) { m_d3dContext->Release(); m_d3dContext = nullptr; }
}

void Live2DBackend::setRenderDevice(BgfxRenderDevice* device) {
    m_renderDevice = device;
    if (!device) return;

    auto* internalData = bgfx::getInternalData();
    if (!internalData || !internalData->context) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[Live2D] bgfx not using D3D11");
        return;
    }

    m_d3dDevice = static_cast<ID3D11Device*>(internalData->context);
    m_d3dDevice->GetImmediateContext(&m_d3dContext);
    CubismRenderer_D3D11::SetConstantSettings(1, m_d3dDevice);
    m_deviceShared = true;
    SDL_Log("[Live2D] D3D11 device shared from bgfx");
}

// ============================================================
// Model loading
// ============================================================
bool Live2DBackend::loadModelInternal(Live2DModel& model) {
    std::string dir = dirName(model.dir);

    // 1. Load .model3.json (model.dir is the full path to .model3.json)
    auto jsonData = readFile(model.dir);
    if (jsonData.empty()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[Live2D] Cannot read %s", model.dir.c_str());
        return false;
    }

    auto* setting = CubismModelSettingJson::Create(
        reinterpret_cast<const csmByte*>(jsonData.data()),
        static_cast<csmSizeInt>(jsonData.size())
    );
    if (!setting) return false;

    // 2. Load .moc3
    std::string mocPath = joinPath(dir, setting->GetModelFileName());
    auto mocBytes = readFile(mocPath);
    if (mocBytes.empty()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[Live2D] Cannot read %s", mocPath.c_str());
        CubismModelSettingJson::Delete(setting);
        return false;
    }

    model.mocData = csmAllocateAligned(mocBytes.data(),
        static_cast<csmSizeInt>(mocBytes.size()), csmAlignofMoc);
    if (!model.mocData) { CubismModelSettingJson::Delete(setting); return false; }

    auto* moc = CubismMoc::Create(model.mocData,
        static_cast<csmSizeInt>(mocBytes.size()));
    if (!moc) { CubismModelSettingJson::Delete(setting); return false; }

    // 3. Create user model
    model.userModel = std::make_unique<CubismUserModel>();
    model.userModel->LoadModel(moc->CreateModel());

    // 4. Preload motions (.motion3.json)
    for (csmInt32 i = 0; i < setting->GetMotionGroupCount(); ++i) {
        const csmChar* group = setting->GetMotionGroupName(i);
        csmInt32 count = setting->GetMotionCount(group);
        for (csmInt32 j = 0; j < count; ++j) {
            std::string motionFile = setting->GetMotionFileName(group, j);
            std::string motionPath = joinPath(dir, motionFile);
            auto motionData = readFile(motionPath);
            if (!motionData.empty()) {
                std::string key = group + std::string("/") + motionFile;
                model.motionCache[key] = std::move(motionData);
            }
        }
    }

    // 5. Preload expressions (.exp3.json)
    for (csmInt32 i = 0; i < setting->GetExpressionCount(); ++i) {
        const csmChar* expName = setting->GetExpressionName(i);
        const csmChar* expFile = setting->GetExpressionFileName(i);
        std::string expPath = joinPath(dir, expFile);
        auto expData = readFile(expPath);
        if (!expData.empty()) {
            model.expressionCache[expName] = std::move(expData);
        }
    }

    // 6. Create renderer
    if (m_deviceShared) {
        createRendererForModel(model);
    }

    CubismModelSettingJson::Delete(setting);
    SDL_Log("[Live2D] Model loaded: %s (%zu motions, %zu expressions)",
        model.name.c_str(), model.motionCache.size(), model.expressionCache.size());
    return true;
}

bool Live2DBackend::createRendererForModel(Live2DModel& model) {
    if (!m_d3dDevice || !model.userModel) return false;

    auto* renderer = CubismRenderer::Create();
    if (!renderer) return false;

    auto* d3dRenderer = dynamic_cast<CubismRenderer_D3D11*>(renderer);
    if (!d3dRenderer) { CubismRenderer::Delete(renderer); return false; }

    d3dRenderer->Initialize(model.userModel->GetModel());

    // Staging texture for D3D11 ¡ú CPU readback
    D3D11_TEXTURE2D_DESC sd = {};
    sd.Width = model.renderWidth;
    sd.Height = model.renderHeight;
    sd.MipLevels = 1;
    sd.ArraySize = 1;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.Usage = D3D11_USAGE_STAGING;
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    m_d3dDevice->CreateTexture2D(&sd, nullptr, (ID3D11Texture2D**)&model.stagingTex);

    // bgfx dynamic texture (RGBA8)
    model.bgfxTex = bgfx::createTexture2D(
        model.renderWidth, model.renderHeight,
        false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE
    );
    model.bgfxTexValid = bgfx::isValid(model.bgfxTex);

    model.renderer = renderer;
    return true;
}

// ============================================================
// Per-frame render
// ============================================================
void Live2DBackend::uploadToBgfx(Live2DModel& model) {
    if (!model.stagingTex || !model.bgfxTexValid) return;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = m_d3dContext->Map(
        static_cast<ID3D11Texture2D*>(model.stagingTex),
        0, D3D11_MAP_READ, 0, &mapped
    );
    if (FAILED(hr)) return;

    auto* mem = bgfx::copy(mapped.pData,
        static_cast<uint32_t>(mapped.RowPitch * model.renderHeight));
    bgfx::updateTexture2D(model.bgfxTex, 0, 0, 0, 0,
        model.renderWidth, model.renderHeight, mem);

    m_d3dContext->Unmap(static_cast<ID3D11Texture2D*>(model.stagingTex), 0);
}

void Live2DBackend::render(float dt) {
    if (!m_deviceShared || !m_d3dContext) return;

    for (auto& [handle, model] : m_models) {
        if (!model->visible || !model->renderer || !model->userModel) continue;

        auto* d3dRenderer = static_cast<CubismRenderer_D3D11*>(model->renderer);
        auto* cubismModel = model->userModel->GetModel();
        if (!cubismModel) continue;

        // Update model (parameters, physics, motions, expressions)
        model->userModel->Update();

        // Render Cubism to its internal D3D11 targets
        d3dRenderer->StartFrame(m_d3dContext);
        d3dRenderer->SetRenderState(m_d3dDevice, m_d3dContext,
            model->renderWidth, model->renderHeight);
        d3dRenderer->DrawModel();
        d3dRenderer->EndFrame();

        // Copy Cubism output ¡ú staging ¡ú bgfx
        auto& rts = d3dRenderer->GetModelRenderTargets();
        if (!rts.IsEmpty() && model->stagingTex) {
            ID3D11ShaderResourceView* srv = rts[0].GetTextureView();
            if (srv) {
                ID3D11Resource* res = nullptr;
                srv->GetResource(&res);
                if (res) {
                    m_d3dContext->CopyResource(
                        static_cast<ID3D11Texture2D*>(model->stagingTex), res);
                    res->Release();
                }
            }
        }

        uploadToBgfx(*model);

        // Blit bgfx texture to screen
        if (m_renderDevice && model->bgfxTexValid) {
            m_renderDevice->blitTexture(0, model->bgfxTex,
                model->x, model->y,
                static_cast<float>(model->renderWidth)  * model->scale,
                static_cast<float>(model->renderHeight) * model->scale,
                static_cast<uint8_t>(model->opacity * 255.0f));
        }
    }
}

// ============================================================
// Motion playback
// ============================================================
bool Live2DBackend::playMotion(int handle, const std::string& name) {
    auto it = m_models.find(handle);
    if (it == m_models.end() || !it->second->userModel) return false;

    auto& model = *it->second;

    // Find in motion cache (try exact key first, then fuzzy match)
    auto mit = model.motionCache.find(name);
    if (mit == model.motionCache.end()) {
        // Try matching just the file name
        for (auto& [key, data] : model.motionCache) {
            if (key.find(name) != std::string::npos) {
                mit = model.motionCache.find(key);
                break;
            }
        }
    }
    if (mit == model.motionCache.end()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[Live2D] Motion not found: %s", name.c_str());
        return false;
    }

    auto& data = mit->second;
    auto* motion = CubismMotion::Create(
        reinterpret_cast<const csmByte*>(data.data()),
        static_cast<csmSizeInt>(data.size())
    );
    if (!motion) return false;

    auto* manager = model.userModel->GetMotionManager();
    csmInt32 handle2 = manager->StartMotion(motion, false);
    SDL_Log("[Live2D] Motion playing: %s (handle=%d)", mit->first.c_str(), handle2);
    return handle2 >= 0;
}

// ============================================================
// Expression
// ============================================================
void Live2DBackend::setExpression(int handle, const std::string& name) {
    auto it = m_models.find(handle);
    if (it == m_models.end() || !it->second->userModel) return;

    auto& model = *it->second;
    auto eit = model.expressionCache.find(name);
    if (eit == model.expressionCache.end()) return;

    auto& data = eit->second;
    auto* expression = CubismExpressionMotion::Create(
        reinterpret_cast<const csmByte*>(data.data()),
        static_cast<csmSizeInt>(data.size())
    );
    if (!expression) return;

    auto* manager = model.userModel->GetExpressionManager();
    manager->StartMotion(expression, false);
}

// ============================================================
// Parameter control
// ============================================================
void Live2DBackend::setParameter(int handle, const std::string& param, float value) {
    auto it = m_models.find(handle);
    if (it == m_models.end() || !it->second->userModel) return;
    auto* cubismModel = it->second->userModel->GetModel();
    if (!cubismModel) return;
    csmInt32 id = csmGetParameterId(cubismModel, param.c_str());
    if (id >= 0) csmSetParameterValue(cubismModel, id, value);
}

// ============================================================
// Model lifecycle
// ============================================================
int Live2DBackend::loadModel(const std::string& path, const std::string& name) {
    int handle = m_nextHandle++;
    auto model = std::make_unique<Live2DModel>();
    model->dir = path;
    model->name = name;
    if (!loadModelInternal(*model)) return -1;
    m_models[handle] = std::move(model);
    return handle;
}

void Live2DBackend::unloadModel(int handle) { m_models.erase(handle); }
bool Live2DBackend::isLoaded(int handle) const { return m_models.count(handle) > 0; }

void Live2DBackend::showModel(int handle, float x, float y, float scale) {
    auto it = m_models.find(handle);
    if (it == m_models.end()) return;
    it->second->visible = true;
    it->second->x = x; it->second->y = y; it->second->scale = scale;
}

void Live2DBackend::hideModel(int handle) {
    auto it = m_models.find(handle);
    if (it != m_models.end()) it->second->visible = false;
}

void Live2DBackend::setOpacity(int handle, float opacity) {
    auto it = m_models.find(handle);
    if (it != m_models.end()) it->second->opacity = opacity;
}

// ============================================================
// Lua bindings
// ============================================================
static Live2DBackend* g_live2d = nullptr;

static int lua_l2d_load_model(lua_State* L) {
    if (!g_live2d) { lua_pushinteger(L, -1); return 1; }
    const char* path = luaL_checkstring(L, 1);
    const char* name = luaL_optstring(L, 2, "model");
    int handle = g_live2d->loadModel(path, name);
    lua_pushinteger(L, handle);
    return 1;
}

static int lua_l2d_unload_model(lua_State* L) {
    if (!g_live2d) return 0;
    int handle = (int)luaL_checkinteger(L, 1);
    g_live2d->unloadModel(handle);
    return 0;
}

static int lua_l2d_show_model(lua_State* L) {
    if (!g_live2d) return 0;
    int handle = (int)luaL_checkinteger(L, 1);
    float x = (float)luaL_optnumber(L, 2, 0);
    float y = (float)luaL_optnumber(L, 3, 0);
    float scale = (float)luaL_optnumber(L, 4, 1.0);
    g_live2d->showModel(handle, x, y, scale);
    return 0;
}

static int lua_l2d_hide_model(lua_State* L) {
    if (!g_live2d) return 0;
    g_live2d->hideModel((int)luaL_checkinteger(L, 1));
    return 0;
}

static int lua_l2d_set_opacity(lua_State* L) {
    if (!g_live2d) return 0;
    g_live2d->setOpacity((int)luaL_checkinteger(L, 1),
        (float)luaL_checknumber(L, 2));
    return 0;
}

static int lua_l2d_play_motion(lua_State* L) {
    if (!g_live2d) { lua_pushboolean(L, 0); return 1; }
    bool ok = g_live2d->playMotion(
        (int)luaL_checkinteger(L, 1),
        luaL_checkstring(L, 2));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lua_l2d_set_expression(lua_State* L) {
    if (!g_live2d) return 0;
    g_live2d->setExpression((int)luaL_checkinteger(L, 1),
        luaL_checkstring(L, 2));
    return 0;
}

static int lua_l2d_set_parameter(lua_State* L) {
    if (!g_live2d) return 0;
    g_live2d->setParameter((int)luaL_checkinteger(L, 1),
        luaL_checkstring(L, 2),
        (float)luaL_checknumber(L, 3));
    return 0;
}

static int lua_l2d_is_loaded(lua_State* L) {
    if (!g_live2d) { lua_pushboolean(L, 0); return 1; }
    lua_pushboolean(L, g_live2d->isLoaded((int)luaL_checkinteger(L, 1)) ? 1 : 0);
    return 1;
}

static const luaL_Reg l2d_functions[] = {
    { "load_model",    lua_l2d_load_model    },
    { "unload_model",  lua_l2d_unload_model  },
    { "show_model",    lua_l2d_show_model    },
    { "hide_model",    lua_l2d_hide_model    },
    { "set_opacity",   lua_l2d_set_opacity   },
    { "play_motion",   lua_l2d_play_motion   },
    { "set_expression",lua_l2d_set_expression},
    { "set_parameter", lua_l2d_set_parameter },
    { "is_loaded",     lua_l2d_is_loaded     },
    { nullptr, nullptr }
};

void registerLive2DBinding(void* luaState) {
    auto* L = static_cast<lua_State*>(luaState);
    luaL_newlib(L, l2d_functions);
    lua_setglobal(L, "live2d");
    SDL_Log("[Live2D] Lua bindings registered (live2d.*)");
}

// Global setter (called by Engine when backend is created)
void Live2DBackend_setGlobal(Live2DBackend* backend) {
    g_live2d = backend;
}

} // namespace Caesura

#endif // CAESURA_HAS_LIVE2D
