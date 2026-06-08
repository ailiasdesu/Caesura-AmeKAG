#pragma once
#ifdef CAESURA_HAS_LIVE2D

#include "../IAnimationBackend.h"
#include <string>
#include <unordered_map>
#include <memory>

namespace Caesura {

class Live2DBackend : public IAnimationBackend {
public:
    bool init() override;
    void shutdown() override;

    int  loadModel(const std::string& path, const std::string& name) override;
    void unloadModel(int handle) override;
    bool isLoaded(int handle) const override;

    void showModel(int handle, float x, float y, float scale) override;
    void hideModel(int handle) override;
    void setOpacity(int handle, float opacity) override;
    void render(float dt) override;

    bool playMotion(int handle, const std::string& name) override;
    void setExpression(int handle, const std::string& name) override;
    void setParameter(int handle, const std::string& param, float value) override;

    const char* name() const override { return "Live2D"; }

private:
    struct ModelState;
    std::unordered_map<int, std::unique_ptr<ModelState>> m_models;
    int m_nextHandle = 1;
};

void registerLive2DBinding(void* luaState);

} // namespace Caesura

#endif // CAESURA_HAS_LIVE2D
