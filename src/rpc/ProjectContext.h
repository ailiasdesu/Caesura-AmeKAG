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

#include <cstring>
#include <filesystem>
#include <string>

// SDL_GetBasePath -- the SAME anchor main.cpp uses for the editor webRoot:
// the executable's own directory (SDL3: malloc'd UTF-8 string, NULL on
// failure). Declared by hand on purpose: the Rpc TU has no SDL include path,
// <windows.h> under /Zc:preprocessor exhausts the MSVC compiler heap (C1060)
// and its own redeclaration of GetModuleFileNameW conflicts (C2733), and
// bx/platform.h defines _WIN32_WINNT=0x0601 which cpp-httplib (included later
// in the same TU) rejects with "#error ... Windows 10 or later". The symbol
// resolves at link time -- every consumer of this header links SDL3.
extern "C" {
    const char* SDL_GetBasePath(void);
}

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

    // Executable's own directory via SDL_GetBasePath (declared above). The
    // returned UTF-8 string is owned by SDL -- never freed here.
    static fs::path executableDirectory() {
        const char* base = SDL_GetBasePath();
        if (base == nullptr || *base == '\0') return fs::current_path();
        // Paths outside ASCII (this checkout's repo path is not) must survive:
        // the input is UTF-8, and fs::path(char*) on MSVC would interpret it
        // via the ANSI code page (GBK on zh-CN) and resolve the wrong folder.
        std::string utf8(base);
        const fs::path dir = fs::path(
            std::u8string(reinterpret_cast<const char8_t*>(utf8.data()),
                          utf8.size())).parent_path();
        return dir.empty() ? fs::current_path() : dir;
    }

    static fs::path discoverSourceRoot(bool useMacroFirst) {
        // 1. Release-package anchor FIRST: the executable's own directory
        //    (mirrors main.cpp's SDL_GetBasePath webRoot anchoring). Three
        //    levels up covers the macOS .app/Contents/MacOS/ bundle layout
        //    that BUNDLE DESTINATION . already produces. This must win over
        //    CAESURA_SOURCE_DIR: on the machine that BUILT the package the
        //    macro still points at a live source tree, and an honest
        //    stranger-path run of that package would silently create projects
        //    inside the checkout instead of next to the executable.
        {
            fs::path probe = executableDirectory();
            for (int i = 0; i < 3 && !probe.empty(); ++i) {
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
        if (useMacroFirst) {
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
