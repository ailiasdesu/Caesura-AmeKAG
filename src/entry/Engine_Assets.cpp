// Engine_Assets.cpp -- composition-root registration of CARC asset providers.
//
// LAYERED VFS MOUNT POLICY (C1)
// -----------------------------------------------------------------------------
// The engine mounts every recognized .carc it finds next to the executable's
// working directory plus an optional dlc/ subdirectory, ordered by priority
// (ProviderChain::sortByPriority resolves reads highest-first):
//
//   40  patch.carc, patch_*.carc     hotfix layer -- overrides everything
//   30  dlc_*.carc, dlc/*.carc       downloadable content
//   20  lang_*.carc                  localization packs
//   10  base.carc, data.carc, game.carc   shipped game data
//   (5)  DirAssetProvider "" and "assets"  -- loose files, registered by
//        AssetManager::init(), always the lowest layer
//
// A file whose name matches none of those patterns is DELIBERATELY NOT mounted:
// an unrecognized .carc in the game directory stays inert instead of joining
// the chain at a guessed priority.
//
// TRUST BOUNDARY (audited decision, t13)
// -----------------------------------------------------------------------------
// Auto-mounting patch_*.carc at the top priority does NOT widen the engine's
// attack surface: anyone able to drop a file into the game directory can
// already replace base.carc or the executable itself, so no additional
// signature gate here would contain them. (CARCReader::verifySignature exists
// but needs a public key that the shipping layout does not provide; no caller
// in the engine invokes IAssetProvider::verify today -- that is a separate,
// unbuilt trust-chain feature, not something this mount path can fake.)
//
// The realistic hazard is OPERATIONAL, not adversarial: a leftover or
// half-downloaded patch_*.carc silently overriding shipped content, with the
// symptom appearing far from the cause. The mitigation is therefore
// diagnostic, not preventive:
//   * every mount is printed unconditionally (printf, not DEBUG_INFO, which
//     compiles to nothing in a Release build -- exactly the build a player
//     bug report comes from);
//   * a patch layer is additionally reported through DEBUG_WARN (unconditional
//     in every build) so "an override was active" is visible in logs/ without
//     having to reproduce with a debug binary.

#include "../archive/CARCReader.h"
#include "../archive/CarcAssetProvider.h"
#include "../resource/AssetManager.h"
#include "../debug/api/DebugLog.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <set>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace Caesura {

// Mount priorities. Named so the table is greppable from the tests and from
// the docs instead of being seven magic numbers inline.
constexpr int kCarcPriorityPatch = 40;
constexpr int kCarcPriorityDlc   = 30;
constexpr int kCarcPriorityLang  = 20;
constexpr int kCarcPriorityBase  = 10;
constexpr int kCarcPriorityNone  = -1;  // unrecognized -> not mounted

// Classify one archive file name. `inDlcDir` marks files found inside dlc/,
// which are treated as DLC regardless of their file name.
// Exposed (not in an anonymous namespace) so tests can pin the table.
int carcMountPriority(const std::string& filename, bool inDlcDir) {
    if (filename == "patch.carc" || filename.rfind("patch_", 0) == 0) {
        return kCarcPriorityPatch;
    }
    if (filename.rfind("dlc_", 0) == 0 || inDlcDir) {
        return kCarcPriorityDlc;
    }
    if (filename.rfind("lang_", 0) == 0) {
        return kCarcPriorityLang;
    }
    if (filename == "base.carc" || filename == "data.carc" || filename == "game.carc") {
        return kCarcPriorityBase;
    }
    return kCarcPriorityNone;
}

// One resolved mount candidate. Internal to this translation unit: tests
// observe the plan through carcMountPlan() below, which returns a flat
// (name, priority) list so no aggregate type has to cross a TU boundary.
namespace {
struct CarcMountCandidate {
    std::string path;   // path to open
    std::string name;   // file name (identity for dedup + logging)
    int         priority;
};
}  // namespace

// Collect the mounts for a game root: root/*.carc plus root/dlc/*.carc,
// sorted by descending priority then by name so the order is deterministic
// regardless of directory-iteration order.
//
// Dedup is keyed by FILE NAME, not by the iterator's path spelling. The
// earlier draft keyed on fs::path::generic_string() ("./base.carc" when
// scanning ".") and then re-checked a fallback list with the bare name
// ("base.carc"), so the key never matched and base.carc was mounted TWICE --
// observed as two "Registered CARC (priority 10)" lines for one file. Two
// providers over the same archive is not merely noisy: each holds its own
// open stream and read position for the same bytes.
static std::vector<CarcMountCandidate> collectCarcMounts(const fs::path& root) {
    std::vector<CarcMountCandidate> candidates;
    std::set<std::string> seenNames;
    std::error_code ec;

    auto scan = [&](const fs::path& dir, bool inDlcDir) {
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file(ec)) continue;
            if (entry.path().extension().string() != ".carc") continue;

            const std::string filename = entry.path().filename().string();
            const int priority = carcMountPriority(filename, inDlcDir);
            if (priority == kCarcPriorityNone) continue;      // inert by design
            if (!seenNames.insert(filename).second) continue;  // already mounted

            candidates.push_back({ entry.path().generic_string(), filename, priority });
        }
    };

    scan(root, false);
    scan(root / "dlc", true);

    std::sort(candidates.begin(), candidates.end(),
              [](const CarcMountCandidate& a, const CarcMountCandidate& b) {
                  if (a.priority != b.priority) return a.priority > b.priority;
                  return a.name < b.name;
              });
    return candidates;
}

// Test seam: the resolved mount plan for `root`, as (file name, priority) in
// mount order. Lets the mount policy -- classification, dedup, ordering, and
// the "unrecognized names stay inert" rule -- be asserted without a GPU, an
// Engine instance, or a process-wide chdir.
std::vector<std::pair<std::string, int>> carcMountPlan(const std::string& root) {
    std::vector<std::pair<std::string, int>> plan;
    for (const auto& cand : collectCarcMounts(fs::path(root))) {
        plan.emplace_back(cand.name, cand.priority);
    }
    return plan;
}

void registerDefaultAssetProviders(AssetManager& assetManager) {
    for (const auto& cand : collectCarcMounts(".")) {
        auto reader = std::make_unique<carc::CARCReader>();
        if (!reader->open(cand.path)) continue;

        assetManager.addProvider(std::make_unique<carc::CarcAssetProvider>(
            std::move(reader), cand.priority, "CARC:" + cand.name));

        // Unconditional: this record must survive a Release build, because
        // "which layer served this asset" is the first question a player bug
        // report raises.
        printf("[Engine] Registered CARC (priority %d): %s\n",
               cand.priority, cand.path.c_str());

        if (cand.priority == kCarcPriorityPatch) {
            // DEBUG_WARN is not compiled out in Release (unlike DEBUG_INFO).
            DEBUG_WARN(SubSys::Archive, ErrCode::Ok,
                       "[Engine] Patch layer active: %s overrides shipped content",
                       cand.name.c_str());
        }
    }
}

} // namespace Caesura
