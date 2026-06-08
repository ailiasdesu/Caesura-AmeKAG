#ifdef CAESURA_HAS_LIVE2D

#include "Live2DBackend.h"

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
#include <Motion/CubismExpressionMotion.hpp>
#include <Rendering/CubismRenderer.hpp>

// D3D11 backend
#ifdef _WIN32
#include <Rendering/D3D11/CubismRenderer_D3D11.hpp>
#include <Rendering/D3D11/CubismOffscreenManager_D3D11.hpp>
#include <Rendering/D3D11/CubismOffscreenSurface_D3D11.hpp>
#include <Rendering/D3D11/CubismOffscreenRenderTarget_D3D11.hpp>
#endif

#include <fstream>
#include <vector>
#include <SDL3/SDL.h>

namespace Caesura {

using namespace Csm;

// ============================================================
// File I/O helpers
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

// ============================================================
// Live2DModel destructor
// ============================================================
Live2DBackend::Live2DModel::~Live2DModel() {
    userModel.reset();
    if (mocData) {
        csmFreeAlignedMoc(mocData);
        mocData = nullptr;
    }
#ifdef _WIN32
    if (renderer) {
        auto* r = static_cast<Rendering::CubismRenderer_D3D11*>(renderer);
        Rendering::CubismRenderer::DeleteRendererAndSurface(r);
        renderer = nullptr;
    }
#endif
}

// ============================================================
// Cubism allocator (engine-managed)
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
// init / shutdown
// ============================================================
bool Live2DBackend::init() {
    static EngineAllocator allocator;
    CubismFramework::Option option;
    option.LogFunction = cubismLog;
    option.LoggingLevel = CubismFramework::Option::LogLevel_Verbose;

    if (!CubismFramework::StartUp(&allocator, &option)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[Live2D] CubismFramework::StartUp failed");
        return false;
    }
    CubismFramework::Initialize();
    m_initialized = true;
    SDL_Log("[Live2D] Initialized (Cubism 5 SDK)");
    return true;
}

void Live2DBackend::shutdown() {
    m_models.clear();
    if (m_initialized) {
        CubismFramework::Dispose();
        m_initialized = false;
    }
}

// ============================================================
// Model loading
// ============================================================
bool Live2DBackend::loadModelInternal(Live2DModel& model) {
    std::string basePath = model.path;
    if (!basePath.empty() && basePath.back() != '/' && basePath.back() != '\\')
        basePath += '/';

    // 1. Load .model3.json
    std::string modelJsonPath = basePath + model.name + ".model3.json";
    auto jsonData = readFile(modelJsonPath);
    if (jsonData.empty()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[Live2D] Cannot read %s", modelJsonPath.c_str());
        return false;
    }

    auto* modelSetting = CubismModelSettingJson::Create(
        reinterpret_cast<const csmByte*>(jsonData.data()),
        static_cast<csmSizeInt>(jsonData.size())
    );
    if (!modelSetting) return false;

    // 2. Load .moc3
    std::string mocPath = basePath + modelSetting->GetModelFileName();
    auto mocData = readFile(mocPath);
    if (mocData.empty()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[Live2D] Cannot read %s", mocPath.c_str());
        CubismModelSettingJson::Delete(modelSetting);
        return false;
    }

    model.mocData = csmAllocateAligned(
        mocData.data(), static_cast<csmSizeInt>(mocData.size()), csmAlignofMoc
    );
    if (!model.mocData) {
        CubismModelSettingJson::Delete(modelSetting);
        return false;
    }

    auto* cubismMoc = CubismMoc::Create(
        model.mocData, static_cast<csmSizeInt>(mocData.size())
    );
    if (!cubismMoc) {
        CubismModelSettingJson::Delete(modelSetting);
        return false;
    }

    // 3. Create user model
    model.userModel = std::make_unique<CubismUserModel>();
    model.userModel->LoadModel(cubismMoc->CreateModel());

    // 4. Load textures
    for (csmInt32 i = 0; i < modelSetting->GetTextureCount(); ++i) {
        std::string texPath = basePath + modelSetting->GetTextureFileName(i);
        // Texture loading will be bridged through bgfx later
        SDL_Log("[Live2D] Texture %d: %s", i, texPath.c_str());
    }

    // TODO: Load motions, expressions, physics from modelSetting
    
    CubismModelSettingJson::Delete(modelSetting);
    SDL_Log("[Live2D] Model loaded: %s", model.name.c_str());
    return true;
}

void Live2DBackend::createRenderer(Live2DModel& model) {
#ifdef _WIN32
    // Create D3D11 renderer — needs bgfx D3D11 device sharing
    // For now: store render target size, actual renderer created when device is available
    model.renderTargetWidth  = 1280;
    model.renderTargetHeight = 720;
    // Deferred: CubismRenderer_D3D11::Create() needs a D3D11 device
#endif
}

// ============================================================
// Public API implementations
// ============================================================
int Live2DBackend::loadModel(const std::string& path, const std::string& name) {
    int handle = m_nextHandle++;
    auto model = std::make_unique<Live2DModel>();
    model->path = path;
    model->name = name;

    if (!loadModelInternal(*model)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[Live2D] Failed to load model: %s", name.c_str());
        return -1;
    }

    m_models[handle] = std::move(model);
    return handle;
}

void Live2DBackend::unloadModel(int handle) {
    m_models.erase(handle);
}

bool Live2DBackend::isLoaded(int handle) const {
    return m_models.count(handle) > 0;
}

void Live2DBackend::showModel(int handle, float x, float y, float scale) {
    auto it = m_models.find(handle);
    if (it == m_models.end()) return;
    auto& m = *it->second;
    m.visible = true;
    m.x = x;
    m.y = y;
    m.scale = scale;
}

void Live2DBackend::hideModel(int handle) {
    auto it = m_models.find(handle);
    if (it != m_models.end()) it->second->visible = false;
}

void Live2DBackend::setOpacity(int handle, float opacity) {
    auto it = m_models.find(handle);
    if (it != m_models.end()) it->second->opacity = opacity;
}

void Live2DBackend::render(float dt) {
    for (auto& [handle, model] : m_models) {
        if (!model->visible || !model->userModel) continue;

        auto* cubismModel = model->userModel->GetModel();
        if (!cubismModel) continue;

        // TODO: CubismRenderer update + draw
        // For now: update model parameters only
        cubismModel->Update();
    }
}

bool Live2DBackend::playMotion(int handle, const std::string& name) {
    auto it = m_models.find(handle);
    if (it == m_models.end() || !it->second->userModel) return false;

    // TODO: Load and play motion from .motion3.json
    (void)name;
    return true;
}

void Live2DBackend::setExpression(int handle, const std::string& name) {
    auto it = m_models.find(handle);
    if (it == m_models.end()) return;
    // TODO: Load and set expression from .exp3.json
    (void)name;
}

void Live2DBackend::setParameter(int handle, const std::string& param, float value) {
    auto it = m_models.find(handle);
    if (it == m_models.end() || !it->second->userModel) return;

    auto* cubismModel = it->second->userModel->GetModel();
    if (!cubismModel) return;

    csmInt32 id = csmGetParameterId(cubismModel, param.c_str());
    if (id >= 0) {
        csmSetParameterValue(cubismModel, id, value);
    }
}

void registerLive2DBinding(void* luaState) {
    // TODO: Lua bindings for Live2D
    (void)luaState;
}

} // namespace Caesura

#endif // CAESURA_HAS_LIVE2D
