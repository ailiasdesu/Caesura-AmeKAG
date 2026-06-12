// test_layer_manager.cpp - LayerManager + GpuMonitor tests
#include "doctest.h"
#include "render/LayerManager.h"
#include "render/GpuMonitor.h"

using namespace Caesura;

TEST_CASE("LayerManager::singleton accessible") {
    LayerManager& lm = LayerManager::instance();
    (void)lm;
}

TEST_CASE("GpuMonitor::metrics default gpuTimeMs") {
    GpuMonitor gm;
    CHECK(gm.metrics().gpuTimeMs >= 0.0);
}

TEST_CASE("GpuMonitor::current quality is HIGH by default") {
    GpuMonitor gm;
    CHECK(gm.currentQuality() == GpuQuality::HIGH);
}

TEST_CASE("LayerManager::setVisible/setOpacity/setPosition no-crash") {
    LayerManager& lm = LayerManager::instance();
    auto t = ILayerManager::LayerType::BG;
    lm.setVisible(t, true);
    lm.setOpacity(t, 0.5f);
    lm.setPosition(t, 100.0f, 200.0f);
}

TEST_CASE("LayerManager::markDirty") {
    LayerManager& lm = LayerManager::instance();
    lm.markDirty(ILayerManager::LayerType::BG, 0, 0, 100, 100);
}
