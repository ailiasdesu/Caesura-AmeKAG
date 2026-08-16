#include "PathConfinement.h"

#include <cctype>
#include <filesystem>

namespace Caesura {

std::string confineToModelRoot(const std::string& path) {
    namespace fs = std::filesystem;
    // [round 97] Fail closed on empty input: fs::absolute("") resolves to
    // the CWD on macOS (libc++) but to an empty path on Windows, so without
    // this guard an empty path is accepted as 'inside the root' on macOS
    // (round 96 test caught the divergence).
    if (path.empty()) return {};
    std::error_code ec;
    const fs::path root = fs::current_path(ec);
    if (ec) return {};

    fs::path abs = fs::absolute(fs::path(path), ec);
    if (ec) return {};
    fs::path canon;
    {
        std::error_code ec2;
        canon = fs::weakly_canonical(abs, ec2);
        if (ec2) {
            // Canonicalization failed (e.g. non-ASCII path under Windows ACP).
            // Resolve the existing parent prefix so symlinked/junctioned
            // directories are still containment-checked, then re-attach the
            // file name. If even the parent cannot be resolved, reject.
            const fs::path parentCanon = fs::canonical(abs.parent_path(), ec2);
            if (ec2) return {};
            const fs::path leaf = abs.filename();
            // Reject dot components: they are only reachable when
            // weakly_canonical errored and could bypass the prefix check.
            if (leaf == "." || leaf == "..") return {};
            canon = parentCanon / leaf;
            // The final component is re-attached lexically; reject when it is
            // a symlink so it cannot point outside the root (directories are
            // already contained by the canonicalized parent). Fail closed on
            // stat errors, and reject Windows junctions explicitly (MSVC
            // is_symlink reports file_type::junction as false).
            std::error_code ec3;
            const fs::file_status st = fs::symlink_status(canon, ec3);
            if (ec3 || fs::is_symlink(st)) return {};
#ifdef _WIN32
            if (st.type() == fs::file_type::junction) return {};
#endif
        }
    }
    fs::path rootNorm = fs::weakly_canonical(root, ec);
    if (ec) rootNorm = root.lexically_normal();

    auto norm = [](std::string s) {
        for (char& ch : s) {
            if (ch == static_cast<char>(92)) ch = '/';  // backslash
#ifdef _WIN32
            // Case-insensitive comparison only on Windows (POSIX is
            // case-sensitive; folding there would widen containment).
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
#endif
        }
        return s;
    };
    const std::string r = norm(rootNorm.string());
    const std::string c = norm(canon.string());
    if (c == r) return canon.string();              // the root itself
    if (c.rfind(r + "/", 0) == 0) return canon.string();  // under the root
    return {};                                      // escaped or outside root
}

} // namespace Caesura
