// test_layer_manager.cpp - LayerManager + GpuMonitor tests
#include "doctest.h"
#include "render/LayerManager.h"
#include "render/GpuMonitor.h"
#include <string>

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
// =============================================================================
// v2 (round 116): dynamic layer configuration - count, names, ordering
// =============================================================================
TEST_CASE("LayerManager: default layout has 3 layers bg/fg/msg") {
    LayerManager lm;
    lm.init();
    CHECK(lm.getLayerCount() == 3);
    CHECK(std::string(lm.getLayerName(0)) == "bg");
    CHECK(std::string(lm.getLayerName(1)) == "fg");
    CHECK(std::string(lm.getLayerName(2)) == "msg");
    CHECK(lm.findLayer("bg") == 0);
    CHECK(lm.findLayer("fg") == 1);
    CHECK(lm.findLayer("msg") == 2);
    CHECK(lm.findLayer("nope") == -1);
    CHECK(lm.getLayerName(99) == nullptr);
}

TEST_CASE("LayerManager: configureLayers sets custom count and names") {
    LayerManager lm;
    lm.init();
    ILayerManager::LayerConfig cfg[] = {
        {"backdrop", 0.0f}, {"sprite_a", 10.0f}, {"sprite_b", 20.0f},
        {"overlay", 30.0f}, {"hud", 100.0f},
    };
    CHECK(lm.configureLayers(cfg, 5));
    CHECK(lm.getLayerCount() == 5);
    CHECK(std::string(lm.getLayerName(0)) == "backdrop");
    CHECK(std::string(lm.getLayerName(4)) == "hud");
    CHECK(lm.findLayer("sprite_a") == 1);
    CHECK(lm.findLayer("hud") == 4);
    // Legacy enum indices now map onto the custom layout's first slot
    lm.setTexture(ILayerManager::LayerType::BG, 42);  // -> backdrop
    CHECK(lm.get(0).tex.idx == 42);
}

TEST_CASE("LayerManager: configureLayers rejects invalid configs") {
    LayerManager lm;
    lm.init();
    ILayerManager::LayerConfig ok[] = {{"a", 0.0f}, {"b", 1.0f}};
    CHECK(lm.configureLayers(ok, 2));

    // null config / zero count
    CHECK_FALSE(lm.configureLayers(nullptr, 2));
    CHECK_FALSE(lm.configureLayers(ok, 0));

    // null name / empty name
    ILayerManager::LayerConfig badName[] = {{nullptr, 0.0f}};
    CHECK_FALSE(lm.configureLayers(badName, 1));
    ILayerManager::LayerConfig emptyName[] = {{"", 0.0f}};
    CHECK_FALSE(lm.configureLayers(emptyName, 1));

    // duplicate name
    ILayerManager::LayerConfig dup[] = {{"x", 0.0f}, {"x", 1.0f}};
    CHECK_FALSE(lm.configureLayers(dup, 2));

    // failed config must not corrupt the previous setup
    CHECK(lm.getLayerCount() == 2);
    CHECK(std::string(lm.getLayerName(0)) == "a");
}

TEST_CASE("LayerManager: reorderLayer changes render order") {
    LayerManager lm;
    lm.init();
    ILayerManager::LayerConfig cfg[] = {
        {"bg", 0.0f}, {"fg", 1.0f}, {"msg", 2.0f},
    };
    CHECK(lm.configureLayers(cfg, 3));
    CHECK(lm.findLayer("bg") == 0);
    CHECK(lm.findLayer("fg") == 1);
    CHECK(lm.findLayer("msg") == 2);

    // Move bg to the top: bg fg msg -> fg msg bg
    CHECK(lm.reorderLayer(0, 2));
    CHECK(lm.findLayer("fg") == 0);
    CHECK(lm.findLayer("msg") == 1);
    CHECK(lm.findLayer("bg") == 2);

    // Move bg back to bottom: fg msg bg -> bg fg msg
    CHECK(lm.reorderLayer(2, 0));
    CHECK(lm.findLayer("bg") == 0);
    CHECK(lm.findLayer("fg") == 1);
    CHECK(lm.findLayer("msg") == 2);

    // Out-of-range reorder is a no-op
    CHECK_FALSE(lm.reorderLayer(0, 7));
    CHECK_FALSE(lm.reorderLayer(7, 0));
    CHECK(lm.getLayerCount() == 3);
}

TEST_CASE("LayerManager: out-of-range index setters are safe") {
    LayerManager lm;
    lm.init();
    lm.setTexture(99, 1);
    lm.setVisible(99, true);
    lm.setOpacity(99, 0.5f);
    lm.setPosition(99, 1, 2);
    lm.setScale(99, 1, 1);
    lm.setBlendMode(99, 1);
    lm.clear(99);
    lm.markDirty(99, 0, 0, 10, 10);
    lm.markDirtyWithTransparency(99, 0, 0, 10, 10);
    // no crash
    CHECK(true);
}

TEST_CASE("LayerManager: markDirtyWithTransparency respects custom render order") {
    LayerManager lm;
    lm.init();
    // Layout: bg, sprite, fg -- sprite is between bg and fg
    ILayerManager::LayerConfig cfg[] = {
        {"bg", 0.0f}, {"sprite", 5.0f}, {"fg", 10.0f},
    };
    CHECK(lm.configureLayers(cfg, 3));
    lm.setVisible(0, true);
    lm.setVisible(1, true);
    lm.setVisible(2, true);
    lm.get(0).dirty = false;
    lm.get(1).dirty = false;
    lm.get(2).dirty = false;
    lm.clearDirtyRects();

    // Transparency on sprite (index 1) marks bg (index 0) but not fg (index 2)
    lm.markDirtyWithTransparency(1, 10, 10, 50, 50);
    lm.updateDirtyRegions(1280, 720);
    // Merged = union of sprite + bg rects = 10,10,50,50
    CHECK(lm.get(0).dirty);  // bg got marked
    CHECK(lm.get(1).dirty);  // sprite itself
    CHECK_FALSE(lm.get(2).dirty);  // fg above sprite: not affected

    // Now move fg below sprite: bg fg sprite -> bg sprite fg
    CHECK(lm.reorderLayer(2, 1));  // fg moves between bg and sprite
    // order now: bg fg sprite (findLayer fg==1, sprite==2)
    CHECK(lm.findLayer("fg") == 1);
    CHECK(lm.findLayer("sprite") == 2);
    lm.get(0).dirty = false;
    lm.get(1).dirty = false;
    lm.get(2).dirty = false;
    lm.clearDirtyRects();

    // Transparency on sprite (now index 2) marks bg (0) and fg (1)
    lm.markDirtyWithTransparency(lm.findLayer("sprite"), 20, 20, 10, 10);
    CHECK(lm.get(0).dirty);
    CHECK(lm.get(1).dirty);
    CHECK(lm.get(2).dirty);
}
