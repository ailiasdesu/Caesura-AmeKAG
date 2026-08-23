// PackagingService.cpp -- build/package logic extracted from EditorServer.cpp.

#include "PackagingService.h"

#include "archive/api/IArchiveWriter.h"

#include <cstdint>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace Caesura {
namespace rpc {
namespace service {

namespace {

bool isStoryPathAllowed(const std::string& path) {
    static const char* const kPrefixes[] = {
        "assets/", "demo/", "tests/projects/", "projects/"};
    for (const char* prefix : kPrefixes) {
        if (path.rfind(prefix, 0) == 0) return true;
    }
    return false;
}

bool sanitizeWebOutName(const std::string& raw, std::string& out) {
    out.clear();
    for (unsigned char ch : raw) {
        if (std::isalnum(ch) || ch == '_' || ch == '-') {
            out.push_back(static_cast<char>(ch));
        }
    }
    return !out.empty();
}

std::string logTailOf(const std::string& log) {
    constexpr size_t kMaxLines = 30;
    std::vector<std::string> lines;
    std::istringstream stream(log);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    if (lines.size() > kMaxLines) {
        lines.erase(lines.begin(),
                    lines.end() - static_cast<std::ptrdiff_t>(kMaxLines));
    }
    std::string joined;
    for (const auto& entry : lines) {
        joined += entry;
        joined += '\n';
    }
    return joined;
}

std::string confineToBuild(const std::string& p) {
    if (p.empty()) return {};
    std::string norm = p;
    for (auto& ch : norm) {
        if (ch == static_cast<char>(92)) ch = '/';
    }
    if (norm.find("..") != std::string::npos) return {};
    if (norm.find(':') != std::string::npos) return {};
    if (norm[0] == '/') return {};
    if (norm.rfind("build/", 0) != 0) norm = "build/" + norm;
    return norm;
}

} // namespace

ServiceResult PackagingService::build(const std::string& rawOutputPath,
                                      const std::string& rawKeyPath) {
    std::string outputPath = confineToBuild(rawOutputPath);
    std::string keyPath = confineToBuild(rawKeyPath);
    if (outputPath.empty() || keyPath.empty()) {
        return ServiceResult::err(400, "outputPath/keyPath must be relative paths under build/");
    }

    std::vector<std::pair<std::string, std::string>> files;
    for (const char* dir : {"scripts", "assets"}) {
        if (!fs::exists(dir)) continue;
        try {
            for (const auto& entry : fs::recursive_directory_iterator(dir)) {
                if (!entry.is_regular_file()) continue;
                std::string rel = entry.path().string();
                for (auto& c : rel) if (c == '\\') c = '/';
                files.push_back({rel, entry.path().string()});
            }
        } catch (...) {}
    }
    if (files.empty()) return ServiceResult::err(400, "No files to package");

    std::error_code ec;
    fs::create_directories("build", ec);

    auto writer = m_writerFactory ? m_writerFactory() : nullptr;
    if (!writer) return ServiceResult::err(503, "Archive writer is not configured");
    if (!writer->create(outputPath, keyPath, keyPath + ".pub")) {
        return ServiceResult::err(500, "Failed to create CARC archive");
    }
    for (const auto& [relPath, diskPath] : files) {
        std::ifstream ifs(diskPath, std::ios::binary);
        if (!ifs.is_open()) continue;
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)),
                                  std::istreambuf_iterator<char>());
        writer->addFile(relPath, data.data(), data.size());
    }
    if (!writer->finalize()) return ServiceResult::err(500, "Failed to finalize CARC archive");

    std::error_code se;
    const uintmax_t fileSize = fs::file_size(outputPath, se);
    return ServiceResult::ok({{"status", "ok"},
                              {"path", outputPath},
                              {"size", static_cast<unsigned long long>(fileSize)},
                              {"files", files.size()}});
}

ServiceResult PackagingService::packageWeb(const std::string& storyPath,
                                           const std::string& outName) {
    std::string sp = storyPath;
    for (auto& ch : sp) {
        if (ch == static_cast<char>(92)) ch = '/';
    }
    if (sp.find("..") != std::string::npos) {
        return ServiceResult::err(400, "storyPath must not contain '..'");
    }
    if (sp.find(':') != std::string::npos) {
        return ServiceResult::err(400, "storyPath must be a relative repository path (no drive/scheme)");
    }
    if (!sp.empty() && sp.front() == '/') {
        return ServiceResult::err(400, "storyPath must be a relative repository path");
    }
    for (unsigned char ch : sp) {
        const bool safe = std::isalnum(ch) || ch == '/' || ch == '_' ||
                          ch == '-' || ch == '.' || ch == ' ';
        if (!safe) return ServiceResult::err(400, "storyPath contains unsupported characters");
    }
    if (!isStoryPathAllowed(sp)) {
        return ServiceResult::err(400, "storyPath must live under assets/, demo/, tests/projects/ or projects/");
    }

    std::string name = outName.empty() ? "example_game" : outName;
    if (!outName.empty() && !sanitizeWebOutName(outName, name)) {
        return ServiceResult::err(400, "outName contains no usable characters ([A-Za-z0-9_-])");
    }
    const std::string outDir = "dist/" + name;

    fs::path scriptPath = m_ctx.sourceRoot() / "scripts" / "package_game.sh";
    if (!fs::exists(scriptPath)) {
        return ServiceResult{503, {{"ok", false},
                                   {"error", "scripts/package_game.sh not found (engine must run inside the repository)"}}};
    }

    std::string scriptArg;
    try {
        const fs::path repoRoot = scriptPath.parent_path().parent_path();
        if (!fs::exists(repoRoot / fs::path(sp))) {
            return ServiceResult::err(400, "storyPath not found: " + sp);
        }
        scriptArg = fs::relative(scriptPath, m_ctx.buildRoot()).generic_string();
    } catch (...) {
        scriptArg.clear();
    }
    if (scriptArg.empty()) {
        return ServiceResult{503, {{"ok", false},
                                   {"error", "failed to resolve scripts/package_game.sh relative to the engine working directory"}}};
    }
    for (auto& ch : scriptArg) {
        if (ch == static_cast<char>(92)) ch = '/';
    }

    const std::string command = "bash \"" + scriptArg + "\" --out \"" +
                                outDir + "\" \"" + sp + "\" 2>&1";
    std::string output;
#if defined(_WIN32)
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = ::popen(command.c_str(), "r");
#endif
    if (!pipe) {
        return ServiceResult{500, {{"ok", false},
                                   {"error", "failed to spawn bash (is git bash on PATH?)"},
                                   {"outputDir", outDir}}};
    }
    constexpr size_t kMaxLogBytes = 1u << 20;
    char buffer[4096];
    size_t chunk = 0;
    while ((chunk = fread(buffer, 1, sizeof(buffer), pipe)) > 0) {
        if (output.size() < kMaxLogBytes) output.append(buffer, chunk);
    }
#if defined(_WIN32)
    const int exitCode = _pclose(pipe);
#else
    const int exitCode = ::pclose(pipe);
#endif
    const std::string tail = logTailOf(output);

    if (exitCode != 0) {
        return ServiceResult{500, {{"status", "error"},
                                   {"ok", false},
                                   {"error", "package_game.sh failed with exit code " +
                                                 std::to_string(exitCode)},
                                   {"outputDir", outDir},
                                   {"logTail", tail}}};
    }
    return ServiceResult::ok({{"status", "ok"},
                              {"ok", true},
                              {"outputDir", outDir},
                              {"logTail", tail}});
}

} // namespace service
} // namespace rpc
} // namespace Caesura
