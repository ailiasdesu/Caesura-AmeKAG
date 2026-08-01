#pragma once

#include <string>

namespace Caesura {

// Security: every file read triggered by model loading (model3.json, .moc3,
// textures, expressions, FrameworkShaders/*.fx via cubismLoadFile) must stay
// under the process working directory ("model root"). Rejects `..` escapes and
// absolute paths outside the root. Symlinks are resolved when possible;
// on failure we fall back to lexical normalization so UTF-8/Chinese paths
// keep working under Windows ACP.
//
// Returns the canonical confined path when the input resolves inside the
// process working directory, or an empty string when it escapes / cannot be
// verified (fail closed).
//
// This helper intentionally has no dependency on the Cubism SDK so it can be
// unit-tested in every build configuration (CAESURA_HAS_LIVE2D on or off).
std::string confineToModelRoot(const std::string& path);

} // namespace Caesura
