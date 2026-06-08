#ifdef CAESURA_HAS_LIVE2D

#include "Live2DBackend.h"
// Live2D Cubism SDK headers — available after SDK download
// #include <CubismFramework.hpp>
// #include <CubismModelSettingJson.hpp>
// etc.

namespace Caesura {

struct Live2DBackend::ModelState {
    std::string path;
    std::string name;
    bool   visible = false;
    float  x = 0, y = 0, scale = 1.0f, opacity = 1.0f;
    std::string currentMotion;
    std::string currentExpression;
};

bool Live2DBackend::init() {
    // TODO: CubismFramework::Initialize()
    return true;
}

void Live2DBackend::shutdown() {
    m_models.clear();
}

int Live2DBackend::loadModel(const std::string& path, const std::string& name) {
    int handle = m_nextHandle++;
    auto state = std::make_unique<ModelState>();
    state->path = path;
    state->name = name;
    m_models[handle] = std::move(state);
    return handle;
}

void Live2DBackend::unloadModel(int handle) { m_models.erase(handle); }
bool Live2DBackend::isLoaded(int handle) const { return m_models.count(handle) > 0; }
void Live2DBackend::showModel(int handle, float x, float y, float scale) {
    auto it = m_models.find(handle);
    if (it != m_models.end()) { it->second->visible = true; it->second->x = x; it->second->y = y; it->second->scale = scale; }
}
void Live2DBackend::hideModel(int handle) { auto it = m_models.find(handle); if (it != m_models.end()) it->second->visible = false; }
void Live2DBackend::setOpacity(int handle, float opacity) { auto it = m_models.find(handle); if (it != m_models.end()) it->second->opacity = opacity; }
void Live2DBackend::render(float) { /* TODO: bgfx quad rendering */ }
bool Live2DBackend::playMotion(int handle, const std::string& name) { auto it = m_models.find(handle); if (it != m_models.end()) { it->second->currentMotion = name; return true; } return false; }
void Live2DBackend::setExpression(int handle, const std::string& name) { auto it = m_models.find(handle); if (it != m_models.end()) it->second->currentExpression = name; }
void Live2DBackend::setParameter(int handle, const std::string&, float) { /* TODO */ (void)handle; }

void registerLive2DBinding(void*) { /* TODO: Lua bindings */ }

} // namespace Caesura

#endif // CAESURA_HAS_LIVE2D
