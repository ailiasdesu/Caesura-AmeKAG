#ifdef CAESURA_HAS_LIVE2D

#include "Live2DBackend.h"

// Cubism Framework
#include <CubismFramework.hpp>
#include <Model/CubismMoc.hpp>
#include <Model/CubismModel.hpp>
#include <Model/CubismUserModel.hpp>
#include <ICubismModelSetting.hpp>
#include <CubismModelSettingJson.hpp>
#include <stb_image.h>
#include <Motion/CubismMotion.hpp>
#include <Motion/CubismMotionManager.hpp>
#include <Motion/CubismMotionQueueManager.hpp>
#include <Motion/CubismExpressionMotion.hpp>
#include <Motion/CubismExpressionMotionManager.hpp>
#include <Rendering/CubismRenderer.hpp>
#ifdef _WIN32
#include <Rendering/D3D11/CubismRenderer_D3D11.hpp>
#else
#include <Rendering/OpenGL/CubismRenderer_OpenGLES2.hpp>
#endif

// Engine
#include "Live2DUserModel.h"
#include "ILive2DRenderPath.h"
#include "../../render/api/IRenderDevice.h"
#ifdef _WIN32
#include "D3D11NativeRenderPath.h"
#else
#include "OpenGLReadbackRenderPath.h"
#include "OpenGLSharedRenderPath.h"
#ifdef __APPLE__
#include "MetalNativeRenderPath.h"
#endif
#endif

#include <fstream>
#include <vector>
#include <cstring>
#include <SDL3/SDL.h>

namespace Caesura {

using namespace Csm;
using namespace Csm::Rendering;
using namespace Live2D::Cubism::Core;

// ============================================================
// File helpers
// ============================================================
static std::vector<char> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};
    size_t size = file.tellg();
    if (size == 0) return {};
    file.seekg(0);
    std::vector<char> data(size);
    file.read(data.data(), size);
    if (!file.good()) return {};
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
// Cubism allocator
// ============================================================
namespace {
    class EngineAllocator : public ICubismAllocator {
    public:
        void* Allocate(const csmSizeType size) override { return SDL_malloc(size); }
        void  Deallocate(void* memory) override { SDL_free(memory); }
        void* AllocateAligned(const csmSizeType size, csmUint32 alignment) override {
            return SDL_aligned_alloc(alignment, size);
        }
        void  DeallocateAligned(void* alignedMemory) override { SDL_aligned_free(alignedMemory); }
    };

    static void cubismLog(const csmChar* message) {
        SDL_Log("[Live2D] %s", message);
    }

    // CubismFramework file loader (used for FrameworkShaders/*.fx).
    static csmByte* cubismLoadFile(const std::string filePath, csmSizeInt* size) {
        auto data = readFile(filePath);
        if (data.empty()) {
            if (size) *size = 0;
            return nullptr;
        }
        csmByte* buf = static_cast<csmByte*>(SDL_malloc(data.size()));
        if (!buf) {
            if (size) *size = 0;
            return nullptr;
        }
        std::memcpy(buf, data.data(), data.size());
        if (size) *size = static_cast<csmSizeInt>(data.size());
        return buf;
    }

    static void cubismReleaseBytes(csmByte* buffer) {
        SDL_free(buffer);
    }

    // Create an RGBA8 bgfx texture from decoded pixels (model texture).
    static bgfx::TextureHandle createBgfxTexture(int width, int height,
                                                 const unsigned char* pixels) {
        if (width <= 0 || height <= 0 || !pixels) return BGFX_INVALID_HANDLE;
        const bgfx::Memory* mem = bgfx::copy(pixels,
            static_cast<uint32_t>(width) * static_cast<uint32_t>(height) * 4);
        return bgfx::createTexture2D(static_cast<uint16_t>(width),
            static_cast<uint16_t>(height), false, 1, bgfx::TextureFormat::RGBA8,
            BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT, mem);
    }
}

// ============================================================
// Live2DModel destructor
// ============================================================
Live2DBackend::Live2DModel::~Live2DModel() {
    if (setting) {
        delete setting;
        setting = nullptr;
    }
    // renderer is owned by CubismUserModel (~CubismUserModel calls DeleteRenderer);
    // an explicit CubismRenderer::Delete here would double-free.
    userModel.reset();
    if (bgfxTexValid && bgfx::isValid(bgfxTex)) {
        bgfx::destroy(bgfxTex);
        bgfxTexValid = false;
    }
    for (bgfx::TextureHandle tex : textures) {
        if (bgfx::isValid(tex)) bgfx::destroy(tex);
    }
    textures.clear();
}

// ============================================================
// init / shutdown
// ============================================================
bool Live2DBackend::init() {
    const auto rendererType = bgfx::getRendererType();
#ifdef _WIN32
    if (rendererType != bgfx::RendererType::Direct3D11) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "[Live2D] Windows build requires the bgfx D3D11 renderer");
        return false;
    }
#elif defined(__APPLE__)
    if (rendererType != bgfx::RendererType::OpenGL &&
        rendererType != bgfx::RendererType::OpenGLES &&
        rendererType != bgfx::RendererType::Metal) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "[Live2D] Unsupported macOS bgfx renderer: %s",
            bgfx::getRendererName(rendererType));
        return false;
    }
#else
    if (rendererType != bgfx::RendererType::OpenGL &&
        rendererType != bgfx::RendererType::OpenGLES) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "[Live2D] Linux build requires the bgfx OpenGL renderer");
        return false;
    }
#endif

    static EngineAllocator allocator;
    static CubismFramework::Option option;
    option.LogFunction = cubismLog;
    option.LoggingLevel = CubismFramework::Option::LogLevel_Verbose;
    option.LoadFileFunction = cubismLoadFile;
    option.ReleaseBytesFunction = cubismReleaseBytes;

    if (!CubismFramework::StartUp(&allocator, &option)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[Live2D] StartUp failed");
        return false;
    }
    CubismFramework::Initialize();
    m_initialized = true;

#ifdef _WIN32
    auto* d3dPath = new D3D11NativeRenderPath();
    if (!d3dPath->init(1280, 720)) {
        delete d3dPath;
        shutdown();
        return false;
    }
    delete m_renderPath;
    m_renderPath = d3dPath;
#else
#ifdef __APPLE__
    if (rendererType == bgfx::RendererType::Metal) {
        auto* metalPath = new MetalNativeRenderPath();
        if (!metalPath->init(1280, 720)) {
            delete metalPath;
            shutdown();
            return false;
        }
        delete m_renderPath;
        m_renderPath = metalPath;
    } else
#endif
    {
        ILive2DRenderPath* glPath = new OpenGLSharedRenderPath();
        if (!glPath->init(1280, 720)) {
            delete glPath;
            auto* readbackPath = new OpenGLReadbackRenderPath();
            if (!readbackPath->init(1280, 720)) {
                delete readbackPath;
                shutdown();
                return false;
            }
            glPath = readbackPath;
        }
        delete m_renderPath;
        m_renderPath = glPath;
    }
#endif

    SDL_Log("[Live2D] CubismFramework 5 initialized (render path: %s)", m_renderPath->name());
    return true;
}

Live2DBackend::~Live2DBackend() = default;

void Live2DBackend::shutdown() {
    m_models.clear();
    if (m_renderPath) {
        m_renderPath->shutdown();
        delete m_renderPath;
        m_renderPath = nullptr;
    }
    if (m_initialized) {
        CubismFramework::Dispose();
        m_initialized = false;
    }
    m_deviceReady = false;
}

void Live2DBackend::setRenderDevice(IRenderDevice* device) {
    m_renderDevice = device;
    m_deviceReady = (device != nullptr);
}

// ============================================================
// Model loading �?Cubism 5 API
// ============================================================
bool Live2DBackend::loadModelInternal(Live2DModel& model) {
    std::string dir = dirName(model.dir);

    // 1. Load .model3.json
    model.settingJson = readFile(model.dir);
    if (model.settingJson.empty()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[Live2D] Cannot read: %s", model.dir.c_str());
        // Note: model.setting was set to nullptr by cubismLog, but model.settingJson
        // was already consumed. We should NOT try to clean up model.setting here
        // (the null destructor guard handles it). Just bail.
        return false;
    }
    model.setting = new CubismModelSettingJson(
        reinterpret_cast<const csmByte*>(model.settingJson.data()),
        static_cast<csmSizeInt>(model.settingJson.size())
    );

    // 2. Load .moc3
    std::string mocPath = joinPath(dir, model.setting->GetModelFileName());
    model.mocData = readFile(mocPath);
    if (model.mocData.empty()) return false;

    // 3. Create user model (Live2DUserModel for protected member access)
    model.userModel = std::make_unique<Live2DUserModel>();
    model.userModel->LoadModel(
        reinterpret_cast<const csmByte*>(model.mocData.data()),
        static_cast<csmSizeInt>(model.mocData.size())
    );

    // 4. Create renderer + bgfx texture
    if (!createRenderer(model)) return false;

    // 5. Load textures
    model.textures.resize(static_cast<size_t>(model.setting->GetTextureCount()),
                          BGFX_INVALID_HANDLE);
    for (csmInt32 i = 0; i < model.setting->GetTextureCount(); ++i) {
        std::string texPath = joinPath(dir, model.setting->GetTextureFileName(i));
        auto texData = readFile(texPath);
        if (!texData.empty()) {
            // Create Cubism texture from loaded PNG data
            int w, h, comp;
            unsigned char* pixels = stbi_load_from_memory(
                reinterpret_cast<const stbi_uc*>(texData.data()),
                static_cast<int>(texData.size()), &w, &h, &comp, 4);
            if (pixels) {
#ifdef _WIN32
                // D3D11: hand the model texture to Cubism as a shader-resource view.
                if (m_renderPath) {
                    auto* d3dPath = static_cast<D3D11NativeRenderPath*>(m_renderPath);
                    ID3D11ShaderResourceView* srv = d3dPath->createModelTexture(w, h, pixels);
                    if (srv && model.renderer) {
                        static_cast<CubismRenderer_D3D11*>(model.renderer)->BindTexture(i, srv);
                    }
                }
#else
                model.textures[i] = createBgfxTexture(w, h, pixels);
#endif
                stbi_image_free(pixels);
                SDL_Log("[Live2D] Texture %d loaded: %dx%d", i, w, h);
            } else {
                SDL_Log("[Live2D] Texture %d failed to decode: %s", i, texPath.c_str());
            }
        }
    }

    // 7. Cache expressions
    if (model.setting->GetExpressionCount() > 0) {
        for (csmInt32 i = 0; i < model.setting->GetExpressionCount(); ++i) {
            std::string exprName = model.setting->GetExpressionName(i);
            std::string exprPath = joinPath(dir, model.setting->GetExpressionFileName(i));
            auto exprData = readFile(exprPath);
            if (!exprData.empty()) {
                model.expressionCache[exprName] = std::move(exprData);
            }
        }
    }

    SDL_Log("[Live2D] Model loaded: %s", model.name.c_str());
    return true;
}

bool Live2DBackend::createRenderer(Live2DModel& model) {
    if (!model.userModel) return false;

    model.userModel->CreateRenderer(model.renderWidth, model.renderHeight);
#ifdef _WIN32
    model.renderer = model.userModel->GetRenderer<CubismRenderer_D3D11>();
#else
    model.renderer = model.userModel->GetRenderer<CubismRenderer_OpenGLES2>();
#endif

    if (!model.renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[Live2D] Failed to create OpenGL renderer");
        return false;
    }

    // Create bgfx texture for output
    model.bgfxTex = bgfx::createTexture2D(
        model.renderWidth, model.renderHeight,
        false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_BLIT_DST | BGFX_SAMPLER_POINT
    );
    model.bgfxTexValid = bgfx::isValid(model.bgfxTex);

    SDL_Log("[Live2D] Renderer created (%dx%d)", model.renderWidth, model.renderHeight);
    return true;
}

// ============================================================
// Per-frame: Cubism render �?render path �?bgfx
// ============================================================
void Live2DBackend::render(float dt) {
    if (!m_renderPath) return;
    for (auto& [handle, model] : m_models) {
        if (!model->visible || !model->renderer || !model->userModel) continue;

        auto* cubismModel = model->userModel->GetModel();
        if (!cubismModel) continue;

        // Update model (motions, expressions)
        static_cast<Live2DUserModel*>(model->userModel.get())->motionManager()->UpdateMotion(cubismModel, dt);
        static_cast<Live2DUserModel*>(model->userModel.get())->expressionManager()->UpdateMotion(cubismModel, dt);
        // Recompute model vertices/deformations before drawing (csmUpdateModel).
        cubismModel->Update();

        // Cubism render �?bgfx (via pluggable render path)
        m_renderPath->beginFrame(static_cast<CubismRenderer*>(model->renderer));
        m_renderPath->endFrame(static_cast<CubismRenderer*>(model->renderer), model->bgfxTex);

        // Blit bgfx texture to screen
        if (m_renderDevice && model->bgfxTexValid) {
            m_renderDevice->blitTexture(0, model->bgfxTex.idx,
                model->x, model->y,
                static_cast<float>(model->renderWidth)  * model->scale,
                static_cast<float>(model->renderHeight) * model->scale,
                static_cast<uint8_t>(model->opacity * 255.0f));
        }
    }
}

// ============================================================
// Motion playback �?Cubism 5 API
// ============================================================
bool Live2DBackend::playMotion(int handle, const std::string& name) {
    auto it = m_models.find(handle);
    if (it == m_models.end() || !it->second->userModel || !it->second->setting) return false;

    auto& model = *it->second;

    auto mit = model.motionCache.find(name);
    if (mit == model.motionCache.end()) {
        for (auto& [key, data] : model.motionCache) {
            if (key.find(name) != std::string::npos) { mit = model.motionCache.find(key); break; }
        }
    }
    if (mit == model.motionCache.end()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[Live2D] Motion not found: %s", name.c_str());
        return false;
    }

    auto& data = mit->second;
    auto* motion = model.userModel->LoadMotion(
        reinterpret_cast<const csmByte*>(data.data()),
        static_cast<csmSizeInt>(data.size()),
        name.c_str(),
        nullptr, nullptr,
        model.setting
    );
    if (!motion) return false;

    static_cast<Live2DUserModel*>(model.userModel.get())->motionManager()->StartMotion(motion, false);
    return true;
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
    auto* expression = model.userModel->LoadExpression(
        reinterpret_cast<const csmByte*>(data.data()),
        static_cast<csmSizeInt>(data.size()),
        name.c_str()
    );
    if (!expression) return;

    static_cast<Live2DUserModel*>(model.userModel.get())->expressionManager()->StartMotion(expression, false);
}

// ============================================================
// Parameter �?Cubism 5 API
// ============================================================
void Live2DBackend::setParameter(int handle, const std::string& param, float value) {
    auto it = m_models.find(handle);
    if (it == m_models.end() || !it->second->userModel) return;
    auto* cubismModel = it->second->userModel->GetModel();
    if (!cubismModel) return;
    auto* rawModel = cubismModel->GetModel();
    if (!rawModel) return;

    csmInt32 count = csmGetParameterCount(rawModel);
    const char** ids = csmGetParameterIds(rawModel);
    float* values = csmGetParameterValues(rawModel);
    for (csmInt32 i = 0; i < count; ++i) {
        if (ids[i] && param == ids[i]) {
            values[i] = value;
            return;
        }
    }
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

} // namespace Caesura

#endif
