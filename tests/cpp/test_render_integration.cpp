// test_render_integration.cpp - render pipeline integration tests
#include "doctest.h"
#include "render/BgfxRenderDevice.h"
#include "render/ParticleSystem.h"
#include "render/TextRenderer.h"
#include "minigame/BgfxMiniGameBackend.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <array>
#include <cstdint>
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
#endif
