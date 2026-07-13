#include "doctest.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

bool isSourceFile(const std::filesystem::path& path) {
    const auto ext = path.extension().string();
    return ext == ".c" || ext == ".cc" || ext == ".cpp" || ext == ".cxx" ||
           ext == ".h" || ext == ".hh" || ext == ".hpp" || ext == ".hxx";
}

std::filesystem::path findRepoRoot() {
    auto path = std::filesystem::current_path();
    while (!path.empty()) {
        if (std::filesystem::exists(path / "src") &&
            std::filesystem::exists(path / "tests" / "cpp")) {
            return path;
        }
        const auto parent = path.parent_path();
        if (parent == path) break;
        path = parent;
    }
    return {};
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}

size_t countOccurrences(std::string_view text, std::string_view needle) {
    if (needle.empty()) return 0;

    size_t count = 0;
    size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string_view::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

bool isValidUtf8(std::string_view bytes) {
    size_t i = 0;
    while (i < bytes.size()) {
        const auto c = static_cast<unsigned char>(bytes[i]);
        if (c <= 0x7F) {
            ++i;
            continue;
        }

        size_t continuationCount = 0;
        uint32_t codePoint = 0;
        if ((c & 0xE0) == 0xC0) {
            continuationCount = 1;
            codePoint = c & 0x1F;
            if (codePoint == 0) return false;
        } else if ((c & 0xF0) == 0xE0) {
            continuationCount = 2;
            codePoint = c & 0x0F;
        } else if ((c & 0xF8) == 0xF0) {
            continuationCount = 3;
            codePoint = c & 0x07;
        } else {
            return false;
        }

        if (i + continuationCount >= bytes.size()) return false;
        for (size_t j = 1; j <= continuationCount; ++j) {
            const auto next = static_cast<unsigned char>(bytes[i + j]);
            if ((next & 0xC0) != 0x80) return false;
            codePoint = (codePoint << 6) | (next & 0x3F);
        }

        if ((continuationCount == 1 && codePoint < 0x80) ||
            (continuationCount == 2 && codePoint < 0x800) ||
            (continuationCount == 3 && codePoint < 0x10000) ||
            codePoint > 0x10FFFF ||
            (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
            return false;
        }

        i += continuationCount + 1;
    }
    return true;
}

void collectInvalidUtf8Files(const std::filesystem::path& root,
                             std::vector<std::filesystem::path>& invalid) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file() || !isSourceFile(entry.path())) continue;
        if (!isValidUtf8(readFile(entry.path()))) {
            invalid.push_back(entry.path());
        }
    }
}

} // namespace

TEST_CASE("C++ source files are valid UTF-8") {
    const auto repoRoot = findRepoRoot();
    REQUIRE_FALSE(repoRoot.empty());

    std::vector<std::filesystem::path> invalid;
    collectInvalidUtf8Files(repoRoot / "src", invalid);
    collectInvalidUtf8Files(repoRoot / "tests" / "cpp", invalid);

    std::ostringstream message;
    message << "Invalid UTF-8 source files:";
    for (const auto& path : invalid) {
        message << "\n  " << std::filesystem::relative(path, repoRoot).generic_string();
    }
    INFO(message.str());
    CHECK(invalid.empty());
}

TEST_CASE("BackendRegistry implementation depends only on render interface") {
    const auto repoRoot = findRepoRoot();
    REQUIRE_FALSE(repoRoot.empty());

    const std::string source = readFile(repoRoot / "src" / "di" / "BackendRegistry.cpp");
    CHECK(source.find("BgfxRenderDevice") == std::string::npos);
    CHECK(source.find("../render/BgfxRenderDevice.h") == std::string::npos);
}

TEST_CASE("Render device interface does not expose bgfx handle types") {
    const auto repoRoot = findRepoRoot();
    REQUIRE_FALSE(repoRoot.empty());

    const std::string source = readFile(repoRoot / "src" / "render" / "api" / "IRenderDevice.h");
    CHECK(source.find("#include <bgfx/bgfx.h>") == std::string::npos);
    CHECK(source.find("../render/BgfxDebugCallback.h") == std::string::npos);
    CHECK(source.find("BgfxDebugCallback") == std::string::npos);
    CHECK(source.find("bgfx::") == std::string::npos);
}

TEST_CASE("Backend interfaces do not provide concrete default behavior") {
    const auto repoRoot = findRepoRoot();
    REQUIRE_FALSE(repoRoot.empty());

    const std::string render = readFile(repoRoot / "src" / "render" / "api" / "IRenderDevice.h");
    CHECK(render.find("virtual void beginShutdown() {") == std::string::npos);
    CHECK(render.find("virtual void flagDeviceLost() {") == std::string::npos);
    CHECK(render.find("virtual bool consumeDeviceLost() {") == std::string::npos);
    CHECK(render.find("virtual RenderUniformHandle getDefaultSampler() const {") == std::string::npos);
    CHECK(render.find("virtual RenderProgramHandle getFallbackProgram() const {") == std::string::npos);
    CHECK(render.find("virtual RenderRuntimeInfo getRuntimeInfo() const {") == std::string::npos);
    CHECK(render.find("virtual bool setPreferredBackend(const char*) {") == std::string::npos);

    const std::string saves = readFile(repoRoot / "src" / "storage" / "api" / "ISaveProvider.h");
    CHECK(saves.find("virtual bool pushToCloud(const std::string& slotPath) {") == std::string::npos);
    CHECK(saves.find("virtual bool pullFromCloud(const std::string& slotPath) {") == std::string::npos);
    CHECK(saves.find("virtual bool supportsCloudSync() const {") == std::string::npos);
}

TEST_CASE("Main entry point delegates script bootstrap helpers") {
    const auto repoRoot = findRepoRoot();
    REQUIRE_FALSE(repoRoot.empty());

    const auto helperPath = repoRoot / "src" / "entry" / "StartupScripts.cpp";
    const std::string source = readFile(repoRoot / "src" / "main.cpp");
    CHECK(source.find("fopen(\"scripts/kag/init.lua\", \"r\")") == std::string::npos);
    CHECK(source.find("lua_getglobal(L, \"package\")") == std::string::npos);
    CHECK(source.find("Caesura::discoverStartupScriptDir()") != std::string::npos);
    CHECK(source.find("Caesura::configureStartupLuaPath(L, scriptDir)") != std::string::npos);
    REQUIRE(std::filesystem::exists(helperPath));

    const std::string helper = readFile(helperPath);
    CHECK(countOccurrences(helper, "fopen(\"scripts/kag/init.lua\", \"r\")") == 1);
    CHECK(countOccurrences(helper, "lua_getglobal(L, \"package\")") == 1);
}

TEST_CASE("Main entry point leaves GPU monitor selection to Engine") {
    const auto repoRoot = findRepoRoot();
    REQUIRE_FALSE(repoRoot.empty());

    const std::string source = readFile(repoRoot / "src" / "main.cpp");
    CHECK(source.find("#include \"render/GpuMonitor.h\"") == std::string::npos);
    CHECK(source.find("config.gpuMonitor") == std::string::npos);
}

TEST_CASE("Main entry point avoids unused Steam concrete backend include") {
    const auto repoRoot = findRepoRoot();
    REQUIRE_FALSE(repoRoot.empty());

    const std::string source = readFile(repoRoot / "src" / "main.cpp");
    CHECK(source.find("#include \"steam/SteamBackend.h\"") == std::string::npos);
}

TEST_CASE("Main entry point leaves default optional backends to Engine") {
    const auto repoRoot = findRepoRoot();
    REQUIRE_FALSE(repoRoot.empty());

    const std::string source = readFile(repoRoot / "src" / "main.cpp");
    CHECK(source.find("#include \"audio/NullAudioBackend.h\"") == std::string::npos);
    CHECK(source.find("#include \"minigame/NullMiniGameBackend.h\"") == std::string::npos);
    CHECK(source.find("#include \"live2d/NullAnimationBackend.h\"") == std::string::npos);
    CHECK(source.find("#include \"steam/NullSteamBackend.h\"") == std::string::npos);
    CHECK(source.find("#include \"input/InputRouter.h\"") == std::string::npos);
    CHECK(source.find("#include \"render/VideoPlayer.h\"") == std::string::npos);
    CHECK(source.find("config.audio    = new Caesura::NullAudioBackend") == std::string::npos);
    CHECK(source.find("config.miniGame = new Caesura::NullMiniGameBackend") == std::string::npos);
    CHECK(source.find("config.animation") == std::string::npos);
    CHECK(source.find("config.steam") == std::string::npos);
    CHECK(source.find("config.lua         =") == std::string::npos);
    CHECK(source.find("config.inputRouter") == std::string::npos);
    CHECK(source.find("config.videoPlayer") == std::string::npos);
}

TEST_CASE("Main entry point uses texture manager interface") {
    const auto repoRoot = findRepoRoot();
    REQUIRE_FALSE(repoRoot.empty());

    const std::string source = readFile(repoRoot / "src" / "main.cpp");
    CHECK(source.find("#include \"render/TextureManager.h\"") == std::string::npos);
    CHECK(source.find("#include \"render/api/ITextureManager.h\"") == std::string::npos);
    CHECK(source.find("#include \"di/BackendRegistry.h\"") == std::string::npos);
    CHECK(source.find("BackendRegistry::instance().getTextureManager()") == std::string::npos);
    CHECK(source.find("setDevMode(devMode)") == std::string::npos);
    CHECK(source.find("Caesura::applyDevModeToTextureManager(L)") != std::string::npos);
}

TEST_CASE("Main entry point delegates CARC startup validation") {
    const auto repoRoot = findRepoRoot();
    REQUIRE_FALSE(repoRoot.empty());

    const auto helperPath = repoRoot / "src" / "entry" / "StartupValidation.cpp";
    const std::string source = readFile(repoRoot / "src" / "main.cpp");
    CHECK(source.find("#include \"archive/CARCReader.h\"") == std::string::npos);
    CHECK(source.find("carc::CARCReader") == std::string::npos);
    CHECK(source.find("verifySignature()") == std::string::npos);
    CHECK(source.find("Caesura::validateCarcOnStartup(L)") != std::string::npos);
    REQUIRE(std::filesystem::exists(helperPath));

    const std::string helper = readFile(helperPath);
    CHECK(countOccurrences(helper, "[main] CARC startup validation enabled.") == 1);
    CHECK(helper.find("carc::CARCReader") != std::string::npos);
    CHECK(helper.find("verifySignature()") != std::string::npos);
}

TEST_CASE("Install layout includes configured demo entry script") {
    const auto repoRoot = findRepoRoot();
    REQUIRE_FALSE(repoRoot.empty());

    const std::string config = readFile(repoRoot / "scripts" / "config.lua");
    CHECK(config.find("config.entry_script = \"../demo/entry.lua\"") != std::string::npos);

    const std::string cmake = readFile(repoRoot / "CMakeLists.txt");
    CHECK(cmake.find("install(DIRECTORY scripts/ DESTINATION scripts)") != std::string::npos);
    CHECK(cmake.find("install(DIRECTORY demo/ DESTINATION demo)") != std::string::npos);
}

TEST_CASE("Install layout includes FFmpeg runtime DLLs when FFmpeg is bundled") {
    const auto repoRoot = findRepoRoot();
    REQUIRE_FALSE(repoRoot.empty());

    const std::string cmake = readFile(repoRoot / "CMakeLists.txt");
    CHECK(cmake.find("file(COPY ${FFMPEG_BIN_DIR}/ DESTINATION ${CMAKE_BINARY_DIR}/${CFG}") != std::string::npos);
    CHECK(cmake.find("install(DIRECTORY ${FFMPEG_BIN_DIR}/ DESTINATION .") != std::string::npos);
    CHECK(cmake.find("FILES_MATCHING PATTERN \"*.dll\"") != std::string::npos);
}

TEST_CASE("Engine core avoids unused concrete adapter dependencies") {
    const auto repoRoot = findRepoRoot();
    REQUIRE_FALSE(repoRoot.empty());

    const std::string source = readFile(repoRoot / "src" / "entry" / "Engine.cpp");
    CHECK(source.find("../platform/SDL3PlatformBackend.h") == std::string::npos);
    CHECK(source.find("../audio/SoLoudAudioEngine.h") == std::string::npos);
    CHECK(source.find("SoLoudAudioEngine") == std::string::npos);
    CHECK(source.find("../render/BgfxRenderDevice.h") == std::string::npos);
    CHECK(source.find("../script/bindings/RenderBinding.h") == std::string::npos);
    CHECK(source.find("../minigame/BgfxMiniGameBackend.h") == std::string::npos);
}

TEST_CASE("Engine core delegates renderer runtime operations to render interface") {
    const auto repoRoot = findRepoRoot();
    REQUIRE_FALSE(repoRoot.empty());

    const std::string source = readFile(repoRoot / "src" / "entry" / "Engine.cpp");
    CHECK(source.find("#include <bgfx/bgfx.h>") == std::string::npos);
    CHECK(source.find("bgfx::") == std::string::npos);
    CHECK(source.find("BGFX_") == std::string::npos);
    CHECK(source.find("bgfx shutdown complete") == std::string::npos);
    CHECK(source.find("bgfx reinit") == std::string::npos);
}

TEST_CASE("Engine core delegates CARC asset provider registration") {
    const auto repoRoot = findRepoRoot();
    REQUIRE_FALSE(repoRoot.empty());

    const std::string source = readFile(repoRoot / "src" / "entry" / "Engine.cpp");
    CHECK(source.find("../archive/CARCReader.h") == std::string::npos);
    CHECK(source.find("../archive/CarcAssetProvider.h") == std::string::npos);
    CHECK(source.find("carc::CARCReader") == std::string::npos);
    CHECK(source.find("carc::CarcAssetProvider") == std::string::npos);
}

TEST_CASE("Engine core delegates default backend construction") {
    const auto repoRoot = findRepoRoot();
    REQUIRE_FALSE(repoRoot.empty());

    const std::string source = readFile(repoRoot / "src" / "entry" / "Engine.cpp");
    CHECK(source.find("../audio/NullAudioBackend.h") == std::string::npos);
    CHECK(source.find("../minigame/NullMiniGameBackend.h") == std::string::npos);
    CHECK(source.find("../live2d/NullAnimationBackend.h") == std::string::npos);
    CHECK(source.find("../steam/SteamBackend.h") == std::string::npos);
    CHECK(source.find("../steam/NullSteamBackend.h") == std::string::npos);
    CHECK(source.find("../live2d/Live2D/Live2DBackend.h") == std::string::npos);
    CHECK(source.find("NullAudioBackend") == std::string::npos);
    CHECK(source.find("NullMiniGameBackend") == std::string::npos);
    CHECK(source.find("NullAnimationBackend") == std::string::npos);
    CHECK(source.find("std::make_unique<SteamBackend>") == std::string::npos);
    CHECK(source.find("std::make_unique<NullSteamBackend>") == std::string::npos);
    CHECK(source.find("Live2DBackend") == std::string::npos);
    CHECK(source.find("static_cast<Live2DBackend&>") == std::string::npos);
}

TEST_CASE("Engine core leaves save system initialization to storage binding") {
    const auto repoRoot = findRepoRoot();
    REQUIRE_FALSE(repoRoot.empty());

    const std::string engine = readFile(repoRoot / "src" / "entry" / "Engine.cpp");
    CHECK(engine.find("../storage/SaveManager.h") == std::string::npos);
    CHECK(engine.find("SaveManager::instance().init(\"saves/\")") == std::string::npos);

    const std::string binding = readFile(repoRoot / "src" / "storage" / "SaveBinding.cpp");
    CHECK(binding.find("SaveManager::instance().init(\"saves/\")") != std::string::npos);
}

TEST_CASE("Engine core delegates Lua registry service injection") {
    const auto repoRoot = findRepoRoot();
    REQUIRE_FALSE(repoRoot.empty());

    const auto helperPath = repoRoot / "src" / "entry" / "Engine_LuaRegistry.cpp";
    const std::string source = readFile(repoRoot / "src" / "entry" / "Engine.cpp");

    CHECK(source.find("\"Caesura.RenderDevice\"") == std::string::npos);
    CHECK(source.find("\"Caesura.AudioBackend\"") == std::string::npos);
    CHECK(source.find("\"Caesura.PlatformBackend\"") == std::string::npos);
    CHECK(source.find("\"Caesura.InputRouter\"") == std::string::npos);
    CHECK(source.find("\"Caesura.VideoPlayer\"") == std::string::npos);
    CHECK(source.find("\"Caesura.TextureManager\"") == std::string::npos);
    CHECK(source.find("\"Caesura.AsyncLoader\"") == std::string::npos);
    CHECK(source.find("\"Caesura.DebugManager\"") == std::string::npos);
    CHECK(source.find("\"Caesura.MiniGameBackend\"") == std::string::npos);
    CHECK(source.find("registerEngineLuaRegistryServices(") != std::string::npos);
    CHECK(source.find("registerMiniGameLuaRegistryService(") != std::string::npos);
    REQUIRE(std::filesystem::exists(helperPath));

    const std::string helper = readFile(helperPath);
    CHECK(countOccurrences(helper, "\"Caesura.RenderDevice\"") == 1);
    CHECK(countOccurrences(helper, "\"Caesura.AudioBackend\"") == 1);
    CHECK(countOccurrences(helper, "\"Caesura.PlatformBackend\"") == 1);
    CHECK(countOccurrences(helper, "\"Caesura.InputRouter\"") == 1);
    CHECK(countOccurrences(helper, "\"Caesura.VideoPlayer\"") == 1);
    CHECK(countOccurrences(helper, "\"Caesura.TextureManager\"") == 1);
    CHECK(countOccurrences(helper, "\"Caesura.AsyncLoader\"") == 1);
    CHECK(countOccurrences(helper, "\"Caesura.DebugManager\"") == 1);
    CHECK(countOccurrences(helper, "\"Caesura.MiniGameBackend\"") == 1);
}
