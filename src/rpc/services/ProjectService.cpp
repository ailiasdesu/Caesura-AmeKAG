// ProjectService.cpp -- implementation extracted from EditorServer.cpp
// (task book §14 layering; behavior parity is enforced by headless_http_smoke).

#include "ProjectService.h"

#include <cstdint>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace Caesura {
namespace rpc {
namespace service {

namespace {

std::string nowIsoUtc() {
    const std::time_t t = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

Json defaultProjectMeta(const std::string& name) {
    const std::string now = nowIsoUtc();
    return Json{
        {"name", name},
        {"template", ""},
        {"version", "1.0"},
        {"language", "zh"},
        {"description", ""},
        {"created", now},
        {"modified", now},
    };
}

void overlayStringMeta(Json& base, const Json& stored) {
    for (auto it = stored.begin(); it != stored.end(); ++it) {
        if (it.value().is_string()) base[it.key()] = it.value();
    }
}

bool validProjectName(const std::string& name) {
    if (name.empty()) return false;
    for (char c : name) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')) {
            return false;
        }
    }
    return true;
}

} // namespace

ServiceResult ProjectService::listTemplates() {
    Json out = Json::array();
    const fs::path root = m_ctx.templatesRoot();
    std::ifstream mf(root / "manifest.json");
    if (mf) {
        try {
            auto m = Json::parse(mf);
            if (m.contains("templates") && m["templates"].is_array()) {
                out = m["templates"];
            }
        } catch (const std::exception&) {
            out = Json::array();
        }
    }
    if (out.empty()) {
        std::error_code ec;
        for (const auto& e : fs::directory_iterator(root, ec)) {
            if (ec || !e.is_directory()) continue;
            out.push_back({{"id", e.path().filename().string()},
                           {"name", e.path().filename().string()},
                           {"description", ""},
                           {"dir", e.path().filename().string()}});
        }
    }
    return ServiceResult::ok(out);
}

ServiceResult ProjectService::create(const std::string& templateId,
                                     const std::string& name) {
  try {
    return createImpl(templateId, name);
  } catch (const std::exception& ex) {
    return ServiceResult::err(500, std::string("create failed: ") + ex.what());
  }
}

ServiceResult ProjectService::createImpl(const std::string& templateId,
                                         const std::string& name) {
    if (name.empty()) return ServiceResult::err(400, "Missing project name");
    const bool known = templateId == "blank" || templateId == "basic" ||
                       templateId == "live2d" || templateId == "kag3" ||
                       templateId == "showcase" || templateId.empty();
    if (!known) return ServiceResult::err(400, "Unknown template");
    if (!validProjectName(name)) return ServiceResult::err(400, "Invalid project name");

    fs::path projectsRoot = m_ctx.projectRoot();
    std::error_code ec;
    fs::create_directories(projectsRoot, ec);

    fs::path dest = projectsRoot / name;
    if (fs::exists(dest, ec)) return ServiceResult::err(409, "Project already exists");

    const std::string dir = templateId.empty() ? "basic" : templateId;
    const std::string safeDir = [&] {
        std::string n = dir;
        for (auto& ch : n) if (ch == char(92)) ch = '/';
        return n;
    }();
    if (safeDir.find("..") != std::string::npos || safeDir.front() == '/' ||
        safeDir.find(':') != std::string::npos) {
        return ServiceResult::err(400, "Template not found");
    }
    fs::path src = m_ctx.templatesRoot() / safeDir;
    if (!fs::exists(src, ec)) return ServiceResult::err(400, "Template not found");

    std::error_code copyEc;
    fs::copy(src, dest, fs::copy_options::recursive |
                            fs::copy_options::overwrite_existing, copyEc);
    if (copyEc) return ServiceResult::err(500, "Failed to copy template");

    try {
        fs::path metaFile = dest / "caesura.project.json";
        if (fs::exists(metaFile, ec)) {
            std::ifstream metaIn(metaFile);
            std::ostringstream metaBuf;
            metaBuf << metaIn.rdbuf();
            auto meta = Json::parse(metaBuf.str());
            meta["name"] = name;
            meta["template"] = templateId.empty() ? "basic" : templateId;
            meta["created"] = nowIsoUtc();
            meta["modified"] = meta["created"];
            std::ofstream out(metaFile, std::ios::trunc);
            out << meta.dump(2);
        }
    } catch (const std::exception&) { /* best-effort */ }

    return ServiceResult::ok({{"ok", true}, {"path", dest.string()}});
}

ServiceResult ProjectService::duplicate(const std::string& src,
                                        const std::string& name) {
    if (src.empty() || name.empty()) return ServiceResult::err(400, "Missing srcPath/name");
    if (!validProjectName(name)) return ServiceResult::err(400, "Invalid project name");

    fs::path projectsRoot = m_ctx.projectRoot();
    std::error_code ec;
    const std::string norm = [&] {
        std::string safe = name;
        for (auto& ch : safe) if (ch == char(92)) ch = '/';
        return safe;
    }();
    fs::path srcDir = projectsRoot / norm;
    // src may be "projects/<name>" reference or bare <name>
    std::string srcName = src;
    const std::string prefix = "projects/";
    if (srcName.rfind(prefix, 0) == 0) srcName = srcName.substr(prefix.size());
    if (!validProjectName(srcName)) return ServiceResult::err(404, "Source project not found");
    fs::path srcPath = projectsRoot / srcName;
    if (!fs::exists(srcPath, ec) || !fs::is_directory(srcPath, ec)) {
        return ServiceResult::err(404, "Source project not found");
    }
    fs::path dest = projectsRoot / norm;
    if (fs::exists(dest, ec)) return ServiceResult::err(409, "Project already exists");

    std::error_code copyEc;
    fs::copy(srcPath, dest, fs::copy_options::recursive |
                                fs::copy_options::overwrite_existing, copyEc);
    if (copyEc) return ServiceResult::err(500, "Failed to duplicate");
    return ServiceResult::ok({{"ok", true}, {"path", dest.string()}});
}

ServiceResult ProjectService::importProject(const std::string& srcPath,
                                            const std::string& name) {
    if (srcPath.empty() || name.empty()) return ServiceResult::err(400, "Missing srcPath/name");
    if (!validProjectName(name)) return ServiceResult::err(400, "Invalid project name");

    fs::path src(srcPath);
    std::error_code ec;
    if (!fs::exists(src, ec) || !fs::is_directory(src, ec)) {
        return ServiceResult::err(404, "Source directory not found");
    }
    if (!fs::exists(src / "story.ks", ec) && !fs::exists(src / "entry.lua", ec)) {
        return ServiceResult::err(400, "Not a Caesura project (missing story.ks/entry.lua)");
    }
    fs::path projectsRoot = m_ctx.projectRoot();
    std::error_code mkEc;
    fs::create_directories(projectsRoot, mkEc);
    fs::path dest = projectsRoot / name;
    if (fs::exists(dest, ec)) return ServiceResult::err(409, "Project already exists");

    std::error_code copyEc;
    fs::copy(src, dest, fs::copy_options::recursive |
                            fs::copy_options::overwrite_existing, copyEc);
    if (copyEc) return ServiceResult::err(500, "Failed to import project");
    return ServiceResult::ok({{"ok", true}, {"path", dest.string()}});
}

ServiceResult ProjectService::list() {
    Json out = Json::array();
    try {
        fs::path projectsRoot = m_ctx.projectRoot();
        std::error_code ec;
        if (!fs::exists(projectsRoot, ec)) {
            fs::create_directories(projectsRoot, ec);
            return ServiceResult::ok(out);
        }
        for (const auto& e : fs::directory_iterator(projectsRoot, ec)) {
            if (ec) break;
            std::error_code de;
            if (!e.is_directory(de) || de) continue;
            const std::string name = e.path().filename().string();
            std::error_code te;
            const auto t = fs::last_write_time(e.path(), te);
            Json entry = {{"path", ("projects/" + name)},
                          {"name", name},
                          {"template", ""},
                          {"modified", te ? std::to_string(static_cast<long long>(t.time_since_epoch().count())) : ""}};
            fs::path metaFile = e.path() / "caesura.project.json";
            std::error_code me;
            if (fs::exists(metaFile, me)) {
                try {
                    std::ifstream in(metaFile);
                    std::ostringstream buf;
                    buf << in.rdbuf();
                    auto meta = Json::parse(buf.str());
                    if (meta.contains("template") && meta["template"].is_string())
                        entry["template"] = meta["template"];
                } catch (const std::exception&) { /* keep defaults */ }
            }
            out.push_back(std::move(entry));
        }
    } catch (const std::exception&) {
        return ServiceResult::ok(Json::array());
    }
    return ServiceResult::ok(out);
}

ServiceResult ProjectService::metaGet(const std::string& path) {
    // path must be "projects/<name>" (confined) -- reject everything else.
    const std::string prefix = "projects/";
    std::string norm = path;
    if (norm.find("..") != std::string::npos || norm.empty()) {
        return ServiceResult::err(400, "Invalid path");
    }
    if (!norm.starts_with(prefix)) return ServiceResult::err(400, "Invalid path");
    const std::string name = norm.substr(prefix.size());
    if (!validProjectName(name)) return ServiceResult::err(400, "Invalid path");

    const fs::path projectDir = m_ctx.projectRoot() / name;
    std::error_code ec;
    if (!fs::exists(projectDir, ec)) return ServiceResult::err(404, "Project not found");

    Json meta = defaultProjectMeta(name);
    bool inferred = true;
    const fs::path metaFile = projectDir / "caesura.project.json";
    std::error_code me;
    if (fs::exists(metaFile, me)) {
        try {
            std::ifstream in(metaFile);
            std::ostringstream buf;
            buf << in.rdbuf();
            overlayStringMeta(meta, Json::parse(buf.str()));
            inferred = false;
        } catch (const std::exception&) { /* keep defaults */ }
    }
    return ServiceResult::ok({{"ok", true},
                              {"path", projectDir.string()},
                              {"inferred", inferred},
                              {"meta", std::move(meta)}});
}

ServiceResult ProjectService::metaSave(const std::string& path, Json meta) {
    // Validation parity with the original handler (smoke covers it):
    // language is a closed set; description is bounded free text.
    if (meta.contains("language") && meta["language"].is_string()) {
        const std::string lang = meta["language"].get<std::string>();
        if (lang != "zh" && lang != "en" && lang != "ja") {
            return ServiceResult::err(400, "language must be one of zh/en/ja");
        }
    } else if (meta.contains("language") && !meta["language"].is_null()) {
        return ServiceResult::err(400, "language must be a string");
    }
    if (meta.contains("description") && meta["description"].is_string() &&
        meta["description"].get<std::string>().size() > 2000) {
        return ServiceResult::err(400, "description too long (max 2000)");
    }
    // same path confinement as metaGet
    const std::string prefix = "projects/";
    std::string norm = path;
    if (norm.find("..") != std::string::npos || norm.empty() ||
        !norm.starts_with(prefix)) {
        return ServiceResult::err(400, "Invalid path");
    }
    const std::string name = norm.substr(prefix.size());
    if (!validProjectName(name)) return ServiceResult::err(400, "Invalid path");

    const fs::path projectDir = m_ctx.projectRoot() / name;
    std::error_code ec;
    if (!fs::exists(projectDir, ec)) return ServiceResult::err(404, "Project not found");

    Json final_meta = defaultProjectMeta(name);
    overlayStringMeta(final_meta, meta);
    try {
        const std::string now = nowIsoUtc();
        final_meta["modified"] = now;
        std::ofstream out(projectDir / "caesura.project.json", std::ios::trunc);
        out << final_meta.dump(2);
    } catch (const std::exception&) {
        return ServiceResult::err(500, "Failed to save metadata");
    }
    return ServiceResult::ok({{"ok", true},
                              {"path", projectDir.string()},
                              {"meta", std::move(final_meta)}});
}

} // namespace service
} // namespace rpc
} // namespace Caesura
