// test_live2d.cpp — Live2D/Animation backend tests
#include "doctest.h"
#include "live2d/api/IAnimationBackend.h"
#include "live2d/NullAnimationBackend.h"

using namespace Caesura;

TEST_CASE("NullAnimationBackend::init returns true") {
    NullAnimationBackend anim;
    CHECK(anim.init() == true);
}

TEST_CASE("NullAnimationBackend::name") {
    NullAnimationBackend anim;
    CHECK(std::string(anim.name()) == "NullAnimation");
}

TEST_CASE("NullAnimationBackend::model lifecycle returns safe defaults") {
    NullAnimationBackend anim;
    int handle = anim.loadModel("model.model3.json", "test_model");
    CHECK(handle == 0);  // always returns 0 (no model loaded)
    CHECK(anim.isLoaded(handle) == false);
    anim.unloadModel(handle);  // should not crash
}

TEST_CASE("NullAnimationBackend::rendering methods are no-ops") {
    NullAnimationBackend anim;
    anim.showModel(1, 100.0f, 200.0f, 1.5f);
    anim.hideModel(1);
    anim.setOpacity(1, 0.5f);
    anim.render(0.016f);
    // No crash = pass
}

TEST_CASE("NullAnimationBackend::animation control returns false/void") {
    NullAnimationBackend anim;
    CHECK(anim.playMotion(1, "idle") == false);
    anim.setExpression(1, "happy");
    anim.setParameter(1, "ParamAngleX", 30.0f);
    // No crash = pass
}

TEST_CASE("NullAnimationBackend::shutdown is idempotent") {
    NullAnimationBackend anim;
    anim.shutdown();
    anim.shutdown();  // second call should not crash
}

TEST_CASE("IAnimationBackend interface completeness") {
    // Verify the interface is usable through base pointer
    IAnimationBackend* iface = new NullAnimationBackend();
    CHECK(iface->init() == true);
    CHECK(std::string(iface->name()) == "NullAnimation");
    CHECK(iface->isLoaded(0) == false);
    iface->render(0.0f);
    iface->shutdown();
    delete iface;
}

TEST_CASE("NullAnimationBackend::loadModel with empty strings") {
    NullAnimationBackend anim;
    int handle = anim.loadModel("", "");
    CHECK(handle == 0);
    anim.unloadModel(handle);
}
