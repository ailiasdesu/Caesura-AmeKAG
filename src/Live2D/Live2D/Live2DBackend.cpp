#ifdef CAESURA_HAS_LIVE2D

#include "Live2DBackend.h"

// GL MUST come before Cubism headers (CSM_TARGET_WIN_GL needs GL types)
#ifdef _WIN32
#include <GL/glew.h>
#ifdef _MSC_VER
#pragma comment(lib, "opengl32.lib")
#endif
#endif

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
#include "OpenGLReadbackRenderPath.h"
#include "D3D11NativeRenderPath.h"
#include "OpenGLSharedRenderPath.h"
#include "MetalNativeRenderPath.h"

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
using namespace Live2D::Cubism::Core;

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
}

// ============================================================
// Live2DModel destructor
// ============================================================
Live2DBackend::Live2DModel::~Live2DModel() {
    if (setting) {
        delete setting;
        setting = nullptr;
    }
    if (renderer) {
        CubismRenderer::Delete(static_cast<CubismRenderer*>(renderer));
        renderer = nullptr;
    }
    userModel.reset();
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

    // Select render path based on bgfx backend
    const auto rendererType = bgfx::getRendererType();
    switch (rendererType) {
    case bgfx::RendererType::Direct3D11: {
        auto* path = new D3D11NativeRenderPath();
        if (path->init(1280, 720)) { delete m_renderPath; m_renderPath = path; }
        else { delete path; goto fallback; }
        break;
    }
    case bgfx::RendererType::OpenGL:
    case bgfx::RendererType::OpenGLES: {
        auto* path = new OpenGLSharedRenderPath();
        if (path->init(1280, 720)) { delete m_renderPath; m_renderPath = path; }
        else { delete path; goto fallback; }
        break;
    }
    case bgfx::RendererType::Metal: {
        // Try Metal first (stub until macOS developer validates)
        auto* path = new MetalNativeRenderPath();
        if (path->init(1280, 720)) { delete m_renderPath; m_renderPath = path; }
        else { delete path; goto fallback; }
        break;
    }
    default:
    fallback:
        m_renderPath = new OpenGLReadbackRenderPath();
        m_renderPath->init(1280, 720);
        break;
    }

    m_initialized = true;
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
                model.textures[i] = ILive2DRenderPath::createTexture(w, h, pixels);
                stbi_image_free(pixels);
                SDL_Log("[Live2D] Texture %d loaded: %dx%d", i, w, h);
            } else {
                SDL_Log("[Live2D] Texture %d failed to decode: %s", i, texPath.c_str());
            if (!motionData.empty()) {
                model.motionCache[motionPath] = std::move(motionData);
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

        // Cubism render �?bgfx (via pluggable render path)
        m_renderPath->beginFrame(static_cast<CubismRenderer*>(model->renderer));
        m_renderPath->endFrame(static_cast<CubismRenderer*>(model->renderer), model->bgfxTex);

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

// ============================================================
// Lua bindings
// ============================================================
static Live2DBackend* g_live2d = nullptr;

static int lua_l2d_load_model(lua_State* L) {
    if (!g_live2d) { lua_pushinteger(L, -1); return 1; }
    int handle = g_live2d->loadModel(luaL_checkstring(L, 1), luaL_optstring(L, 2, "model"));
    lua_pushinteger(L, handle);
    return 1;
}

static int lua_l2d_unload_model(lua_State* L) { if (g_live2d) g_live2d->unloadModel((int)luaL_checkinteger(L, 1)); return 0; }
static int lua_l2d_show_model(lua_State* L) { if (g_live2d) g_live2d->showModel((int)luaL_checkinteger(L, 1), (float)luaL_optnumber(L, 2, 0), (float)luaL_optnumber(L, 3, 0), (float)luaL_optnumber(L, 4, 1.0)); return 0; }
static int lua_l2d_hide_model(lua_State* L) { if (g_live2d) g_live2d->hideModel((int)luaL_checkinteger(L, 1)); return 0; }
static int lua_l2d_set_opacity(lua_State* L) { if (g_live2d) g_live2d->setOpacity((int)luaL_checkinteger(L, 1), (float)luaL_checknumber(L, 2)); return 0; }

static int lua_l2d_play_motion(lua_State* L) {
    bool ok = g_live2d && g_live2d->playMotion((int)luaL_checkinteger(L, 1), luaL_checkstring(L, 2));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int lua_l2d_set_expression(lua_State* L) { if (g_live2d) g_live2d->setExpression((int)luaL_checkinteger(L, 1), luaL_checkstring(L, 2)); return 0; }
static int lua_l2d_set_parameter(lua_State* L) { if (g_live2d) g_live2d->setParameter((int)luaL_checkinteger(L, 1), luaL_checkstring(L, 2), (float)luaL_checknumber(L, 3)); return 0; }
static int lua_l2d_is_loaded(lua_State* L) { lua_pushboolean(L, g_live2d && g_live2d->isLoaded((int)luaL_checkinteger(L, 1)) ? 1 : 0); return 1; }

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

void Live2DBackend_setGlobal(Live2DBackend* backend) { g_live2d = backend; }

} // namespace Caesura

#endif
