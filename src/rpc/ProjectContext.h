#pragma once
// ProjectContext.h -- single source of truth for filesystem roots in the
// editor layer (Validation-Release task book §7 "ProjectContext / Path
// resolver 收口").
//
// Rule: business code must not write "fs::current_path() / ..." to guess
// engine roots. Everything resolves through ProjectContext:
//
//   ProjectContext
//   ├── sourceRoot   (repo root; CAESURA_SOURCE_DIR macro or upward walk)
//   ├── projectRoot  (sourceRoot/projects)
//   ├── templatesRoot (sourceRoot/tools/project_templates)
//   ├── assetRoot    (sourceRoot/assets)
//   ├── buildRoot    (directory the editor executable lives in)
//   ├── outputRoot   (buildRoot/dist -- where packaging writes)
//   └── platform     (windows|linux|macos|other)
//
// Design: header-only value type; constructed once from the engine's
// environment. Kept inside src/rpc (editor-internal, not api/) so editor
// RPC may evolve freely without touching the public interface surface.

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace Caesura {

class ProjectContext {
public:
    // Build the context from the executable environment.
    //   useMacroFirst: prefer the CMake-injected CAESURA_SOURCE_DIR so
    //   out-of-tree builds (WSL/CI) resolve to the real source tree.
    static ProjectContext fromEnvironment(bool useMacroFirst = true) {
        ProjectContext c;
        c.m_sourceRoot = discoverSourceRoot(useMacroFirst);
        c.m_buildRoot = fs::current_path();  // exe's working directory
        c.m_platform = detectPlatform();
        return c;
    }

    fs::path sourceRoot() const { return m_sourceRoot; }
    fs::path projectRoot() const { return m_sourceRoot / "projects"; }
    fs::path templatesRoot() const { return m_sourceRoot / "tools" / "project_templates"; }
    fs::path assetRoot() const { return m_sourceRoot / "assets"; }
    fs::path buildRoot() const { return m_buildRoot; }
    fs::path outputRoot() const { return m_buildRoot / "dist"; }
    const std::string& platform() const { return m_platform; }

    // Resolve a story path that is repo-relative (e.g. "demo/example_game/story.ks"
    // or "tests/projects/first_vn/story.ks") against the source root.
    fs::path resolveStory(const std::string& relative) const {
        return m_sourceRoot / fs::path(relative);
    }

private:
    static fs::path discoverSourceRoot(bool useMacroFirst) {
#ifdef CAESURA_SOURCE_DIR
        if (useMacroFirst) {
            const fs::path fromMacro(CAESURA_SOURCE_DIR);
            if (fs::exists(fromMacro / "tools" / "project_templates") &&
                fs::exists(fromMacro / "src")) {
                return fromMacro;
            }
        }
#endif
        fs::path probe = fs::current_path();
        while (!probe.empty()) {
            if (fs::exists(probe / "tools" / "project_templates") &&
                fs::exists(probe / "src")) {
                return probe;
            }
            fs::path parent = probe.parent_path();
            if (parent == probe) break;
            probe = parent;
        }
        return fs::current_path();
    }

    static std::string detectPlatform() {
#if defined(_WIN32)
        return "windows";
#elif defined(__APPLE__)
        return "macos";
#elif defined(__linux__)
        return "linux";
#else
        return "other";
#endif
    }

    fs::path m_sourceRoot;
    fs::path m_buildRoot;
    std::string m_platform;
};

} // namespace Caesura
