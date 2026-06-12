// NullAnimationBackend.cpp — PNG fallback for SDK-less environments (R2.1)
#include "NullAnimationBackend.h"
#include "../di/BackendRegistry.h"
#include "../render/api/ITextureManager.h"
#include "../render/IRenderDevice.h"
#include <cstdio>
#include <algorithm>

namespace Caesura {

bool NullAnimationBackend::isImagePath(const std::string& path) {
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower.ends_with(".png") || lower.ends_with(".jpg") ||
           lower.ends_with(".jpeg") || lower.ends_with(".bmp");
}

bool NullAnimationBackend::init() {
    m_initialized = true;
    printf("[NullAnimation] PNG fallback initialized.\n");
    return true;
}

void NullAnimationBackend::shutdown() {
    if (!m_initialized) return;
    // Release all loaded textures
    auto* tm = BackendRegistry::instance().getTextureManager();
    for (auto& [handle, sprite] : m_sprites) {
        if (sprite.textureId != 0 && tm) {
            tm->destroyTexture(sprite.textureId);
        }
    }
    m_sprites.clear();
    m_nextHandle = 1;
    m_initialized = false;
}

int NullAnimationBackend::loadModel(const std::string& path, const std::string& /*name*/) {
    if (!isImagePath(path)) {
        // Not an image format we can handle as static sprite
        return 0;
    }

    auto* tm = BackendRegistry::instance().getTextureManager();
    if (!tm) {
        printf("[NullAnimation] TextureManager not available, cannot load: %s\n", path.c_str());
        return 0;
    }

    uint32_t texId = tm->loadTexture(path);
    if (texId == 0) {
        printf("[NullAnimation] Failed to load image: %s\n", path.c_str());
        return 0;
    }

    int handle = m_nextHandle++;
    StaticSprite sprite;
    sprite.textureId = texId;
    m_sprites[handle] = sprite;
    printf("[NullAnimation] Loaded static sprite #%d: %s (tex=%u)\n", handle, path.c_str(), texId);
    return handle;
}

void NullAnimationBackend::unloadModel(int handle) {
    auto it = m_sprites.find(handle);
    if (it == m_sprites.end()) return;

    auto* tm = BackendRegistry::instance().getTextureManager();
    if (tm && it->second.textureId != 0) {
        tm->destroyTexture(it->second.textureId);
    }
    m_sprites.erase(it);
}

bool NullAnimationBackend::isLoaded(int handle) const {
    return m_sprites.find(handle) != m_sprites.end();
}

void NullAnimationBackend::showModel(int handle, float x, float y, float scale) {
    auto it = m_sprites.find(handle);
    if (it == m_sprites.end()) return;
    it->second.x = x;
    it->second.y = y;
    it->second.scale = scale;
    it->second.visible = true;
}

void NullAnimationBackend::hideModel(int handle) {
    auto it = m_sprites.find(handle);
    if (it == m_sprites.end()) return;
    it->second.visible = false;
}

void NullAnimationBackend::setOpacity(int handle, float opacity) {
    auto it = m_sprites.find(handle);
    if (it == m_sprites.end()) return;
    it->second.opacity = opacity;
}

void NullAnimationBackend::render(float /*dt*/) {
    auto* rd = BackendRegistry::instance().getRenderDevice();
    if (!rd) return;

    for (auto& [handle, sprite] : m_sprites) {
        if (!sprite.visible || sprite.textureId == 0) continue;
        // Submit static sprite via bgfx (simplified: use the render device)
        // In a real implementation, this would submit a textured quad to bgfx
        // For now, log the render attempt
    }
}

bool NullAnimationBackend::playMotion(int /*handle*/, const std::string& /*name*/) {
    return false; // No animation in null backend
}

void NullAnimationBackend::setExpression(int /*handle*/, const std::string& /*name*/) {
    // No-op in null backend
}

void NullAnimationBackend::setParameter(int /*handle*/, const std::string& /*param*/, float /*value*/) {
    // No-op in null backend
}

} // namespace Caesura
