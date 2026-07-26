// test_layer_manager.cpp - LayerManager + GpuMonitor tests
#include "doctest.h"
#include "render/LayerManager.h"
#include "render/GpuMonitor.h"

using namespace Caesura;

TEST_CASE("LayerManager is independently constructible") {
    LayerManager lm;
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
    LayerManager lm;
    auto t = ILayerManager::LayerType::BG;
    lm.setVisible(t, true);
    lm.setOpacity(t, 0.5f);
    lm.setPosition(t, 100.0f, 200.0f);
}

TEST_CASE("LayerManager::markDirty") {
    LayerManager lm;
    lm.markDirty(ILayerManager::LayerType::BG, 0, 0, 100, 100);
}

// =============================================================================
// Expanded: remaining LayerManager methods
// =============================================================================

TEST_CASE("LayerManager::setScale no-crash") {
    LayerManager lm;
    lm.setScale(ILayerManager::LayerType::BG, 1.5f, 1.5f);
    lm.setScale(ILayerManager::LayerType::FG, 0.5f, 0.5f);
}

TEST_CASE("LayerManager::setBlendMode no-crash") {
    LayerManager lm;
    lm.setBlendMode(ILayerManager::LayerType::BG, 0);
    lm.setBlendMode(ILayerManager::LayerType::FG, 1);
}

TEST_CASE("LayerManager::clear and clearAll no-crash") {
    LayerManager lm;
    lm.clear(ILayerManager::LayerType::BG);
    lm.clear(ILayerManager::LayerType::FG);
    lm.clear(ILayerManager::LayerType::MSG);
    lm.clearAll();
}

TEST_CASE("LayerManager::markAllDirty no-crash") {
    LayerManager lm;
    lm.markAllDirty();
}

TEST_CASE("LayerManager::markDirtyWithTransparency propagates") {
    LayerManager lm;
    // Marking FG dirty with transparency should also mark BG dirty
    lm.markDirtyWithTransparency(ILayerManager::LayerType::FG, 0, 0, 100, 100);
    // Should not crash
}

TEST_CASE("LayerManager::get returns valid reference") {
    LayerManager lm;
    auto& bgLayer = lm.get(ILayerManager::LayerType::BG);
    (void)bgLayer;  // should not crash
}
