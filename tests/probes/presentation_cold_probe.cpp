// Real Engine producer/consumer for whole-page pixels and offline mixer PCM.
// This executable observes native backends; it does not implement persistence.
#include "entry/Engine.h"
#include "entry/EngineConfig.h"
#include "audio/SoLoudAudioEngine.h"
#include "di/BackendRegistry.h"
#include "platform/api/IPlatformBackend.h"
#include "render/BgfxRenderDevice.h"
#include "resource/api/IImageDecoder.h"
#include "script/api/ILuaManager.h"
#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace {
using namespace Caesura;
namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;
constexpr int kWidth = 640, kHeight = 360;
constexpr unsigned kRate = 48000, kChannels = 2, kBlock = 512;
constexpr unsigned kPrefixFrames = 8192, kSegmentFrames = 65536;
constexpr float kSavedGain = 0.625f;
constexpr size_t kPageBytes = size_t(kWidth) * kHeight * 4;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

// A real SDL surface created hidden from the outset. No null GPU platform.
class HiddenPlatform final : public IPlatformBackend {
public:
    ~HiddenPlatform() override { shutdown(); }
    bool init(const char* title, int width, int height) override {
        if (window_) return true;
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) return false;
        initialized_ = true;
        const SDL_PropertiesID props = SDL_CreateProperties();
        if (!props) return false;
        SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height);
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
        window_ = SDL_CreateWindowWithProperties(props);
        SDL_DestroyProperties(props);
        width_ = width; height_ = height;
        return window_ != nullptr;
    }
    void shutdown() override {
        if (window_) { SDL_DestroyWindow(window_); window_ = nullptr; }
        if (initialized_) { SDL_Quit(); initialized_ = false; }
    }
    bool pollEvent() override { SDL_Event event{}; return SDL_PollEvent(&event); }
    MouseState getMouseState() const override {
        MouseState value;
        const auto buttons = SDL_GetMouseState(&value.x, &value.y);
        value.leftDown = (buttons & SDL_BUTTON_LMASK) != 0;
        return value;
    }
    uint64_t getTicksMs() const override { return SDL_GetTicks(); }
    void* getNativeWindowHandle() const override {
        return window_ ? SDL_GetPointerProperty(SDL_GetWindowProperties(window_),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr) : nullptr;
    }
    int getWindowWidth() const override { return width_; }
    int getWindowHeight() const override { return height_; }
    void setFullscreen(bool enabled) override {
        if (window_) SDL_SetWindowFullscreen(window_, enabled);
    }
    void resizeWindow(int width, int height) override {
        if (window_ && SDL_SetWindowSize(window_, width, height)) {
            width_ = width; height_ = height;
        }
    }
    const char* getBackendName() const override { return "SDL3 hidden probe"; }
    bool startTextInput() override { return window_ && SDL_StartTextInput(window_); }
    bool stopTextInput() override { return window_ && SDL_StopTextInput(window_); }
    bool setTextInputRect(int x, int y, int w, int h, int cursor) override {
        SDL_Rect rect{x, y, w, h};
        return window_ && SDL_SetTextInputArea(window_, &rect, cursor);
    }
    bool isTextInputActive() const override { return window_ && SDL_TextInputActive(window_); }
private:
    SDL_Window* window_ = nullptr;
    bool initialized_ = false;
    int width_ = kWidth, height_ = kHeight;
};

Bytes readBytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) return {};
    const auto length = input.tellg();
    if (length <= 0 || length > 16 * 1024 * 1024) return {};
    Bytes value(static_cast<size_t>(length));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(value.data()), length)) return {};
    return value;
}

void writeBytes(const fs::path& path, const void* bytes, size_t size) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(bool(output), "Cannot create observation file: " + path.string());
    output.write(static_cast<const char*>(bytes), static_cast<std::streamsize>(size));
    output.close();
    require(bool(output), "Cannot finish observation file: " + path.string());
}

void writeReport(const fs::path& output, const json& report) {
    const auto content = report.dump(2);
    writeBytes(output / "result.json", content.data(), content.size());
}

json audioDescription(const AudioRestoreState& state) {
    return {{"path", state.bgmPath}, {"position", state.position},
        {"gain", state.gain}, {"looping", state.looping}};
}

class Probe {
public:
    Probe(fs::path output, json& report) : output_(std::move(output)), report_(report) {
        EngineConfig config;
        config.width = kWidth; config.height = kHeight;
        config.title = "Caesura U11 cold presentation probe";
        config.editorMode = true;
        config.renderBackend = "dx11";
        config.platform = new HiddenPlatform;
        config.render = new BgfxRenderDevice;
        audio_ = new SoLoudAudioEngine(SoLoudAudioEngine::OutputMode::ManualMix);
        config.audio = audio_;
        engine_ = std::make_unique<Engine>(std::move(config));
        require(engine_->init(), "Real Engine initialization failed");
        require(BackendRegistry::instance().getAudioBackend() == audio_, "Audio silently degraded");
        require(BackendRegistry::instance().getAudioRestore() == audio_, "Native audio restore missing");
        require(bgfx::getRendererType() == bgfx::RendererType::Direct3D11, "D3D11 required");
        require(audio_->soloud().getBackendId() == SoLoud::Soloud::NULLDRIVER, "Manual mixer required");
        require(audio_->soloud().getBackendSamplerate() == kRate, "Unexpected sample rate");
        require(audio_->soloud().getBackendChannels() == kChannels, "Stereo mixer required");
        vm_ = BackendRegistry::instance().getLuaManager();
        require(vm_ && vm_->state(), "Real Engine VM missing");
        runLua("U11Presentation = assert(dofile('tests/projects/u11_restore/presentation_fixture.lua'))");
        // This fixed value is a test fixture key, never a user credential.
        runLua("assert(KAG.set_encryption_key(string.rep('K',32)))");
        audio_->setGlobalVolume(0.75f);
        audio_->setBusVolume("bgm", 0.8f);
        bgfx::setDebug(BGFX_DEBUG_NONE);
        report_["host"] = {{"renderer", "Direct3D11"}, {"audio", "SoLoud NULLDRIVER"},
            {"sample_rate", kRate}, {"channels", kChannels}, {"block_frames", kBlock},
            {"width", kWidth}, {"height", kHeight}, {"master_gain", 0.75},
            {"bgm_bus_gain", 0.8}, {"resampler", "production defaults"}};
    }

    void runLua(const char* source) {
        vm_->resetInstructionBudget();
        lua_State* state = vm_->state();
        const int top = lua_gettop(state);
        const int status = luaL_dostring(state, source);
        std::string error;
        if (status != LUA_OK) {
            const char* value = lua_tostring(state, -1);
            error = value ? value : "non-string Lua error";
        }
        lua_settop(state, top);
        require(status == LUA_OK, "Probe Lua: " + error);
    }

    Bytes capturePage(const std::string& name) {
        // Warm the actual command stream without changing runner/audio time.
        drawFrame(); drawFrame();
        const fs::path png = output_ / (name + ".png");
        require(!fs::exists(png), "Capture must not reuse a stale filename");
        const auto requestPath = fs::relative(png, fs::current_path()).generic_string();
        require(engine_->renderDevice().requestScreenshot(requestPath), "Screenshot request failed");
        auto* decoder = BackendRegistry::instance().getImageDecoder();
        require(decoder != nullptr, "Native CPU image decoder unavailable");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        for (unsigned attempt = 0; attempt < 120; ++attempt) {
            drawFrame();
            const auto encoded = readBytes(png);
            if (!encoded.empty()) {
                auto decoded = decoder->decode(encoded.data(), encoded.size(), kPageBytes);
                if (decoded.ok) {
                    require(decoded.width == kWidth && decoded.height == kHeight
                        && decoded.rgba.size() == kPageBytes, "Screenshot dimensions changed");
                    writeBytes(output_ / (name + ".rgba"), decoded.rgba.data(), decoded.rgba.size());
                    report_["pages"][name] = {{"png", name + ".png"},
                        {"rgba", name + ".rgba"}, {"bytes", decoded.rgba.size()},
                        {"wait_frames", attempt + 1}};
                    writeReport(output_, report_);
                    return decoded.rgba;
                }
            }
            require(std::chrono::steady_clock::now() < deadline, "Screenshot completion timed out");
            SDL_Delay(1);
        }
        throw std::runtime_error("No decodable final-frame screenshot in the bounded frame budget");
    }

    void startBgm(const char* path, float gain, bool loop) {
        const auto handle = audio_->playBGM(path, 0);
        require(handle != 0, "Real BGM did not start");
        audio_->soloud().setVolume(handle, gain);
        audio_->soloud().setLooping(handle, loop);
    }

    void advanceAudio(unsigned frames) {
        std::vector<float> block(kBlock * kChannels);
        while (frames) {
            const unsigned count = (std::min)(frames, kBlock);
            audio_->soloud().mix(block.data(), count);
            audio_->update(0);
            frames -= count;
        }
    }

    void mixSegment(const std::string& name) {
        const auto before = audio_->captureAudioState();
        std::vector<float> samples(size_t(kSegmentFrames) * kChannels);
        for (unsigned offset = 0; offset < kSegmentFrames; offset += kBlock) {
            audio_->soloud().mix(samples.data() + size_t(offset) * kChannels, kBlock);
            audio_->update(0);
        }
        double energy = 0, peak = 0;
        for (const float sample : samples) {
            require(std::isfinite(sample), "Mixer emitted non-finite PCM");
            energy += double(sample) * sample;
            peak = (std::max)(peak, double(std::abs(sample)));
        }
        static_assert(sizeof(float) == 4, "Evidence requires float32");
        writeBytes(output_ / (name + ".f32"), samples.data(), samples.size() * sizeof(float));
        report_["audio"][name] = {{"pcm", name + ".f32"}, {"frames", kSegmentFrames},
            {"before", audioDescription(before)}, {"after", audioDescription(audio_->captureAudioState())},
            {"rms", std::sqrt(energy / samples.size())}, {"peak", peak},
            {"voice_playing", audio_->isVoicePlaying()}, {"se_playing", audio_->isSEPlaying()}};
        writeReport(output_, report_);
    }

    void producer() {
        capturePage("blank");
        runLua("U11Presentation.producer_page()");
        startBgm("assets/u11/tone.wav", kSavedGain, true);
        advanceAudio(kPrefixFrames);
        const auto saved = capturePage("saved-a");
        const auto second = capturePage("saved-b");
        require(saved == second, "Unchanged producer page is not pixel-stable");
        report_["saved_audio"] = audioDescription(audio_->captureAudioState());
        runLua("U11Presentation.save()");
        mixSegment("continuous");
        startBgm("assets/u11/tone.wav", kSavedGain, true);
        advanceAudio(kPrefixFrames + 3 * kBlock);
        mixSegment("wrong-position");
        startBgm("assets/u11/tone.wav", kSavedGain / 2, true);
        advanceAudio(kPrefixFrames);
        mixSegment("wrong-gain");
        runLua("U11Presentation.hide_text()");
        capturePage("text-hidden");
        runLua("U11Presentation.changed_page()");
        capturePage("changed");
        startBgm("assets/u11/changed.wav", 0.3f, false);
        mixSegment("changed");
        runLua("U11Presentation.load()");
        capturePage("hot");
        mixSegment("hot");
    }

    void consumer() {
        capturePage("blank");
        runLua("U11Presentation.bootstrap()");
        capturePage("bootstrap");
        startBgm("assets/u11/changed.wav", 0.3f, false);
        mixSegment("bootstrap");
        // No reference state or resource handles cross the process boundary.
        runLua("U11Presentation.load()");
        capturePage("cold");
        mixSegment("cold");
    }

    void shutdown() { engine_->shutdown(); }
private:
    void drawFrame() {
        vm_->resetInstructionBudget();
        engine_->renderDevice().touch(VIEW_MAIN);
        engine_->renderOneFrame();
        runLua("assert(U11Presentation.rendered_frames>0); assert(not U11Presentation.last_render_error, U11Presentation.last_render_error)");
    }
    fs::path output_;
    json& report_;
    std::unique_ptr<Engine> engine_;
    SoLoudAudioEngine* audio_ = nullptr; // Engine owns the exact observed backend.
    ILuaManager* vm_ = nullptr;
};

struct Arguments { std::wstring role; fs::path resources, output; };

Arguments arguments(int argc, wchar_t** argv) {
    require(argc == 7, "Usage: --role producer|consumer --resource-root DIR --output-dir DIR");
    Arguments result;
    for (int i = 1; i < argc; i += 2) {
        const std::wstring key = argv[i];
        if (key == L"--role") result.role = argv[i + 1];
        else if (key == L"--resource-root") result.resources = argv[i + 1];
        else if (key == L"--output-dir") result.output = argv[i + 1];
        else throw std::runtime_error("Unknown probe argument");
    }
    require(result.role == L"producer" || result.role == L"consumer", "Invalid probe role");
    require(fs::is_directory(result.resources) && fs::is_directory(result.output), "Missing probe directory");
    result.resources = fs::canonical(result.resources);
    result.output = fs::canonical(result.output);
    return result;
}
}

int wmain(int argc, wchar_t** argv) {
    fs::path output;
    json report = {{"status", "RUNNING"}, {"pid", GetCurrentProcessId()}};
    try {
        const auto args = arguments(argc, argv);
        output = args.output;
        fs::current_path(args.resources);
        report["role"] = args.role == L"producer" ? "producer" : "consumer";
        Probe probe(output, report);
        if (args.role == L"producer") probe.producer(); else probe.consumer();
        probe.shutdown();
        report["status"] = "PASS";
        writeReport(output, report);
        std::cout << "U11 PRESENTATION PROBE PASS\n";
        return 0;
    } catch (const std::exception& error) {
        report["status"] = "FAIL";
        report["error"] = error.what();
        std::cerr << "U11 PRESENTATION PROBE FAILED: " << error.what() << '\n';
        if (!output.empty()) {
            try { writeReport(output, report); } catch (...) {}
        }
        return 1;
    }
}
