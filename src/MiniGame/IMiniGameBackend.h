#pragma once
// ===========================================================================
// Caesura (AmeKAG) -- IMiniGameBackend (v1.0.0-alpha-3 reserved)
// ===========================================================================
// Abstract 3D mini-game backend interface.
// Reserved for future 3D mini-game scenes embedded in visual novel flow.
//
// Lifecycle:  KAG scene �� mini_game:enter �� active loop �� mini_game:leave �� KAG
//
// Thread safety:
//   - update() may be dispatched to JobSystem workers (pure CPU: physics,
//     skeleton animation, particle simulation, scene culling).
//   - render() is called on main thread only (bgfx is not thread-safe).
//   - All other methods are main-thread-only.
//
// Concrete implementations (future):
//   - BgfxMiniGameBackend  (bgfx-native 3D, most likely first impl)
//   - UnityMiniGameBackend (embedded Unity runtime via native plugin)
//   - GodotMiniGameBackend (GDExtension bridge)
// ===========================================================================

#include <string>
#include <functional>
#include <cstdint>

struct lua_State;  // forward decl: global scope, not under namespace

namespace Caesura {

class IMiniGameBackend {
public:
    virtual ~IMiniGameBackend() = default;

    // -- Lifecycle ---------------------------------------------------------
    // Called from Engine::init() after bgfx and Lua are ready.
    // Implementations allocate GPU resources here.
    virtual bool init() = 0;

    // Called from Engine::shutdown(). Release all GPU/CPU resources.
    virtual void shutdown() = 0;

    // -- Scene management (reserved) ---------------------------------------
    // Load a 3D scene definition (glTF, OBJ, or custom format).
    // Returns 0 on failure, opaque scene handle on success.
    virtual uint32_t loadScene(const std::string& path) = 0;

    // Unload a previously loaded scene. Safe to call with invalid handle.
    virtual void unloadScene(uint32_t sceneHandle) = 0;

    // -- Mode switching ----------------------------------------------------
    // Enter mini-game mode: hides KAG layers, activates 3D camera/input.
    // Lua calls this via b:mini_game("enter", scene_handle).
    virtual void enter(uint32_t sceneHandle) = 0;

    // Leave mini-game mode: stops 3D loop, restores KAG rendering.
    // Lua calls this via b:mini_game("leave").
    virtual void leave() = 0;

    // True while enter() has been called and leave() has not.
    virtual bool isActive() const = 0;

    // -- Game loop hooks ---------------------------------------------------
    // Called every frame when isActive() == true.
    //
    // update(dt): pure CPU work (physics step, animation advance, AI ticks).
    //   Safe to run on JobSystem worker threads.
    //   Returns false if the mini-game wants to trigger a transition back
    //   to KAG (e.g., game-over condition met).
    virtual bool update(float deltaTime) = 0;

    // render(): GPU submission (bgfx calls). Main thread only.
    //   Called after KAG render pass when isActive().
    virtual void render() = 0;

    // -- Input routing -----------------------------------------------------
    // Forward SDL events to the mini-game when active.
    // Returns true if the mini-game consumed the event (KAG should ignore it).
    virtual bool processEvent(const void* sdlEvent) = 0;

    // -- Lua bridge (reserved) ---------------------------------------------
    // Called from Lua: b:mini_game("call", function_name, ...)
    // The implementation pushes return values onto the Lua stack.
    // Returns number of return values.
    virtual int luaCall(lua_State* L, const char* method) = 0;

    // -- Backend identification --------------------------------------------
        // -- Renderer binding (set before init()) --
    virtual void setRenderDevice(class IRenderDevice* dev) = 0;

virtual const char* getBackendName() const = 0;
};

// ===========================================================================
// MiniGame mode constants (reserved for Lua <-> C++ protocol)
// ===========================================================================

// SDL user event codes reserved for mini-game interop
enum class MiniGameEvent : uint32_t {
    Entered     = 0x9000,  // mini-game activated �� Lua callback
    Left        = 0x9001,  // mini-game deactivated �� Lua callback
    Transition  = 0x9002,  // mini-game wants to transition back to KAG
    Error       = 0x9003,  // mini-game fatal error
};

// Reserved KAG <-> MiniGame transition types
enum class MiniGameTransition : uint8_t {
    FadeToBlack     = 0,  // fade �� load �� fade in (default)
    Instant         = 1,  // instant switch, no transition
    Portal          = 2,  // custom transition effect
};

} // namespace Caesura