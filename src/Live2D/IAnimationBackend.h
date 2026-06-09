#pragma once
#include <string>
#include <cstdint>

namespace Caesura {

// Abstract animation backend — Live2D, Spine, Rive, etc.
// Default: NullAnimationBackend (no-op).

class IAnimationBackend {
public:
    virtual ~IAnimationBackend() = default;

    virtual bool init() = 0;
    virtual void shutdown() = 0;

    // Model lifecycle
    virtual int  loadModel(const std::string& path, const std::string& name) = 0;
    virtual void unloadModel(int handle) = 0;
    virtual bool isLoaded(int handle) const = 0;

    // Rendering
    virtual void showModel(int handle, float x, float y, float scale) = 0;
    virtual void hideModel(int handle) = 0;
    virtual void setOpacity(int handle, float opacity) = 0;
    virtual void render(float dt) = 0;

    // Animation control
    virtual bool playMotion(int handle, const std::string& name) = 0;
    virtual void setExpression(int handle, const std::string& name) = 0;
    virtual void setParameter(int handle, const std::string& param, float value) = 0;

    virtual const char* name() const = 0;
};

} // namespace Caesura
