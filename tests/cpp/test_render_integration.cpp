// test_render_integration.cpp - render pipeline integration tests
#include "doctest.h"
#include "render/BgfxRenderDevice.h"
#include "render/ParticleSystem.h"
#include "render/TextRenderer.h"
#include "render/SmaMeshRenderer.h"
#include "minigame/BgfxMiniGameBackend.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#endif

using namespace Caesura;

#if defined(_WIN32)
namespace {

constexpr wchar_t kVfxGpuChildEnv[] = L"CAESURA_VFX_GPU_SMOKE_CHILD";
constexpr wchar_t kVfxGpuTestCase[] =
    L"Render: D3D11 VFX fade produces deterministic offscreen pixels";

constexpr wchar_t kFontGpuChildEnv[] = L"CAESURA_FONT_GPU_SMOKE_CHILD";
constexpr wchar_t kFontGpuTestCase[] =
    L"Render: D3D11 CJK TTF load and render smoke";

constexpr wchar_t kMiniGameGpuChildEnv[] = L"CAESURA_MINIGAME_GPU_SMOKE_CHILD";
constexpr wchar_t kMiniGameGpuTestCase[] =
    L"Render: D3D11 mini-game scene enter/render/leave smoke";

constexpr wchar_t kSmaGpuChildEnv[] = L"CAESURA_SMA_GPU_SMOKE_CHILD";
constexpr wchar_t kSmaGpuTestCase[] =
    L"Render: D3D11 SMA GPU skinning matches CPU skinning";

bool isGpuChildProcess(const wchar_t* envName) {
    wchar_t value[2] = {};
    return GetEnvironmentVariableW(envName, value, 2) > 0;
}

DWORD runGpuChildProcess(const wchar_t* envName, const wchar_t* testCaseName) {
    wchar_t executable[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return ERROR_INSUFFICIENT_BUFFER;

    const DWORD previousSize = GetEnvironmentVariableW(envName, nullptr, 0);
    std::wstring previousValue;
    if (previousSize > 0) {
        previousValue.resize(previousSize);
        GetEnvironmentVariableW(
            envName, previousValue.data(), previousSize);
        previousValue.resize(previousSize - 1);
    }

    if (!SetEnvironmentVariableW(envName, L"1")) return GetLastError();

    std::wstring command =
        L"\"" + std::wstring(executable) + L"\" --test-case=\"" +
        testCaseName + L"\" --no-version";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(
        nullptr,
        command.data(),
        nullptr,
        nullptr,
        TRUE,
        0,
        nullptr,
        nullptr,
        &startup,
        &process);
    const DWORD createError = created ? ERROR_SUCCESS : GetLastError();

    SetEnvironmentVariableW(
        envName,
        previousSize > 0 ? previousValue.c_str() : nullptr);
    if (!created) return createError;

    const DWORD waitResult = WaitForSingleObject(process.hProcess, 60000);
    DWORD exitCode = ERROR_TIMEOUT;
    if (waitResult == WAIT_OBJECT_0) {
        GetExitCodeProcess(process.hProcess, &exitCode);
    } else {
        TerminateProcess(process.hProcess, ERROR_TIMEOUT);
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return exitCode;
}

class HiddenSdlWindow {
public:
    HiddenSdlWindow(int width, int height) {
        if (!SDL_Init(SDL_INIT_VIDEO)) return;
        m_sdlInitialized = true;

        const SDL_PropertiesID props = SDL_CreateProperties();
        if (props == 0) return;
        SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING,
                              "Caesura VFX GPU smoke");
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height);
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
        m_window = SDL_CreateWindowWithProperties(props);
        SDL_DestroyProperties(props);
    }

    ~HiddenSdlWindow() {
        if (m_window) SDL_DestroyWindow(m_window);
        if (m_sdlInitialized) SDL_Quit();
    }

    HiddenSdlWindow(const HiddenSdlWindow&) = delete;
    HiddenSdlWindow& operator=(const HiddenSdlWindow&) = delete;

    explicit operator bool() const { return m_window != nullptr; }

    void* nativeHandle() const {
        if (!m_window) return nullptr;
        return SDL_GetPointerProperty(
            SDL_GetWindowProperties(m_window),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER,
            nullptr);
    }

private:
    SDL_Window* m_window = nullptr;
    bool m_sdlInitialized = false;
};

class BgfxTexture {
public:
    BgfxTexture() = default;
    explicit BgfxTexture(bgfx::TextureHandle handle) : m_handle(handle) {}
    ~BgfxTexture() {
        if (bgfx::isValid(m_handle)) bgfx::destroy(m_handle);
    }

    BgfxTexture(const BgfxTexture&) = delete;
    BgfxTexture& operator=(const BgfxTexture&) = delete;

    bgfx::TextureHandle get() const { return m_handle; }
    bool valid() const { return bgfx::isValid(m_handle); }

private:
    bgfx::TextureHandle m_handle = BGFX_INVALID_HANDLE;
};

class BgfxFrameBuffer {
public:
    explicit BgfxFrameBuffer(bgfx::FrameBufferHandle handle) : m_handle(handle) {}
    ~BgfxFrameBuffer() {
        if (bgfx::isValid(m_handle)) bgfx::destroy(m_handle);
    }

    BgfxFrameBuffer(const BgfxFrameBuffer&) = delete;
    BgfxFrameBuffer& operator=(const BgfxFrameBuffer&) = delete;

    bgfx::FrameBufferHandle get() const { return m_handle; }
    bool valid() const { return bgfx::isValid(m_handle); }

private:
    bgfx::FrameBufferHandle m_handle = BGFX_INVALID_HANDLE;
};

} // namespace
#endif

TEST_CASE("Render: device default values") {
    BgfxRenderDevice rd;
    CHECK(rd.getBackbufferWidth() == 1280);
    CHECK(rd.getBackbufferHeight() == 720);
    CHECK(rd.getBackendName() != nullptr);
}

TEST_CASE("Render: device double shutdown idempotent") {
    BgfxRenderDevice rd;
    rd.shutdown();
    rd.shutdown();
    CHECK(rd.getBackendName() != nullptr);
}

TEST_CASE("Render: ParticleSystem create/destroy emitter") {
    ParticleSystem ps;
    Emitter cfg;
    int eid = ps.createEmitter(cfg);
    CHECK(eid >= 0);
    ps.destroyEmitter(eid);
    ps.destroyEmitter(eid);
}

TEST_CASE("Render: ParticleSystem update with no emitters") {
    ParticleSystem ps;
    ps.update(0.016f, 1280, 720);
    CHECK(ps.aliveCount() <= 1024);
}

TEST_CASE("Render: ParticleSystem multiple create/destroy") {
    ParticleSystem ps;
    Emitter cfg;
    int e1 = ps.createEmitter(cfg);
    int e2 = ps.createEmitter(cfg);
    CHECK(e1 >= 0);
    CHECK(e2 >= 0);
    CHECK(e1 != e2);
    ps.destroyEmitter(e1);
    ps.destroyEmitter(e2);
}

TEST_CASE("Render: ParticleSystem emit without init") {
    ParticleSystem ps;
    Emitter cfg;
    int eid = ps.createEmitter(cfg);
    ps.emit(eid, 5);
    ps.update(0.1f, 1280, 720);
    ps.destroyEmitter(eid);
}

TEST_CASE("Render: TextRenderer shutdown is idempotent before init") {
    TextRenderer renderer;
    renderer.shutdown();
    renderer.shutdown();
    CHECK_FALSE(renderer.isInitialized());
}

#if defined(_WIN32)
TEST_CASE("Render: D3D11 VFX fade produces deterministic offscreen pixels") {
    if (!isGpuChildProcess(kVfxGpuChildEnv)) {
        CHECK(runGpuChildProcess(kVfxGpuChildEnv, kVfxGpuTestCase) == ERROR_SUCCESS);
        return;
    }

    constexpr uint16_t kWidth = 4;
    constexpr uint16_t kHeight = 4;
    constexpr uint16_t kVfxView = 10;
    constexpr uint16_t kReadbackView = 11;

    HiddenSdlWindow window(kWidth, kHeight);
    REQUIRE(window);
    REQUIRE(window.nativeHandle() != nullptr);

    BgfxRenderDevice device;
    REQUIRE(device.setPreferredBackend("dx11"));
    REQUIRE(device.init(window.nativeHandle(), kWidth, kHeight));
    REQUIRE(bgfx::isValid(device.getVFXProgram()));

    {
        std::array<uint8_t, kWidth * kHeight * 4> sourcePixels{};
        for (size_t i = 0; i < sourcePixels.size(); i += 4) {
            sourcePixels[i + 0] = 32;
            sourcePixels[i + 1] = 64;
            sourcePixels[i + 2] = 96;
            sourcePixels[i + 3] = 255;
        }

        BgfxTexture source(bgfx::createTexture2D(
            kWidth, kHeight, false, 1, bgfx::TextureFormat::RGBA8,
            BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            bgfx::copy(sourcePixels.data(), static_cast<uint32_t>(sourcePixels.size()))));
        REQUIRE(source.valid());

        bgfx::TextureHandle outputTexture = bgfx::createTexture2D(
            kWidth, kHeight, false, 1, bgfx::TextureFormat::RGBA8,
            BGFX_TEXTURE_RT |
                BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        REQUIRE(bgfx::isValid(outputTexture));

        BgfxFrameBuffer output(bgfx::createFrameBuffer(1, &outputTexture, true));
        REQUIRE(output.valid());

        BgfxTexture readback(bgfx::createTexture2D(
            kWidth, kHeight, false, 1, bgfx::TextureFormat::RGBA8,
            BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK));
        REQUIRE(readback.valid());

        bgfx::setViewRect(kVfxView, 0, 0, kWidth, kHeight);
        bgfx::setViewClear(kVfxView, BGFX_CLEAR_COLOR, 0x000000ff, 1.0f, 0);
        bgfx::setViewFrameBuffer(kVfxView, output.get());

        device.submitVFX(
            kVfxView,
            RenderTextureHandle{source.get().idx},
            1,
            1.0f,
            1.0f, 0.0f, 0.0f,
            0.0f,
            0.0f, 0.0f);
        bgfx::blit(kReadbackView, readback.get(), 0, 0, outputTexture);

        uint32_t currentFrame = bgfx::frame();
        std::array<uint8_t, kWidth * kHeight * 4> result{};
        const uint32_t readyFrame = bgfx::readTexture(readback.get(), result.data());
        while (currentFrame < readyFrame) currentFrame = bgfx::frame();

        for (size_t i = 0; i < result.size(); i += 4) {
            CHECK(result[i + 0] == 255);
            CHECK(result[i + 1] == 0);
            CHECK(result[i + 2] == 0);
            CHECK(result[i + 3] == 255);
        }

        bgfx::setViewFrameBuffer(kVfxView, BGFX_INVALID_HANDLE);
        bgfx::frame();
    }

    bgfx::frame();
    device.shutdown();
}

TEST_CASE("Render: D3D11 CJK TTF load and render smoke") {
    if (!isGpuChildProcess(kFontGpuChildEnv)) {
        CHECK(runGpuChildProcess(kFontGpuChildEnv, kFontGpuTestCase) == ERROR_SUCCESS);
        return;
    }

    constexpr uint16_t kWidth = 128;
    constexpr uint16_t kHeight = 64;
    HiddenSdlWindow window(kWidth, kHeight);
    REQUIRE(window);
    REQUIRE(window.nativeHandle() != nullptr);

    BgfxRenderDevice device;
    REQUIRE(device.setPreferredBackend("dx11"));
    REQUIRE(device.init(window.nativeHandle(), kWidth, kHeight));

    TextRenderer text;
    REQUIRE(text.init(&device));

    // Locate the shipped CJK font (test CWD is build/tests/Debug).
    const char* candidates[] = {
        "../../../assets/fonts/NotoSansCJKsc-Regular.otf",
        "assets/fonts/NotoSansCJKsc-Regular.otf",
        "../../assets/fonts/NotoSansCJKsc-Regular.otf",
    };
    const char* fontPath = nullptr;
    for (const char* c : candidates) {
        if (std::filesystem::exists(c)) { fontPath = c; break; }
    }
    REQUIRE_MESSAGE(fontPath != nullptr, "CJK font asset not found");

    // R7: real face load (OTF via FreeType) + ASCII/CJK atlas rasterization
    // + GPU atlas upload + face switch to FontId::TTF.
    REQUIRE(text.loadTTF(fontPath, 24.0f));
    CHECK(text.currentFont() == FontId::TTF);
    CHECK(text.lineHeight() > 16.0f);
    CHECK(bgfx::isValid(text.fontTexture()));

    // R7: CJK + ruby rendering on a real GPU frame must not crash.
    constexpr uint16_t kTextView = 10;
    bgfx::setViewRect(kTextView, 0, 0, kWidth, kHeight);
    bgfx::setViewClear(kTextView, BGFX_CLEAR_COLOR, 0x000000ff, 1.0f, 0);
    text.renderText(kTextView, "Caesura 引擎测试 ABC 123",
                    4.0f, 4.0f, TextColor::White());
    text.renderRuby(kTextView, "漢字", "かんじ",
                    4.0f, 24.0f, TextColor::White());
    bgfx::frame();

    // TextRenderer must release its bgfx resources while the GPU context is
    // still alive: bgfx::destroy after bgfx::shutdown is undefined behaviour.
    text.shutdown();
    device.shutdown();
}

TEST_CASE("Render: D3D11 mini-game scene enter/render/leave smoke") {
    if (!isGpuChildProcess(kMiniGameGpuChildEnv)) {
        CHECK(runGpuChildProcess(kMiniGameGpuChildEnv, kMiniGameGpuTestCase) == ERROR_SUCCESS);
        return;
    }

    constexpr uint16_t kWidth = 128;
    constexpr uint16_t kHeight = 72;
    HiddenSdlWindow window(kWidth, kHeight);
    REQUIRE(window);
    REQUIRE(window.nativeHandle() != nullptr);

    BgfxRenderDevice device;
    REQUIRE(device.setPreferredBackend("dx11"));
    REQUIRE(device.init(window.nativeHandle(), kWidth, kHeight));

    BgfxMiniGameBackend miniGame;
    miniGame.setRenderDevice(&device);
    REQUIRE(miniGame.init());

    const char* sceneCandidates[] = {
        "../../../demo/minigame_scene.json",
        "demo/minigame_scene.json",
        "../../demo/minigame_scene.json",
    };
    const char* scenePath = nullptr;
    for (const char* c : sceneCandidates) {
        if (std::filesystem::exists(c)) { scenePath = c; break; }
    }
    REQUIRE_MESSAGE(scenePath != nullptr, "minigame scene JSON not found");

    // C2: GPU lifecycle -- enter -> update -> render -> leave with a real
    // scene descriptor (previously only verified without a GPU context).
    const uint32_t scene = miniGame.loadScene(scenePath);
    REQUIRE(scene != 0);
    miniGame.enter(scene);
    CHECK(miniGame.isActive());
    CHECK(miniGame.update(0.016f));
    miniGame.render();
    bgfx::frame();
    miniGame.leave();
    CHECK_FALSE(miniGame.isActive());

    miniGame.shutdown();
    device.shutdown();
}

TEST_CASE("Render: D3D11 SMA GPU skinning matches CPU skinning") {
    if (!isGpuChildProcess(kSmaGpuChildEnv)) {
        CHECK(runGpuChildProcess(kSmaGpuChildEnv, kSmaGpuTestCase) == ERROR_SUCCESS);
        return;
    }

    constexpr uint16_t kWidth = 128;
    constexpr uint16_t kHeight = 72;
    constexpr uint16_t kDrawView = 10;
    constexpr uint16_t kReadbackView = 11;

    HiddenSdlWindow window(kWidth, kHeight);
    REQUIRE(window);
    REQUIRE(window.nativeHandle() != nullptr);

    BgfxRenderDevice device;
    REQUIRE(device.setPreferredBackend("dx11"));
    REQUIRE(device.init(window.nativeHandle(), kWidth, kHeight));

    {
        // The S5 compute pipeline must actually be engaged for this test
        // to mean anything (a broken compute program would silently fall
        // back to the CPU path and the pixel comparison would trivially
        // pass). Declared inside the scope so it is destroyed before the
        // device shuts down below (bgfx handles must die before bgfx::shutdown).
        SmaMeshRenderer renderer;
        renderer.init();
        REQUIRE(renderer.isInitialized());
        REQUIRE(renderer.gpuSkinAvailable());

        // A white 1x1 solid texture for the mesh.
        const uint8_t whitePx[4] = { 255, 255, 255, 255 };
        bgfx::TextureHandle whiteTex = bgfx::createTexture2D(
            1, 1, false, 1, bgfx::TextureFormat::RGBA8,
            BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            bgfx::copy(whitePx, sizeof(whitePx)));
        REQUIRE(bgfx::isValid(whiteTex));

        bgfx::TextureHandle outputTexture = bgfx::createTexture2D(
            kWidth, kHeight, false, 1, bgfx::TextureFormat::RGBA8,
            BGFX_TEXTURE_RT | BGFX_SAMPLER_POINT
                | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        REQUIRE(bgfx::isValid(outputTexture));
        BgfxFrameBuffer output(bgfx::createFrameBuffer(1, &outputTexture, true));
        REQUIRE(output.valid());

        auto makeReadback = [&]() {
            return BgfxTexture(bgfx::createTexture2D(
                kWidth, kHeight, false, 1, bgfx::TextureFormat::RGBA8,
                BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK));
        };
        BgfxTexture readbackGpu = makeReadback();
        BgfxTexture readbackCpu = makeReadback();
        REQUIRE(readbackGpu.valid());
        REQUIRE(readbackCpu.valid());

        bgfx::setViewRect(kDrawView, 0, 0, kWidth, kHeight);
        bgfx::setViewClear(kDrawView, BGFX_CLEAR_COLOR, 0x000000ff, 1.0f, 0);
        bgfx::setViewFrameBuffer(kDrawView, output.get());

        // Two-bone quad: vertex 0 blends bone0/bone1 50/50, the rest
        // follow bone0 only (matches the CPU reference mesh of S2 tests).
        SMAMesh mesh;
        mesh.vertices = {
            { 0.f, 0.f, 0.f, 0.f, 0, 0.5f, 1, 0.5f },
            { 40.f, 0.f, 1.f, 0.f, 0, 1.f, 1, 0.f },
            { 40.f, 40.f, 1.f, 1.f, 0, 1.f, 1, 0.f },
            { 0.f, 40.f, 0.f, 1.f, 0, 1.f, 1, 0.f },
        };
        mesh.indices = { 0, 1, 2, 0, 2, 3 };
        const MeshHandle h = renderer.createMesh(mesh);
        REQUIRE(h);

        std::vector<BonePose> poses(2);
        poses[0].rot = 0.3f;
        poses[0].scale = 1.1f;
        poses[0].ox = 30.f;
        poses[0].oy = 10.f;
        poses[1].ox = 60.f;
        poses[1].oy = 20.f;

        auto renderFrame = [&](SkinMode mode, bgfx::TextureHandle readback,
                               std::array<uint8_t, 128 * 72 * 4>& out) {
            renderer.setSkinMode(mode);
            renderer.updateMesh(h, poses);
            renderer.drawMesh(kDrawView, h, whiteTex.idx, 20.f, 5.f, 1.0f, 1.f);
            bgfx::blit(kReadbackView, readback, 0, 0, outputTexture);
            uint32_t currentFrame = bgfx::frame();
            const uint32_t readyFrame = bgfx::readTexture(readback, out.data());
            while (currentFrame < readyFrame) currentFrame = bgfx::frame();
        };

        std::array<uint8_t, kWidth * kHeight * 4> gpuPixels{};
        std::array<uint8_t, kWidth * kHeight * 4> cpuPixels{};
        renderFrame(SkinMode::Cpu, readbackCpu.get(), cpuPixels);
        renderFrame(SkinMode::Gpu, readbackGpu.get(), gpuPixels);

        // Same mesh, same pose, same draw transform: the GPU compute skin
        // and the CPU soft-skinner must rasterize identically. Per-channel
        // tolerance 1 covers float rounding; a small budget covers
        // sub-pixel edge flips. A real skinning error shifts the whole
        // quad (hundreds of pixels) and blows the budget.
        int diffs = 0;
        int whiteGpu = 0, whiteCpu = 0;
        for (size_t i = 0; i < gpuPixels.size(); i += 4) {
            const int a = gpuPixels[i + 0], b = cpuPixels[i + 0];
            if (a < b - 1 || a > b + 1) ++diffs;
            if (a > 250 && gpuPixels[i + 1] > 250 && gpuPixels[i + 2] > 250) ++whiteGpu;
            if (b > 250 && cpuPixels[i + 1] > 250 && cpuPixels[i + 2] > 250) ++whiteCpu;
        }
        CHECK_MESSAGE(diffs < 128,
                      "GPU/CPU skin mismatch on " << diffs << " channels");
        // Coverage must agree too: a transform difference changes the
        // drawn area (rotated/scaled quad), not just edge pixels.
        CHECK(std::abs(whiteGpu - whiteCpu) <= std::max(2, whiteGpu / 50));

        bgfx::setViewFrameBuffer(kDrawView, BGFX_INVALID_HANDLE);
        bgfx::frame();
        renderer.destroyMesh(h);
        bgfx::destroy(whiteTex);
        bgfx::frame();
    }
    // Scope block above releases every bgfx resource BEFORE the device is
    // shut down (destroying handles after bgfx::shutdown is UB).
    device.shutdown();
}

// ---------------------------------------------------------------------------
// Round 19: S5 host-side cost benchmark. The GPU compute path must reduce
// per-frame host work versus CPU soft skinning: for a large mesh (8k
// vertices / 64 bones) the GPU path (pose pack + dispatch submit) must
// cost LESS host time than the CPU path (per-vertex blend + upload).
// This is the S5 value proposition (the GPU does the math; the host only
// packs 64 vec4s). Host-side timing is deterministic across WARP and real
// GPUs, so the assertion holds everywhere the compute path is available.
// ---------------------------------------------------------------------------
constexpr wchar_t kSmaPerfChildEnv[] = L"CAESURA_SMA_PERF_CHILD";
constexpr wchar_t kSmaPerfTestCase[] =
    L"Render: D3D11 SMA GPU skin host-cost benchmark";

TEST_CASE("Render: D3D11 SMA GPU skin host-cost benchmark") {
    if (!isGpuChildProcess(kSmaPerfChildEnv)) {
        CHECK(runGpuChildProcess(kSmaPerfChildEnv, kSmaPerfTestCase) == ERROR_SUCCESS);
        return;
    }

    constexpr uint16_t kWidth = 128;
    constexpr uint16_t kHeight = 72;
    constexpr uint16_t kDrawView = 20;
    constexpr int kCols = 128;   // 8192 vertices
    constexpr int kRows = 64;
    constexpr int kVertCount = kCols * kRows;
    constexpr int kFrames = 120;  // 20 warmup + 100 measured
    constexpr int kWarmup = 20;

    HiddenSdlWindow window(kWidth, kHeight);
    REQUIRE(window);
    REQUIRE(window.nativeHandle() != nullptr);

    BgfxRenderDevice device;
    REQUIRE(device.setPreferredBackend("dx11"));
    REQUIRE(device.init(window.nativeHandle(), kWidth, kHeight));

    {
        SmaMeshRenderer renderer;
        renderer.init();
        REQUIRE(renderer.isInitialized());
        REQUIRE(renderer.gpuSkinAvailable());

        // Deterministic pseudo-random large mesh: 8k verts, 64 bones,
        // dual-bone weights, ~16k triangles.
        SMAMesh mesh;
        mesh.vertices.reserve(kVertCount);
        for (int r = 0; r < kRows; ++r) {
            for (int c = 0; c < kCols; ++c) {
                SMAMeshVertex v;
                v.x = static_cast<float>(c);
                v.y = static_cast<float>(r);
                v.u = kCols > 1 ? static_cast<float>(c) / (kCols - 1) : 0.f;
                v.v = kRows > 1 ? static_cast<float>(r) / (kRows - 1) : 0.f;
                v.bone0 = static_cast<uint16_t>((c + r * 3) % 64);
                v.w0 = 0.6f;
                v.bone1 = static_cast<uint16_t>((c * 7 + r) % 64);
                v.w1 = 0.4f;
                mesh.vertices.push_back(v);
            }
        }
        mesh.indices.reserve((kCols - 1) * (kRows - 1) * 6);
        for (int r = 0; r < kRows - 1; ++r) {
            for (int c = 0; c < kCols - 1; ++c) {
                const int i = r * kCols + c;
                mesh.indices.push_back(static_cast<uint16_t>(i));
                mesh.indices.push_back(static_cast<uint16_t>(i + 1));
                mesh.indices.push_back(static_cast<uint16_t>(i + kCols));
                mesh.indices.push_back(static_cast<uint16_t>(i + 1));
                mesh.indices.push_back(static_cast<uint16_t>(i + kCols + 1));
                mesh.indices.push_back(static_cast<uint16_t>(i + kCols));
            }
        }
        REQUIRE(mesh.indices.size() % 3 == 0);

        const MeshHandle h = renderer.createMesh(mesh);
        REQUIRE(h);

        std::vector<BonePose> poses(64);
        for (size_t i = 0; i < poses.size(); ++i) {
            poses[i].rot = static_cast<float>((i * 7) % 360) * 0.01f;
            poses[i].scale = 0.9f + static_cast<float>((i * 13) % 20) * 0.01f;
            poses[i].ox = static_cast<float>((i * 3) % 50);
            poses[i].oy = static_cast<float>((i * 11) % 40);
        }

        // White 1x1 texture (a mesh draw always binds one).
        const uint8_t whitePx[4] = { 255, 255, 255, 255 };
        bgfx::TextureHandle whiteTex = bgfx::createTexture2D(
            1, 1, false, 1, bgfx::TextureFormat::RGBA8,
            BGFX_SAMPLER_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            bgfx::copy(whitePx, sizeof(whitePx)));
        REQUIRE(bgfx::isValid(whiteTex));

        bgfx::setViewRect(kDrawView, 0, 0, kWidth, kHeight);
        bgfx::setViewClear(kDrawView, BGFX_CLEAR_COLOR, 0x000000ff, 1.0f, 0);
        bgfx::frame();  // flush setup

        auto measure = [&](SkinMode mode) {
            renderer.setSkinMode(mode);
            using Clock = std::chrono::steady_clock;
            double totalMs = 0.0;
            for (int i = 0; i < kFrames; ++i) {
                const auto t0 = Clock::now();
                renderer.updateMesh(h, poses);
                renderer.drawMesh(kDrawView, h, whiteTex.idx, 20.f, 5.f, 1.0f, 1.f);
                const auto t1 = Clock::now();
                bgfx::frame();  // submit (outside the timed region)
                if (i >= kWarmup) {
                    totalMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
                }
            }
            return totalMs / static_cast<double>(kFrames - kWarmup);
        };

        const double cpuMs = measure(SkinMode::Cpu);
        const double gpuMs = measure(SkinMode::Gpu);

        MESSAGE("SMA perf (8k verts, 64 bones): CPU " << cpuMs
                << " ms/frame host, GPU " << gpuMs << " ms/frame host");
        // Value proposition: the compute path must cost LESS host time
        // (pose pack + dispatch submit) than soft skinning + upload.
        CHECK_MESSAGE(gpuMs < cpuMs,
                      "GPU host path (" << gpuMs << "ms) not cheaper than CPU ("
                      << cpuMs << "ms)");

        bgfx::setViewFrameBuffer(kDrawView, BGFX_INVALID_HANDLE);
        bgfx::frame();
        renderer.destroyMesh(h);
        bgfx::destroy(whiteTex);
        bgfx::frame();
    }
    device.shutdown();
}

#endif
