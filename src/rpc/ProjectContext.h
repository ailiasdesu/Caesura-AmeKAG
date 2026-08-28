#pragma once
// ProjectContext.h -- single source of truth for filesystem roots in the
// editor layer (Validation-Release task book §7 "ProjectContext / Path
// resolver 收口").
//
// Rule: business code must not write "fs::current_path() / ..." to guess
// engine roots. Everything resolves through ProjectContext:
//
//   ProjectContext
//   ├── sourceRoot   (injected exe anchor > CAESURA_SOURCE_DIR macro > CWD walk)
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
    //   exeDir: optional executable-directory anchor injected by the
    //   composition root (src/main.cpp -> EditorServer::setSourceAnchor),
    //   mirroring the webRoot SDL_GetBasePath anchoring. When non-empty it is
    //   probed FIRST and wins over the compile-time CAESURA_SOURCE_DIR macro:
    //   a release package must resolve to itself even when it was built on a
    //   machine whose macro still points at a live source tree (Sprint 4 --
    //   the anchor always precedes the macro, never the other way round).
    //   When empty the anchor step is skipped and the macro (in-tree /
    //   out-of-tree dev builds) then the CWD walk-up are used -- the
    //   pre-injection dev and test behavior.
    static ProjectContext fromEnvironment(const fs::path& exeDir = {}) {
        ProjectContext c;
        c.m_sourceRoot = discoverSourceRoot(exeDir);
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
    // A candidate root is either a source checkout (tools/project_templates +
    // src) or a release package layout (tools/project_templates + scripts +
    // demo, NO src/ -- source never ships). Both map to the SAME sourceRoot so
    // Project Manager templates resolve identically in-tree and in a package.
    static bool looksLikeEngineRoot(const fs::path& p) {
        std::error_code ec;
        if (!fs::exists(p / "tools" / "project_templates", ec)) return false;
        if (fs::exists(p / "src", ec)) return true;                          // checkout
        return fs::exists(p / "scripts", ec) && fs::exists(p / "demo", ec);  // package
    }

    static fs::path discoverSourceRoot(const fs::path& exeDir) {
        // 1. Composition-root anchor (EXECUTABLE's own directory, injected by
        //    src/main.cpp -> EditorServer::setSourceAnchor, mirroring the
        //    webRoot SDL_GetBasePath anchoring). Three levels up covers the
        //    macOS .app/Contents/MacOS/ bundle layout that BUNDLE DESTINATION
        //    . already produces. MUST win over CAESURA_SOURCE_DIR: on the
        //    machine that BUILT the package the macro still points at a live
        //    source tree, and an honest stranger-path run of that package
        //    would silently create projects inside the checkout instead of
        //    next to the executable.
        if (!exeDir.empty()) {
            fs::path probe = exeDir;
            // Four probes = the executable's own directory plus three ascents:
            // a release package keeps the exe at the root (hit on the first
            // probe), while .app/Contents/MacOS needs all three.
            for (int i = 0; i < 4 && !probe.empty(); ++i) {
                if (looksLikeEngineRoot(probe)) return probe;
                fs::path parent = probe.parent_path();
                if (parent == probe) break;
                probe = parent;
            }
        }
        // 2. Compile-time source root: in-tree / out-of-tree dev builds
        //    (WSL/CI build dirs are NOT on the exe walk chain); the macro
        //    never exists on a stranger's machine.
#ifdef CAESURA_SOURCE_DIR
        {
            const fs::path fromMacro(CAESURA_SOURCE_DIR);
            if (looksLikeEngineRoot(fromMacro)) return fromMacro;
        }
#endif
        // 3. Legacy: walk up from the CWD (both layouts).
        fs::path probe = fs::current_path();
        while (!probe.empty()) {
            if (looksLikeEngineRoot(probe)) return probe;
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
