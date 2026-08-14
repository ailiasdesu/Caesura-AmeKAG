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
// -----------------------------------------------------------------------------
// DirtyRect pure math + scissor decision (G8): merge()/area()/empty() and the
// 75%-of-frame scissor heuristic are pure logic -- no GPU needed.
// -----------------------------------------------------------------------------

TEST_CASE("DirtyRect: empty rects are inert in merge") {
    DirtyRect a{10, 10, 100, 100};
    DirtyRect empty{};
    DirtyRect merged = a;
    merged.merge(empty);       // merging an empty rect changes nothing
    CHECK(merged.x == 10);
    CHECK(merged.y == 10);
    CHECK(merged.w == 100);
    CHECK(merged.h == 100);

    DirtyRect into{};
    into.merge(a);             // merging into empty adopts the rect
    CHECK(into.x == 10);
    CHECK(into.y == 10);
    CHECK(into.w == 100);
    CHECK(into.h == 100);
}

TEST_CASE("DirtyRect: merge grows to bounding box") {
    DirtyRect a{10, 10, 100, 100};   // x 10..110, y 10..110
    DirtyRect b{50, 50, 200, 40};    // x 50..250, y 50..90
    DirtyRect m = a;
    m.merge(b);
    CHECK(m.x == 10);
    CHECK(m.y == 10);
    CHECK(m.w == 240);               // 250 - 10
    CHECK(m.h == 100);               // 110 - 10
    CHECK(m.area() == 240u * 100u);
}

TEST_CASE("DirtyRect: merge is commutative in bounding box") {
    DirtyRect a{10, 20, 30, 40};
    DirtyRect b{100, 5, 15, 300};
    DirtyRect m1 = a; m1.merge(b);
    DirtyRect m2 = b; m2.merge(a);
    CHECK(m1.x == m2.x);
    CHECK(m1.y == m2.y);
    CHECK(m1.w == m2.w);
    CHECK(m1.h == m2.h);
}

TEST_CASE("DirtyRect: adjacent rects merge without gaps") {
    DirtyRect a{0, 0, 100, 100};
    DirtyRect b{100, 0, 100, 100};   // shares the right edge
    DirtyRect m = a;
    m.merge(b);
    CHECK(m.x == 0);
    CHECK(m.y == 0);
    CHECK(m.w == 200);
    CHECK(m.h == 100);
}

TEST_CASE("DirtyRect: merge stays correct near uint16 limits") {
    // Regression: x+w can exceed 65535; the old code cast to uint16_t early,
    // wrapping the max and producing a wrong (inverted) rect.
    DirtyRect a{60000, 60000, 6000, 6000};   // right edge 66000 > 65535
    DirtyRect b{100, 100, 50, 50};
    DirtyRect m = a;
    m.merge(b);
    // Bounding box: x 100..66000 -> clamped to 100..65535 (representable).
    CHECK(m.x == 100);
    CHECK(m.y == 100);
    CHECK(uint32_t(m.x) + m.w == 65535u);    // right edge clamped, NOT wrapped
    CHECK(uint32_t(m.y) + m.h == 65535u);
    CHECK(m.w == 65435);                     // 65535 - 100
    CHECK(m.h == 65435);
}

TEST_CASE("DirtyRect: area is width*height in 32-bit") {
    DirtyRect a{0, 0, 60000, 60000};
    CHECK(a.area() == 60000u * 60000u);      // 3.6e9, no 16-bit wrap
}

TEST_CASE("Scissor decision: empty dirty -> full frame redraw (no scissor)") {
    CHECK_FALSE(LayerManager::shouldUseScissorFor(DirtyRect{}, 1280, 720));
}

TEST_CASE("Scissor decision: small dirty area uses scissor") {
    // 100x100 = 10000 px vs frame 1280*720 = 921600; 10000 < 75%.
    DirtyRect r{0, 0, 100, 100};
    CHECK(LayerManager::shouldUseScissorFor(r, 1280, 720));
}

TEST_CASE("Scissor decision: exactly 75% still uses scissor (<=)") {
    // Frame 1280x720 -> 75% = 691200 px. 720x960 = 691200.
    DirtyRect r{0, 0, 720, 960};
    CHECK(LayerManager::shouldUseScissorFor(r, 1280, 720));
}

TEST_CASE("Scissor decision: over 75% redraws the whole frame") {
    // 1000x1000 = 1e6 px > 691200 (75% of 1280x720).
    DirtyRect r{0, 0, 1000, 1000};
    CHECK_FALSE(LayerManager::shouldUseScissorFor(r, 1280, 720));
}

TEST_CASE("Scissor decision: zero-size frame never scissor-crashes") {
    DirtyRect r{0, 0, 100, 100};
    // Degenerate frame: frameArea 0 -> 75% = 0 -> dirty 10000 > 0 -> false.
    CHECK_FALSE(LayerManager::shouldUseScissorFor(r, 0, 0));
}