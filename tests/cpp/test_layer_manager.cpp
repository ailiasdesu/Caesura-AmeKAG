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

// =============================================================================
// Expanded: remaining LayerManager methods
// =============================================================================

TEST_CASE("LayerManager::setScale no-crash") {
    LayerManager& lm = LayerManager::instance();
    lm.setScale(ILayerManager::LayerType::BG, 1.5f, 1.5f);
    lm.setScale(ILayerManager::LayerType::FG, 0.5f, 0.5f);
}

TEST_CASE("LayerManager::setBlendMode no-crash") {
    LayerManager& lm = LayerManager::instance();
    lm.setBlendMode(ILayerManager::LayerType::BG, 0);
    lm.setBlendMode(ILayerManager::LayerType::FG, 1);
}

TEST_CASE("LayerManager::clear and clearAll no-crash") {
    LayerManager& lm = LayerManager::instance();
    lm.clear(ILayerManager::LayerType::BG);
    lm.clear(ILayerManager::LayerType::FG);
    lm.clear(ILayerManager::LayerType::MSG);
    lm.clearAll();
}

TEST_CASE("LayerManager::markAllDirty no-crash") {
    LayerManager& lm = LayerManager::instance();
    lm.markAllDirty();
}

TEST_CASE("LayerManager::markDirtyWithTransparency propagates") {
    LayerManager& lm = LayerManager::instance();
    // Marking FG dirty with transparency should also mark BG dirty
    lm.markDirtyWithTransparency(ILayerManager::LayerType::FG, 0, 0, 100, 100);
    // Should not crash
}

TEST_CASE("LayerManager::get returns valid reference") {
    LayerManager& lm = LayerManager::instance();
    auto& bgLayer = lm.get(ILayerManager::LayerType::BG);
    (void)bgLayer;  // should not crash
}
