extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "NullMiniGameBackend.h"
#include <cstdio>

namespace Caesura {

bool NullMiniGameBackend::init() {
    printf("[MiniGame] Null backend initialized (3D mini-game support reserved, not yet implemented).\n");
    return true;
}

void NullMiniGameBackend::shutdown() {
    printf("[MiniGame] Null backend shut down.\n");
    m_active = false;
}

uint32_t NullMiniGameBackend::loadScene(const std::string& path) {
    printf("[MiniGame] Reserved: loadScene('%s') ¡ú handle %u (no-op)\n",
           path.c_str(), m_nextSceneHandle);
    return m_nextSceneHandle++;
}

void NullMiniGameBackend::unloadScene(uint32_t sceneHandle) {
    printf("[MiniGame] Reserved: unloadScene(%u) (no-op)\n", sceneHandle);
}

void NullMiniGameBackend::enter(uint32_t sceneHandle) {
    printf("[MiniGame] Reserved: enter(scene=%u) (no-op)\n", sceneHandle);
    m_active = true;
}

void NullMiniGameBackend::leave() {
    printf("[MiniGame] Reserved: leave() (no-op)\n");
    m_active = false;
}

bool NullMiniGameBackend::isActive() const { return m_active; }

bool NullMiniGameBackend::update(float deltaTime) {
    (void)deltaTime;
    // No-op: return true to keep mini-game running until leave() is called
    return true;
}

void NullMiniGameBackend::render() {
    // No-op: no 3D geometry to submit
}

bool NullMiniGameBackend::processEvent(const void* sdlEvent) {
    (void)sdlEvent;
    return false;  // mini-game doesn't consume any input
}

int NullMiniGameBackend::luaCall(lua_State* L, const char* method) {
    // Reserved for future Lua-to-mini-game RPC
    printf("[MiniGame] Reserved: luaCall('%s') (no-op)\n", method);
    lua_pushboolean(L, 1);
    return 1;
}

} // namespace Caesura