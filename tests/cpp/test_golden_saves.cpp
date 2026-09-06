// ===========================================================================
//  test_golden_saves.cpp -- Golden Save cross-version migration fixtures
//  Synthetic schema envelopes exercise native migration. A separate actual
//  v1.0.1 release sample below carries producer provenance. Fixtures live in
//  tests/golden_saves/golden_save_v{1..5}.json -- plaintext envelopes in the
//  exact on-disk format SaveManager::save() writes:
//    {schema_version, timestamp, scene, token_index, thumbnail,
//     engine_version, data}
//
//  Version differences (mirror SaveManager::registerBuiltinMigrations):
//    v1 -> v2 : data gains "playtime" (int seconds)
//    v2 -> v3 : data gains "minigame"  (object)
//    v3 -> v4 : data gains "live2d"    (object)
//    v4 -> v5 : data gains "editor"    (object)
//  Each synthetic fixture carries fields defined by the selected schema; the
//  migration chain must supply the rest without touching existing keys.
// ===========================================================================

#include "doctest.h"
#include "storage/SaveManager.h"
#include "TestPaths.h"
#include "di/BackendRegistry.h"
#include "archive/CryptoEngine.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace Caesura;

namespace {

std::filesystem::path goldenDir() {
    return std::filesystem::path(CAESURA_SOURCE_DIR) / "tests" / "golden_saves";
}

std::string readFileBytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.is_open());
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

// Plant a golden envelope into a fresh save dir as save_<slot>.json so
// SaveManager::load() reads it through the real file -> decrypt -> parse ->
// migrate pipeline.
void plantGoldenSave(const std::filesystem::path& dir, int slot, int version) {
    const std::string bytes = readFileBytes(goldenDir() /
        ("golden_save_v" + std::to_string(version) + ".json"));
    const auto dst = dir / ("save_" + std::to_string(slot) + ".json");
    std::ofstream out(dst, std::ios::binary | std::ios::trunc);
    REQUIRE(out.is_open());
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    REQUIRE(out.good());
}

std::string markerOf(int version) { return "golden-v" + std::to_string(version); }

// Registers a real CryptoEngine in BackendRegistry for the duration of a
// test case, restoring whatever was there before (same pattern as
// test_save_roundtrip.cpp -- the helper is file-local there, so it is
// duplicated here rather than shared).
class ScopedCryptoRegistration {
public:
    ScopedCryptoRegistration()
        : m_previous(BackendRegistry::instance().getCryptoEngine()) {
        BackendRegistry::instance().setCryptoEngine(&m_engine);
    }
    ~ScopedCryptoRegistration() {
        BackendRegistry::instance().setCryptoEngine(m_previous);
    }
    ScopedCryptoRegistration(const ScopedCryptoRegistration&) = delete;
    ScopedCryptoRegistration& operator=(const ScopedCryptoRegistration&) = delete;

private:
    carc::CryptoEngine m_engine;
    carc::ICryptoEngine* m_previous;
};

} // namespace

// ---------------------------------------------------------------------------
// Every schema version loads and migrates to the current chain head.
// ---------------------------------------------------------------------------
TEST_CASE("GoldenSaves: every schema version v1..v5 loads and migrates to current") {
    for (int version = 1; version <= 5; ++version) {
        CAPTURE(version);
        TestPaths::ScopedTempDir dir("golden_migrate_v" + std::to_string(version));
        SaveManager sm;
        sm.init(dir.string());
        sm.clearEncryptionKey();
        const int current = sm.currentSchemaVersion();
        REQUIRE(current >= 5);

        const int slot = version % 100;
        plantGoldenSave(dir.path(), slot, version);

        SaveMeta meta;
        json loaded = sm.load(slot, &meta);
        REQUIRE_FALSE(loaded.empty());

        // Meta reports the POST-migration schema version.
        CHECK(meta.schemaVersion == current);
        CHECK(meta.slot == slot);
        CHECK(meta.timestamp > 0);

        // Pre-migration identity fields survive untouched.
        CHECK(loaded["marker"].get_ref<const std::string&>() == markerOf(version));
        CHECK(loaded["player"]["name"] == "Ame");
        CHECK(loaded["current_label"].is_string());
        CHECK(loaded["settings"]["bgm_volume"].is_number());

        // The additive chain ran to completion for every source version.
        REQUIRE(loaded.contains("playtime"));
        REQUIRE(loaded.contains("minigame"));
        REQUIRE(loaded.contains("live2d"));
        REQUIRE(loaded.contains("editor"));

        switch (version) {
        case 1:  // nothing of the later eras existed; chain defaults them all
            CHECK(loaded["playtime"] == 0);
            CHECK(loaded["minigame"].empty());
            CHECK(loaded["live2d"].empty());
            CHECK(loaded["editor"].empty());
            break;
        case 2:  // playtime authored by v2 survives verbatim
            CHECK(loaded["playtime"] == 3725);
            CHECK(loaded["minigame"].empty());
            CHECK(loaded["live2d"].empty());
            CHECK(loaded["editor"].empty());
            break;
        case 3:  // playtime + minigame payload preserved
            CHECK(loaded["playtime"] == 9875);
            CHECK(loaded["minigame"]["high_score"] == 12500);
            CHECK(loaded["minigame"]["courses_unlocked"] == 2);
            CHECK(loaded["live2d"].empty());
            CHECK(loaded["editor"].empty());
            break;
        case 4:  // playtime + minigame + live2d payload preserved
            CHECK(loaded["playtime"] == 15430);
            CHECK(loaded["minigame"]["high_score"] == 18300);
            CHECK(loaded["live2d"]["model_id"] == "hana_school_uniform");
            CHECK(loaded["live2d"]["expression"] == "smile");
            CHECK(loaded["editor"].empty());
            break;
        case 5:  // current version: loaded exactly as authored
        default:
            CHECK(loaded["playtime"] == 21600);
            CHECK(loaded["minigame"]["high_score"] == 20100);
            CHECK(loaded["live2d"]["model_id"] == "hana_school_uniform");
            CHECK(loaded["live2d"]["expression"] == "tearful_smile");
            CHECK(loaded["editor"]["active_project"] == "golden_vn");
            CHECK(loaded["editor"]["cursor_line"] == 1024);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Fixture fidelity: each raw sample really is shaped like its era. Keeps the
// fixtures honest when the migration chain grows a v6 -- an accidentally
// regenerated/edited fixture fails here instead of weakening the load tests.
// ---------------------------------------------------------------------------
TEST_CASE("GoldenSaves: fixtures stay faithful to their era (raw envelope audit)") {
    struct Era {
        int version;
        std::vector<const char*> laterKeys; // fields this era must NOT have
    };
    const std::vector<Era> eras = {
        {1, {"playtime", "minigame", "live2d", "editor"}},
        {2, {"minigame", "live2d", "editor"}},
        {3, {"live2d", "editor"}},
        {4, {"editor"}},
        {5, {}},
    };

    for (const auto& era : eras) {
        CAPTURE(era.version);
        const auto bytes = readFileBytes(goldenDir() /
            ("golden_save_v" + std::to_string(era.version) + ".json"));
        json envelope = json::parse(bytes);

        CHECK(envelope["schema_version"] == era.version);
        CHECK(envelope.contains("scene"));
        CHECK(envelope.contains("token_index"));
        CHECK(envelope.contains("timestamp"));
        CHECK(envelope.contains("thumbnail"));
        CHECK(envelope.contains("engine_version"));
        CHECK(envelope.contains("data"));
        CHECK(envelope["data"].is_object());

        const json& data = envelope["data"];
        CHECK(data["marker"].get_ref<const std::string&>() == markerOf(era.version));
        for (const char* key : era.laterKeys) {
            CHECK_FALSE(data.contains(key));
        }
    }
}

// ---------------------------------------------------------------------------
// listSaves surfaces all five golden envelopes with their on-disk metadata
// (listSaves reports the RAW stored schema_version, no migration).
// ---------------------------------------------------------------------------
TEST_CASE("GoldenSaves: listSaves surfaces all five golden envelopes") {
    TestPaths::ScopedTempDir dir("golden_list");
    SaveManager sm;
    sm.init(dir.string());

    const std::vector<std::string> scenes = {
        "prologue_station", "chapter1_classroom", "chapter2_festival",
        "chapter3_rooftop", "finale_epilogue",
    };
    for (int version = 1; version <= 5; ++version) {
        plantGoldenSave(dir.path(), version - 1, version);
    }

    auto saves = sm.listSaves();
    REQUIRE(saves.size() == 5);
    for (size_t i = 0; i < saves.size(); ++i) {
        CAPTURE(i);
        CHECK(saves[i].slot == static_cast<int>(i));
        CHECK(saves[i].sceneName == scenes[i]);
        CHECK(saves[i].schemaVersion == static_cast<int>(i) + 1);
        CHECK(saves[i].tokenIndex > 0);
        CHECK(saves[i].timestamp > 0);
    }
}

// ---------------------------------------------------------------------------
// Encryption x migration: a CAES-framed copy of the oldest golden save still
// migrates after decryption, and the engine's own keyed save output is really
// encrypted on disk and loads back identical.
// ---------------------------------------------------------------------------
TEST_CASE("GoldenSaves: encrypted golden v1 migrates after decryption") {
    ScopedCryptoRegistration cryptoRegistration; // registers carc::CryptoEngine
    TestPaths::ScopedTempDir dir("golden_encrypted");
    SaveManager sm;
    sm.init(dir.string());

    uint8_t key[32];
    for (int i = 0; i < 32; ++i) key[i] = static_cast<uint8_t>(0xA0 + i);
    sm.setEncryptionKey(key);
    REQUIRE(sm.isEncryptionEnabled());

    // Wrap the plaintext v1 envelope in the CAES frame the engine writes:
    // [4B magic][12B nonce][16B tag][ciphertext]
    const std::string plain = readFileBytes(goldenDir() / "golden_save_v1.json");
    carc::CryptoEngine crypto;
    uint8_t nonce[12];
    uint8_t tag[16];
    crypto.generateNonce(nonce, 12);
    auto cipher = crypto.encrypt(reinterpret_cast<const uint8_t*>(plain.data()),
                                 plain.size(), key, 32, nonce, 12, tag, 16);
    REQUIRE_FALSE(cipher.empty());

    std::string framed;
    framed.reserve(4 + sizeof(nonce) + sizeof(tag) + cipher.size());
    framed.append("CAES", 4);
    framed.append(reinterpret_cast<const char*>(nonce), sizeof(nonce));
    framed.append(reinterpret_cast<const char*>(tag), sizeof(tag));
    framed.append(reinterpret_cast<const char*>(cipher.data()), cipher.size());

    {
        std::ofstream out(dir.path() / "save_42.json", std::ios::binary | std::ios::trunc);
        REQUIRE(out.is_open());
        out.write(framed.data(), static_cast<std::streamsize>(framed.size()));
    }

    SaveMeta meta;
    json loaded = sm.load(42, &meta);
    REQUIRE_FALSE(loaded.empty());  // decrypted successfully with the right key
    CHECK(meta.schemaVersion == sm.currentSchemaVersion());  // migrated too
    CHECK(loaded["marker"].get_ref<const std::string&>() == "golden-v1");
    CHECK(loaded["playtime"] == 0);          // v1 -> v2 default
    CHECK(loaded["minigame"].empty());       // v2 -> v3 default
    CHECK(loaded.contains("editor"));        // chain ran to the head

    // Engine-keyed save output round-trips byte-level: really CAES on disk,
    // and load returns the identical data object.
    const json payload = loaded;
    REQUIRE(sm.save(43, payload, "encrypted_golden_v5", 99));
    {
        const auto bytes = readFileBytes(dir.path() / "save_43.json");
        REQUIRE(bytes.size() >= 32);
        CHECK(std::memcmp(bytes.data(), "CAES", 4) == 0);
    }
    const json back = sm.load(43);
    REQUIRE_FALSE(back.empty());
    CHECK(back == payload);
}

TEST_CASE("U11: a captured v1.0.1 release KAG save loads through the current native reader") {
    const auto bytes = readFileBytes(goldenDir() / "release-v1.0.1-kag-save.json");
    const auto provenance = json::parse(readFileBytes(goldenDir() / "release-v1.0.1-provenance.json"));
    uint8_t hash[32]{};
    carc::CryptoEngine::sha256(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), hash);
    const char* hex = "0123456789abcdef";
    std::string digest;
    for (const auto byte : hash) { digest += hex[byte >> 4]; digest += hex[byte & 15]; }
    REQUIRE(digest == "1144e1968848fc0a9e9519fdc6f88f13fe9a446b946fd5bc0b52624f6abb125b");
    CHECK(provenance.at("save_sha256") == digest);
    CHECK(provenance.at("kind") == "released-engine-produced-kag-save");
    CHECK(provenance.at("release_tag") == "v1.0.1");
    CHECK(provenance.at("envelope_schema_version") == 5);
    CHECK(provenance.at("kag_data_schema_version") == 2);
    CHECK(provenance.at("exit_code") == 0);

    TestPaths::ScopedTempDir directory("u11_real_legacy");
    {
        std::ofstream output(directory.path() / "save_37.json", std::ios::binary);
        REQUIRE(output.good());
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        REQUIRE(output.good());
    }
    SaveManager manager;
    manager.init(directory.string());
    SaveMeta meta;
    const auto loaded = manager.load(37, &meta);
    REQUIRE(loaded.is_object());
    CHECK(meta.schemaVersion == manager.currentSchemaVersion());
    CHECK(meta.sceneName == "tests/projects/u11_legacy_release/story.ks");
    CHECK(meta.tokenIndex == 4);
    CHECK(loaded.at("schema_version") == 2);
    CHECK(loaded.at("f").at("route") == "forest");
    CHECK(loaded.at("f").at("score") == 7);
    CHECK(loaded.at("f").at("legacy_nested").at("enabled") == true);
    CHECK(loaded.at("f").at("legacy_nested").at("sequence") == json::array({2, 4, 8}));
    CHECK(loaded.at("sf").at("profile_marker") == "release-v1.0.1");
    CHECK(readFileBytes(directory.path() / "save_37.json") == bytes);
}
