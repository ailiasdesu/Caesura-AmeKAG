#include "doctest.h"

#include "di/BackendRegistry.h"
#include "live2d/NullAnimationBackend.h"
#include "live2d/PathConfinement.h"
#include "live2d/api/IAnimationBackend.h"
#include "render/api/IRenderDevice.h"
#include "render/api/ITextureManager.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

using namespace Caesura;

namespace {

class FakeTextureManager final : public ITextureManager {
public:
    bool initialize() override { return true; }
    bool initialize(bool) override { return true; }
    void shutdown() override {}
    void setDevMode(bool) override {}

    uint32_t loadTexture(const std::string& path) override {
        loadedPaths.push_back(path);
        if (failLoads) return 0;
        const uint32_t id = nextId++;
        liveIds.insert(id);
        return id;
    }

    uint32_t loadTextureFromMemory(const uint8_t*, uint32_t,
                                   const std::string&) override {
        return 0;
    }
    uint32_t loadTextureFromRGBA(const uint8_t*, uint16_t, uint16_t,
                                 const std::string&) override {
        return 0;
    }
    uint32_t createSolidTexture(uint8_t, uint8_t, uint8_t, uint8_t) override {
        return 0;
    }
    uint32_t getPlaceholderTexture() override { return 0; }

    void destroyTexture(uint32_t id) override {
        destroyedIds.push_back(id);
        liveIds.erase(id);
    }

    uint32_t getTextureHandle(uint32_t id) const override {
        return isValid(id) ? rawHandleBase + id : 0;
    }

    void getTextureSizeById(uint32_t id, uint16_t& width,
                            uint16_t& height) const override {
        if (!isValid(id)) {
            width = 0;
            height = 0;
            return;
        }
        width = textureWidth;
        height = textureHeight;
    }

    bool isValid(uint32_t id) const override {
        return liveIds.contains(id);
    }

    uint64_t totalTextureBytes() const override { return 0; }
    bool checkBudget(uint32_t, uint16_t, uint16_t) override { return true; }
    void trackTexture(uint32_t, uint64_t) override {}
    void untrackTexture(uint32_t) override {}

    bool failLoads = false;
    uint32_t nextId = 11;
    uint32_t rawHandleBase = 80;
    uint16_t textureWidth = 100;
    uint16_t textureHeight = 200;
    std::vector<std::string> loadedPaths;
    std::vector<uint32_t> destroyedIds;
    std::unordered_set<uint32_t> liveIds;
};

class RecordingRenderDevice final : public IRenderDevice {
public:
    struct Blit {
        uint16_t view = 0;
        uint32_t texture = 0;
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        uint8_t opacity = 0;
    };

    bool init(void*, int width, int height) override {
        backbufferWidth = width;
        backbufferHeight = height;
        return true;
    }
    bool isInitialized() const override { return true; }
    void beginShutdown() override {}
    void shutdown() override {}
    void flushAllRTT() override {}
    void beginFrame() override {}
    void endFrame() override {}
    void commit_frame() override {}
    void advanceFrame() override {}
    void setScreenOffset(int, int) override {}
    void setViewRect(uint16_t, uint16_t, uint16_t, uint16_t, uint16_t) override {}
    void setViewClear(uint16_t, uint16_t, uint32_t, float, uint8_t) override {}
    void touch(uint16_t) override {}
    ViewportHandle createRenderTarget(int, int) override { return {}; }
    void destroyRenderTarget(ViewportHandle) override {}
    void blitViewport(ViewportHandle, uint16_t, float, float, float, float) override {}
    RenderTextureHandle getViewportTexture(ViewportHandle) override { return {}; }
    int getBackbufferWidth() const override { return backbufferWidth; }
    int getBackbufferHeight() const override { return backbufferHeight; }
    void resize(int width, int height) override {
        backbufferWidth = width;
        backbufferHeight = height;
    }
    void blitTexture(uint16_t view, uint32_t texture, float x, float y,
                     float width, float height, uint8_t opacity) override {
        blits.push_back({view, texture, x, y, width, height, opacity});
    }
    void stretchBlt(uint16_t, uint32_t, float, float, float, float,
                    uint32_t, float, float, float, float, int) override {}
    void affineBlt(uint16_t, uint32_t, float, float, float, float,
                   uint32_t, float, float, float, float,
                   const float[6]) override {}
    void beginBatch() override {}
    void flushBatch() override {}
    void setDebugName(uint16_t, const std::string&) override {}
    void drawDebugOverlay(const std::string&) override {}
    bool requestScreenshot(const std::string&) override { return false; }
    bool recoverDevice(void*, int width, int height) override {
        resize(width, height);
        return true;
    }
    void flagDeviceLost() override {}
    bool consumeDeviceLost() override { return false; }
    void renderText(uint16_t, const std::string&, float, float,
                    uint8_t, uint8_t, uint8_t, uint8_t) override {}
    void renderRuby(uint16_t, const std::string&, const std::string&, float, float,
                    uint8_t, uint8_t, uint8_t, uint8_t) override {}
    void setFont(int) override {}
    bool loadTTF(const char*, float) override { return false; }
    float textLineHeight() const override { return 0.0f; }
    void submitBlend(uint16_t, RenderTextureHandle, RenderTextureHandle, int,
                     float, float, float) override {}
    void submitTransition(uint16_t, RenderTextureHandle, RenderTextureHandle,
                          RenderTextureHandle, int, float) override {}
    void submitVFX(uint16_t, RenderTextureHandle, int, float, float, float,
                   float, float, float, float) override {}
    void fillViewport(ViewportHandle, uint8_t, uint8_t, uint8_t, uint8_t) override {}
    bool setColorFilter(ColorFilterPreset) override { return true; }
    RenderUniformHandle getDefaultSampler() const override { return {}; }
    RenderProgramHandle getFallbackProgram() const override { return {}; }
    const char* getBackendName() const override { return "RecordingRender"; }
    RenderRuntimeInfo getRuntimeInfo() const override {
        return {getBackendName(), backbufferWidth, backbufferHeight, 0, true};
    }
    bool setPreferredBackend(const char*) override { return false; }

    int backbufferWidth = 1280;
    int backbufferHeight = 720;
    std::vector<Blit> blits;
};

class RegistryScope final {
public:
    RegistryScope(ITextureManager* textureManager, IRenderDevice* renderDevice)
        : registry(BackendRegistry::instance())
        , previousTextureManager(registry.getTextureManager())
        , previousRenderDevice(registry.getRenderDevice()) {
        registry.setTextureManager(textureManager);
        registry.setRenderDevice(renderDevice);
    }

    ~RegistryScope() {
        registry.setTextureManager(previousTextureManager);
        registry.setRenderDevice(previousRenderDevice);
    }

private:
    BackendRegistry& registry;
    ITextureManager* previousTextureManager = nullptr;
    IRenderDevice* previousRenderDevice = nullptr;
};

struct NullAnimationFixture {
    FakeTextureManager textures;
    RecordingRenderDevice renderer;
    RegistryScope registry{&textures, &renderer};
    NullAnimationBackend animation;
};

} // namespace

TEST_CASE("NullAnimationBackend requires init and recognizes static images") {
    NullAnimationFixture fixture;

    CHECK(fixture.animation.loadModel("hero.png", "hero") == 0);
    CHECK(fixture.textures.loadedPaths.empty());
    CHECK(fixture.animation.init());
    CHECK(fixture.animation.init());
    CHECK(std::string(fixture.animation.name()) == "NullAnimation+PNG");

    const int handle = fixture.animation.loadModel("hero.PNG", "hero");
    CHECK(handle == 1);
    CHECK(fixture.animation.isLoaded(handle));
    CHECK(fixture.textures.loadedPaths == std::vector<std::string>{"hero.PNG"});

    CHECK(fixture.animation.loadModel("hero.model3.json", "hero") == 0);
    CHECK(fixture.animation.loadModel("", "hero") == 0);
    CHECK(fixture.textures.loadedPaths.size() == 1);
    fixture.animation.shutdown();
}

TEST_CASE("NullAnimationBackend exposes load failures without allocating handles") {
    NullAnimationFixture fixture;
    fixture.textures.failLoads = true;
    REQUIRE(fixture.animation.init());

    CHECK(fixture.animation.loadModel("missing.png", "missing") == 0);
    CHECK_FALSE(fixture.animation.isLoaded(0));
    CHECK(fixture.textures.destroyedIds.empty());
    fixture.animation.shutdown();
}

TEST_CASE("NullAnimationBackend rejects and releases textures without dimensions") {
    NullAnimationFixture fixture;
    fixture.textures.textureWidth = 0;
    REQUIRE(fixture.animation.init());

    CHECK(fixture.animation.loadModel("empty.png", "empty") == 0);
    CHECK_FALSE(fixture.animation.isLoaded(1));
    CHECK(fixture.textures.destroyedIds == std::vector<uint32_t>{11});
    CHECK(fixture.textures.liveIds.empty());
    fixture.animation.shutdown();
    CHECK(fixture.textures.destroyedIds.size() == 1);
}

TEST_CASE("NullAnimationBackend stays optional without a texture service") {
    RecordingRenderDevice renderer;
    RegistryScope registry(nullptr, &renderer);
    NullAnimationBackend animation;

    CHECK(animation.init());
    CHECK(animation.loadModel("hero.png", "hero") == 0);
    CHECK_FALSE(animation.isLoaded(1));
    animation.shutdown();
    animation.shutdown();
}

TEST_CASE("NullAnimationBackend renders PNG state through abstract backends") {
    NullAnimationFixture fixture;
    REQUIRE(fixture.animation.init());
    const int handle = fixture.animation.loadModel("hero.png", "hero");
    REQUIRE(handle == 1);

    fixture.animation.showModel(handle, 10.0f, 20.0f, 1.5f);
    fixture.animation.setOpacity(handle, 0.25f);
    fixture.animation.render(0.016f);

    REQUIRE(fixture.renderer.blits.size() == 1);
    const auto& first = fixture.renderer.blits.front();
    CHECK(first.view == VIEW_MAIN);
    CHECK(first.texture == fixture.textures.rawHandleBase + 11);
    CHECK(first.x == doctest::Approx(10.0f));
    CHECK(first.y == doctest::Approx(20.0f));
    CHECK(first.width == doctest::Approx(150.0f));
    CHECK(first.height == doctest::Approx(300.0f));
    CHECK(first.opacity == 64);

    fixture.animation.hideModel(handle);
    fixture.animation.render(0.016f);
    CHECK(fixture.renderer.blits.size() == 1);

    fixture.animation.showModel(handle, 30.0f, 40.0f, 2.0f);
    fixture.animation.setOpacity(handle, 2.0f);
    fixture.animation.render(0.016f);
    REQUIRE(fixture.renderer.blits.size() == 2);
    CHECK(fixture.renderer.blits.back().opacity == 255);
    CHECK(fixture.renderer.blits.back().width == doctest::Approx(200.0f));
    CHECK(fixture.renderer.blits.back().height == doctest::Approx(400.0f));

    fixture.animation.unloadModel(handle);
    CHECK_FALSE(fixture.animation.isLoaded(handle));
    CHECK(fixture.textures.destroyedIds == std::vector<uint32_t>{11});
    fixture.animation.unloadModel(handle);
    CHECK(fixture.textures.destroyedIds.size() == 1);
    fixture.animation.shutdown();
    CHECK(fixture.textures.destroyedIds.size() == 1);
}

TEST_CASE("NullAnimationBackend shutdown releases each PNG once and resets handles") {
    NullAnimationFixture fixture;
    REQUIRE(fixture.animation.init());
    const int first = fixture.animation.loadModel("left.png", "left");
    const int second = fixture.animation.loadModel("right.png", "right");
    REQUIRE(first == 1);
    REQUIRE(second == 2);

    fixture.animation.shutdown();
    CHECK_FALSE(fixture.animation.isLoaded(first));
    CHECK_FALSE(fixture.animation.isLoaded(second));
    std::sort(fixture.textures.destroyedIds.begin(), fixture.textures.destroyedIds.end());
    CHECK(fixture.textures.destroyedIds == std::vector<uint32_t>{11, 12});

    fixture.animation.shutdown();
    CHECK(fixture.textures.destroyedIds.size() == 2);

    REQUIRE(fixture.animation.init());
    CHECK(fixture.animation.loadModel("again.png", "again") == 1);
    fixture.animation.shutdown();
    CHECK(fixture.textures.destroyedIds.size() == 3);
}

TEST_CASE("NullAnimationBackend remains safe through IAnimationBackend") {
    NullAnimationFixture fixture;
    IAnimationBackend* animation = &fixture.animation;

    REQUIRE(animation->init());
    CHECK_FALSE(animation->playMotion(999, "idle"));
    animation->showModel(999, 0.0f, 0.0f, 1.0f);
    animation->setOpacity(999, 0.5f);
    animation->hideModel(999);
    animation->unloadModel(999);
    animation->render(0.0f);
    CHECK(fixture.renderer.blits.empty());
    animation->shutdown();
}

TEST_CASE("confineToModelRoot rejects absolute paths outside the working directory") {
    // Absolute paths outside the process CWD (model root) must be rejected.
#ifdef _WIN32
    CHECK(Caesura::confineToModelRoot("C:/Windows/win.ini").empty());
    CHECK(Caesura::confineToModelRoot("C:/Program Files").empty());
    // Root-relative drive path (no drive letter) resolves relative to CWD drive.
    CHECK(Caesura::confineToModelRoot("\\server\\share\\file").empty());
#else
    CHECK(Caesura::confineToModelRoot("/etc/passwd").empty());
    CHECK(Caesura::confineToModelRoot("/usr/share").empty());
#endif
}

TEST_CASE("confineToModelRoot rejects dot-dot escapes") {
    // Escaping upward from the CWD via .. is rejected.
    CHECK(Caesura::confineToModelRoot("../escape.txt").empty());
    CHECK(Caesura::confineToModelRoot("../../escape.txt").empty());
    CHECK(Caesura::confineToModelRoot("a/../../escape.txt").empty());
}

TEST_CASE("confineToModelRoot handles dot components") {
    // "." resolves to the working directory itself, which is the model root
    // and is allowed (the guard explicitly permits the root).
    const std::string root = Caesura::confineToModelRoot(".");
    CHECK_FALSE(root.empty());
    // ".." resolves to the parent of the root and must be rejected.
    CHECK(Caesura::confineToModelRoot("..").empty());
}

TEST_CASE("confineToModelRoot accepts paths inside the working directory") {
    // A path under the CWD is confined and returned non-empty.
    const std::string confined = Caesura::confineToModelRoot(".");
    CHECK_FALSE(confined.empty());
    // A plain relative file name under the root passes (may not exist on disk,
    // but containment is a lexical/canonical property of the prefix).
    const std::string file = Caesura::confineToModelRoot("live2d_test/Haru/Haru.model3.json");
    CHECK_FALSE(file.empty());
}

TEST_CASE("confineToModelRoot rejects prefix look-alike outside the root") {
    // A sibling whose name starts with the root string must not pass the
    // naive prefix check (boundary: root + "/" is required).
    const std::string root = std::filesystem::current_path().string();
    const std::string lookalike = root + "X";
    if (std::filesystem::exists(lookalike)) {
        CHECK(Caesura::confineToModelRoot(lookalike).empty());
    } else {
        // Lookalike does not exist; containment still resolves canonically
        // outside the root and must be rejected.
        CHECK(Caesura::confineToModelRoot(lookalike + "/f.txt").empty());
    }
}

TEST_CASE("confineToModelRoot resolves symlinks and keeps containment") {
    // Create a temp dir with a symlink that points outside the root; the
    // symlinked path must be rejected (resolved target escapes the root).
    namespace fs = std::filesystem;
    const fs::path tmp = fs::temp_directory_path() / ("pc_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::remove_all(tmp);
    fs::create_directories(tmp);
    const fs::path target = tmp / "target.txt";
    { std::ofstream(target) << "x"; }
    const fs::path link = tmp / "link.txt";
    std::error_code ec;
    fs::create_symlink(target, link, ec);
    if (!ec) {
        // The symlink target lives outside the model root (CWD) unless the
        // temp dir happens to be inside it; either way the link path itself
        // is outside the CWD and must be rejected.
        CHECK(Caesura::confineToModelRoot(link.string()).empty());
    }
    fs::remove_all(tmp);
}
