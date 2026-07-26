#pragma once

namespace Caesura {

// ============================================================================
// IDeviceLostListener — callback interface for GPU device loss recovery
// ============================================================================
// Modules that hold GPU resources (textures, shaders, buffers, RTTs)
// register themselves with BackendRegistry to receive device loss events.
//
// onDeviceLost():  Called BEFORE bgfx::shutdown().  Destroy all GPU handles,
//                  release all bgfx resources, but preserve CPU-side data
//                  (file paths, pixel buffers, font metadata, etc.)
//
// onDeviceRestored(): Called AFTER bgfx::init() succeeds.  Recreate all GPU
//                     resources from preserved CPU-side mirrors.
//
// Both callbacks run on the main thread with Lua execution paused.
// Neither callback should call bgfx::frame() or submit draw calls.

class IDeviceLostListener {
public:
    virtual ~IDeviceLostListener() = default;
    virtual void onDeviceLost() = 0;
    virtual void onDeviceRestored() = 0;
};

} // namespace Caesura
