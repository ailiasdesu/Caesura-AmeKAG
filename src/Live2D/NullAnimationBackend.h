#pragma once
#include "IAnimationBackend.h"

namespace Caesura {

class NullAnimationBackend : public IAnimationBackend {
public:
    bool init() override { return true; }
    void shutdown() override {}
    int  loadModel(const std::string&, const std::string&) override { return 0; }
    void unloadModel(int) override {}
    bool isLoaded(int) const override { return false; }
    void showModel(int, float, float, float) override {}
    void hideModel(int) override {}
    void setOpacity(int, float) override {}
    void render(float) override {}
    bool playMotion(int, const std::string&) override { return false; }
    void setExpression(int, const std::string&) override {}
    void setParameter(int, const std::string&, float) override {}
    const char* name() const override { return "NullAnimation"; }
};

} // namespace Caesura
